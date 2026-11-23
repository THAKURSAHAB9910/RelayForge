#pragma once

#include "relayforge/storage_engine.hpp"
#include <sqlite3.h>
#include <mutex>
#include <string>

namespace relayforge {

class SqliteStorage : public StorageEngine {
public:
    explicit SqliteStorage(std::string db_path, bool enable_wal = true, bool sync_normal = true);
    ~SqliteStorage() override;

    bool initialize() override;
    void close() override;

    std::pair<bool, Event> insert_event(const Event& event) override;

    bool update_event_status(
        const std::string& event_id,
        EventStatus new_status,
        int attempt_count,
        const std::string& last_error = "",
        const std::string& execution_result = "",
        int64_t scheduled_at = 0,
        double duration_ms = 0.0
    ) override;

    std::optional<Event> get_event(const std::string& event_id) override;
    std::optional<Event> get_event_by_idempotency_key(const std::string& key) override;

    std::vector<Event> get_unsettled_events() override;
    std::vector<Event> get_dead_letter_events(int limit = 100, int offset = 0) override;
    bool reset_event_for_replay(const std::string& event_id) override;

    std::vector<AuditLogEntry> get_audit_log(const std::string& event_id) override;

    size_t count_events_by_status(EventStatus status) override;
    size_t count_total_events() override;

    const std::string& get_db_path() const { return db_path_; }

private:
    std::string db_path_;
    bool enable_wal_;
    bool sync_normal_;
    sqlite3* db_ = nullptr;
    mutable std::mutex db_mutex_;

    bool execute_sql(const std::string& sql);
    Event parse_event_row(sqlite3_stmt* stmt);
};

} // namespace relayforge
