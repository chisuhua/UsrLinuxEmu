/*
 * test_gpu_callback_integration.cpp — 验证 HardwarePullerEmu → GlobalScheduler → Callback 链
 *
 * TDD RED 阶段: 此测试应该在修复前 FAIL，修复后 PASS
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

#include "doorbell_emu.h"
#include "hardware_puller_emu.h"
#include "scheduler/global_scheduler.h"
#include "gpu_types.h"
#include "gpu_queue.h"

// 模拟的 gpu_hal_ops，仅实现 mem_read
static int mock_mem_read(void* ctx, uint64_t addr, void* out, size_t size) {
  (void)ctx;
  (void)addr;
  // 返回预定义的 gpfifo entry
  static gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.priv = 0;
  entry.method = GPU_OP_LAUNCH_KERNEL;  // 0x100
  entry.subchannel = 0;
  // grid_dim: packDimX=bits[0-15], packDimY=bits[16-23], packDimZ=bits[24-31]
  entry.payload[0] = 0;                              // kernel_idx = 0
  entry.payload[1] = 1 | (2u << 16) | (3u << 24);  // grid=(1,2,3) - 正确
  entry.payload[2] = 4 | (5u << 16) | (6u << 24);  // block=(4,5,6) - 正确
  entry.payload[3] = 0;
  entry.payload[4] = 0;
  entry.payload[5] = 0;
  entry.payload[6] = 0;
  entry.semaphore_va = 0;
  entry.semaphore_value = 0;
  entry.release = 0;

  if (size >= sizeof(entry)) {
    std::memcpy(out, &entry, sizeof(entry));
    return 0;
  }
  return -1;
}

static int mock_mem_write(void* ctx, uint64_t addr, const void* data, size_t size) {
  (void)ctx;
  (void)addr;
  (void)data;
  (void)size;
  return 0;
}

int test_callback_chain() {
  std::cout << "=== test_callback_chain ===\n";

  // 测试状态
  std::atomic<bool> callback_called(false);
  std::string captured_kernel_name;
  uint32_t captured_grid_x = 0, captured_grid_y = 0, captured_grid_z = 0;
  uint32_t captured_block_x = 0, captured_block_y = 0, captured_block_z = 0;

  // 创建组件
  struct gpu_hal_ops hal = {};
  hal.mem_read = mock_mem_read;
  hal.mem_write = mock_mem_write;

  DoorbellEmu doorbell;
  GlobalScheduler scheduler;

  // 设置 callback（这是本次修复的核心）
  scheduler.setLaunchCallback(
      [&](const char* kernel_name, uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
          uint32_t block_x, uint32_t block_y, uint32_t block_z, uint32_t shared_mem) {
        callback_called.store(true);
        captured_kernel_name = kernel_name;
        captured_grid_x = grid_x;
        captured_grid_y = grid_y;
        captured_grid_z = grid_z;
        captured_block_x = block_x;
        captured_block_y = block_y;
        captured_block_z = block_z;
        (void)shared_mem;
      });

  // 注册内核
  scheduler.registerKernel(0, "simple_kernel");
  scheduler.registerKernel(1, "matmul_kernel");

  // 创建 puller
  HardwarePullerEmu puller(&hal, &doorbell, &scheduler);
  puller.start();

  // 模拟 GPFIFO 提交
  uint64_t gpfifo_addr = 0x10000000;
  uint32_t entry_count = 1;
  puller.submitBatch(gpfifo_addr, entry_count);

  // 触发 doorbell
  doorbell.write(0);

  // 等待 puller 线程处理
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  puller.stop();

  // 验证 callback 被调用
  if (!callback_called.load()) {
    std::cerr << "[FAIL] callback was not called\n";
    return 1;
  }

  if (captured_kernel_name != "simple_kernel") {
    std::cerr << "[FAIL] expected kernel_name='simple_kernel', got '" << captured_kernel_name
              << "'\n";
    return 1;
  }

  if (captured_grid_x != 1 || captured_grid_y != 2 || captured_grid_z != 3) {
    std::cerr << "[FAIL] grid mismatch, expected (1,2,3), got (" << captured_grid_x << ","
              << captured_grid_y << "," << captured_grid_z << ")\n";
    return 1;
  }

  if (captured_block_x != 4 || captured_block_y != 5 || captured_block_z != 6) {
    std::cerr << "[FAIL] block mismatch, expected (4,5,6), got (" << captured_block_x << ","
              << captured_block_y << "," << captured_block_z << ")\n";
    return 1;
  }

  std::cout << "[PASS] test_callback_chain\n";
  return 0;
}

int main() {
  std::cout << "GPU Callback Integration Test\n";
  std::cout << "================================\n";

  int result = test_callback_chain();

  if (result == 0) {
    std::cout << "\n=== ALL TESTS PASSED ===\n";
  } else {
    std::cout << "\n=== TESTS FAILED ===\n";
  }

  return result;
}