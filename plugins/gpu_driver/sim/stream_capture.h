/*
 * sim/stream_capture.h — Stream Capture state machine (C ABI)
 *
 * Phase 3.1 of the sim-stream-primitive-support change (ACCEPTED 2026-07-05).
 * Wraps the GPU's queue/stream capture lifecycle for cuStreamBeginCapture /
 * cuStreamEndCapture. Maintains a state machine per (stream_id) and emits
 * a graph handle at end() that the sim_graph module later consumes.
 *
 * Architecture: ③ Hardware Simulation layer.
 * Per ADR-036 three-way separation.
 *
 * State machine (Oracle P3-L1):
 *   NONE  → begin()  → ACTIVE
 *   ACTIVE → end()   → NONE  (+ emits graph_handle, monotonic counter 1, 2, …)
 *   ACTIVE → begin() → INVALID (double begin)
 *   INVALID → begin()/end() → INVALID (until pool reset)
 *   Note: status() can always be queried.
 *
 * Thread Safety (per design.md): NOT required (single-threaded driver
 * dispatch). No locks. Capture mode ≠ GLOBAL returns -EINVAL (Fix-10).
 */

#ifndef SIM_STREAM_CAPTURE_H
#define SIM_STREAM_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Capture mode enum (Oracle H5 fix) */
typedef enum {
  SIM_CAPTURE_MODE_GLOBAL       = 0,  /* Maps to CU_STREAM_CAPTURE_MODE_GLOBAL */
  SIM_CAPTURE_MODE_THREAD_LOCAL = 1,  /* Reserved: returns -EINVAL in Phase 3.1 */
  SIM_CAPTURE_MODE_RELAXED      = 2,  /* Reserved: returns -EINVAL in Phase 3.1 */
} sim_capture_mode_t;

/* Stream capture state (also matches gpu_stream_capture_status_args.status_out) */
typedef enum {
  SIM_STREAM_CAPTURE_NONE    = 0,
  SIM_STREAM_CAPTURE_ACTIVE  = 1,
  SIM_STREAM_CAPTURE_INVALID = 2,
} sim_stream_capture_status_t;

/**
 * Begin capturing GPU operations on stream_id.
 *
 * @param stream_id  Queue/stream handle (input)
 * @param mode       SIM_CAPTURE_MODE_* (input, GLOBAL only supported in 3.1)
 * @return 0 on success
 *         -EINVAL  invalid args / unsupported mode
 *         -EINVAL  already ACTIVE or INVALID (transition rules)
 */
int sim_stream_capture_begin(uint32_t stream_id, uint32_t mode);

/**
 * End capturing, returning the graph handle for downstream consumption.
 *
 * @param stream_id          Queue/stream handle (input)
 * @param graph_handle_out   Output graph handle (≥ 1 on success, monotonic)
 * @return 0 on success
 *         -EINVAL  invalid args
 *         -EINVAL  stream not ACTIVE
 */
int sim_stream_capture_end(uint32_t stream_id, uint64_t *graph_handle_out);

/**
 * Query current capture state.
 *
 * @param stream_id    Queue/stream handle (input)
 * @param status_out   Output status (pointer; required)
 * @return 0 on success (status written); -EINVAL if args invalid.
 */
int sim_stream_capture_status(uint32_t stream_id,
                              sim_stream_capture_status_t *status_out);

/**
 * Test-only helper: clear the entire capture table. Production code MUST
 * NOT call this; it resets both state AND the monotonic graph_handle counter.
 */
void sim_stream_capture_reset_for_test(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* SIM_STREAM_CAPTURE_H */
