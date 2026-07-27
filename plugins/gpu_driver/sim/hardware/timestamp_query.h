// sim/hardware/timestamp_query.h - ADR-057 Profiling Hooks (Task 5.1)
//
// C-ABI timestamp query object: record logical tick at submission time,
// resolve later to read back the tick value.
//
// Task 5.1 scope: handle create/record/resolve/destroy lifecycle only.
// Global tick counter (Task 5.3) and Puller DISPATCH integration
// (Task 5.4) are NOT in this task.
#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

struct SimTimestampQuery;

/**
 * @brief Create a new timestamp query object.
 *
 * The query starts in an unrecorded state; resolve() will return -EAGAIN
 * until record() is called.
 *
 * @return Pointer to the new query, or nullptr on allocation failure.
 */
SimTimestampQuery* sim_timestamp_query_create(void);

/**
 * @brief Record a logical tick value into the query.
 *
 * After this call, resolve() will return the recorded tick.
 *
 * @param q           Query handle (may be nullptr; call is a no-op).
 * @param entry_index Pushbuffer entry index that triggered the record.
 * @param tick        Logical tick value at record time.
 */
void sim_timestamp_query_record(SimTimestampQuery* q,
                                uint64_t entry_index,
                                uint64_t tick);

/**
 * @brief Resolve the query to retrieve the recorded tick.
 *
 * @param q          Query handle (may be nullptr; returns -EINVAL).
 * @param timeout_ms Reserved for future use (Task 5.3+ may add polling).
 * @return Recorded tick value cast to int, or -EAGAIN if record() was
 *         never called, or -EINVAL if q is nullptr.
 */
int sim_timestamp_query_resolve(SimTimestampQuery* q, uint64_t timeout_ms);

/**
 * @brief Destroy a timestamp query object.
 *
 * Defensive: passing nullptr or an already-destroyed handle is a safe
 * no-op.
 *
 * @param q Query handle (may be nullptr).
 */
void sim_timestamp_query_destroy(SimTimestampQuery* q);

#ifdef __cplusplus
}
#endif
