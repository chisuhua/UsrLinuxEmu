/**
 * test_gpu_ringbuffer.cpp — GpuQueueEmu Ring Buffer 单元测试 (ADR-024)
 *
 * 测试 GpuQueueEmu 的 Ring Buffer 读写行为：
 * - attachSharedMemory / dequeue / pendingCount
 * - 多 entry 循环
 * - 空队列 / 满队列边界
 * - wrap-around (环形索引)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "gpu_queue_emu.h"
#include "gpu_queue.h"
#include "gpu_types.h"

static int failures = 0;
#define EXPECT_EQ(a, b) do { \
  if ((a) != (b)) { \
    std::cerr << "FAIL: " << #a << " == " << #b \
              << " (got " << (a) << ")\n"; \
    failures++; \
  } \
} while(0)

#define EXPECT_TRUE(c) do { \
  if (!(c)) { \
    std::cerr << "FAIL: " << #c << " is false\n"; \
    failures++; \
  } \
} while(0)

#define EXPECT_FALSE(c) do { \
  if ((c)) { \
    std::cerr << "FAIL: " << #c << " is true\n"; \
    failures++; \
  } \
} while(0)

// 分配零初始化的 Ring Buffer 共享内存（64 字节对齐）
// entry_count: GPFIFO entry 个数; 返回 nullptr 表示分配失败
static void* alloc_ring_shm(size_t entry_count) {
    size_t shm_size = sizeof(gpu_ring_header) + entry_count * sizeof(gpu_gpfifo_entry);
    size_t aligned_size = (shm_size + 63) & ~63ULL;
    void* p = std::aligned_alloc(64, aligned_size);
    if (p) memset(p, 0, shm_size);
    return p;
}

// ========== 测试用例 ==========

/** 基本 enqueue/dequeue 循环 */
int test_basic_enqueue_dequeue() {
  GpuQueueEmu queue(0, GPU_QUEUE_COMPUTE, 50, 16);

  // 分配共享内存
  size_t shm_size = sizeof(gpu_ring_header) + 16 * sizeof(gpu_gpfifo_entry);
  void* shm = alloc_ring_shm(16);
  if (!shm) { std::cerr << "FAIL: aligned_alloc failed\n"; return 1; }

  int ret = queue.attachSharedMemory(shm, shm_size);
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(queue.ringHeader() != nullptr);
  EXPECT_FALSE(queue.hasPending());
  EXPECT_EQ(queue.pendingCount(), 0u);

  // 直接写 Ring Buffer（模拟用户态）
  auto* header = queue.ringHeader();
  auto* entries = reinterpret_cast<gpu_gpfifo_entry*>(
      static_cast<uint8_t*>(shm) + sizeof(gpu_ring_header));

  // 写入 entry 0
  entries[0].valid = 1;
  entries[0].method = GPU_OP_MEMCPY;
  entries[0].subchannel = 0;
  entries[0].payload[0] = 0x1000;
  entries[0].payload[1] = 0x2000;
  entries[0].payload[2] = 256;
  header->write_idx = 1;

  EXPECT_TRUE(queue.hasPending());
  EXPECT_EQ(queue.pendingCount(), 1u);

  // 消费 entry 0
  gpu_gpfifo_entry out;
  EXPECT_TRUE(queue.dequeue(&out));
  EXPECT_EQ(out.valid, 1u);
  EXPECT_EQ(out.method, GPU_OP_MEMCPY);
  EXPECT_EQ(out.payload[0], 0x1000ull);
  EXPECT_EQ(header->read_idx, 1u);
  EXPECT_FALSE(queue.hasPending());
  EXPECT_EQ(queue.pendingCount(), 0u);

  free(shm);
  std::cout << "PASS: test_basic_enqueue_dequeue\n";
  return failures > 0 ? 1 : 0;
}

/** 多 entry 顺序读写 */
int test_multiple_entries() {
  GpuQueueEmu queue(0, GPU_QUEUE_COMPUTE, 50, 64);

  size_t shm_size = sizeof(gpu_ring_header) + 64 * sizeof(gpu_gpfifo_entry);
  void* shm = alloc_ring_shm(64);
  if (!shm) { std::cerr << "FAIL: aligned_alloc failed\n"; return 1; }

  queue.attachSharedMemory(shm, shm_size);
  auto* header = queue.ringHeader();
  auto* entries = reinterpret_cast<gpu_gpfifo_entry*>(
      static_cast<uint8_t*>(shm) + sizeof(gpu_ring_header));

  // 写入 5 个 entry
  for (int i = 0; i < 5; i++) {
    entries[i].valid = 1;
    entries[i].method = GPU_OP_LAUNCH_KERNEL;
    entries[i].subchannel = static_cast<u32>(i);
    entries[i].payload[0] = static_cast<u64>(i * 0x100);
  }
  header->write_idx = 5;

  EXPECT_EQ(queue.pendingCount(), 5u);

  // 按序消费
  for (int i = 0; i < 5; i++) {
    gpu_gpfifo_entry out;
    EXPECT_TRUE(queue.dequeue(&out));
    EXPECT_EQ(out.subchannel, static_cast<u32>(i));
    EXPECT_EQ(out.payload[0], static_cast<u64>(i * 0x100));
  }

  EXPECT_FALSE(queue.hasPending());
  EXPECT_EQ(queue.pendingCount(), 0u);

  free(shm);
  std::cout << "PASS: test_multiple_entries\n";
  return failures > 0 ? 1 : 0;
}

/** 空队列 dequeue 返回 false */
int test_empty_queue() {
  GpuQueueEmu queue(0, GPU_QUEUE_COPY, 0, 16);

  size_t shm_size = sizeof(gpu_ring_header) + 16 * sizeof(gpu_gpfifo_entry);
  void* shm = alloc_ring_shm(16);
  if (!shm) { std::cerr << "FAIL: aligned_alloc failed\n"; return 1; }

  queue.attachSharedMemory(shm, shm_size);

  // 空队列
  gpu_gpfifo_entry out;
  EXPECT_FALSE(queue.dequeue(&out));
  EXPECT_FALSE(queue.hasPending());
  EXPECT_EQ(queue.pendingCount(), 0u);

  free(shm);
  std::cout << "PASS: test_empty_queue\n";
  return failures > 0 ? 1 : 0;
}

/** Ring Buffer wrap-around */
int test_wrap_around() {
  GpuQueueEmu queue(0, GPU_QUEUE_COMPUTE, 50, 4);  // 小容量

  size_t shm_size = sizeof(gpu_ring_header) + 4 * sizeof(gpu_gpfifo_entry);
  void* shm = alloc_ring_shm(4);
  if (!shm) { std::cerr << "FAIL: aligned_alloc failed\n"; return 1; }

  queue.attachSharedMemory(shm, shm_size);
  auto* header = queue.ringHeader();
  auto* entries = reinterpret_cast<gpu_gpfifo_entry*>(
      static_cast<uint8_t*>(shm) + sizeof(gpu_ring_header));

  // 第一阶段：填充 4 个 entry（填满整圈）
  for (int i = 0; i < 4; i++) {
    entries[i].valid = 1;
    entries[i].method = GPU_OP_MEMCPY;
    entries[i].payload[0] = static_cast<u64>(i);
  }
  header->write_idx = 4;

  // 全部消费掉
  for (int i = 0; i < 4; i++) {
    gpu_gpfifo_entry out;
    EXPECT_TRUE(queue.dequeue(&out));
    EXPECT_EQ(out.payload[0], static_cast<u64>(i));
  }
  EXPECT_EQ(header->read_idx, 4u);

  // 第二阶段：再写 2 个（回绕: write_idx=4→6, 物理 slot = 4%4=0, 5%4=1）
  entries[0].payload[0] = 100;  // slot 0 (write_idx=4 → 4%4=0)
  entries[1].payload[0] = 200;  // slot 1 (write_idx=5 → 5%4=1)
  header->write_idx = 6;

  // dequeue 从 read_idx=4 开始: 4%4=0 → entries[0], 5%4=1 → entries[1]
  EXPECT_EQ(queue.pendingCount(), 2u);
  EXPECT_TRUE(queue.hasPending());

  gpu_gpfifo_entry out;
  EXPECT_TRUE(queue.dequeue(&out));
  EXPECT_EQ(out.payload[0], 100ull);

  EXPECT_TRUE(queue.dequeue(&out));
  EXPECT_EQ(out.payload[0], 200ull);

  EXPECT_FALSE(queue.hasPending());
  EXPECT_EQ(header->read_idx, 6u);

  free(shm);
  std::cout << "PASS: test_wrap_around\n";
  return failures > 0 ? 1 : 0;
}

/** attachSharedMemory 错误处理 */
int test_attach_errors() {
  GpuQueueEmu queue(0, GPU_QUEUE_COMPUTE, 0, 16);

  // null ptr → -EFAULT
  EXPECT_EQ(queue.attachSharedMemory(nullptr, 1024), -EFAULT);

  // too small → -EINVAL
  void* shm = std::aligned_alloc(64, 64);
  if (!shm) { std::cerr << "FAIL: aligned_alloc failed\n"; return 1; }
  EXPECT_EQ(queue.attachSharedMemory(shm, 8), -EINVAL);
  free(shm);

  std::cout << "PASS: test_attach_errors\n";
  return failures > 0 ? 1 : 0;
}

// ========== main ==========

int main() {
  int total = 0;
  int passed = 0;

  auto run = [&](const char* name, int(*fn)()) {
    total++;
    failures = 0;
    int ret = fn();
    if (ret == 0) passed++;
    else std::cerr << ">>> FAILED: " << name << "\n\n";
  };

  run("test_basic_enqueue_dequeue", test_basic_enqueue_dequeue);
  run("test_multiple_entries", test_multiple_entries);
  run("test_empty_queue", test_empty_queue);
  run("test_wrap_around", test_wrap_around);
  run("test_attach_errors", test_attach_errors);

  std::cout << "\n=== " << passed << "/" << total << " tests passed ===\n";
  return (passed == total) ? 0 : 1;
}
