# RelayForge: Technical Design Decisions & Trade-Offs

## 1. Delivery Guarantees: Why Universal Exactly-Once Is an Illusion

A core mandate in production distributed systems is intellectual honesty regarding delivery guarantees:
> **RelayForge provides strict At-Least-Once Delivery Semantics with Idempotent De-duplication.**

### 1.1 The Theoretical Boundary
In any distributed or networked environment subject to the Two Generals' Problem and network partitions:
- If a worker crashes **after** performing a non-transactional side-effect (e.g., dispatching an email, charging a credit card, updating an external warehouse) but **before** acknowledging completion in the database, the engine **must** re-deliver that event upon recovery.
- Claiming "universal exactly-once delivery" without distributed two-phase commit (2PC) or universal transaction coordinators across all external systems is fundamentally misleading.

### 1.2 The RelayForge Solution: Effectively-Once
RelayForge combines two primitives to achieve **effectively-once processing**:
1. **Idempotency Keys**: Upstream clients supply unique idempotency keys. Repeated submissions return the cached previous result without re-executing business logic.
2. **Audit Trails & Status Verification**: Every execution records attempts, duration, and output in the `event_audit_log` table.

---

## 2. Storage Architecture: SQLite WAL Mode vs. Append-Only Flat Files vs. LSM-Trees

| Dimension | Pure Flat File Log | LSM-Tree (e.g. RocksDB) | SQLite with WAL Mode (RelayForge) |
|---|---|---|---|
| **Transactional Correctness** | Custom fsync protocol needed; corruptions during crash require custom repair. | Strong ACID, but heavy build dependencies and opaque C++ bindings. | Full ACID via standard WAL. Zero corruptions across crashes. |
| **Random Status Updates** | Expensive (requires compaction, rewrite, or tombstones). | Fast append, but point status queries require compaction. | $O(\log N)$ updates with B-Tree indices on `id`, `status`, and `scheduled_at`. |
| **Crash Recovery Time** | Must scan entire log file sequentially ($O(N)$). | Must replay write-ahead log & SSTable manifests. | Instant query: `SELECT * FROM events WHERE status IN ('PENDING', 'IN_FLIGHT')` using index ($O(\log N)$). |
| **Concurrency** | Single writer, complex reader coordination. | High concurrency, high write amplification. | Single serialized writer with unlimited concurrent readers. |

**Verdict**: SQLite with WAL mode (`PRAGMA journal_mode = WAL`, `PRAGMA synchronous = NORMAL`) gives RelayForge rock-solid durability, zero data loss, sub-millisecond point updates, and instant crash recovery without external daemon overhead.

---

## 3. Priority Scheduling & In-Memory Queues

### 3.1 Design Choice: Dual Heap Architecture
Instead of a single priority queue with expensive timer re-scans, RelayForge separates:
1. **Active Priority Queue**: Sorted by `(priority ASC, created_at ASC)`. Workers block directly on condition variable `cv_active_` and pop immediately when events arrive.
2. **Delayed Retry Queue**: Min-heap sorted by `scheduled_at ASC`. A dedicated background timer thread sleeps on condition variable `cv_delayed_` until the earliest timestamp. When the timestamp expires, it shifts events to `Active Priority Queue`.

### 3.2 Bounded Queues vs. Unbounded Buffers (Backpressure Economics)
Many naive event engines use unbounded in-memory queues (e.g. `std::vector` or unbounded channels). When upstream traffic spikes:
- Memory grows unbounded until the OS Out-Of-Memory (OOM) killer terminates the process, causing sudden data loss.
- In RelayForge, `max_queue_depth` is strictly enforced. When the queue reaches capacity, `RelayEngine::ingest` returns `HTTP 429 Backpressure Exceeded`, protecting both the engine and already-persisted events.

---

## 4. Failure Handling: Why Full Jitter Trumps Pure Exponential Backoff

In standard exponential backoff:
$$T_{\text{wait}} = T_0 \cdot 2^{\text{attempt}-1}$$
If 1,000 requests fail simultaneously due to a temporary network blip, all 1,000 retry at exactly $T_0$, then all 1,000 retry at $2 T_0$. This causes massive **thundering herd storms** that repeatedly crash downstream dependencies.

In RelayForge's Full Jitter algorithm:
$$T_{\text{wait}} = \text{UniformRandom}(0, \min(T_{\text{max}}, T_0 \cdot 2^{\text{attempt}-1}))$$
This spreads retries across the entire interval, ensuring downstream services recover smoothly under a flattened retry curve.

---

## 5. Crash Recovery: Poison Pill Prevention

If an event payload has a flaw that triggers a catastrophic fatal signal or segmentation violation in user code, an uncontrolled retry loop will crash the engine indefinitely every time it reboots.

**RelayForge's Defense**:
- When `RecoveryManager` detects an event in `IN_FLIGHT` on boot, it records that an execution attempt occurred.
- If `attempt_count >= max_retries`, the event is quarantined directly to `DEAD_LETTER`.
- This ensures a poison pill can never cause a reboot loop.
