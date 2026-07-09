/*
 * test_gpu_fence_return.cpp — 验证 S3.5 Fence 返回机制
 *
 * TDD RED 阶段: 测试应该在修复前 FAIL，修复后 PASS
 *
 * 问题：puller path (异步) 不返回 fence_id，导致 WAIT_FENCE timeout
 * 目标：即使在异步路径中，也应该返回 fence_id 并在完成时 signal
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

int test_fence_return_on_puller_path() {
  std::cout << "=== test_fence_return_on_puller_path ===\n";

  // 加载插件
  ModuleLoader::load_plugins("plugins");

  // 打开设备
  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  if (!dev) {
    std::cerr << "[FAIL] Failed to open /dev/gpgpu0\n";
    return 1;
  }

  int fd = 0;

  int ret = 0;
  // Create VA Space and Queue first (required after Issue #2 refactoring)
  struct gpu_va_space_args va_args = {};
  va_args.page_size = 0;
  ret = dev->fops->ioctl(fd, GPU_IOCTL_CREATE_VA_SPACE, &va_args);
  if (ret != 0) {
    std::cerr << "[FAIL] CREATE_VA_SPACE failed: " << ret << "\n";
    return 1;
  }

  struct gpu_queue_args q_args = {};
  q_args.va_space_handle = va_args.va_space_handle;
  q_args.queue_type = 0;
  q_args.priority = 0;
  q_args.ring_buffer_size = 16;
  ret = dev->fops->ioctl(fd, GPU_IOCTL_CREATE_QUEUE, &q_args);
  if (ret != 0) {
    std::cerr << "[FAIL] CREATE_QUEUE failed: " << ret << "\n";
    return 1;
  }

  // 分配一个 BO 用于 memcpy 测试
  struct gpu_alloc_bo_args alloc_args = {};
  alloc_args.size = 4096;
  alloc_args.domain = GPU_MEM_DOMAIN_VRAM;
  alloc_args.flags = 0;

  ret = dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &alloc_args);
  if (ret != 0) {
    std::cerr << "[FAIL] ALLOC_BO failed: " << ret << "\n";
    return 1;
  }
  std::cout << "[INFO] Allocated BO: handle=" << alloc_args.handle
            << " va=0x" << std::hex << alloc_args.gpu_va << std::dec << "\n";

  // 构造一个 MEMCPY 命令
  struct gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.priv = 0;
  entry.method = GPU_OP_MEMCPY;
  entry.subchannel = 0;
  entry.payload[0] = 0x1000;              // src (任意地址)
  entry.payload[1] = alloc_args.gpu_va;  // dst (已分配的 BO)
  entry.payload[2] = 4096;               // size
  entry.payload[3] = 0;
  entry.payload[4] = 0;
  entry.payload[5] = 0;
  entry.payload[6] = 0;
  entry.semaphore_va = 0;
  entry.semaphore_value = 0;
  entry.release = 0;

  // 提交批次 - 注意：不包含 FENCE 操作，所以走 puller path
  struct gpu_pushbuffer_args pb_args = {};
  pb_args.stream_id = static_cast<u32>(q_args.queue_handle);
  pb_args.va_space_handle = va_args.va_space_handle;
  pb_args.entries_addr = reinterpret_cast<u64>(&entry);
  pb_args.count = 1;
  pb_args.flags = 0;
  pb_args.fence_id = 0;  // 输出参数，应该被设置

  std::cout << "[INFO] Submitting batch on puller path (no FENCE op)...\n";
  ret = dev->fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb_args);

  if (ret != 0) {
    std::cerr << "[FAIL] PUSHBUFFER_SUBMIT_BATCH failed: " << ret << "\n";
    return 1;
  }

  // 检查 fence_id 是否被设置 (S3.5 核心要求)
  // ADR-040 D3: puller path 现在使用 sim_fence_id_alloc()，返回 range [1<<32, INT64_MAX]。
  // 旧测试检查 fence_id < 256 (HAL range) 已被 sim fence 命名空间替代。
  // 验证 fence_id 在 sim fence 范围 (>= SIM_FENCE_ID_BASE) 即可。
  if (pb_args.fence_id < (1ULL << 32)) {
    std::cerr << "[FAIL] fence_id out of sim range: " << pb_args.fence_id
              << " (expected >= " << (1ULL << 32) << ")\n";
    return 1;
  }

  std::cout << "[INFO] fence_id returned: " << pb_args.fence_id
            << " (sim range, ADR-040 D3)\n";

  // 等待 fence (最多 100ms，应该足够完成一个 memcpy)
  struct gpu_wait_fence_args fence_args = {};
  fence_args.fence_id = pb_args.fence_id;
  fence_args.timeout_ms = 100;
  fence_args.status = 0;

  std::cout << "[INFO] Waiting for fence " << fence_args.fence_id << "...\n";
  ret = dev->fops->ioctl(fd, GPU_IOCTL_WAIT_FENCE, &fence_args);

  if (ret != 0) {
    std::cerr << "[FAIL] WAIT_FENCE failed: " << ret << "\n";
    return 1;
  }

  if (fence_args.status != 1) {
    std::cerr << "[FAIL] fence not signaled (status=" << fence_args.status << ")\n";
    std::cerr << "[FAIL] S3.5: Fence should be signaled after puller completes batch\n";
    return 1;
  }

  std::cout << "[PASS] Fence was signaled (status=1)\n";

  // 清理
  dev->fops->ioctl(fd, GPU_IOCTL_FREE_BO, &alloc_args.handle);
  dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_QUEUE, &q_args.queue_handle);
  dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_VA_SPACE, &va_args.va_space_handle);

  std::cout << "[PASS] test_fence_return_on_puller_path\n";
  return 0;
}

int main() {
  std::cout << "GPU Fence Return Mechanism Test (S3.5)\n";
  std::cout << "======================================\n";

  int result = test_fence_return_on_puller_path();

  if (result == 0) {
    std::cout << "\n=== ALL TESTS PASSED ===\n";
  } else {
    std::cout << "\n=== TESTS FAILED ===\n";
  }

  return result;
}