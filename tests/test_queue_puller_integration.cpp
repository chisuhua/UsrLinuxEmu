/**
 * test_queue_puller_integration.cpp — Queue + Puller 集成测试 (ADR-024)
 *
 * 测试完整提交路径:
 *   User writes Ring Buffer → Doorbell → Puller FETCH → DECODE → ...
 *
 * 覆盖场景:
 * - Queue + Puller 生命周期关联
 * - Ring Buffer entry → Puller 消费
 * - 多 Queue 并发
 * - Puller register/unregister Queue
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>

#include "gpu_types.h"
#include "gpu_hal.h"
#include "doorbell_emu.h"
#include "hardware_puller_emu.h"
#include "gpu_queue_emu.h"
#include "gpu_queue.h"

static int failures = 0;
#define EXPECT_EQ(a, b) do { \
  if ((a) != (b)) { \
    std::cerr << "FAIL: " << #a << " == " << #b \
              << " (got " << (a) << " line " << __LINE__ << ")\n"; \
    failures++; \
  } \
} while(0)

#define EXPECT_TRUE(c) do { \
  if (!(c)) { \
    std::cerr << "FAIL: " << #c << " is false (line " << __LINE__ << ")\n"; \
    failures++; \
  } \
} while(0)

#define EXPECT_FALSE(c) do { \
  if ((c)) { \
    std::cerr << "FAIL: " << #c << " is true (line " << __LINE__ << ")\n"; \
    failures++; \
  } \
} while(0)

template<typename Func>
bool wait_for_state(Func&& pred, int timeout_ms = 200, int poll_interval_ms = 1) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    if (pred()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
  }
  return pred();
}

// ========== Mock HAL ==========

static int mock_hal_register_read(void* ctx, uint64_t off, uint64_t* out) { (void)ctx; (void)off; *out = 0; return 0; }
static int mock_hal_register_write(void* ctx, uint64_t off, uint64_t val) { (void)ctx; (void)off; (void)val; return 0; }
static int mock_hal_mem_read(void* ctx, uint64_t dev_addr, void* host_buf, uint64_t size) {
  (void)ctx; (void)dev_addr;
  memset(host_buf, 0, size);
  return 0;
}
static int mock_hal_mem_write(void* ctx, uint64_t dev_addr, const void* host_buf, uint64_t size) {
  (void)ctx; (void)dev_addr; (void)host_buf; (void)size;
  return 0;
}
static int mock_hal_mem_alloc(void* ctx, uint64_t size, uint64_t* out) {
  (void)ctx; (void)size;
  static uint64_t next_addr = 0x100000;
  *out = next_addr;
  next_addr += size;
  return 0;
}
static int mock_hal_mem_free(void* ctx, uint64_t addr) { (void)ctx; (void)addr; return 0; }
static int mock_hal_fence_create(void* ctx, uint64_t* out) { (void)ctx; *out = 1; return 0; }
static int mock_hal_fence_read(void* ctx, uint64_t id, uint64_t* out) { (void)ctx; (void)id; *out = 1; return 0; }
static void mock_hal_doorbell_ring(void* ctx, uint32_t qid) { (void)ctx; (void)qid; }
static void mock_hal_interrupt_raise(void* ctx, uint32_t vec) { (void)ctx; (void)vec; }
static void mock_hal_time_wait(void* ctx, uint64_t us) { (void)ctx; (void)us; }

static struct gpu_hal_ops make_mock_hal() {
  struct gpu_hal_ops hal;
  memset(&hal, 0, sizeof(hal));
  hal.ctx = nullptr;
  hal.register_read = mock_hal_register_read;
  hal.register_write = mock_hal_register_write;
  hal.mem_read = mock_hal_mem_read;
  hal.mem_write = mock_hal_mem_write;
  hal.mem_alloc = mock_hal_mem_alloc;
  hal.mem_free = mock_hal_mem_free;
  hal.fence_create = mock_hal_fence_create;
  hal.fence_read = mock_hal_fence_read;
  hal.doorbell_ring = mock_hal_doorbell_ring;
  hal.interrupt_raise = mock_hal_interrupt_raise;
  hal.time_wait = mock_hal_time_wait;
  return hal;
}

// ========== 测试用例 ==========

/** 基本: Queue 注册到 Puller + Doorbell 触发消费 */
int test_queue_register_and_doorbell() {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  // 创建 Queue
  auto queue = std::make_shared<GpuQueueEmu>(1, GPU_QUEUE_COMPUTE, 50, 16);

  // 分配共享内存
  size_t shm_size = sizeof(gpu_ring_header) + 16 * sizeof(gpu_gpfifo_entry);
  void* shm = std::aligned_alloc(64, shm_size);
  EXPECT_TRUE(shm != nullptr);
  memset(shm, 0, shm_size);
  queue->attachSharedMemory(shm, shm_size);

  // 注册 Queue 到 Puller
  puller.registerQueue(queue.get());

  // 在 Ring Buffer 写入 entry（模拟用户态）
  auto* header = queue->ringHeader();
  auto* entries = reinterpret_cast<gpu_gpfifo_entry*>(
      static_cast<uint8_t*>(shm) + sizeof(gpu_ring_header));

  entries[0].valid = 1;
  entries[0].method = GPU_OP_MEMCPY;
  entries[0].payload[0] = 0xABCD;
  header->write_idx = 1;

  EXPECT_TRUE(queue->hasPending());
  EXPECT_EQ(queue->pendingCount(), 1u);

  // 启动 Puller
  puller.start();

  // 触发 Doorbell → 唤醒 Puller
  doorbell.write(queue->doorbellId());

  // 等 Puller 消费完
  bool consumed = wait_for_state([&]() {
    return !queue->hasPending();
  }, 500, 5);

  puller.stop();

  EXPECT_TRUE(consumed);
  EXPECT_EQ(queue->pendingCount(), 0u);

  free(shm);
  std::cout << "PASS: test_queue_register_and_doorbell\n";
  return failures > 0 ? 1 : 0;
}

/** Puller 从 Queue 消费后正确更新 read_idx */
int test_queue_read_idx_advance() {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  auto queue = std::make_shared<GpuQueueEmu>(2, GPU_QUEUE_COMPUTE, 50, 16);
  size_t shm_size = sizeof(gpu_ring_header) + 16 * sizeof(gpu_gpfifo_entry);
  void* shm = std::aligned_alloc(64, shm_size);
  EXPECT_TRUE(shm != nullptr);
  memset(shm, 0, shm_size);
  queue->attachSharedMemory(shm, shm_size);
  puller.registerQueue(queue.get());

  auto* header = queue->ringHeader();
  auto* entries = reinterpret_cast<gpu_gpfifo_entry*>(
      static_cast<uint8_t*>(shm) + sizeof(gpu_ring_header));

  // 写入 3 个 entry
  for (int i = 0; i < 3; i++) {
    entries[i].valid = 1;
    entries[i].method = GPU_OP_MEMCPY;
  }
  header->write_idx = 3;
  EXPECT_EQ(queue->pendingCount(), 3u);

  // 启动 Puller
  puller.start();

  // 触发一次 Doorbell → Puller 应消费所有 3 个 entry
  doorbell.write(queue->doorbellId());

  bool consumed = wait_for_state([&]() {
    return queue->pendingCount() == 0;
  }, 1000, 5);
  EXPECT_TRUE(consumed);

  puller.stop();
  EXPECT_EQ(header->read_idx, 3u);

  free(shm);
  std::cout << "PASS: test_queue_read_idx_advance\n";
  return failures > 0 ? 1 : 0;
}

/** 多 Queue 并发: 两个 Queue 各自有 entry */
int test_multi_queue_concurrent() {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  // 创建两个 Queue
  auto queue1 = std::make_shared<GpuQueueEmu>(10, GPU_QUEUE_COMPUTE, 50, 16);
  auto queue2 = std::make_shared<GpuQueueEmu>(20, GPU_QUEUE_COPY, 30, 16);

  auto setup_queue = [&](auto& q) -> void* {
    size_t shm_size = sizeof(gpu_ring_header) + 16 * sizeof(gpu_gpfifo_entry);
    void* shm = std::aligned_alloc(64, shm_size);
    EXPECT_TRUE(shm != nullptr);
    memset(shm, 0, shm_size);
    q->attachSharedMemory(shm, shm_size);
    puller.registerQueue(q.get());
    return shm;
  };

  void* shm1 = setup_queue(queue1);
  void* shm2 = setup_queue(queue2);

  auto write_entry = [](auto* header, auto* entries, int slot, u64 val) {
    entries[slot].valid = 1;
    entries[slot].method = GPU_OP_MEMCPY;
    entries[slot].subchannel = static_cast<u32>(val & 0xFF);
    header->write_idx = static_cast<uint32_t>(slot + 1);
  };

  auto* h1 = queue1->ringHeader();
  auto* e1 = reinterpret_cast<gpu_gpfifo_entry*>(
      static_cast<uint8_t*>(shm1) + sizeof(gpu_ring_header));
  write_entry(h1, e1, 0, 0xAA);

  auto* h2 = queue2->ringHeader();
  auto* e2 = reinterpret_cast<gpu_gpfifo_entry*>(
      static_cast<uint8_t*>(shm2) + sizeof(gpu_ring_header));
  write_entry(h2, e2, 0, 0xBB);

  puller.start();

  // 同时触发两个 doorbell
  doorbell.write(queue1->doorbellId());
  doorbell.write(queue2->doorbellId());

  // 等两个 Queue 都清空
  bool consumed = wait_for_state([&]() {
    return !queue1->hasPending() && !queue2->hasPending();
  }, 500, 5);
  EXPECT_TRUE(consumed);

  puller.stop();

  EXPECT_EQ(queue1->pendingCount(), 0u);
  EXPECT_EQ(queue2->pendingCount(), 0u);

  free(shm1);
  free(shm2);
  std::cout << "PASS: test_multi_queue_concurrent\n";
  return failures > 0 ? 1 : 0;
}

/** Unregister Queue 后 Puller 不再消费; 重新注册后恢复 */
int test_unregister_stops_consumption() {
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  auto queue = std::make_shared<GpuQueueEmu>(99, GPU_QUEUE_COMPUTE, 50, 16);
  size_t shm_size = sizeof(gpu_ring_header) + 16 * sizeof(gpu_gpfifo_entry);
  void* shm = std::aligned_alloc(64, shm_size);
  EXPECT_TRUE(shm != nullptr);
  memset(shm, 0, shm_size);
  queue->attachSharedMemory(shm, shm_size);

  auto* header = queue->ringHeader();
  auto* entries = reinterpret_cast<gpu_gpfifo_entry*>(
      static_cast<uint8_t*>(shm) + sizeof(gpu_ring_header));

  // 写入 1 个 entry, 先不注册 Queue
  entries[0].valid = 1;
  entries[0].method = GPU_OP_MEMCPY;
  header->write_idx = 1;

  // 启动 Puller (没有已注册 Queue)
  puller.start();

  // 触发 doorbell — Puller 会醒来, 但 active_queues_ 为空, 不会消费
  doorbell.write(queue->doorbellId());
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(queue->hasPending());  // 仍然有 pending

  // 注册 Queue
  puller.registerQueue(queue.get());
  // 再次触发 doorbell — Puller 现在应消费
  doorbell.write(queue->doorbellId());

  bool consumed = wait_for_state([&]() {
    return !queue->hasPending();
  }, 1000, 5);
  EXPECT_TRUE(consumed);
  EXPECT_EQ(header->read_idx, 1u);

  // 再写 1 个 entry, 测试 unregister 后不再消费
  entries[0].payload[0] = 42;
  header->write_idx = 2;

  puller.unregisterQueue(queue->queueId());
  doorbell.write(queue->doorbellId());
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  // 注销后, Puller 的 wake predicate 不检查已注销的 queue
  // onDoorbell 会唤醒 Puller, 但 scanQueues 找不到活跃 queue → 不消费
  EXPECT_TRUE(queue->hasPending());

  free(shm);
  std::cout << "PASS: test_unregister_stops_consumption\n";
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

  run("test_queue_register_and_doorbell", test_queue_register_and_doorbell);
  run("test_queue_read_idx_advance", test_queue_read_idx_advance);
  run("test_multi_queue_concurrent", test_multi_queue_concurrent);
  run("test_unregister_stops_consumption", test_unregister_stops_consumption);

  std::cout << "\n=== " << passed << "/" << total << " tests passed ===\n";
  return (passed == total) ? 0 : 1;
}
