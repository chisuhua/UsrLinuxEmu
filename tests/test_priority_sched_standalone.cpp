// test_priority_sched_standalone.cpp
// Priority scheduling tests for GPU CP Phase 5.5 (Stage 4.4)
//
// TDD RED phase: Tests for priority-ordered dispatch, starvation protection,
// and priority inheritance in GlobalScheduler.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include "global_scheduler.h"

// ========== Priority Order Test ==========

int test_priority_order() {
  GlobalScheduler scheduler;

  gpu_gpfifo_entry low_entry = {};
  low_entry.valid = 1;
  low_entry.method = GPU_OP_LAUNCH_KERNEL;
  low_entry.semaphore_va = 0x1000;

  gpu_gpfifo_entry normal_entry = {};
  normal_entry.valid = 1;
  normal_entry.method = GPU_OP_LAUNCH_KERNEL;
  normal_entry.semaphore_va = 0x2000;

  gpu_gpfifo_entry high_entry = {};
  high_entry.valid = 1;
  high_entry.method = GPU_OP_LAUNCH_KERNEL;
  high_entry.semaphore_va = 0x3000;

  scheduler.enqueue_with_priority(low_entry, EngineType::COMPUTE, GPU_CHAN_PRI_LOW);
  scheduler.enqueue_with_priority(normal_entry, EngineType::COMPUTE, GPU_CHAN_PRI_NORMAL);
  scheduler.enqueue_with_priority(high_entry, EngineType::COMPUTE, GPU_CHAN_PRI_HIGH);

  WorkItem item1, item2, item3;
  if (!scheduler.dequeue(&item1)) {
    std::cerr << "FAIL: first dequeue failed\n";
    return 1;
  }
  if (!scheduler.dequeue(&item2)) {
    std::cerr << "FAIL: second dequeue failed\n";
    return 1;
  }
  if (!scheduler.dequeue(&item3)) {
    std::cerr << "FAIL: third dequeue failed\n";
    return 1;
  }

  if (item1.entry.semaphore_va != 0x3000) {
    std::cerr << "FAIL: expected HIGH (0x3000) first, got 0x"
              << std::hex << item1.entry.semaphore_va << "\n";
    return 1;
  }
  if (item2.entry.semaphore_va != 0x2000) {
    std::cerr << "FAIL: expected NORMAL (0x2000) second, got 0x"
              << std::hex << item2.entry.semaphore_va << "\n";
    return 1;
  }
  if (item3.entry.semaphore_va != 0x1000) {
    std::cerr << "FAIL: expected LOW (0x1000) third, got 0x"
              << std::hex << item3.entry.semaphore_va << "\n";
    return 1;
  }

  std::cout << "PASS: test_priority_order\n";
  return 0;
}

// ========== Same Priority FIFO Order Test ==========

int test_same_priority_fifo() {
  GlobalScheduler scheduler;

  gpu_gpfifo_entry entry1 = {};
  entry1.valid = 1;
  entry1.method = GPU_OP_LAUNCH_KERNEL;
  entry1.semaphore_va = 0x100;

  gpu_gpfifo_entry entry2 = {};
  entry2.valid = 1;
  entry2.method = GPU_OP_LAUNCH_KERNEL;
  entry2.semaphore_va = 0x200;

  gpu_gpfifo_entry entry3 = {};
  entry3.valid = 1;
  entry3.method = GPU_OP_LAUNCH_KERNEL;
  entry3.semaphore_va = 0x300;

  scheduler.enqueue_with_priority(entry1, EngineType::COMPUTE, GPU_CHAN_PRI_NORMAL);
  scheduler.enqueue_with_priority(entry2, EngineType::COMPUTE, GPU_CHAN_PRI_NORMAL);
  scheduler.enqueue_with_priority(entry3, EngineType::COMPUTE, GPU_CHAN_PRI_NORMAL);

  WorkItem item1, item2, item3;
  scheduler.dequeue(&item1);
  scheduler.dequeue(&item2);
  scheduler.dequeue(&item3);

  if (item1.entry.semaphore_va != 0x100 ||
      item2.entry.semaphore_va != 0x200 ||
      item3.entry.semaphore_va != 0x300) {
    std::cerr << "FAIL: same-priority entries should maintain FIFO order\n";
    return 1;
  }

  std::cout << "PASS: test_same_priority_fifo\n";
  return 0;
}

// ========== Starvation Protection Test ==========

int test_starvation_protection() {
  GlobalScheduler scheduler;

  for (int i = 0; i < 10; i++) {
    gpu_gpfifo_entry high_entry = {};
    high_entry.valid = 1;
    high_entry.method = GPU_OP_LAUNCH_KERNEL;
    high_entry.semaphore_va = 0x1000 + i;
    scheduler.enqueue_with_priority(high_entry, EngineType::COMPUTE, GPU_CHAN_PRI_HIGH);
  }

  gpu_gpfifo_entry low_entry = {};
  low_entry.valid = 1;
  low_entry.method = GPU_OP_LAUNCH_KERNEL;
  low_entry.semaphore_va = 0xFF;
  scheduler.enqueue_with_priority(low_entry, EngineType::COMPUTE, GPU_CHAN_PRI_LOW);

  bool low_dispatched = false;
  for (int cycle = 0; cycle < 12; cycle++) {
    WorkItem item;
    if (!scheduler.dequeue(&item)) {
      break;
    }
    if (item.entry.semaphore_va == 0xFF) {
      low_dispatched = true;
      break;
    }
  }

  if (!low_dispatched) {
    std::cerr << "FAIL: LOW entry was never dispatched (starved)\n";
    return 1;
  }

  std::cout << "PASS: test_starvation_protection\n";
  return 0;
}

// ========== Priority Inheritance Test ==========

int test_priority_inheritance() {
  GlobalScheduler scheduler;

  // Channel 0 is LOW priority
  gpu_gpfifo_entry low_entry = {};
  low_entry.valid = 1;
  low_entry.method = GPU_OP_LAUNCH_KERNEL;
  low_entry.semaphore_va = 0x100;
  scheduler.enqueue_with_priority(low_entry, EngineType::COMPUTE,
                                  GPU_CHAN_PRI_LOW, 0);

  // Channel 1 is REALTIME and will block on a semaphore signalled by channel 0
  gpu_gpfifo_entry rt_entry = {};
  rt_entry.valid = 1;
  rt_entry.method = GPU_OP_LAUNCH_KERNEL;
  rt_entry.semaphore_va = 0x200;
  scheduler.enqueue_with_priority(rt_entry, EngineType::COMPUTE,
                                  GPU_CHAN_PRI_REALTIME, 1);

  // Boost channel 0 (LOW -> REALTIME) via priority inheritance
  // Now both are REALTIME, but channel 0 was enqueued first (FIFO)
  scheduler.boost_priority(0, GPU_CHAN_PRI_REALTIME);

  WorkItem item1;
  if (!scheduler.dequeue(&item1)) {
    std::cerr << "FAIL: dequeue after boost failed\n";
    return 1;
  }

  // After boost, channel 0's entry (0x100) should be dispatched before
  // the REALTIME entry (0x200) because it was boosted to the same
  // priority level and enqueued first (FIFO within same priority).
  if (item1.entry.semaphore_va != 0x100) {
    std::cerr << "FAIL: expected boosted LOW entry (0x100) first, got 0x"
              << std::hex << item1.entry.semaphore_va << "\n";
    return 1;
  }

  // Restore original priority
  scheduler.restore_priority(0);

  // Verify the map is cleared
  if (scheduler.has_inherited_priority(0)) {
    std::cerr << "FAIL: inherited priority should be cleared after restore\n";
    return 1;
  }

  std::cout << "PASS: test_priority_inheritance\n";
  return 0;
}

// ========== Main ==========

int main() {
  int result = 0;

  std::cout << "=== Priority Scheduling TDD Tests ===\n";

  result |= test_priority_order();
  result |= test_same_priority_fifo();
  result |= test_starvation_protection();
  result |= test_priority_inheritance();

  if (result == 0) {
    std::cout << "\n=== ALL TESTS PASSED ===\n";
  } else {
    std::cout << "\n=== SOME TESTS FAILED ===\n";
  }

  return result;
}
