// sim/hardware/timestamp_query.cpp - ADR-057 Profiling Hooks (Task 5.1)
//
// Implementation: handle-table pattern so that double-destroy is a safe
// no-op (defensive). Each query is stored in a vector; the opaque handle
// returned to the caller is the index+1 (0 == invalid/nullptr).
//
// Task 5.1 scope: record/resolve lifecycle only. Global tick counter
// (Task 5.3) and Puller DISPATCH integration (Task 5.4) are NOT here.
#include "timestamp_query.h"

#include <cerrno>
#include <cstdint>
#include <mutex>
#include <vector>

namespace {

// Internal query state.
struct TimestampQueryEntry {
  bool recorded = false;
  uint64_t entry_index = 0;
  uint64_t tick = 0;
};

// Handle-table: index 0 is reserved as "invalid/destroyed".
// Slot value is nullptr when the slot is free.
std::vector<TimestampQueryEntry*>& query_table() {
  static std::vector<TimestampQueryEntry*> table;
  return table;
}

std::mutex& table_mutex() {
  static std::mutex mtx;
  return mtx;
}

// Allocate a slot and return its 1-based index, or 0 on failure.
// The opaque SimTimestampQuery* is actually this index reinterpreted.
uint64_t alloc_slot(TimestampQueryEntry* entry) {
  std::lock_guard<std::mutex> lock(table_mutex());
  auto& table = query_table();
  for (size_t i = 0; i < table.size(); ++i) {
    if (table[i] == nullptr) {
      table[i] = entry;
      return static_cast<uint64_t>(i + 1);
    }
  }
  table.push_back(entry);
  return static_cast<uint64_t>(table.size());
}

// Lookup a slot by 1-based index, returns nullptr if invalid/freed.
TimestampQueryEntry* lookup_slot(uint64_t handle) {
  if (handle == 0) return nullptr;
  std::lock_guard<std::mutex> lock(table_mutex());
  auto& table = query_table();
  size_t idx = static_cast<size_t>(handle - 1);
  if (idx >= table.size()) return nullptr;
  return table[idx];
}

// Free a slot by 1-based index; returns the entry pointer (or nullptr).
TimestampQueryEntry* free_slot(uint64_t handle) {
  if (handle == 0) return nullptr;
  std::lock_guard<std::mutex> lock(table_mutex());
  auto& table = query_table();
  size_t idx = static_cast<size_t>(handle - 1);
  if (idx >= table.size()) return nullptr;
  TimestampQueryEntry* entry = table[idx];
  table[idx] = nullptr;
  return entry;
}

}  // namespace

extern "C" {

SimTimestampQuery* sim_timestamp_query_create(void) {
  auto* entry = new (std::nothrow) TimestampQueryEntry();
  if (entry == nullptr) return nullptr;
  uint64_t handle = alloc_slot(entry);
  if (handle == 0) {
    delete entry;
    return nullptr;
  }
  // Encode the handle as the opaque pointer. We use the 1-based index
  // directly as the address value so that nullptr == invalid.
  return reinterpret_cast<SimTimestampQuery*>(handle);
}

void sim_timestamp_query_record(SimTimestampQuery* q, uint64_t entry_index,
                                uint64_t tick) {
  uint64_t handle = reinterpret_cast<uint64_t>(q);
  auto* entry = lookup_slot(handle);
  if (entry == nullptr) return;
  entry->recorded = true;
  entry->entry_index = entry_index;
  entry->tick = tick;
}

int sim_timestamp_query_resolve(SimTimestampQuery* q,
                                uint64_t /*timeout_ms*/) {
  uint64_t handle = reinterpret_cast<uint64_t>(q);
  auto* entry = lookup_slot(handle);
  if (entry == nullptr) return -EINVAL;
  if (!entry->recorded) return -EAGAIN;
  return static_cast<int>(entry->tick);
}

void sim_timestamp_query_destroy(SimTimestampQuery* q) {
  uint64_t handle = reinterpret_cast<uint64_t>(q);
  TimestampQueryEntry* entry = free_slot(handle);
  delete entry;  // delete nullptr is safe
}

}  // extern "C"
