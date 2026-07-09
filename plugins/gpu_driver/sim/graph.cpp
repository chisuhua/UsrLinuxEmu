/*
 * sim/graph.cpp — CUDA Graph metadata simulation
 *
 * Phase 3.1: records node metadata.
 * Phase 4 (ADR-041): instantiate precompiles graph nodes into GPFIFO entries
 *   stored in a HAL-addressable sim-internal heap.
 * Phase 4 (ADR-043): launch is a pure read-only lookup (no fence alloc,
 *   no Puller interaction). The drv layer's handleGraphLaunch owns the
 *   fence lifecycle.
 */

#include "graph.h"
#include "fence_id.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>

#include "gpu_types.h"

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

/* ExecEntry (per ADR-041 + spec.md MODIFIED): holds the precompiled GPFIFO
 * buffer plus the resolved kernel addresses. source_graph is informational
 * only (graph may be destroyed before exec). */
struct ExecEntry {
  uint64_t source_graph;
  std::vector<uint64_t> kernel_addrs;
  std::vector<gpu_gpfifo_entry> precompiled_entries;
  uint64_t gpfifo_gpu_addr;   /* HAL-addressable GPU VA from sim heap */
  uint32_t entry_count;
};

std::map<uint64_t, GraphEntry> graph_table_;
std::map<uint64_t, ExecEntry>  exec_table_;

uint64_t next_graph_handle_ = 1;
uint64_t next_exec_handle_  = 1;

/* ── Sim-internal heap (ADR-041 §3.2, Phase 4) ───────────────────────── */

constexpr uint64_t SIM_HEAP_BASE = 0x20000000ULL;
std::vector<uint8_t> sim_heap_;

void* sim_gpfifo_alloc(size_t size, uint64_t* gpu_addr_out) {
  size_t offset = sim_heap_.size();
  sim_heap_.resize(offset + size, 0);
  *gpu_addr_out = SIM_HEAP_BASE + offset;
  return sim_heap_.data() + offset;
}

/* ── GPFIFO pack helpers (ADR-041 D4) ─────────────────────────────────── */

static uint64_t pack_grid_dim(uint32_t x, uint32_t y, uint32_t z) {
  return static_cast<uint64_t>(x) |
         (static_cast<uint64_t>(y) << 16) |
         (static_cast<uint64_t>(z) << 24);
}

static uint64_t pack_block_dim(uint32_t x, uint32_t y, uint32_t z) {
  return static_cast<uint64_t>(x) |
         (static_cast<uint64_t>(y) << 8) |
         (static_cast<uint64_t>(z) << 16);
}

/* Resolve kernargs BO handle to a GPU VA (Phase 4 deterministic mapping).
 *
 * Per design.md §3.4, Phase 5 will call hal_->mem_lookup for real BO→VA
 * resolution. Phase 4 uses a deterministic base + stride to keep tests
 * stable and avoid requiring a HAL context in the sim layer. */
static uint64_t resolve_kernargs_va(uint64_t bo_handle) {
  constexpr uint64_t KERNARGS_VA_BASE  = 0x30000000ULL;
  constexpr uint64_t KERNARGS_VA_STRIDE = 0x00001000ULL;  /* 4 KB per BO */
  if (bo_handle == 0) return 0;
  return KERNARGS_VA_BASE + bo_handle * KERNARGS_VA_STRIDE;
}

/* Resolve kernel_index to a placeholder GPU VA. Real implementation will
 * query a kernel table; Phase 4 uses a deterministic mapping. */
static uint64_t resolve_kernel_va(uint32_t kernel_index) {
  constexpr uint64_t KERNEL_VA_BASE = 0x40000000ULL;
  constexpr uint64_t KERNEL_VA_STRIDE = 0x00001000ULL;
  return KERNEL_VA_BASE + static_cast<uint64_t>(kernel_index) * KERNEL_VA_STRIDE;
}

/* graph_to_gpfifo (ADR-041 D4): translate graph nodes into gpu_gpfifo_entry
 * records. Reuses the same pack convention as gpfifo_translator.h:18-19. */
void graph_to_gpfifo(const GraphEntry& graph, std::vector<gpu_gpfifo_entry>& out) {
  out.clear();
  out.reserve(graph.nodes.size());

  for (const auto& node : graph.nodes) {
    gpu_gpfifo_entry entry{};
    entry.valid = 1;
    entry.priv = 0;
    entry.subchannel = 0;
    entry._reserved = 0;
    entry.semaphore_va = 0;
    entry.semaphore_value = 0;
    entry.release = 0;
    entry._pad = 0;

    if (node.type == SIM_GRAPH_NODE_KERNEL) {
      entry.method = GPU_OP_LAUNCH_KERNEL;
      entry.payload[0] = resolve_kernel_va(node.kernel.kernel_index);
      entry.payload[1] = pack_grid_dim(node.kernel.grid_x,
                                        node.kernel.grid_y,
                                        node.kernel.grid_z);
      entry.payload[2] = pack_block_dim(node.kernel.block_x,
                                         node.kernel.block_y,
                                         node.kernel.block_z);
      entry.payload[3] = resolve_kernargs_va(node.kernel.kernargs_bo_handle);
      for (int i = 4; i < 7; ++i) entry.payload[i] = 0;
    } else if (node.type == SIM_GRAPH_NODE_MEMCPY) {
      entry.method = GPU_OP_MEMCPY;
      entry.payload[0] = node.memcpy.src_va;
      entry.payload[1] = node.memcpy.dst_va;
      entry.payload[2] = node.memcpy.size;
      entry.payload[3] = static_cast<uint64_t>(node.memcpy.is_h2d);
      for (int i = 4; i < 7; ++i) entry.payload[i] = 0;
    } else {
      continue;
    }
    out.push_back(entry);
  }
}

/* ── Validation ──────────────────────────────────────────────────────── */

bool validate_kernargs(const GraphEntry& g) {
  for (const auto& n : g.nodes) {
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

  ExecEntry exec;
  exec.source_graph = graph_handle;
  exec.gpfifo_gpu_addr = 0;
  exec.entry_count = 0;

  /* Resolve kernel addresses for metadata. */
  for (const auto& n : it->second.nodes) {
    if (n.type == SIM_GRAPH_NODE_KERNEL) {
      exec.kernel_addrs.push_back(resolve_kernel_va(n.kernel.kernel_index));
    }
  }

  /* Precompile nodes to GPFIFO entries. */
  graph_to_gpfifo(it->second, exec.precompiled_entries);
  exec.entry_count = static_cast<uint32_t>(exec.precompiled_entries.size());

  if (exec.entry_count > 0) {
    size_t bytes = exec.precompiled_entries.size() * sizeof(gpu_gpfifo_entry);
    void* sim_buf = sim_gpfifo_alloc(bytes, &exec.gpfifo_gpu_addr);
    std::memcpy(sim_buf, exec.precompiled_entries.data(), bytes);
  }

  uint64_t eh = next_exec_handle_++;
  exec_table_[eh] = std::move(exec);
  *exec_handle_out = eh;
  return 0;
}

int sim_graph_launch(uint64_t exec_handle, uint32_t stream_id,
                     uint64_t *gpfifo_addr_out, uint32_t *entry_count_out) {
  (void)stream_id;  /* Reserved for Phase 3.x queue integration */

  if (!gpfifo_addr_out || !entry_count_out)
    return -EINVAL;

  auto it = exec_table_.find(exec_handle);
  if (it == exec_table_.end())
    return -EINVAL;

  *gpfifo_addr_out = it->second.gpfifo_gpu_addr;
  *entry_count_out = it->second.entry_count;
  return 0;
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
  sim_heap_.clear();
  next_graph_handle_ = 1;
  next_exec_handle_  = 1;
}

}  // extern "C"
