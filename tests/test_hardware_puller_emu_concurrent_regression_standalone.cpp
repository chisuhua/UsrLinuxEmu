/*
 * test_hardware_puller_emu_concurrent_regression_standalone.cpp
 *
 * Issue #21 regression test — HardwarePullerEmu scanQueues race lock-in.
 *
 * Background (per d09d6bf commit message):
 *   The HardwarePullerEmu background thread (`runLoop`) iterated
 *   `active_queues_` (an std::map, red-black tree) without holding
 *   `mutex_`.  Concurrent `unregisterQueue()` from the test/driver
 *   thread did `active_queues_.erase(qid)` under `mutex_`.  Concurrent
 *   modification + iteration of an std::map is UB; gdb caught the
 *   SIGSEGV inside `std::_Rb_tree_increment` when test_gpu_ioctl_
 *   standalone ran immediately before test_gpu_plugin (the prior test
 *   left pending fences/queues that got unregistered mid-iteration).
 *
 *   The fix uses a snapshot pattern under mutex_ — copy (qid, queue*)
 *   pairs to a local vector, then iterate the snapshot without the
 *   lock.  This avoids both the race AND the deadlock that would
 *   occur if we held `mutex_` across `queue->dequeue()` (which has
 *   its own per-queue lock).
 *
 * This test locks in that fix via a concurrent stress test:
 *   - Spin up the puller (which calls scanQueues on every wakeup).
 *   - From a worker thread, rapidly register() / unregister() queues.
 *   - Ring the doorbell repeatedly to force scanQueues activity.
 *   - Run for thousands of cycles.
 *   - Stop puller, check no crash.
 *
 * If this test SIGSEGVs or hangs on clang+g++ builds, the
 * HardwarePullerEmu snapshot pattern has regressed — STOP and restore
 * the Issue #21 fix (see d09d6bf for the canonical patch).
 */

#include "catch_amalgamated.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "doorbell_emu.h"
#include "gpu_hal.h"
#include "gpu_queue.h"
#include "gpu_queue_emu.h"
#include "gpu_types.h"
#include "hardware_puller_emu.h"

namespace {

// ---- Mock HAL: returns zeros and never crashes. ----

int mock_hal_register_read(void* ctx, uint64_t off, uint64_t* out) {
  (void)ctx; (void)off; *out = 0; return 0;
}
int mock_hal_register_write(void* ctx, uint64_t off, uint64_t val) {
  (void)ctx; (void)off; (void)val; return 0;
}
int mock_hal_mem_read(void* ctx, uint64_t dev_addr, void* host_buf, uint64_t size) {
  (void)ctx; (void)dev_addr; memset(host_buf, 0, size); return 0;
}
int mock_hal_mem_write(void* ctx, uint64_t dev_addr, const void* host_buf, uint64_t size) {
  (void)ctx; (void)dev_addr; (void)host_buf; (void)size; return 0;
}
int mock_hal_mem_alloc(void* ctx, uint64_t size, uint64_t* out) {
  (void)ctx; (void)size; static uint64_t next_addr = 0x100000;
  *out = next_addr; next_addr += size; return 0;
}
int mock_hal_mem_free(void* ctx, uint64_t addr) {
  (void)ctx; (void)addr; return 0;
}
int mock_hal_fence_create(void* ctx, uint64_t* out) {
  (void)ctx; *out = 1; return 0;
}
int mock_hal_fence_read(void* ctx, uint64_t id, uint64_t* out) {
  (void)ctx; (void)id; *out = 1; return 0;
}
void mock_hal_doorbell_ring(void* ctx, uint32_t qid) {
  (void)ctx; (void)qid;
}
void mock_hal_interrupt_raise(void* ctx, uint32_t vec) {
  (void)ctx; (void)vec;
}
void mock_hal_time_wait(void* ctx, uint64_t us) {
  (void)ctx; (void)us;
}

struct gpu_hal_ops make_mock_hal() {
  struct gpu_hal_ops hal;
  std::memset(&hal, 0, sizeof(hal));
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

}  // namespace

TEST_CASE("Issue #21 lock-in: HardwarePullerEmu safe under concurrent register/unregister",
          "[sim][hardware_puller_emu][regression][issue-21]")
{
  /*
   * Stress test the exact race the original fix targeted: the puller
   * background thread is doing `scanQueues()` (which iterates
   * `active_queues_`), while the main test thread is rapidly
   * registerQueue()/unregisterQueue()'ing.  Pre-fix this would
   * SEGFAULT inside `std::_Rb_tree_increment` on clang+g++.
   */
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  // Pre-allocate a pool of queues that live for the duration of the test
  // so registerQueue/unregisterQueue is the only thing racing, not
  // allocation/deallocation.
  constexpr int kQueuePool = 16;
  std::vector<std::unique_ptr<GpuQueueEmu>> queues;
  queues.reserve(kQueuePool);
  for (int i = 0; i < kQueuePool; ++i) {
    queues.push_back(std::unique_ptr<GpuQueueEmu>(
        new GpuQueueEmu(static_cast<uint32_t>(i + 1), 0, 50, 16)));
  }

  std::atomic<bool> stop{false};
  std::atomic<int> register_count{0};
  std::atomic<int> unregister_count{0};

  // Worker thread: relentlessly register + unregister different queue IDs.
  std::thread churn([&]() {
    int local_counter = 0;
    while (!stop.load(std::memory_order_relaxed)) {
      uint32_t qid = static_cast<uint32_t>((local_counter % kQueuePool) + 1);
      GpuQueueEmu* q = queues[qid - 1].get();
      puller.registerQueue(q);
      register_count.fetch_add(1, std::memory_order_relaxed);
      // Yield between operations to maximize the chance of the
      // puller thread interleaving between our register and unregister.
      std::this_thread::sleep_for(std::chrono::microseconds(50));
      puller.unregisterQueue(qid);
      unregister_count.fetch_add(1, std::memory_order_relaxed);
      std::this_thread::sleep_for(std::chrono::microseconds(50));
      local_counter++;
    }
  });

  // Start the puller AFTER setting up the worker so the runLoop thread
  // is running while churn is active.  Doorbell rings keep the puller
  // awake and scanning queues.
  puller.start();

  // Hammer the doorbell to force the puller out of any pure IDLE
  // paths and into scanQueues() repeatedly.  This is what would
  // exercise the concurrent erase+iterate pattern at high rate.
  for (int i = 0; i < 3000 && !stop.load(std::memory_order_relaxed); ++i) {
    doorbell.write(0);
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }

  stop.store(true, std::memory_order_relaxed);
  churn.join();
  puller.stop();

  // Sanity gates — if either of these fails, the test setup is broken
  // (e.g., puller thread didn't pump or churn didn't actually race),
  // and we'd silently miss regressions.
  CHECK(register_count.load() > 100);
  CHECK(unregister_count.load() > 100);
  // Reached without crashing.  No specific value to check — the test
  // passing means no UB happened.
  SUCCEED("puller survived concurrent register/unregister cycle without UB");
}

TEST_CASE("Issue #21 lock-in: HardwarePullerEmu scanQueues survives rapid reconfiguration",
          "[sim][hardware_puller_emu][regression][issue-21]")
{
  /*
   * Variant: the same race, but running on a separate thread that
   * repeatedly registers, immediately submits a batch, and unregisters.
   * Exercises the path where scanQueues sees a queue with pending work
   * during erase — the most UB-prone interleaving.
   */
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  constexpr int kQueuePool = 8;
  std::vector<std::unique_ptr<GpuQueueEmu>> queues;
  queues.reserve(kQueuePool);
  for (int i = 0; i < kQueuePool; ++i) {
    queues.push_back(std::unique_ptr<GpuQueueEmu>(
        new GpuQueueEmu(static_cast<uint32_t>(i + 1), 0, 50, 16)));
  }

  std::atomic<bool> stop{false};
  std::atomic<int> submit_count{0};

  std::thread churn([&]() {
    int local_counter = 0;
    while (!stop.load(std::memory_order_relaxed)) {
      uint32_t qid = static_cast<uint32_t>((local_counter % kQueuePool) + 1);
      GpuQueueEmu* q = queues[qid - 1].get();
      puller.registerQueue(q);
      // Force the puller to want to process something: ring doorbell
      // for this queue id (the puller's doorbell CB just sets the
      // doorbell_pending_ flag and wakes runLoop).
      doorbell.write(qid);
      // Tiny pause so the runLoop can actually iterate and try to
      // consume before we yank the queue.
      std::this_thread::sleep_for(std::chrono::microseconds(20));
      puller.unregisterQueue(qid);
      submit_count.fetch_add(1, std::memory_order_relaxed);
      local_counter++;
    }
  });

  puller.start();

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (std::chrono::steady_clock::now() < deadline) {
    doorbell.write(0);
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }

  stop.store(true, std::memory_order_relaxed);
  churn.join();
  puller.stop();

  CHECK(submit_count.load() > 100);
  SUCCEED("puller survived rapid register/submit/unregister cycle without UB");
}
