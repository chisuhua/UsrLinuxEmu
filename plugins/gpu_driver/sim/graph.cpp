/*
 * sim/graph.cpp — CUDA Graph metadata simulation (Phase 3.1)
 *
 * Records node metadata on graph; instantiate = validate + emit exec;
 * launch = alloc sim fence_id + signal-immediately. No real DAG execution.
 */

#include "graph.h"
#include "fence_id.h"

#include <cerrno>
#include <cstdint>
#include <map>
#include <vector>

namespace {

/* ── Node metadata (per design.md §IOCTL 结构体) ──────────────────────── */

struct KernelNodeMetadata {
  uint32_t kernel_index;
  uint32_t grid_x, grid_y, grid_z;
  uint32_t block_x, block_y, block_z;
  uint64_t kernargs_bo_handle;
};

struct MemcpyNodeMetadata {
  uint64_t src_va;
  uint64_t dst_va;
  uint64_t size;
  int is_h2d;
};

struct NodeMetadata {
  sim_graph_node_type_t type;
  KernelNodeMetadata kernel; /* valid iff type == KERNEL */
  MemcpyNodeMetadata memcpy; /* valid iff type == MEMCPY */
};

/* ── Tables ──────────────────────────────────────────────────────────── */

struct GraphEntry {
  std::vector<NodeMetadata> nodes;
};

struct ExecEntry {
  /* source graph handle is informational only (graph may be destroyed
   * before exec — exec only references its own node metadata copy) */
  uint64_t source_graph;
};

/* Public handle == uint64_t, opaque (per page_fault_handler pattern) */
std::map<uint64_t, GraphEntry> graph_table_;
std::map<uint64_t, ExecEntry>  exec_table_;

uint64_t next_graph_handle_ = 1;  /* monotonic, separate from stream_capture */
uint64_t next_exec_handle_  = 1;

/* ── Helpers ─────────────────────────────────────────────────────────── */

bool validate_kernargs(const GraphEntry &g) {
  for (const auto &n : g.nodes) {
    if (n.type == SIM_GRAPH_NODE_KERNEL && n.kernel.kernargs_bo_handle == 0)
      return false;
  }
  return true;
}

}  // anonymous namespace

extern "C" {

int sim_graph_create(uint64_t *graph_handle_out) {
  if (!graph_handle_out)
    return -EINVAL;
  uint64_t h = next_graph_handle_++;
  graph_table_[h] = GraphEntry{};
  *graph_handle_out = h;
  return 0;
}

int sim_graph_destroy(uint64_t graph_handle) {
  auto git = graph_table_.find(graph_handle);
  if (git == graph_table_.end())
    return -1;
  graph_table_.erase(git);
  /* Destroy execs that referred to this graph (best-effort cleanup) */
  for (auto eit = exec_table_.begin(); eit != exec_table_.end(); ) {
    if (eit->second.source_graph == graph_handle)
      eit = exec_table_.erase(eit);
    else
      ++eit;
  }
  return 0;
}

int sim_graph_add_kernel_node(uint64_t graph_handle,
                              uint32_t kernel_index,
                              uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                              uint32_t block_x, uint32_t block_y, uint32_t block_z,
                              uint64_t *kernargs_bo_handle) {
  auto it = graph_table_.find(graph_handle);
  if (it == graph_table_.end())
    return -1;

  NodeMetadata n;
  n.type = SIM_GRAPH_NODE_KERNEL;
  n.kernel.kernel_index = kernel_index;
  n.kernel.grid_x = grid_x;
  n.kernel.grid_y = grid_y;
  n.kernel.grid_z = grid_z;
  n.kernel.block_x = block_x;
  n.kernel.block_y = block_y;
  n.kernel.block_z = block_z;
  n.kernel.kernargs_bo_handle = (kernargs_bo_handle ? *kernargs_bo_handle : 0);
  it->second.nodes.push_back(n);
  return 0;
}

int sim_graph_add_memcpy_node(uint64_t graph_handle,
                              uint64_t src_va, uint64_t dst_va, uint64_t size,
                              int is_h2d) {
  auto it = graph_table_.find(graph_handle);
  if (it == graph_table_.end())
    return -1;

  NodeMetadata n;
  n.type = SIM_GRAPH_NODE_MEMCPY;
  n.memcpy.src_va = src_va;
  n.memcpy.dst_va = dst_va;
  n.memcpy.size  = size;
  n.memcpy.is_h2d = is_h2d;
  it->second.nodes.push_back(n);
  return 0;
}

int sim_graph_instantiate(uint64_t graph_handle, uint64_t *exec_handle_out) {
  if (!exec_handle_out)
    return -EINVAL;
  auto it = graph_table_.find(graph_handle);
  if (it == graph_table_.end())
    return -1;

  if (!validate_kernargs(it->second))
    return -EINVAL;

  uint64_t eh = next_exec_handle_++;
  exec_table_[eh] = ExecEntry{graph_handle};
  *exec_handle_out = eh;
  return 0;
}

int64_t sim_graph_launch(uint64_t exec_handle, uint32_t stream_id) {
  (void)stream_id;  /* Stream ID reserved for Phase 3.x queue integration */

  auto it = exec_table_.find(exec_handle);
  if (it == exec_table_.end())
    return -EINVAL;

  /* Allocate sim-layer fence_id (range [(1<<32), INT64_MAX]) */
  int64_t fence_id = sim_fence_id_alloc();
  if (fence_id < 0)
    return -1;

  /* PoC: signal immediately to simulate completed execution.
   * Phase 3.x: defer to GpuQueueEmu::submit() path completion. */
  sim_fence_id_signal(static_cast<uint64_t>(fence_id));

  return fence_id;
}

int sim_graph_destroy_exec(uint64_t exec_handle) {
  auto it = exec_table_.find(exec_handle);
  if (it == exec_table_.end())
    return -1;
  exec_table_.erase(it);
  return 0;
}

void sim_graph_reset_for_test(void) {
  graph_table_.clear();
  exec_table_.clear();
  next_graph_handle_ = 1;
  next_exec_handle_  = 1;
}

}  // extern "C"
