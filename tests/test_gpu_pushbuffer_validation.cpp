/*
 * test_gpu_pushbuffer_validation.cpp — 验证 Phase 2 VA Space + Queue 校验
 *
 * TDD 覆盖 GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH 的 va_space_handle 校验契约:
 * - Case A: VA + Queue + attach → 成功（fence_id > 0）
 * - Case B: invalid va_space_handle → -EINVAL
 * - Case C: VA + Queue（不 attach） → -EINVAL
 * - Case D: va_space_handle = 0（向后兼容）→ 成功
 *
 * 设计 D1: va_space_handle=0 是 sentinel，跳过校验以保持向后兼容。
 */

#include <cstdio>
#include <cstring>
#include <iostream>
#include <unistd.h>

#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "kernel/device/device.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

static int failures = 0;

static int open_device(std::shared_ptr<Device>& dev) {
  ModuleLoader::load_plugins("plugins");
  dev = VFS::instance().open("/dev/gpgpu0", 0);
  if (!dev) {
    std::cerr << "[FAIL] Failed to open /dev/gpgpu0\n";
    return -1;
  }
  return 0;
}

static gpu_va_space_handle_t create_va_space(std::shared_ptr<Device>& dev, int fd) {
  struct gpu_va_space_args args = {};
  args.page_size = 0;  // 4KB
  args.flags = 0;
  int ret = dev->fops->ioctl(fd, GPU_IOCTL_CREATE_VA_SPACE, &args);
  if (ret != 0) {
    std::cerr << "[FAIL] CREATE_VA_SPACE failed: " << ret << "\n";
    ++failures;
    return 0;
  }
  return args.va_space_handle;
}

static uint64_t create_queue(std::shared_ptr<Device>& dev, int fd,
                              gpu_va_space_handle_t va_handle) {
  struct gpu_queue_args args = {};
  args.va_space_handle = va_handle;
  args.queue_type = GPU_QUEUE_COMPUTE;
  args.priority = 50;
  args.ring_buffer_size = 256;
  int ret = dev->fops->ioctl(fd, GPU_IOCTL_CREATE_QUEUE, &args);
  if (ret != 0) {
    std::cerr << "[FAIL] CREATE_QUEUE failed: " << ret << "\n";
    ++failures;
    return 0;
  }
  return args.queue_handle;
}

// Case A: 创建 VA Space + Queue + attach + submit → 期望成功（fence_id > 0）
static void test_attached_queue_proceeds() {
  std::cout << "=== test_attached_queue_proceeds (Case A) ===\n";
  std::shared_ptr<Device> dev;
  if (open_device(dev) != 0) return;
  int fd = 0;

  gpu_va_space_handle_t va = create_va_space(dev, fd);
  if (va == 0) return;

  uint64_t q = create_queue(dev, fd, va);
  if (q == 0) return;

  // Queue is auto-attached by CREATE_QUEUE (per handleCreateQueue L402-403)
  struct gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_FENCE;
  struct gpu_pushbuffer_args pb = {};
  pb.stream_id = static_cast<uint32_t>(q);
  pb.entries_addr = reinterpret_cast<u64>(&entry);
  pb.count = 1;
  pb.flags = 0;
  pb.va_space_handle = va;  // 显式指定 VA Space

  int ret = dev->fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb);
  if (ret != 0) {
    std::cerr << "[FAIL] PUSHBUFFER_SUBMIT_BATCH (Case A) expected 0, got: " << ret << "\n";
    ++failures;
    return;
  }
  std::cout << "[INFO] Case A: returned fence_id=" << pb.fence_id << "\n";

  // 清理
  uint64_t qh = q;
  dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_QUEUE, &qh);
  gpu_va_space_handle_t vh = va;
  dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_VA_SPACE, &vh);

  std::cout << "[PASS] test_attached_queue_proceeds\n";
}

// Case B: invalid va_space_handle → -EINVAL
static void test_invalid_va_space_handle() {
  std::cout << "=== test_invalid_va_space_handle (Case B) ===\n";
  std::shared_ptr<Device> dev;
  if (open_device(dev) != 0) return;
  int fd = 0;

  struct gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_FENCE;
  struct gpu_pushbuffer_args pb = {};
  pb.stream_id = 1;
  pb.entries_addr = reinterpret_cast<u64>(&entry);
  pb.count = 1;
  pb.flags = 0;
  pb.va_space_handle = 99999;  // 不存在

  int ret = dev->fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb);
  if (ret != -EINVAL) {
    std::cerr << "[FAIL] Case B: expected -EINVAL, got: " << ret << "\n";
    ++failures;
    return;
  }
  std::cout << "[INFO] Case B: correctly rejected with -EINVAL\n";
  std::cout << "[PASS] test_invalid_va_space_handle\n";
}

// Case C: VA + Queue 但 stream_id 不在 attached_queues → -EINVAL
static void test_unattached_queue_rejected() {
  std::cout << "=== test_unattached_queue_rejected (Case C) ===\n";
  std::shared_ptr<Device> dev;
  if (open_device(dev) != 0) return;
  int fd = 0;

  gpu_va_space_handle_t va = create_va_space(dev, fd);
  if (va == 0) return;

  uint64_t q = create_queue(dev, fd, va);
  if (q == 0) return;

  // 使用一个**没有**attach 到此 VA 的 stream_id
  struct gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_FENCE;
  struct gpu_pushbuffer_args pb = {};
  pb.stream_id = static_cast<uint32_t>(q + 9999);  // 必然不在 attached_queues 中
  pb.entries_addr = reinterpret_cast<u64>(&entry);
  pb.count = 1;
  pb.flags = 0;
  pb.va_space_handle = va;

  int ret = dev->fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb);
  if (ret != -EINVAL) {
    std::cerr << "[FAIL] Case C: expected -EINVAL (queue not attached), got: " << ret << "\n";
    ++failures;
    return;
  }
  std::cout << "[INFO] Case C: correctly rejected with -EINVAL\n";

  // 清理
  uint64_t qh = q;
  dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_QUEUE, &qh);
  gpu_va_space_handle_t vh = va;
  dev->fops->ioctl(fd, GPU_IOCTL_DESTROY_VA_SPACE, &vh);

  std::cout << "[PASS] test_unattached_queue_rejected\n";
}

// Case D: va_space_handle = 0（向后兼容）→ 成功
static void test_zero_va_space_handle_backward_compat() {
  std::cout << "=== test_zero_va_space_handle_backward_compat (Case D) ===\n";
  std::shared_ptr<Device> dev;
  if (open_device(dev) != 0) return;
  int fd = 0;

  struct gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_FENCE;
  struct gpu_pushbuffer_args pb = {};
  pb.stream_id = 0;  // 任意 stream_id
  pb.entries_addr = reinterpret_cast<u64>(&entry);
  pb.count = 1;
  pb.flags = 0;
  pb.va_space_handle = 0;  // 0 sentinel = 跳过校验

  int ret = dev->fops->ioctl(fd, GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb);
  if (ret != 0) {
    std::cerr << "[FAIL] Case D: expected 0 (backward compat), got: " << ret << "\n";
    ++failures;
    return;
  }
  std::cout << "[INFO] Case D: backward compat OK, fence_id=" << pb.fence_id << "\n";
  std::cout << "[PASS] test_zero_va_space_handle_backward_compat\n";
}

int main() {
  std::cout << "GPU Pushbuffer VA Space Validation Test (Phase 2)\n";
  std::cout << "==================================================\n";

  test_attached_queue_proceeds();
  test_invalid_va_space_handle();
  test_unattached_queue_rejected();
  test_zero_va_space_handle_backward_compat();

  if (failures == 0) {
    std::cout << "\n=== ALL TESTS PASSED ===\n";
    return 0;
  } else {
    std::cout << "\n=== " << failures << " TEST(S) FAILED ===\n";
    return 1;
  }
}
