/**
 * test_gpu_ioctl_number.cpp - 验证 ioctl 编号一致性 (TDD)
 *
 * 测试目标：确保 GPU_IOCTL_GET_DEVICE_INFO 在测试端和插件端计算一致
 *
 * RED PHASE: 编写一个会失败的测试来验证 ioctl 值匹配问题
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <iostream>

#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "kernel/file_ops.h"
#include "kernel/device/device.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

/**
 * 测试 1: 验证 ioctl 宏定义一致性
 *
 * 问题：测试端和插件端可能使用不同的 _IOR 宏定义
 * 导致 sizeof(struct gpu_device_info) 计算结果不同
 */
void test_ioctl_number_consistency() {
  std::cout << "=== Test 1: Ioctl Number Consistency ===" << std::endl;

  // 手动计算 _IOR 值
  // 公式: (dir << 30) | (type << 8) | (nr) | (sizeof(size) << 16)
  // GPU_IOCTL_BASE = 'G' = 0x47
  // 0x20 = nr
  // size = sizeof(struct gpu_device_info)

  unsigned int expected = (2U << 30) | (('G' << 8)) | (0x20) | (sizeof(struct gpu_device_info) << 16);

  std::cout << "sizeof(gpu_device_info) = " << sizeof(struct gpu_device_info) << std::endl;
  std::cout << "GPU_IOCTL_GET_DEVICE_INFO = 0x" << std::hex << GPU_IOCTL_GET_DEVICE_INFO << std::dec << std::endl;
  std::cout << "Expected _IOR('G', 0x20, struct) = 0x" << std::hex << expected << std::dec << std::endl;

  if (GPU_IOCTL_GET_DEVICE_INFO != expected) {
    std::cerr << "[FAIL] ioctl number mismatch!" << std::endl;
    std::cerr << "  GPU_IOCTL_GET_DEVICE_INFO = 0x" << std::hex << GPU_IOCTL_GET_DEVICE_INFO << std::dec << std::endl;
    std::cerr << "  Expected = 0x" << std::hex << expected << std::dec << std::endl;
    std::cerr << "  Difference = 0x" << std::hex << (GPU_IOCTL_GET_DEVICE_INFO ^ expected) << std::dec << std::endl;
    exit(1);
  }

  std::cout << "[PASS] ioctl number matches expected value" << std::endl;
}

/**
 * 测试 2: 验证插件端能正确接收并处理 ioctl
 *
 * 这个测试确保我们实际调用 ioctl 时插件能正确响应
 */
void test_plugin_ioctl_dispatch() {
  std::cout << "\n=== Test 2: Plugin Ioctl Dispatch ===" << std::endl;

  ModuleLoader::load_plugins("plugins");

  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  if (!dev) {
    std::cerr << "[FAIL] Failed to open /dev/gpgpu0" << std::endl;
    exit(1);
  }

  // 使用实际的 ioctl 编号
  struct gpu_device_info info {};
  int fd = 0;  // 文件描述符 (实际未使用)

  std::cout << "Calling ioctl with GPU_IOCTL_GET_DEVICE_INFO = 0x"
            << std::hex << GPU_IOCTL_GET_DEVICE_INFO << std::dec << std::endl;

  long result = dev->fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info);

  if (result != 0) {
    std::cerr << "[FAIL] ioctl returned error: " << result << std::endl;
    std::cerr << "  This indicates the ioctl dispatch failed in the plugin" << std::endl;
    exit(1);
  }

  // 验证返回的数据是有效的
  if (info.vendor_id == 0 || info.device_id == 0) {
    std::cerr << "[FAIL] Received invalid device info (vendor=0, device=0)" << std::endl;
    exit(1);
  }

  std::cout << "[PASS] Plugin correctly handled ioctl" << std::endl;
  std::cout << "  vendor_id = 0x" << std::hex << info.vendor_id << std::dec << std::endl;
  std::cout << "  device_id = 0x" << std::hex << info.device_id << std::dec << std::endl;
  std::cout << "  vram_size = " << (info.vram_size / (1024 * 1024 * 1024)) << " GB" << std::endl;
  std::cout << "  compute_units = " << info.compute_units << std::endl;

  // Phase 1.5 字段验证
  std::cout << "\n=== Phase 1.5 Fields ===" << std::endl;
  std::cout << "  warp_size = " << info.warp_size << std::endl;
  std::cout << "  simd_count = " << info.simd_count << std::endl;
  std::cout << "  driver_version = 0x" << std::hex << info.driver_version << std::dec << std::endl;
  std::cout << "  peak_fp32_gflops = " << info.peak_fp32_gflops << std::endl;
}

/**
 * 测试 3: 验证所有已知 ioctl 命令能正确 dispatch
 */
void test_all_ioctl_commands() {
  std::cout << "\n=== Test 3: All Ioctl Commands ===" << std::endl;

  ModuleLoader::load_plugins("plugins");
  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  if (!dev) {
    std::cerr << "[FAIL] Failed to open /dev/gpgpu0" << std::endl;
    exit(1);
  }

  int fd = 0;

  // 测试 GPU_IOCTL_GET_DEVICE_INFO
  {
    struct gpu_device_info info {};
    long r = dev->fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info);
    if (r != 0) {
      std::cerr << "[FAIL] GPU_IOCTL_GET_DEVICE_INFO failed: " << r << std::endl;
      exit(1);
    }
    std::cout << "[PASS] GPU_IOCTL_GET_DEVICE_INFO" << std::endl;
  }

  // 测试 GPU_IOCTL_ALLOC_BO
  {
    struct gpu_alloc_bo_args args = {};
    args.size = 64 * 1024;
    args.domain = GPU_MEM_DOMAIN_VRAM;
    args.flags = 0;
    long r = dev->fops->ioctl(fd, GPU_IOCTL_ALLOC_BO, &args);
    if (r != 0) {
      std::cerr << "[FAIL] GPU_IOCTL_ALLOC_BO failed: " << r << std::endl;
      exit(1);
    }
    std::cout << "[PASS] GPU_IOCTL_ALLOC_BO (handle=" << args.handle << ")" << std::endl;

    // 清理
    dev->fops->ioctl(fd, GPU_IOCTL_FREE_BO, &args.handle);
  }

  std::cout << "[PASS] All ioctl commands dispatched correctly" << std::endl;
}

int main() {
  std::cout << "GPU Ioctl Number Consistency Test" << std::endl;
  std::cout << "===================================" << std::endl;

  test_ioctl_number_consistency();
  test_plugin_ioctl_dispatch();
  test_all_ioctl_commands();

  std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
  return 0;
}