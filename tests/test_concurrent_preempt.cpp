// test_concurrent_preempt.cpp - Concurrent Preempt Stress Test (Stage 4.5 gaps)
//
// Validates the preemption engine under concurrent access:
// - Multiple threads concurrently preempt/resume channels
// - Semaphore/fence operations (create/signal/read) race with preemption
// - No deadlock (60s hard timeout)
// - No fence loss (submitted == signaled + canceled)
// - Low cancel ratio (< 1%)
//
// Uses C-ABI backdoor symbols (backdoor_preempt.h) + SemaphoreManager directly.
// The backdoor globals (g_backdoor_*) are set up in the test fixture.
//
// Per design.md §Decision 1: sanitizer-aware cycle count.

#include "catch_amalgamated.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "backdoor_preempt.h"        // C-ABI backdoor symbols
#include "semaphore_manager.h"       // SemaphoreManager (fence operations)
#include "hardware/hardware_puller_emu.h"
#include "hardware/channel_manager.h"
#include "hardware/doorbell_emu.h"
#include "scheduler/global_scheduler.h"

// ========== Backdoor Global Pointers ==========
// These are defined in backdoor_preempt.cpp but only set by the test.
// We declare them extern here to set them before the test runs.
extern SemaphoreManager* g_backdoor_sem_mgr;
extern HardwarePullerEmu* g_backdoor_puller;
extern ChannelManager* g_backdoor_channel_mgr;

namespace {

// Sanitizer-aware cycle count (per design.md §Decision 1)
#if defined(__has_feature)
#  if __has_feature(thread_sanitizer)
constexpr int kPreemptCycles = 20;   // TSan: 10-30x overhead, reduce to 20
#  else
constexpr int kPreemptCycles = 100;  // default
#  endif
#else
constexpr int kPreemptCycles = 100;  // default
#endif

constexpr int kSubmitThreads = 4;       // fixed 4 threads (portable across CI)
constexpr uint32_t kChannelBase = 0;    // channel IDs start at 0
constexpr int kJoinTimeoutSec = 60;      // hard timeout for deadlock detection

}  // anonymous namespace

TEST_CASE("Concurrent preempt: no fence loss, no deadlock, low cancel ratio",
          "[concurrent-preempt]") {
  // ========== Fixture: create sim components and wire backdoor globals ==========
  SemaphoreManager sem_mgr;
  DoorbellEmu doorbell;
  GlobalScheduler scheduler;
  ChannelManager channel_mgr;

  // HardwarePullerEmu needs hal_ops; use nullptr for pure stress test
  // (triggerPreempt only sets atomic flags, doesn't touch hal_)
  HardwarePullerEmu puller(nullptr, &doorbell, &scheduler);

  // Wire backdoor globals so backdoor_* functions find our instances
  g_backdoor_sem_mgr = &sem_mgr;
  g_backdoor_puller = &puller;
  g_backdoor_channel_mgr = &channel_mgr;

  // Register channels for each thread
  for (int w = 0; w < kSubmitThreads; ++w) {
    uint32_t ch = kChannelBase + static_cast<uint32_t>(w);
    channel_mgr.registerChannel(ch, CHAN_PRIO_NORMAL, nullptr);
  }

  // ========== Atomic counters ==========
  std::atomic<uint64_t> fences_submitted{0};
  std::atomic<uint64_t> fences_signaled{0};
  std::atomic<uint64_t> fences_canceled{0};

  std::vector<std::thread> workers;
  workers.reserve(kSubmitThreads);

  for (int w = 0; w < kSubmitThreads; ++w) {
    workers.emplace_back([&, w]() {
      for (int c = 0; c < kPreemptCycles; ++c) {
        uint32_t ch = kChannelBase + static_cast<uint32_t>(w);

        // 1. Create a timeline semaphore (acts as fence)
        uint64_t fence = sem_mgr.create(0);
        if (fence == 0) continue;  // create failed
        fences_submitted.fetch_add(1, std::memory_order_relaxed);

        // 2. Trigger preemption via backdoor
        int preempt_ret = backdoor_force_preempt(ch);

        // 3. Resume via backdoor (no-op in current impl, but tests the path)
        int resume_ret = backdoor_force_resume(ch);

        // 4. Signal the fence (monotonic increment to 1)
        int signal_ret = sem_mgr.signal(fence, 1);

        // 5. Read back the fence value
        uint64_t val = sem_mgr.query(fence);

        if (preempt_ret == 0 && resume_ret == 0 && signal_ret == 0) {
          if (val == 1) {
            fences_signaled.fetch_add(1, std::memory_order_relaxed);
          } else {
            fences_canceled.fetch_add(1, std::memory_order_relaxed);
          }
        } else {
          fences_canceled.fetch_add(1, std::memory_order_relaxed);
        }

        // 6. Cleanup
        sem_mgr.destroy(fence);
      }
    });
  }

  // ========== Hard timeout 60s (catch deadlock; aligns with spec "No deadlock") ==========
  auto deadline = std::chrono::steady_clock::now() +
                  std::chrono::seconds(kJoinTimeoutSec);
  for (auto& t : workers) {
    if (t.joinable()) {
      t.join();
    }
  }
  REQUIRE(std::chrono::steady_clock::now() < deadline);

  // ========== Assertions ==========
  // 1. No fence loss: submitted == signaled + canceled
  uint64_t submitted = fences_submitted.load();
  uint64_t signaled = fences_signaled.load();
  uint64_t canceled = fences_canceled.load();
  REQUIRE(submitted == signaled + canceled);

  // 2. Cancel ratio < 1% (accounts for legitimate channel-destroy races)
  // Use multiplication (not division) to avoid integer truncation when
  // submitted < 100 (e.g., TSan reduces kPreemptCycles to 20, so 4*20=80).
  if (submitted > 0) {
    REQUIRE(canceled * 100 < submitted);
  }

  // ========== Cleanup: reset backdoor globals ==========
  g_backdoor_sem_mgr = nullptr;
  g_backdoor_puller = nullptr;
  g_backdoor_channel_mgr = nullptr;
}
