/*
 * test_global_scheduler.cpp — TDD: GlobalScheduler FIFO + engine routing
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>

#include "global_scheduler.h"

static std::atomic<int> g_dispatch_count(0);
static std::atomic<EngineType> g_last_engine(EngineType::COMPUTE);

static void reset_counters() {
  g_dispatch_count.store(0);
  g_last_engine.store(EngineType::COMPUTE);
}

int test_scheduler_initial_queue_empty() {
  GlobalScheduler scheduler;

  if (scheduler.queueSize() != 0) {
    std::cerr << "FAIL: initial queue size should be 0, got " << scheduler.queueSize() << "\n";
    return 1;
  }

  std::cout << "PASS: test_scheduler_initial_queue_empty\n";
  return 0;
}

int test_scheduler_enqueue_dequeue() {
  GlobalScheduler scheduler;

  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_LAUNCH_KERNEL;

  scheduler.enqueue(entry, EngineType::COMPUTE);

  if (scheduler.queueSize() != 1) {
    std::cerr << "FAIL: queue size should be 1 after enqueue, got " << scheduler.queueSize() << "\n";
    return 1;
  }

  WorkItem item;
  if (!scheduler.dequeue(&item)) {
    std::cerr << "FAIL: dequeue should return true\n";
    return 1;
  }

  if (scheduler.queueSize() != 0) {
    std::cerr << "FAIL: queue size should be 0 after dequeue, got " << scheduler.queueSize() << "\n";
    return 1;
  }

  if (item.engine != EngineType::COMPUTE) {
    std::cerr << "FAIL: engine should be COMPUTE\n";
    return 1;
  }

  std::cout << "PASS: test_scheduler_enqueue_dequeue\n";
  return 0;
}

int test_scheduler_fifo_order() {
  GlobalScheduler scheduler;

  gpu_gpfifo_entry entry1 = {};
  entry1.valid = 1;
  entry1.method = GPU_OP_LAUNCH_KERNEL;

  gpu_gpfifo_entry entry2 = {};
  entry2.valid = 1;
  entry2.method = GPU_OP_MEMCPY;

  gpu_gpfifo_entry entry3 = {};
  entry3.valid = 1;
  entry3.method = GPU_OP_FENCE;

  scheduler.enqueue(entry1, EngineType::COMPUTE);
  scheduler.enqueue(entry2, EngineType::COPY);
  scheduler.enqueue(entry3, EngineType::FIRMWARE);

  WorkItem item1;
  WorkItem item2;
  WorkItem item3;

  scheduler.dequeue(&item1);
  scheduler.dequeue(&item2);
  scheduler.dequeue(&item3);

  if (item1.engine != EngineType::COMPUTE) {
    std::cerr << "FAIL: first should be COMPUTE\n";
    return 1;
  }
  if (item2.engine != EngineType::COPY) {
    std::cerr << "FAIL: second should be COPY\n";
    return 1;
  }
  if (item3.engine != EngineType::FIRMWARE) {
    std::cerr << "FAIL: third should be FIRMWARE\n";
    return 1;
  }

  std::cout << "PASS: test_scheduler_fifo_order\n";
  return 0;
}

int test_scheduler_select_engine_compute() {
  GlobalScheduler scheduler;

  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_LAUNCH_KERNEL;

  EngineType type = scheduler.selectEngine(entry);
  if (type != EngineType::COMPUTE) {
    std::cerr << "FAIL: LAUNCH_KERNEL should route to COMPUTE\n";
    return 1;
  }

  std::cout << "PASS: test_scheduler_select_engine_compute\n";
  return 0;
}

int test_scheduler_select_engine_copy() {
  GlobalScheduler scheduler;

  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_MEMCPY;

  EngineType type = scheduler.selectEngine(entry);
  if (type != EngineType::COPY) {
    std::cerr << "FAIL: MEMCPY should route to COPY\n";
    return 1;
  }

  std::cout << "PASS: test_scheduler_select_engine_copy\n";
  return 0;
}

int test_scheduler_flush() {
  GlobalScheduler scheduler;

  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_LAUNCH_KERNEL;

  scheduler.enqueue(entry, EngineType::COMPUTE);
  scheduler.enqueue(entry, EngineType::COMPUTE);
  scheduler.enqueue(entry, EngineType::COMPUTE);

  if (scheduler.queueSize() != 3) {
    std::cerr << "FAIL: queue size should be 3 before flush\n";
    return 1;
  }

  scheduler.flush();

  if (scheduler.queueSize() != 0) {
    std::cerr << "FAIL: queue size should be 0 after flush\n";
    return 1;
  }

  std::cout << "PASS: test_scheduler_flush\n";
  return 0;
}

int test_scheduler_dispatch_callback() {
  GlobalScheduler scheduler;
  reset_counters();

  scheduler.setDispatchCallback([](const gpu_gpfifo_entry& e, EngineType eng) {
    g_dispatch_count.fetch_add(1);
    g_last_engine.store(eng);
    (void)e;
  });

  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_LAUNCH_KERNEL;

  scheduler.enqueue(entry, EngineType::COMPUTE);

  WorkItem item;
  if (!scheduler.dequeue(&item)) {
    std::cerr << "FAIL: dequeue should succeed\n";
    return 1;
  }

  if (g_dispatch_count.load() != 0) {
    std::cerr << "FAIL: dispatch callback should not be called by dequeue itself\n";
    return 1;
  }

  std::cout << "PASS: test_scheduler_dispatch_callback\n";
  return 0;
}

int test_scheduler_empty_dequeue() {
  GlobalScheduler scheduler;

  WorkItem item;
  if (scheduler.dequeue(&item)) {
    std::cerr << "FAIL: dequeue on empty queue should return false\n";
    return 1;
  }

  std::cout << "PASS: test_scheduler_empty_dequeue\n";
  return 0;
}

int main() {
  int result = 0;

  std::cout << "=== GlobalScheduler TDD Tests ===\n";

  result |= test_scheduler_initial_queue_empty();
  result |= test_scheduler_enqueue_dequeue();
  result |= test_scheduler_fifo_order();
  result |= test_scheduler_select_engine_compute();
  result |= test_scheduler_select_engine_copy();
  result |= test_scheduler_flush();
  result |= test_scheduler_dispatch_callback();
  result |= test_scheduler_empty_dequeue();

  if (result == 0) {
    std::cout << "\n=== ALL TESTS PASSED ===\n";
  } else {
    std::cout << "\n=== SOME TESTS FAILED ===\n";
  }

  return result;
}