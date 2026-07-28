// test_indirect_buffer_standalone.cpp
// Indirect Buffer JUMP tests for GPU CP Phase 5.5 (Stage 4.4 Task 13-14)
//
// TDD RED phase: Tests for GPU_OP_IB_JUMP (0x109).
// Exercises HardwarePullerEmu IB JUMP behavior synchronously via
// processIbJump(), without starting the Puller thread loop.
//
// IB JUMP semantics:
//   payload[0] = target_gpu_va  (fetch address to jump to)
//   payload[1] = continue_flag  (1 = resume at saved PC after target batch)
//   payload[2] = target_size     (number of entries at target address)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <atomic>
#include <vector>
#include <unordered_map>

#include "gpu_types.h"
#include "gpu_hal.h"
#include "doorbell_emu.h"
#include "hardware_puller_emu.h"

// ========== Mock HAL ==========
//
// The mock mem_read serves entries from an in-memory store so we can
// verify fetch-address switching and validate jump targets.

static std::unordered_map<u64, gpu_gpfifo_entry> g_entry_store;
static std::unordered_map<u64, bool> g_mapped_addrs;

static int mock_hal_register_read(void* ctx, u64 off, u64* out) {
  (void)ctx; (void)off; *out = 0; return 0;
}
static int mock_hal_register_write(void* ctx, u64 off, u64 val) {
  (void)ctx; (void)off; (void)val; return 0;
}
static int mock_hal_mem_read(void* ctx, u64 dev_addr, void* host_buf, u64 size) {
  (void)ctx;
  if (size == sizeof(gpu_gpfifo_entry)) {
    auto it = g_entry_store.find(dev_addr);
    if (it != g_entry_store.end()) {
      std::memcpy(host_buf, &it->second, sizeof(gpu_gpfifo_entry));
      return 0;
    }
    std::memset(host_buf, 0, size);
    return 0;
  }
  std::memset(host_buf, 0, size);
  return 0;
}
static int mock_hal_mem_write(void* ctx, u64 dev_addr, const void* host_buf, u64 size) {
  (void)ctx; (void)dev_addr; (void)host_buf; (void)size; return 0;
}
static int mock_hal_mem_alloc(void* ctx, u64 size, u64* out) {
  (void)ctx; (void)size; *out = 0; return 0;
}
static int mock_hal_mem_free(void* ctx, u64 addr) { (void)ctx; (void)addr; return 0; }
static int mock_hal_fence_create(void* ctx, u64* out) { (void)ctx; *out = 1; return 0; }
static int mock_hal_fence_read(void* ctx, u64 id, u64* out) { (void)ctx; (void)id; *out = 1; return 0; }
static void mock_hal_doorbell_ring(void* ctx, u32 qid) { (void)ctx; (void)qid; }
static void mock_hal_interrupt_raise(void* ctx, u32 vec) { (void)ctx; (void)vec; }
static void mock_hal_time_wait(void* ctx, u64 us) { (void)ctx; (void)us; }

static int mock_hal_mem_read_with_validation(void* ctx, u64 dev_addr, void* host_buf, u64 size) {
  (void)ctx;
  if (g_mapped_addrs.count(dev_addr) == 0) {
    return -EFAULT;
  }
  return mock_hal_mem_read(ctx, dev_addr, host_buf, size);
}

static struct gpu_hal_ops make_mock_hal() {
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

static struct gpu_hal_ops make_validating_hal() {
  struct gpu_hal_ops hal = make_mock_hal();
  hal.mem_read = mock_hal_mem_read_with_validation;
  return hal;
}

static gpu_gpfifo_entry make_ib_jump_entry(u64 target_va, u64 continue_flag, u64 target_size) {
  gpu_gpfifo_entry e{};
  e.valid = 1;
  e.method = GPU_OP_IB_JUMP;
  e.payload[0] = target_va;
  e.payload[1] = continue_flag;
  e.payload[2] = target_size;
  return e;
}

static gpu_gpfifo_entry make_launch_entry() {
  gpu_gpfifo_entry e{};
  e.valid = 1;
  e.method = GPU_OP_LAUNCH_KERNEL;
  return e;
}

static void reset_test_env() {
  g_entry_store.clear();
  g_mapped_addrs.clear();
}

// ========== Test 1: Single JUMP switches fetch address ==========

int test_single_jump() {
  reset_test_env();
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  u64 original_addr = 0x1000;
  u64 jump_target = 0x2000;

  g_mapped_addrs[original_addr] = true;
  g_mapped_addrs[jump_target] = true;

  gpu_gpfifo_entry jump_entry = make_ib_jump_entry(jump_target, 0, 1);
  g_entry_store[original_addr] = jump_entry;

  gpu_gpfifo_entry target_entry = make_launch_entry();
  g_entry_store[jump_target] = target_entry;

  puller.submitBatch(original_addr, 1, 0);

  int ret = puller.processIbJump(jump_entry);

  if (ret != 0) {
    std::cerr << "FAIL: processIbJump should return 0, got " << ret << "\n";
    return 1;
  }
  if (!puller.isInJump()) {
    std::cerr << "FAIL: puller should be in jump state after IB_JUMP\n";
    return 1;
  }
  if (puller.jumpDepth() != 1) {
    std::cerr << "FAIL: jump depth should be 1, got " << puller.jumpDepth() << "\n";
    return 1;
  }
  if (puller.jumpTargetAddr() != jump_target) {
    std::cerr << "FAIL: jump target addr should be 0x" << std::hex << jump_target
              << ", got 0x" << puller.jumpTargetAddr() << std::dec << "\n";
    return 1;
  }

  std::cout << "PASS: test_single_jump\n";
  return 0;
}

// ========== Test 2: Chained JUMP with continue_flag ==========

int test_chained_jump() {
  reset_test_env();
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  u64 original_addr = 0x1000;
  u64 jump_target = 0x2000;

  g_mapped_addrs[original_addr] = true;
  g_mapped_addrs[jump_target] = true;

  gpu_gpfifo_entry jump_entry = make_ib_jump_entry(jump_target, 1, 1);
  g_entry_store[original_addr] = jump_entry;

  gpu_gpfifo_entry target_entry = make_launch_entry();
  g_entry_store[jump_target] = target_entry;

  puller.submitBatch(original_addr, 1, 0);

  int ret = puller.processIbJump(jump_entry);

  if (ret != 0) {
    std::cerr << "FAIL: processIbJump with continue should return 0, got " << ret << "\n";
    return 1;
  }
  if (!puller.isInJump()) {
    std::cerr << "FAIL: puller should be in jump state\n";
    return 1;
  }
  if (!puller.jumpWillContinue()) {
    std::cerr << "FAIL: jump should have continue_flag set\n";
    return 1;
  }

  u64 saved_pc = puller.savedFetchPc();
  if (saved_pc != original_addr) {
    std::cerr << "FAIL: saved PC should be 0x" << std::hex << original_addr
              << ", got 0x" << saved_pc << std::dec << "\n";
    return 1;
  }

  int ret2 = puller.completeIbJump();
  if (ret2 != 0) {
    std::cerr << "FAIL: completeIbJump should return 0, got " << ret2 << "\n";
    return 1;
  }
  if (puller.isInJump()) {
    std::cerr << "FAIL: puller should not be in jump state after completeIbJump\n";
    return 1;
  }
  if (puller.jumpDepth() != 0) {
    std::cerr << "FAIL: jump depth should be 0 after completion, got "
              << puller.jumpDepth() << "\n";
    return 1;
  }

  std::cout << "PASS: test_chained_jump\n";
  return 0;
}

// ========== Test 3: Illegal JUMP to unmapped address ==========

int test_illegal_jump() {
  reset_test_env();
  struct gpu_hal_ops hal = make_validating_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  u64 original_addr = 0x1000;
  u64 unmapped_target = 0xDEADBEEF;
  g_mapped_addrs[original_addr] = true;

  gpu_gpfifo_entry jump_entry = make_ib_jump_entry(unmapped_target, 0, 1);
  g_entry_store[original_addr] = jump_entry;

  puller.submitBatch(original_addr, 1, 0);

  int ret = puller.processIbJump(jump_entry);

  if (ret != -EFAULT) {
    std::cerr << "FAIL: processIbJump to unmapped addr should return -EFAULT, got "
              << ret << "\n";
    return 1;
  }
  if (puller.isInJump()) {
    std::cerr << "FAIL: puller should not be in jump state after failed jump\n";
    return 1;
  }
  if (puller.jumpDepth() != 0) {
    std::cerr << "FAIL: jump depth should be 0 after failed jump, got "
              << puller.jumpDepth() << "\n";
    return 1;
  }

  std::cout << "PASS: test_illegal_jump\n";
  return 0;
}

// ========== Test 4: Nest overflow (>MAX_IB_NEST=4) ==========

int test_nest_overflow() {
  reset_test_env();
  struct gpu_hal_ops hal = make_mock_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  u64 base_addr = 0x10000;
  g_mapped_addrs[base_addr] = true;
  for (int i = 0; i < MAX_IB_NEST; i++) {
    u64 target = base_addr + (i + 1) * 0x1000;
    g_mapped_addrs[target] = true;
    gpu_gpfifo_entry jump_entry = make_ib_jump_entry(target, 1, 1);
    g_entry_store[base_addr + i * 0x1000] = jump_entry;
  }

  puller.submitBatch(base_addr, 1, 0);

  for (int i = 0; i < MAX_IB_NEST; i++) {
    u64 target = base_addr + (i + 1) * 0x1000;
    gpu_gpfifo_entry jump_entry = make_ib_jump_entry(target, 1, 1);

    int ret = puller.processIbJump(jump_entry);
    if (ret != 0) {
      std::cerr << "FAIL: jump " << i << " should succeed, got " << ret << "\n";
      return 1;
    }
  }

  if (puller.jumpDepth() != MAX_IB_NEST) {
    std::cerr << "FAIL: jump depth should be " << MAX_IB_NEST
              << ", got " << puller.jumpDepth() << "\n";
    return 1;
  }

  u64 overflow_target = 0xFFFF0000;
  g_mapped_addrs[overflow_target] = true;
  gpu_gpfifo_entry overflow_entry = make_ib_jump_entry(overflow_target, 1, 1);

  int ret = puller.processIbJump(overflow_entry);
  if (ret != -E2BIG) {
    std::cerr << "FAIL: 5th nested jump should return -E2BIG, got " << ret << "\n";
    return 1;
  }
  if (puller.jumpDepth() != MAX_IB_NEST) {
    std::cerr << "FAIL: jump depth should remain " << MAX_IB_NEST
              << " after overflow, got " << puller.jumpDepth() << "\n";
    return 1;
  }

  std::cout << "PASS: test_nest_overflow\n";
  return 0;
}

// ========== Main ==========

int main() {
  int result = 0;

  std::cout << "=== Indirect Buffer JUMP TDD Tests ===\n";

  result |= test_single_jump();
  result |= test_chained_jump();
  result |= test_illegal_jump();
  result |= test_nest_overflow();

  if (result == 0) {
    std::cout << "\n=== ALL TESTS PASSED ===\n";
  } else {
    std::cout << "\n=== SOME TESTS FAILED ===\n";
  }

  return result;
}
