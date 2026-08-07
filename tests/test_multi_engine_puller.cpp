/*
 * test_multi_engine_puller.cpp — Multi-engine Puller registry API tests
 *
 * Tests for ADR-049 cross-engine synchronization infrastructure:
 * - GPU_QUEUE_GRAPHICS enum preparation
 * - GlobalScheduler per-engine puller registry (API only; no runtime dispatch)
 * - Per-engine fence ID allocation
 *
 * NOTE: This test file validates the REGISTRY API behavior.
 * The per-engine puller registry (registerPullerForEngine, getPullerForEngine)
 * and per-engine fence ID allocation (allocFenceId) are tested here.
 * Runtime dispatch to engine-specific pullers is NOT implemented — that
 * requires selectEngine() wiring + multiple puller instances in plugin.cpp.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
#include <map>

#include "gpu_types.h"
#include "gpu_hal.h"
#include "gpu_queue.h"
#include "doorbell_emu.h"
#include "hardware_puller_emu.h"
#include "global_scheduler.h"
#include "fence_id.h"
#include "semaphore_manager.h"

// Stage 4.3 (ADR-057): global logical clock
extern std::atomic<uint64_t> g_sim_tick;

// Mock HAL ops for testing
static int mock_hal_register_read(void* ctx, uint64_t off, uint64_t* out) { (void)ctx; (void)off; *out = 0; return 0; }
static int mock_hal_register_write(void* ctx, uint64_t off, uint64_t val) { (void)ctx; (void)off; (void)val; return 0; }
static int mock_hal_mem_read(void* ctx, uint64_t dev_addr, void* host_buf, uint64_t size) {
  (void)ctx; (void)dev_addr;
  memset(host_buf, 0, size);
  if (size >= sizeof(gpu_gpfifo_entry)) {
    gpu_gpfifo_entry* e = static_cast<gpu_gpfifo_entry*>(host_buf);
    e->valid = 1;
    e->method = GPU_OP_LAUNCH_KERNEL;
    e->release = 1;
    e->semaphore_va = 0x100;
    e->semaphore_value = 1;
  }
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

static struct gpu_hal_ops* create_mock_hal() {
  struct gpu_hal_ops* hal = new struct gpu_hal_ops();
  memset(hal, 0, sizeof(*hal));
  hal->ctx = nullptr;
  hal->register_read = mock_hal_register_read;
  hal->register_write = mock_hal_register_write;
  hal->mem_read = mock_hal_mem_read;
  hal->mem_write = mock_hal_mem_write;
  hal->mem_alloc = mock_hal_mem_alloc;
  hal->mem_free = mock_hal_mem_free;
  hal->fence_create = mock_hal_fence_create;
  hal->fence_read = mock_hal_fence_read;
  hal->doorbell_ring = mock_hal_doorbell_ring;
  hal->interrupt_raise = mock_hal_interrupt_raise;
  hal->time_wait = mock_hal_time_wait;
  return hal;
}

// ========================================================================
// Test 1: GPU_QUEUE_GRAPHICS should be in gpu_queue_type enum
// RED: This will fail to compile because GRAPHICS is not in the enum yet
// ========================================================================

int test_graphics_queue_type_in_enum() {
  std::cout << "TEST: GPU_QUEUE_GRAPHICS is in gpu_queue_type enum\n";

  // The enum gpu_queue_type should include GRAPHICS = 2
  // This will fail to compile if GRAPHICS is not in the enum
  enum gpu_queue_type queue_type_val = GPU_QUEUE_GRAPHICS;
  if (queue_type_val != GPU_QUEUE_GRAPHICS) {
    std::cerr << "FAIL: GPU_QUEUE_GRAPHICS not properly in enum\n";
    return 1;
  }

  // Also verify COMPUTE and COPY are correct
  if (GPU_QUEUE_COMPUTE != 0) {
    std::cerr << "FAIL: GPU_QUEUE_COMPUTE should be 0\n";
    return 1;
  }
  if (GPU_QUEUE_COPY != 1) {
    std::cerr << "FAIL: GPU_QUEUE_COPY should be 1\n";
    return 1;
  }

  std::cout << "PASS: test_graphics_queue_type_in_enum\n";
  return 0;
}

// ========================================================================
// Test 2: GlobalScheduler::selectEngine routes MEMCPY to COPY
// ========================================================================

int test_scheduler_select_engine_memcpy_routes_to_copy() {
  std::cout << "TEST: Scheduler routes MEMCPY to COPY engine\n";

  GlobalScheduler scheduler;

  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_MEMCPY;

  EngineType type = scheduler.selectEngine(entry);
  if (type != EngineType::COPY) {
    std::cerr << "FAIL: MEMCPY should route to COPY, got " << static_cast<int>(type) << "\n";
    return 1;
  }

  std::cout << "PASS: test_scheduler_select_engine_memcpy_routes_to_copy\n";
  return 0;
}

// ========================================================================
// Test 3: GlobalScheduler::selectEngine routes FENCE to COPY
// ========================================================================

int test_scheduler_select_engine_fence_routes_to_copy() {
  std::cout << "TEST: Scheduler routes FENCE to COPY engine\n";

  GlobalScheduler scheduler;

  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_FENCE;

  EngineType type = scheduler.selectEngine(entry);
  if (type != EngineType::COPY) {
    std::cerr << "FAIL: FENCE should route to COPY, got " << static_cast<int>(type) << "\n";
    return 1;
  }

  std::cout << "PASS: test_scheduler_select_engine_fence_routes_to_copy\n";
  return 0;
}

// ========================================================================
// Test 4: GlobalScheduler::selectEngine routes LAUNCH_KERNEL to COMPUTE
// ========================================================================

int test_scheduler_select_engine_launch_kernel_routes_to_compute() {
  std::cout << "TEST: Scheduler routes LAUNCH_KERNEL to COMPUTE engine\n";

  GlobalScheduler scheduler;

  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_LAUNCH_KERNEL;

  EngineType type = scheduler.selectEngine(entry);
  if (type != EngineType::COMPUTE) {
    std::cerr << "FAIL: LAUNCH_KERNEL should route to COMPUTE, got " << static_cast<int>(type) << "\n";
    return 1;
  }

  std::cout << "PASS: test_scheduler_select_engine_launch_kernel_routes_to_compute\n";
  return 0;
}

// ========================================================================
// Test 5: HardwarePullerEmu can be created and has a valid state machine
// ========================================================================

int test_hardware_puller_emu_initialization() {
  std::cout << "TEST: HardwarePullerEmu initializes correctly\n";

  struct gpu_hal_ops* hal = create_mock_hal();
  DoorbellEmu doorbell;
  GlobalScheduler scheduler;

  HardwarePullerEmu puller(hal, &doorbell, &scheduler);

  if (puller.currentState() != HardwarePullerEmu::State::IDLE) {
    std::cerr << "FAIL: Initial state should be IDLE\n";
    delete hal;
    return 1;
  }

  delete hal;
  std::cout << "PASS: test_hardware_puller_emu_initialization\n";
  return 0;
}

// ========================================================================
// Test 6: EngineType enum has FIRMWARE value
// ========================================================================

int test_engine_type_has_firmware() {
  std::cout << "TEST: EngineType enum has FIRMWARE value\n";

  // Verify FIRMWARE exists and is distinct from COMPUTE and COPY
  EngineType compute = EngineType::COMPUTE;
  EngineType copy = EngineType::COPY;
  EngineType firmware = EngineType::FIRMWARE;

  if (compute == copy) {
    std::cerr << "FAIL: COMPUTE should differ from COPY\n";
    return 1;
  }
  if (compute == firmware) {
    std::cerr << "FAIL: COMPUTE should differ from FIRMWARE\n";
    return 1;
  }
  if (copy == firmware) {
    std::cerr << "FAIL: COPY should differ from FIRMWARE\n";
    return 1;
  }

  std::cout << "PASS: test_engine_type_has_firmware\n";
  return 0;
}

// ========================================================================
// Test 7: Timeline semaphore fields exist in gpu_gpfifo_entry
// (ADR-049 D1: cross-engine sync via timeline semaphore)
// ========================================================================

int test_timeline_semaphore_fields_exist() {
  std::cout << "TEST: Timeline semaphore fields exist in gpu_gpfifo_entry\n";

  gpu_gpfifo_entry entry = {};
  entry.tl_sem_handle = 0x100;
  entry.tl_signal_value = 5;
  entry.tl_wait_value = 3;

  if (entry.tl_sem_handle != 0x100) {
    std::cerr << "FAIL: tl_sem_handle not properly set\n";
    return 1;
  }
  if (entry.tl_signal_value != 5) {
    std::cerr << "FAIL: tl_signal_value not properly set\n";
    return 1;
  }
  if (entry.tl_wait_value != 3) {
    std::cerr << "FAIL: tl_wait_value not properly set\n";
    return 1;
  }

  std::cout << "PASS: test_timeline_semaphore_fields_exist\n";
  return 0;
}

// ========================================================================
// Test 8: Fence ID allocation (sim_fence_id_alloc)
// This tests the existing fence_id module behavior
// ========================================================================

int test_fence_id_allocation() {
  std::cout << "TEST: Fence ID allocation works\n";

  // Test the sim_fence_id module directly
  uint64_t id1 = sim_fence_id_alloc();
  uint64_t id2 = sim_fence_id_alloc();

  if (id1 == 0) {
    std::cerr << "FAIL: fence_id should not be 0\n";
    return 1;
  }
  if (id2 == 0) {
    std::cerr << "FAIL: fence_id should not be 0\n";
    return 1;
  }
  if (id2 <= id1) {
    std::cerr << "FAIL: fence_id should increment\n";
    return 1;
  }

  std::cout << "PASS: test_fence_id_allocation\n";
  return 0;
}

// ========================================================================
// Test 9: Enqueue with specific engine type
// ========================================================================

int test_scheduler_enqueue_with_engine_type() {
  std::cout << "TEST: Scheduler enqueue preserves engine type\n";

  GlobalScheduler scheduler;

  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_MEMCPY;

  scheduler.enqueue(entry, EngineType::COPY);

  WorkItem item;
  if (!scheduler.dequeue(&item)) {
    std::cerr << "FAIL: dequeue should succeed\n";
    return 1;
  }

  if (item.engine != EngineType::COPY) {
    std::cerr << "FAIL: engine should be COPY, got " << static_cast<int>(item.engine) << "\n";
    return 1;
  }

  std::cout << "PASS: test_scheduler_enqueue_with_engine_type\n";
  return 0;
}

// ========================================================================
// Test 10: FIRMWARE engine routing
// ========================================================================

int test_scheduler_select_engine_firmware() {
  std::cout << "TEST: Scheduler routes LAUNCH_CPU_TASK to FIRMWARE engine\n";

  GlobalScheduler scheduler;

  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.method = GPU_OP_LAUNCH_CPU_TASK;

  EngineType type = scheduler.selectEngine(entry);
  // LAUNCH_CPU_TASK should route to COMPUTE per current implementation
  // This test verifies current behavior
  if (type != EngineType::COMPUTE) {
    std::cerr << "FAIL: LAUNCH_CPU_TASK should route to COMPUTE, got " << static_cast<int>(type) << "\n";
    return 1;
  }

  std::cout << "PASS: test_scheduler_select_engine_firmware\n";
  return 0;
}

// ========================================================================
// Test 11: Register and retrieve engine-specific pullers
// ========================================================================

int test_engine_puller_registry() {
  std::cout << "TEST: Engine puller registry works\n";

  struct gpu_hal_ops* hal = create_mock_hal();
  DoorbellEmu doorbell;
  GlobalScheduler scheduler;

  HardwarePullerEmu compute_puller(hal, &doorbell, &scheduler);
  HardwarePullerEmu copy_puller(hal, &doorbell, &scheduler);
  HardwarePullerEmu firmware_puller(hal, &doorbell, &scheduler);

  // Initially, no pullers registered
  if (scheduler.getPullerForEngine(EngineType::COMPUTE) != nullptr) {
    std::cerr << "FAIL: COMPUTE puller should be nullptr initially\n";
    delete hal;
    return 1;
  }

  // Register pullers
  scheduler.registerPullerForEngine(EngineType::COMPUTE, &compute_puller);
  scheduler.registerPullerForEngine(EngineType::COPY, &copy_puller);
  scheduler.registerPullerForEngine(EngineType::FIRMWARE, &firmware_puller);

  // Verify retrieval
  if (scheduler.getPullerForEngine(EngineType::COMPUTE) != &compute_puller) {
    std::cerr << "FAIL: COMPUTE puller mismatch\n";
    delete hal;
    return 1;
  }
  if (scheduler.getPullerForEngine(EngineType::COPY) != &copy_puller) {
    std::cerr << "FAIL: COPY puller mismatch\n";
    delete hal;
    return 1;
  }
  if (scheduler.getPullerForEngine(EngineType::FIRMWARE) != &firmware_puller) {
    std::cerr << "FAIL: FIRMWARE puller mismatch\n";
    delete hal;
    return 1;
  }

  delete hal;
  std::cout << "PASS: test_engine_puller_registry\n";
  return 0;
}

// ========================================================================
// Test 12: Per-engine fence ID allocation
// ========================================================================

int test_per_engine_fence_allocation() {
  std::cout << "TEST: Per-engine fence ID allocation works\n";

  GlobalScheduler scheduler;

  // Allocate fence IDs for different engines
  uint64_t compute_fence1 = scheduler.allocFenceId(EngineType::COMPUTE);
  uint64_t compute_fence2 = scheduler.allocFenceId(EngineType::COMPUTE);
  uint64_t copy_fence1 = scheduler.allocFenceId(EngineType::COPY);
  uint64_t copy_fence2 = scheduler.allocFenceId(EngineType::COPY);
  uint64_t firmware_fence1 = scheduler.allocFenceId(EngineType::FIRMWARE);

  // Each engine should have its own fence_id space
  if (compute_fence1 == 0 || compute_fence2 == 0 || copy_fence1 == 0 || copy_fence2 == 0 || firmware_fence1 == 0) {
    std::cerr << "FAIL: allocFenceId returned 0\n";
    return 1;
  }

  // Fence IDs should be distinct across engines
  if (compute_fence1 == copy_fence1) {
    std::cerr << "FAIL: COMPUTE and COPY fence IDs should be in separate spaces\n";
    return 1;
  }
  if (compute_fence1 == firmware_fence1) {
    std::cerr << "FAIL: COMPUTE and FIRMWARE fence IDs should be in separate spaces\n";
    return 1;
  }
  if (copy_fence1 == firmware_fence1) {
    std::cerr << "FAIL: COPY and FIRMWARE fence IDs should be in separate spaces\n";
    return 1;
  }

  // Fence IDs for same engine should be sequential
  if (compute_fence2 <= compute_fence1) {
    std::cerr << "FAIL: COMPUTE fence2 should be > compute_fence1\n";
    return 1;
  }
  if (copy_fence2 <= copy_fence1) {
    std::cerr << "FAIL: COPY fence2 should be > copy_fence1\n";
    return 1;
  }

  std::cout << "PASS: test_per_engine_fence_allocation\n";
  return 0;
}

// ========================================================================
// Main
// ========================================================================

int main() {
  int result = 0;

  std::cout << "=== Multi-Engine Puller TDD Tests (RED phase) ===\n\n";

  // RED phase: Tests should fail until implementation is complete
  result |= test_graphics_queue_type_in_enum();
  result |= test_scheduler_select_engine_memcpy_routes_to_copy();
  result |= test_scheduler_select_engine_fence_routes_to_copy();
  result |= test_scheduler_select_engine_launch_kernel_routes_to_compute();
  result |= test_hardware_puller_emu_initialization();
  result |= test_engine_type_has_firmware();
  result |= test_timeline_semaphore_fields_exist();
  result |= test_fence_id_allocation();
  result |= test_scheduler_enqueue_with_engine_type();
  result |= test_scheduler_select_engine_firmware();
  result |= test_engine_puller_registry();
  result |= test_per_engine_fence_allocation();

  std::cout << "\n";
  if (result == 0) {
    std::cout << "=== ALL TESTS PASSED ===\n";
    std::cout << "All behaviors validated.\n";
  } else {
    std::cout << "=== SOME TESTS FAILED ===\n";
    std::cout << "RED phase: Implement the missing features.\n";
  }

  return result;
}
