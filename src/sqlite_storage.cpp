#include "relayforge/sqlite_storage.hpp"
#include <iostream>
#include <sstream>

namespace relayforge {

SqliteStorage::SqliteStorage(std::string db_path, bool enable_wal, bool sync_normal)
    : db_path_(std::move(db_path)), enable_wal_(enable_wal), sync_normal_(sync_normal) {}

SqliteStorage::~SqliteStorage() {
    close();
}

void SqliteStorage::close() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    if (db_) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
}

bool SqliteStorage::execute_sql(const std::string& sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "[SqliteStorage] SQL error: " << (err_msg ? err_msg : "unknown")
                  << " in query: " << sql << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool SqliteStorage::initialize() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    int rc = sqlite3_open_v2(db_path_.c_str(), &db_, flags, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "[SqliteStorage] Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    sqlite3_busy_timeout(db_, 5000);

    if (enable_wal_) {
        execute_sql("PRAGMA journal_mode = WAL;");
    }
    if (sync_normal_) {
        execute_sql("PRAGMA synchronous = NORMAL;");
    } else {
        execute_sql("PRAGMA synchronous = FULL;");
    }
    execute_sql("PRAGMA foreign_keys = ON;");
    execute_sql("PRAGMA temp_store = MEMORY;");

    const char* schema_sql = R"(
        CREATE TABLE IF NOT EXISTS events (
            id TEXT PRIMARY KEY,
            idempotency_key TEXT UNIQUE,
            event_type TEXT NOT NULL,
            priority INTEGER NOT NULL,
            payload TEXT NOT NULL,
            status TEXT NOT NULL,
            attempt_count INTEGER NOT NULL DEFAULT 0,
            max_retries INTEGER NOT NULL DEFAULT 3,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL,
            scheduled_at INTEGER NOT NULL,
            last_error TEXT,
            execution_result TEXT
        );

        CREATE INDEX IF NOT EXISTS idx_events_status ON events(status);
        CREATE INDEX IF NOT EXISTS idx_events_priority ON events(priority);
        CREATE INDEX IF NOT EXISTS idx_events_scheduled ON events(scheduled_at);
        CREATE INDEX IF NOT EXISTS idx_events_created ON events(created_at);

        CREATE TABLE IF NOT EXISTS event_audit_log (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            event_id TEXT NOT NULL,
            attempt_number INTEGER NOT NULL,
            status_transition TEXT NOT NULL,
            error_message TEXT,
            duration_ms REAL,
            timestamp INTEGER NOT NULL,
            FOREIGN KEY(event_id) REFERENCES events(id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_audit_event_id ON event_audit_log(event_id);
    )";

    return execute_sql(schema_sql);
}

Event SqliteStorage::parse_event_row(sqlite3_stmt* stmt) {
    Event e;
    e.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    const unsigned char* ikey = sqlite3_column_text(stmt, 1);
    e.idempotency_key = ikey ? reinterpret_cast<const char*>(ikey) : "";
    e.event_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    e.priority = static_cast<EventPriority>(sqlite3_column_int(stmt, 3));
    e.payload = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    e.status = status_from_string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
    e.attempt_count = sqlite3_column_int(stmt, 6);
    e.max_retries = sqlite3_column_int(stmt, 7);
    e.created_at = sqlite3_column_int64(stmt, 8);
    e.updated_at = sqlite3_column_int64(stmt, 9);
    e.scheduled_at = sqlite3_column_int64(stmt, 10);
    const unsigned char* lerr = sqlite3_column_text(stmt, 11);
    e.last_error = lerr ? reinterpret_cast<const char*>(lerr) : "";
    const unsigned char* res = sqlite3_column_text(stmt, 12);
    e.execution_result = res ? reinterpret_cast<const char*>(res) : "";
    return e;
}

std::pair<bool, Event> SqliteStorage::insert_event(const Event& event) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    // Check if idempotency key exists
    if (!event.idempotency_key.empty()) {
        const char* check_sql = "SELECT id, idempotency_key, event_type, priority, payload, status, "
                                "attempt_count, max_retries, created_at, updated_at, scheduled_at, "
                                "last_error, execution_result FROM events WHERE idempotency_key = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, check_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, event.idempotency_key.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                Event existing = parse_event_row(stmt);
                sqlite3_finalize(stmt);
                return {false, existing};
            }
            sqlite3_finalize(stmt);
        }
    }

    const char* insert_sql = "INSERT INTO events (id, idempotency_key, event_type, priority, payload, "
                             "status, attempt_count, max_retries, created_at, updated_at, scheduled_at, "
                             "last_error, execution_result) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[SqliteStorage] Prepare insert error: " << sqlite3_errmsg(db_) << std::endl;
        return {false, event};
    }

    int64_t now = current_time_ms();
    int64_t created = event.created_at ? event.created_at : now;
    int64_t scheduled = event.scheduled_at ? event.scheduled_at : now;

    sqlite3_bind_text(stmt, 1, event.id.c_str(), -1, SQLITE_TRANSIENT);
    if (!event.idempotency_key.empty()) {
        sqlite3_bind_text(stmt, 2, event.idempotency_key.c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 2);
    }
    sqlite3_bind_text(stmt, 3, event.event_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, static_cast<int>(event.priority));
    sqlite3_bind_text(stmt, 5, event.payload.c_str(), -1, SQLITE_TRANSIENT);
    std::string status_str = to_string(event.status);
    sqlite3_bind_text(stmt, 6, status_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, event.attempt_count);
    sqlite3_bind_int(stmt, 8, event.max_retries);
    sqlite3_bind_int64(stmt, 9, created);
    sqlite3_bind_int64(stmt, 10, now);
    sqlite3_bind_int64(stmt, 11, scheduled);
    sqlite3_bind_text(stmt, 12, event.last_error.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, event.execution_result.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        if (rc == SQLITE_CONSTRAINT) {
            // Duplicate detected concurrently
            const char* check_sql = "SELECT id, idempotency_key, event_type, priority, payload, status, "
                                    "attempt_count, max_retries, created_at, updated_at, scheduled_at, "
                                    "last_error, execution_result FROM events WHERE idempotency_key = ? LIMIT 1;";
            sqlite3_stmt* check_stmt = nullptr;
            if (sqlite3_prepare_v2(db_, check_sql, -1, &check_stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(check_stmt, 1, event.idempotency_key.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(check_stmt) == SQLITE_ROW) {
                    Event existing = parse_event_row(check_stmt);
                    sqlite3_finalize(check_stmt);
                    return {false, existing};
                }
                sqlite3_finalize(check_stmt);
            }
        }
        std::cerr << "[SqliteStorage] Insert failed, code: " << rc << " msg: " << sqlite3_errmsg(db_) << std::endl;
        return {false, event};
    }

    // Write audit log
    const char* audit_sql = "INSERT INTO event_audit_log (event_id, attempt_number, status_transition, error_message, duration_ms, timestamp) "
                            "VALUES (?, 0, 'INGESTED', '', 0.0, ?);";
    sqlite3_stmt* audit_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, audit_sql, -1, &audit_stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(audit_stmt, 1, event.id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(audit_stmt, 2, now);
        sqlite3_step(audit_stmt);
        sqlite3_finalize(audit_stmt);
    }

    Event inserted = event;
    inserted.created_at = created;
    inserted.updated_at = now;
    inserted.scheduled_at = scheduled;
    return {true, inserted};
}

bool SqliteStorage::update_event_status(
    const std::string& event_id,
    EventStatus new_status,
    int attempt_count,
    const std::string& last_error,
    const std::string& execution_result,
    int64_t scheduled_at,
    double duration_ms
) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    int64_t now = current_time_ms();
    int64_t sched = (scheduled_at > 0) ? scheduled_at : now;

    const char* sql = "UPDATE events SET status = ?, attempt_count = ?, last_error = ?, "
                      "execution_result = ?, updated_at = ?, scheduled_at = ? WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    std::string new_status_str = to_string(new_status);
    sqlite3_bind_text(stmt, 1, new_status_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, attempt_count);
    sqlite3_bind_text(stmt, 3, last_error.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, execution_result.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, now);
    sqlite3_bind_int64(stmt, 6, sched);
    sqlite3_bind_text(stmt, 7, event_id.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return false;
    }

    // Record audit entry
    const char* audit_sql = "INSERT INTO event_audit_log (event_id, attempt_number, status_transition, error_message, duration_ms, timestamp) "
                            "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* audit_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, audit_sql, -1, &audit_stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(audit_stmt, 1, event_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(audit_stmt, 2, attempt_count);
        sqlite3_bind_text(audit_stmt, 3, new_status_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(audit_stmt, 4, last_error.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(audit_stmt, 5, duration_ms);
        sqlite3_bind_int64(audit_stmt, 6, now);
        sqlite3_step(audit_stmt);
        sqlite3_finalize(audit_stmt);
    }

    return true;
}

std::optional<Event> SqliteStorage::get_event(const std::string& event_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "SELECT id, idempotency_key, event_type, priority, payload, status, "
                      "attempt_count, max_retries, created_at, updated_at, scheduled_at, "
                      "last_error, execution_result FROM events WHERE id = ? LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, event_id.c_str(), -1, SQLITE_STATIC);
    std::optional<Event> result = std::nullopt;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_event_row(stmt);
    }
    sqlite3_finalize(stmt);
    return result;
}

std::optional<Event> SqliteStorage::get_event_by_idempotency_key(const std::string& key) {
    if (key.empty()) return std::nullopt;
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "SELECT id, idempotency_key, event_type, priority, payload, status, "
                      "attempt_count, max_retries, created_at, updated_at, scheduled_at, "
                      "last_error, execution_result FROM events WHERE idempotency_key = ? LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
    std::optional<Event> result = std::nullopt;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_event_row(stmt);
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<Event> SqliteStorage::get_unsettled_events() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<Event> list;

    // Events that were IN_FLIGHT, PENDING, or FAILED_RETRYABLE during restart
    const char* sql = "SELECT id, idempotency_key, event_type, priority, payload, status, "
                      "attempt_count, max_retries, created_at, updated_at, scheduled_at, "
                      "last_error, execution_result FROM events "
                      "WHERE status IN ('PENDING', 'IN_FLIGHT', 'FAILED_RETRYABLE') "
                      "ORDER BY priority ASC, scheduled_at ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return list;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        list.push_back(parse_event_row(stmt));
    }
    sqlite3_finalize(stmt);
    return list;
}

std::vector<Event> SqliteStorage::get_dead_letter_events(int limit, int offset) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<Event> list;

    const char* sql = "SELECT id, idempotency_key, event_type, priority, payload, status, "
                      "attempt_count, max_retries, created_at, updated_at, scheduled_at, "
                      "last_error, execution_result FROM events "
                      "WHERE status = 'DEAD_LETTER' "
                      "ORDER BY updated_at DESC LIMIT ? OFFSET ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return list;
    }

    sqlite3_bind_int(stmt, 1, limit);
    sqlite3_bind_int(stmt, 2, offset);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        list.push_back(parse_event_row(stmt));
    }
    sqlite3_finalize(stmt);
    return list;
}

bool SqliteStorage::reset_event_for_replay(const std::string& event_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    int64_t now = current_time_ms();
    const char* sql = "UPDATE events SET status = 'PENDING', attempt_count = 0, last_error = '', "
                      "updated_at = ?, scheduled_at = ? WHERE id = ? AND status = 'DEAD_LETTER';";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int64(stmt, 1, now);
    sqlite3_bind_int64(stmt, 2, now);
    sqlite3_bind_text(stmt, 3, event_id.c_str(), -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(db_);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE && changes > 0) {
        const char* audit_sql = "INSERT INTO event_audit_log (event_id, attempt_number, status_transition, error_message, duration_ms, timestamp) "
                                "VALUES (?, 0, 'REPLAY_QUEUED', '', 0.0, ?);";
        sqlite3_stmt* audit_stmt = nullptr;
        if (sqlite3_prepare_v2(db_, audit_sql, -1, &audit_stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(audit_stmt, 1, event_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(audit_stmt, 2, now);
            sqlite3_step(audit_stmt);
            sqlite3_finalize(audit_stmt);
        }
        return true;
    }
    return false;
}

std::vector<AuditLogEntry> SqliteStorage::get_audit_log(const std::string& event_id) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    std::vector<AuditLogEntry> logs;

    const char* sql = "SELECT id, event_id, attempt_number, status_transition, error_message, duration_ms, timestamp "
                      "FROM event_audit_log WHERE event_id = ? ORDER BY id ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return logs;
    }

    sqlite3_bind_text(stmt, 1, event_id.c_str(), -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AuditLogEntry entry;
        entry.id = sqlite3_column_int64(stmt, 0);
        entry.event_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        entry.attempt_number = sqlite3_column_int(stmt, 2);
        entry.status_transition = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const unsigned char* err = sqlite3_column_text(stmt, 4);
        entry.error_message = err ? reinterpret_cast<const char*>(err) : "";
        entry.duration_ms = sqlite3_column_double(stmt, 5);
        entry.timestamp = sqlite3_column_int64(stmt, 6);
        logs.push_back(entry);
    }
    sqlite3_finalize(stmt);
    return logs;
}

size_t SqliteStorage::count_events_by_status(EventStatus status) {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "SELECT COUNT(*) FROM events WHERE status = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    std::string s_str = to_string(status);
    sqlite3_bind_text(stmt, 1, s_str.c_str(), -1, SQLITE_TRANSIENT);
    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return count;
}

size_t SqliteStorage::count_total_events() {
    std::lock_guard<std::mutex> lock(db_mutex_);

    const char* sql = "SELECT COUNT(*) FROM events;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);
    return count;
}

} // namespace relayforge
