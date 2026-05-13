/*
 * test_va_space.cpp — 验证 Phase 2 VA Space 抽象
 *
 * TDD RED 阶段: 测试应该在实现前 FAIL，实现后 PASS
 *
 * Phase 2 任务:
 * - GPU_IOCTL_CREATE_VA_SPACE: 创建虚拟地址空间
 * - GPU_IOCTL_DESTROY_VA_SPACE: 销毁虚拟地址空间
 * - GPU_IOCTL_REGISTER_GPU: 注册 GPU 到 VA Space
 * - GPU_IOCTL_CREATE_QUEUE: 验证 VA Space 必须存在
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unistd.h>

#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

int test_va_space_create_destroy() {
  std::cout << "=== test_va_space_create_destroy ===\n";

  // 加载插件
  ModuleLoader::load_plugins("plugins");

  // 打开设备
  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  if (!dev) {
    std::cerr << "[FAIL] Failed to open /dev/gpgpu0\n";
    return 1;
  }

  int fd = 0;

  // 测试 1: CREATE_VA_SPACE with 64KB page size (page_size=1)
  {
    struct gpu_va_space_args args = {};
    args.page_size = 1;  // 64KB
    args.flags = 0;

    int ret = dev->fops->ioctl(fd, GPU_IOCTL_CREATE_VA_SPACE, &args);
    if (ret != 0) {
      std::cerr << "[FAIL] CREATE_VA_SPACE failed: " << ret << "\n";
      return 1;
    }

    if (args.va_space_handle == 0) {
      std::cerr << "[FAIL] CREATE_VA_SPACE returned handle=0\n";
      return 1;
    }

    std::cout << "[INFO] Created VA Space: handle=" << args.va_space_handle
              << " page_size=64KB\n";

    // 测试 2: DESTROY_VA_SPACE
    gpu_va_space_handle_t destroy_handle = args.va_space_handle;
    ret = dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_VA_SPACE, &destroy_handle);
    if (ret != 0) {
      std::cerr << "[FAIL] DESTROY_VA_SPACE failed: " << ret << "\n";
      return 1;
    }

    std::cout << "[INFO] Destroyed VA Space: handle=" << destroy_handle << "\n";
  }

  // 测试 3: CREATE_VA_SPACE with 4KB page size (page_size=0)
  {
    struct gpu_va_space_args args = {};
    args.page_size = 0;  // 4KB
    args.flags = 0;

    int ret = dev->fops->ioctl(fd, GPU_IOCTL_CREATE_VA_SPACE, &args);
    if (ret != 0) {
      std::cerr << "[FAIL] CREATE_VA_SPACE (4KB) failed: " << ret << "\n";
      return 1;
    }

    std::cout << "[INFO] Created VA Space: handle=" << args.va_space_handle
              << " page_size=4KB\n";

    // 清理
    gpu_va_space_handle_t destroy_handle = args.va_space_handle;
    dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_VA_SPACE, &destroy_handle);
  }

  std::cout << "[PASS] test_va_space_create_destroy\n";
  return 0;
}

int test_va_space_with_queue() {
  std::cout << "=== test_va_space_with_queue ===\n";

  // 加载插件
  ModuleLoader::load_plugins("plugins");

  // 打开设备
  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  if (!dev) {
    std::cerr << "[FAIL] Failed to open /dev/gpgpu0\n";
    return 1;
  }

  int fd = 0;

  // 1. 创建 VA Space
  struct gpu_va_space_args va_args = {};
  va_args.page_size = 1;  // 64KB
  va_args.flags = 0;

  int ret = dev->fops->ioctl(fd, GPU_IOCTL_CREATE_VA_SPACE, &va_args);
  if (ret != 0) {
    std::cerr << "[FAIL] CREATE_VA_SPACE failed: " << ret << "\n";
    return 1;
  }

  std::cout << "[INFO] Created VA Space: handle=" << va_args.va_space_handle << "\n";

  // 2. 创建 Queue 附加到 VA Space
  struct gpu_queue_args queue_args = {};
  queue_args.va_space_handle = va_args.va_space_handle;
  queue_args.queue_type = GPU_QUEUE_COMPUTE;
  queue_args.priority = 50;
  queue_args.ring_buffer_size = 256;

  ret = dev->fops->ioctl(fd, GPU_IOCTL_CREATE_QUEUE, &queue_args);
  if (ret != 0) {
    std::cerr << "[FAIL] CREATE_QUEUE failed: " << ret << "\n";
    return 1;
  }

  if (queue_args.queue_handle == 0) {
    std::cerr << "[FAIL] CREATE_QUEUE returned handle=0\n";
    return 1;
  }

  if (queue_args.doorbell_pgoff == 0) {
    std::cerr << "[FAIL] CREATE_QUEUE returned doorbell_pgoff=0\n";
    return 1;
  }

  std::cout << "[INFO] Created Queue: handle=" << queue_args.queue_handle
            << " doorbell_pgoff=0x" << std::hex << queue_args.doorbell_pgoff << std::dec << "\n";

  // 3. 查询 Queue 信息
  struct gpu_queue_info_args query_args = {};
  query_args.queue_handle = queue_args.queue_handle;

  ret = dev->fops->ioctl(fd, GPU_IOCTL_QUERY_QUEUE, &query_args);
  if (ret != 0) {
    std::cerr << "[FAIL] QUERY_QUEUE failed: " << ret << "\n";
    return 1;
  }

  std::cout << "[INFO] Query Queue: type=" << query_args.queue_type
            << " id=" << query_args.queue_id
            << " doorbell_offset=0x" << std::hex << query_args.doorbell_offset << std::dec << "\n";

  // 4. 销毁 Queue
  uint64_t queue_handle = queue_args.queue_handle;
  ret = dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_QUEUE, &queue_handle);
  if (ret != 0) {
    std::cerr << "[FAIL] DESTROY_QUEUE failed: " << ret << "\n";
    return 1;
  }

  // 5. 销毁 VA Space
  gpu_va_space_handle_t va_handle = va_args.va_space_handle;
  ret = dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_VA_SPACE, &va_handle);
  if (ret != 0) {
    std::cerr << "[FAIL] DESTROY_VA_SPACE failed: " << ret << "\n";
    return 1;
  }

  std::cout << "[PASS] test_va_space_with_queue\n";
  return 0;
}

int test_cascade_destroy() {
  std::cout << "=== test_cascade_destroy ===\n";

  // 加载插件
  ModuleLoader::load_plugins("plugins");

  // 打开设备
  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  if (!dev) {
    std::cerr << "[FAIL] Failed to open /dev/gpgpu0\n";
    return 1;
  }

  int fd = 0;

  // 1. 创建 VA Space
  struct gpu_va_space_args va_args = {};
  va_args.page_size = 1;
  va_args.flags = 0;

  int ret = dev->fops->ioctl(fd, GPU_IOCTL_CREATE_VA_SPACE, &va_args);
  if (ret != 0) {
    std::cerr << "[FAIL] CREATE_VA_SPACE failed: " << ret << "\n";
    return 1;
  }

  std::cout << "[INFO] Created VA Space: handle=" << va_args.va_space_handle << "\n";

  // 2. 创建 Queue
  struct gpu_queue_args queue_args = {};
  queue_args.va_space_handle = va_args.va_space_handle;
  queue_args.queue_type = GPU_QUEUE_COMPUTE;
  queue_args.priority = 50;
  queue_args.ring_buffer_size = 256;

  ret = dev->fops->ioctl(fd, GPU_IOCTL_CREATE_QUEUE, &queue_args);
  if (ret != 0) {
    std::cerr << "[FAIL] CREATE_QUEUE failed: " << ret << "\n";
    return 1;
  }

  std::cout << "[INFO] Created Queue: handle=" << queue_args.queue_handle << "\n";

  // 3. 尝试销毁 VA Space（应该失败，因为还有 Queue）
  gpu_va_space_handle_t va_handle = va_args.va_space_handle;
  ret = dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_VA_SPACE, &va_handle);
  if (ret != -EBUSY) {
    std::cerr << "[FAIL] DESTROY_VA_SPACE should fail with EBUSY when queues attached, got: " << ret << "\n";
    return 1;
  }

  std::cout << "[INFO] DESTROY_VA_SPACE correctly returned EBUSY\n";

  // 4. 先销毁 Queue
  uint64_t queue_handle = queue_args.queue_handle;
  ret = dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_QUEUE, &queue_handle);
  if (ret != 0) {
    std::cerr << "[FAIL] DESTROY_QUEUE failed: " << ret << "\n";
    return 1;
  }

  // 5. 现在可以销毁 VA Space
  ret = dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_VA_SPACE, &va_handle);
  if (ret != 0) {
    std::cerr << "[FAIL] DESTROY_VA_SPACE failed: " << ret << "\n";
    return 1;
  }

  std::cout << "[PASS] test_cascade_destroy\n";
  return 0;
}

int test_invalid_va_space() {
  std::cout << "=== test_invalid_va_space ===\n";

  // 加载插件
  ModuleLoader::load_plugins("plugins");

  // 打开设备
  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  if (!dev) {
    std::cerr << "[FAIL] Failed to open /dev/gpgpu0\n";
    return 1;
  }

  int fd = 0;

  // 1. 创建 VA Space 获取有效句柄
  struct gpu_va_space_args va_args = {};
  va_args.page_size = 1;
  va_args.flags = 0;

  int ret = dev->fops->ioctl(fd, GPU_IOCTL_CREATE_VA_SPACE, &va_args);
  if (ret != 0) {
    std::cerr << "[FAIL] CREATE_VA_SPACE failed: " << ret << "\n";
    return 1;
  }

  // 2. 使用无效的 VA Space handle 创建 Queue（应该失败）
  struct gpu_queue_args queue_args = {};
  queue_args.va_space_handle = va_args.va_space_handle + 999;  // 无效
  queue_args.queue_type = GPU_QUEUE_COMPUTE;
  queue_args.priority = 50;
  queue_args.ring_buffer_size = 256;

  ret = dev->fops->ioctl(fd, GPU_IOCTL_CREATE_QUEUE, &queue_args);
  if (ret != -EINVAL) {
    std::cerr << "[FAIL] CREATE_QUEUE with invalid va_space_handle should fail with EINVAL, got: " << ret << "\n";
    return 1;
  }

  std::cout << "[INFO] CREATE_QUEUE correctly rejected invalid va_space_handle\n";

  // 清理
  gpu_va_space_handle_t va_handle = va_args.va_space_handle;
  dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_VA_SPACE, &va_handle);

  std::cout << "[PASS] test_invalid_va_space\n";
  return 0;
}

int main() {
  std::cout << "GPU VA Space Abstraction Test (Phase 2)\n";
  std::cout << "==========================================\n";

  int failures = 0;

  failures += test_va_space_create_destroy();
  failures += test_va_space_with_queue();
  failures += test_cascade_destroy();
  failures += test_invalid_va_space();

  if (failures == 0) {
    std::cout << "\n=== ALL TESTS PASSED ===\n";
    return 0;
  } else {
    std::cout << "\n=== " << failures << " TEST(S) FAILED ===\n";
    return 1;
  }
}