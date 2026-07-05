/*
 * sim/graph.h — CUDA Graph metadata simulation (C ABI)
 *
 * Phase 3.1 of the sim-stream-primitive-support change (ACCEPTED 2026-07-05).
 * Records kernel/memcpy node metadata attached to a graph; instantiate
 * validates node BO handles and emits an executable. Launch returns a
 * sim-layer fence_id (range [(1<<32), INT64_MAX]) per Fix-1/Oracle H4.
 *
 * Architecture: ③ Hardware Simulation layer.
 * Per ADR-036 three-way separation.
 *
 * Out of scope (Phase 3.1 PoC):
 *   - Real DAG execution (instantiate/launch only record + return fence)
 *   - Kernel arg serialization (recorded by kernargs_bo_handle only)
 *   - Multi-device / cross-stream graph dependencies
 *
 * Thread Safety (per design.md): NOT required (single-threaded).
 */

#ifndef SIM_GRAPH_H
#define SIM_GRAPH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Node type enum */
typedef enum {
  SIM_GRAPH_NODE_KERNEL = 1,
  SIM_GRAPH_NODE_MEMCPY = 2,
} sim_graph_node_type_t;

/* Public handles (opaque under the hood) */
typedef uint64_t sim_graph_handle_t;
typedef uint64_t sim_graph_exec_handle_t;

/**
 * Create an empty graph.
 * @param graph_handle_out  Output graph handle (≥ 1 on success)
 * @return 0 on success; -EINVAL if arg null.
 */
int sim_graph_create(uint64_t *graph_handle_out);

/**
 * Destroy a graph (also destroys any executables derived from it).
 * @param graph_handle  Input graph handle
 * @return 0 on success; -1 if handle not found.
 */
int sim_graph_destroy(uint64_t graph_handle);

/**
 * Append a kernel node to the graph.
 *
 * @param graph_handle       Target graph (input)
 * @param kernel_index       Kernel table index (input)
 * @param grid_x/y/z          Grid dimensions (input)
 * @param block_x/y/z         Block dimensions (input)
 * @param kernargs_bo_handle  BO handle for kernel args (input; non-zero ⇒ valid)
 * @return 0 on success; -1 if graph not found.
 */
int sim_graph_add_kernel_node(uint64_t graph_handle,
                              uint32_t kernel_index,
                              uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                              uint32_t block_x, uint32_t block_y, uint32_t block_z,
                              uint64_t *kernargs_bo_handle);

/**
 * Append a memcpy node to the graph.
 */
int sim_graph_add_memcpy_node(uint64_t graph_handle,
                              uint64_t src_va, uint64_t dst_va, uint64_t size,
                              int is_h2d);

/**
 * Validate graph and produce an executable.
 *
 * For PoC: validation = all kernel node kernargs_bo_handle != 0.
 * Full BO resolution deferred (per design §Decisions).
 *
 * @param graph_handle      Input graph
 * @param exec_handle_out   Output executable handle (≥ 1 on success)
 * @return 0 on success; -EINVAL if validation fails; -1 if graph not found.
 */
int sim_graph_instantiate(uint64_t graph_handle, uint64_t *exec_handle_out);

/**
 * Launch an executable. Returns a sim-layer fence_id (≥ 1<<32) which is
 * signaled immediately (PoC simplification; Phase 3.x may defer to queue).
 *
 * @return fence_id (≥ 1<<32) on success;
 *         -EINVAL if exec_handle unknown;
 *         -1 on generic error.
 */
int64_t sim_graph_launch(uint64_t exec_handle, uint32_t stream_id);

/**
 * Destroy an executable. Does NOT destroy source graph.
 */
int sim_graph_destroy_exec(uint64_t exec_handle);

/* Test-only helper */
void sim_graph_reset_for_test(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* SIM_GRAPH_H */
