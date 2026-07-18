/*
 * test_kfd_l1_l2_bridge_standalone.cpp — C-12 E.2.4: L1↔L2 bridge E2E
 *
 * Replaces skeleton (commit ed9ce1e) with real end-to-end tests.
 *
 * L1 = TaskRunner layer (GpuDriverClient stub embedded in this file)
 * L2 = UsrLinuxEmu layer (GpgpuDevice + KFD module via VFS → ioctl dispatch)
 *
 * Per ADR-035 §Rule 5.1 cross-repo sync protocol.
 *
 * Note: this test binary compiles kfd_sim_bridge.cpp directly for Test 3
 * (concurrent bridge access).  Tests 1 & 2 are pure E2E through the
 * VFS → fops->ioctl → GpgpuDevice → kfd_sim_bridge (plugin-internal) path.
 */

#include "catch_amalgamated.hpp"

#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "kernel/file_ops.h"

extern "C" {
#include "kfd_sim_bridge.h"
#include "gpu_ioctl.h"
#include "gpu_types.h"
}

#include <fcntl.h>
#include <thread>
#include <vector>
#include <cstring>
#include <memory>
#include <climits>

using namespace usr_linux_emu;

namespace {

constexpr u64 INVALID_PFN = ~0ULL;

// ── GpuDriverClient Stub (L1 lightweight wrapper) ──────────────────

struct GpuDriverClientStub {
  std::shared_ptr<Device> dev;
  int fd{0};
  bool kfd_reset_done{false};

  void init() {
    ModuleLoader::load_plugins("plugins");
    dev = VFS::instance().open("/dev/gpgpu0", 0);
    REQUIRE(dev != nullptr);
  }

  void kfd_sim_ensure_reset() {
    if (!kfd_reset_done) {
      kfd_sim_reset();
      kfd_reset_done = true;
    }
  }

  long allocBO(u32* out_handle, u64* out_gpu_va) {
    struct gpu_alloc_bo_args args{};
    args.size   = 4096;
    args.domain = GPU_MEM_DOMAIN_VRAM;
    args.flags  = GPU_BO_HOST_VISIBLE;
    long ret = dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &args);
    if (ret == 0) {
      if (out_handle) *out_handle = args.handle;
      if (out_gpu_va) *out_gpu_va = args.gpu_va;
    }
    return ret;
  }

  long freeBO(u32 handle) {
    return dev->fops->ioctl(fd, GPU_IOCTL_FREE_BO, &handle);
  }
};

GpuDriverClientStub& bridge() {
  static GpuDriverClientStub s;
  static bool inited = false;
  if (!inited) { s.init(); inited = true; }
  return s;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════
// Test 1: MAP_MEMORY E2E through VFS → GpgpuDevice → kfd_sim_bridge
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("kfd L1L2 bridge MAP_MEMORY E2E via VFS ioctl",
          "[kfd][l1_l2_bridge][e2e][e024]") {
  auto& cli = bridge();
  cli.kfd_sim_ensure_reset();

  // Allocate BO (MAP_MEMORY requires valid handle)
  u32 handle = 0;
  u64 bo_va  = 0;
  long ret = cli.allocBO(&handle, &bo_va);
  REQUIRE(ret == 0);
  REQUIRE(handle != 0);

  // MAP_MEMORY through VFS → GpgpuDevice → kfd_sim_bridge
  struct gpu_map_memory_args args{};
  args.handle    = handle;
  args.n_devices = 1;
  args.size      = 4096;
  ret = cli.dev->fops->ioctl(cli.fd, GPU_IOCTL_MAP_MEMORY, &args);
  REQUIRE(ret == 0);
  REQUIRE(args.n_success == 1);
  REQUIRE(args.gpu_va != 0);

  // Verify handle cannot be mapped again (duplicate handle → reuse in bridge)
  struct gpu_map_memory_args args2{};
  args2.handle    = handle;
  args2.n_devices = 1;
  args2.size      = 4096;
  ret = cli.dev->fops->ioctl(cli.fd, GPU_IOCTL_MAP_MEMORY, &args2);
  REQUIRE(ret == 0); // bridge re-assigns GPU VA (idempotent behavior)

  // Verify invalid handle returns error
  struct gpu_map_memory_args args3{};
  args3.handle    = 99999;
  args3.n_devices = 1;
  args3.size      = 4096;
  ret = cli.dev->fops->ioctl(cli.fd, GPU_IOCTL_MAP_MEMORY, &args3);
  REQUIRE(ret == -EINVAL);
}

// ═══════════════════════════════════════════════════════════════════
// Test 2: 5 KFD ioctls E2E through the GpgpuDevice IoctlEntry table
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("kfd L1L2 bridge 5 KFD ioctls E2E",
          "[kfd][l1_l2_bridge][e2e][e024]") {
  auto& cli = bridge();
  cli.kfd_sim_ensure_reset();

  // ── 2.1 CREATE_QUEUE ──
  struct gpu_va_space_args va_args{};
  va_args.page_size = 0;
  long ret = cli.dev->fops->ioctl(cli.fd, GPU_IOCTL_CREATE_VA_SPACE, &va_args);
  REQUIRE(ret == 0);
  REQUIRE(va_args.va_space_handle != 0);

  struct gpu_queue_args q_args{};
  q_args.va_space_handle   = va_args.va_space_handle;
  q_args.queue_type        = 0;  // COMPUTE
  q_args.ring_buffer_size  = 4096;
  ret = cli.dev->fops->ioctl(cli.fd, GPU_IOCTL_CREATE_QUEUE, &q_args);
  REQUIRE(ret == 0);
  REQUIRE(q_args.queue_handle != 0);

  // ── 2.2 GET_PROCESS_APERTURE ──
  struct gpu_aperture_info apertures[8]{};
  struct gpu_get_process_aperture_args ap_args{};
  ap_args.num_nodes     = 1;
  ap_args.apertures_ptr = reinterpret_cast<u64>(apertures);
  ret = cli.dev->fops->ioctl(cli.fd, GPU_IOCTL_GET_PROCESS_APERTURE, &ap_args);
  REQUIRE(ret == 0);
  REQUIRE(apertures[0].gpu_id      == 0);
  REQUIRE(apertures[0].lds_base    != 0);
  REQUIRE(apertures[0].lds_limit   != 0);
  REQUIRE(apertures[0].gpuvm_base  != 0);
  REQUIRE(apertures[0].gpuvm_limit != 0);

  // Multi-node aperture
  struct gpu_aperture_info apertures_multi[8]{};
  struct gpu_get_process_aperture_args ap_args2{};
  ap_args2.num_nodes     = 4;
  ap_args2.apertures_ptr = reinterpret_cast<u64>(apertures_multi);
  ret = cli.dev->fops->ioctl(cli.fd, GPU_IOCTL_GET_PROCESS_APERTURE, &ap_args2);
  REQUIRE(ret == 0);
  REQUIRE(apertures_multi[3].gpu_id == 3);
  REQUIRE(apertures_multi[3].gpuvm_base != 0);

  // ── 2.3 UPDATE_QUEUE ──
  struct gpu_update_queue_args uq_args{};
  uq_args.queue_handle = q_args.queue_handle;
  uq_args.queue_flags  = 0;
  ret = cli.dev->fops->ioctl(cli.fd, GPU_IOCTL_UPDATE_QUEUE, &uq_args);
  REQUIRE(ret == 0);

  // UPDATE_QUEUE with invalid handle
  struct gpu_update_queue_args uq_args2{};
  uq_args2.queue_handle = 0;
  uq_args2.queue_flags  = 0;
  ret = cli.dev->fops->ioctl(cli.fd, GPU_IOCTL_UPDATE_QUEUE, &uq_args2);
  REQUIRE(ret < 0);

  // ── 2.4 MAP_MEMORY → UNMAP_MEMORY round trip ──
  u32 bo_handle = 0;
  u64 bo_va = 0;
  ret = cli.allocBO(&bo_handle, &bo_va);
  REQUIRE(ret == 0);

  struct gpu_map_memory_args map_args{};
  map_args.handle    = bo_handle;
  map_args.n_devices = 1;
  map_args.size      = 4096;
  ret = cli.dev->fops->ioctl(cli.fd, GPU_IOCTL_MAP_MEMORY, &map_args);
  REQUIRE(ret == 0);
  REQUIRE(map_args.n_success == 1);
  REQUIRE(map_args.gpu_va != 0);

  // UNMAP_MEMORY
  struct gpu_unmap_memory_args unmap_args{};
  unmap_args.handle    = bo_handle;
  unmap_args.n_devices = 1;
  ret = cli.dev->fops->ioctl(cli.fd, GPU_IOCTL_UNMAP_MEMORY, &unmap_args);
  REQUIRE(ret == 0);
  REQUIRE(unmap_args.n_success == 1);

  // Double-unmap is idempotent (bridge handles missing entry gracefully)
  ret = cli.dev->fops->ioctl(cli.fd, GPU_IOCTL_UNMAP_MEMORY, &unmap_args);
  REQUIRE(ret == 0);

  // Cleanup BO
  ret = cli.freeBO(bo_handle);
  REQUIRE(ret == 0);
}

// ═══════════════════════════════════════════════════════════════════
// Test 3: Concurrent kfd_sim_bridge multi-threaded access
// (exercises bridge-internal KfdSimState + mutex thread safety)
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("kfd L1L2 bridge concurrent multi-threaded",
          "[kfd][l1_l2_bridge][concurrent][e2e][e024]") {
  kfd_sim_reset();
  REQUIRE(kfd_sim_get_page_count() == 0);

  constexpr int N = 8;

  std::vector<std::thread> threads;
  std::vector<u64> gpu_vas(N, 0);
  std::vector<long> results(N, 0);
  threads.reserve(N);

  // Concurrent MAP_MEMORY: N threads each map 1 page
  for (int tid = 0; tid < N; tid++) {
    threads.emplace_back([tid, &gpu_vas, &results]() {
      u32 handle = static_cast<u32>(tid + 1);  // unique handles
      struct gpu_map_memory_args args{};
      args.handle    = handle;
      args.n_devices = 1;
      args.size      = 4096;
      long ret = kfd_sim_handle_map_memory(&args);
      results[tid] = ret;
      if (ret == 0) gpu_vas[tid] = args.gpu_va;
    });
  }
  for (auto& t : threads) t.join();

  // Verify all N insertions succeeded
  for (int i = 0; i < N; i++) {
    REQUIRE(results[i] == 0);
    REQUIRE(gpu_vas[i] != 0);
  }
  REQUIRE(kfd_sim_get_page_count() == static_cast<u32>(N));

  // Verify all GPU VAs have valid PFN entries (entry exists, not INVALID_PFN)
  for (int i = 0; i < N; i++) {
    u64 pfn = kfd_sim_lookup_pfn(gpu_vas[i]);
    REQUIRE(pfn != INVALID_PFN);
  }

  kfd_sim_reset();
}
