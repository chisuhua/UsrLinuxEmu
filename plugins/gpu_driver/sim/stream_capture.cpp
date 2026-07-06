/*
 * sim/stream_capture.cpp — Stream Capture state machine (Phase 3.1)
 *
 * Maintains per-stream capture state and emits monotonic graph handles.
 * No real DAG construction — only records (so sim_graph can later attach
 * node metadata). Thread safety is intentionally NOT provided (driver-side
 * single-threaded dispatch per design.md §Thread Safety).
 */

#include "stream_capture.h"

#include <cerrno>
#include <cstdint>
#include <map>

namespace {

struct StreamCaptureEntry {
  sim_stream_capture_status_t state = SIM_STREAM_CAPTURE_NONE;
};

/* Per-stream state table (key = stream_id) */
std::map<uint32_t, StreamCaptureEntry> stream_capture_table_;

/* Monotonic graph_handle emitter used by end(). Starts at 1; reset only by
 * the test helper. sim_graph consumes these IDs to look up graph metadata. */
uint64_t next_graph_handle_ = 1;

bool is_valid_mode(uint32_t mode) {
  return mode == static_cast<uint32_t>(SIM_CAPTURE_MODE_GLOBAL);
}

}  // anonymous namespace

extern "C" {

int sim_stream_capture_begin(uint32_t stream_id, uint32_t mode) {
  if (!is_valid_mode(mode))
    return -EINVAL;

  auto &entry = stream_capture_table_[stream_id];
  switch (entry.state) {
    case SIM_STREAM_CAPTURE_NONE:
      entry.state = SIM_STREAM_CAPTURE_ACTIVE;
      return 0;
    case SIM_STREAM_CAPTURE_ACTIVE:
      /* double begin → transition to INVALID (Oracle P3-L1) */
      entry.state = SIM_STREAM_CAPTURE_INVALID;
      return -1;
    case SIM_STREAM_CAPTURE_INVALID:
      return -1;
  }
  return -1;  /* unreachable */
}

int sim_stream_capture_end(uint32_t stream_id, uint64_t *graph_handle_out) {
  if (!graph_handle_out)
    return -EINVAL;

  auto it = stream_capture_table_.find(stream_id);
  if (it == stream_capture_table_.end() ||
      it->second.state != SIM_STREAM_CAPTURE_ACTIVE) {
    return -1;
  }

  *graph_handle_out = next_graph_handle_++;
  it->second.state = SIM_STREAM_CAPTURE_NONE;
  return 0;
}

int sim_stream_capture_status(uint32_t stream_id,
                              sim_stream_capture_status_t *status_out) {
  if (!status_out)
    return -EINVAL;

  auto it = stream_capture_table_.find(stream_id);
  if (it == stream_capture_table_.end()) {
    *status_out = SIM_STREAM_CAPTURE_NONE;
    return 0;
  }
  *status_out = it->second.state;
  return 0;
}

void sim_stream_capture_reset_for_test(void) {
  stream_capture_table_.clear();
  next_graph_handle_ = 1;
}

}  // extern "C"
