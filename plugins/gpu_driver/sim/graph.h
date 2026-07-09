/*
 * sim/graph.h — CUDA Graph metadata simulation (C ABI)
 *
 * Phase 3.1 of the sim-stream-primitive-support change (ACCEPTED 2026-07-05).
 * Records kernel/memcpy node metadata attached to a graph; instantiate
 * validates node BO handles, precompiles graph nodes into GPFIFO entries
 * (ADR-041), and emits an executable carrying a HAL-addressable GPFIFO
 * buffer. Launch is a pure lookup — it does NOT signal a fence. The drv
 * layer's handleGraphLaunch allocates a sim fence and submits via
 * GpuQueueEmu::submit() → HardwarePullerEmu, which signals the fence on
 * batch completion (ADR-040, ADR-043).
 *
 * Architecture: ③ Hardware Simulation layer.
 * Per ADR-036 three-way separation.
 *
 * Out of scope:
 *   - Multi-device / cross-stream graph dependencies
 *   - Conditional nodes
 *
 * Thread Safety (per design.md): NOT required (single-threaded dispatch).
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
 * Per ADR-041: precompiles graph nodes into GPFIFO entries and stores
 * them in a HAL-addressable sim-internal heap. Per ADR-043: this is the
 * only sim function that performs serialization; sim_graph_launch is a
 * pure read-only lookup.
 *
 * @param graph_handle      Input graph
 * @param exec_handle_out   Output executable handle (≥ 1 on success)
 * @return 0 on success; -EINVAL if validation fails; -1 if graph not found.
 */
int sim_graph_instantiate(uint64_t graph_handle, uint64_t *exec_handle_out);

/**
 * Look up an executable's precompiled GPFIFO buffer (ADR-043 D4).
 *
 * Pure read-only: returns the HAL-addressable GPU VA and entry count of
 * the precompiled GPFIFO buffer. Does NOT allocate or signal a fence;
 * the drv layer (handleGraphLaunch) is responsible for those.
 *
 * @param exec_handle        Input executable handle
 * @param stream_id          Target queue (currently unused; reserved)
 * @param gpfifo_addr_out    Output: GPU VA of precompiled GPFIFO buffer
 * @param entry_count_out    Output: number of entries in buffer
 * @return 0 on success; -EINVAL if exec_handle unknown or output null.
 */
int sim_graph_launch(uint64_t exec_handle, uint32_t stream_id,
                     uint64_t *gpfifo_addr_out, uint32_t *entry_count_out);

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
