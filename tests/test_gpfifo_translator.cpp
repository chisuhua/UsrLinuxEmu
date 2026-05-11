/*
 * test_gpfifo_translator.cpp — TDD: GpfifoToLaunchParamsTranslator
 * 功能: 测试 GPFIFO entry 到 LaunchParams 的翻译逻辑
 * 作者: Sisyphus
 * 日期: 2026-05-11
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <atomic>

#include "scheduler/translator/gpfifo_translator.h"

static std::atomic<int> g_launch_count(0);
static std::atomic<const char*> g_last_kernel(nullptr);
static std::atomic<uint32_t> g_last_grid_x(0), g_last_grid_y(0), g_last_grid_z(0);
static std::atomic<uint32_t> g_last_block_x(0), g_last_block_y(0), g_last_block_z(0);

static void reset_counters() {
  g_launch_count.store(0);
  g_last_kernel.store(nullptr);
  g_last_grid_x.store(0);
  g_last_grid_y.store(0);
  g_last_grid_z.store(0);
  g_last_block_x.store(0);
  g_last_block_y.store(0);
  g_last_block_z.store(0);
}

int test_translator_basic() {
  ::usr_linux_emu::GpfifoToLaunchParamsTranslator translator;

  translator.setLaunchCallback([](const char* kernel_name,
                                  uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                                  uint32_t block_x, uint32_t block_y, uint32_t block_z,
                                  uint32_t shared_mem) {
    g_launch_count.fetch_add(1);
    g_last_kernel.store(kernel_name);
    g_last_grid_x.store(grid_x);
    g_last_grid_y.store(grid_y);
    g_last_grid_z.store(grid_z);
    g_last_block_x.store(block_x);
    g_last_block_y.store(block_y);
    g_last_block_z.store(block_z);
  });

  translator.registerKernel(0, "matmul_kernel");

  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.payload[0] = 0;  // kernel_idx = 0
  entry.payload[1] = (64 | (1 << 16) | (0 << 24));  // grid: (64, 1, 0)
  entry.payload[2] = (256 | (1 << 8) | (1 << 16));   // block: (256, 1, 1)

  bool result = translator.translate(entry);

  if (!result) {
    std::cerr << "FAIL: translate should return true\n";
    return 1;
  }

  if (g_launch_count.load() != 1) {
    std::cerr << "FAIL: launch callback should be called once\n";
    return 1;
  }

  std::cout << "PASS: test_translator_basic\n";
  return 0;
}

int test_translator_unpack_grid() {
  ::usr_linux_emu::GpfifoToLaunchParamsTranslator translator;

  translator.setLaunchCallback([](const char* kernel_name,
                                  uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                                  uint32_t, uint32_t, uint32_t,
                                  uint32_t) {
    g_last_grid_x.store(grid_x);
    g_last_grid_y.store(grid_y);
    g_last_grid_z.store(grid_z);
  });

  translator.registerKernel(1, "test_kernel");

  // Test grid unpacking: grid_x=128, grid_y=8, grid_z=3
  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.payload[0] = 1;
  entry.payload[1] = (128 | (8 << 16) | (3 << 24));
  entry.payload[2] = (256 | (1 << 8) | (1 << 16));

  translator.translate(entry);

  if (g_last_grid_x.load() != 128) {
    std::cerr << "FAIL: grid_x should be 128, got " << g_last_grid_x.load() << "\n";
    return 1;
  }
  if (g_last_grid_y.load() != 8) {
    std::cerr << "FAIL: grid_y should be 8, got " << g_last_grid_y.load() << "\n";
    return 1;
  }
  if (g_last_grid_z.load() != 3) {
    std::cerr << "FAIL: grid_z should be 3, got " << g_last_grid_z.load() << "\n";
    return 1;
  }

  std::cout << "PASS: test_translator_unpack_grid\n";
  return 0;
}

int test_translator_unpack_block() {
  ::usr_linux_emu::GpfifoToLaunchParamsTranslator translator;

  translator.setLaunchCallback([](const char*,
                                  uint32_t, uint32_t, uint32_t,
                                  uint32_t block_x, uint32_t block_y, uint32_t block_z,
                                  uint32_t) {
    g_last_block_x.store(block_x);
    g_last_block_y.store(block_y);
    g_last_block_z.store(block_z);
  });

  translator.registerKernel(2, "test_kernel");

  // Test block unpacking: block_x=64, block_y=8, block_z=4
  // block_dim packs as: block_x | (block_y << 8) | (block_z << 16) in LOW 32 bits
  // But unpackDimX uses 16-bit mask, so block_x in bits 0-15, block_y in 16-23, block_z in 24-31
  // Value: 64 + (8<<16) + (4<<24) = 64 + 524288 + 67108864 = 67633184
  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.payload[0] = 2;
  entry.payload[1] = (1 | (1 << 16) | (0 << 24));
  entry.payload[2] = 64 + (8 << 16) + (4 << 24);  // = 67633184

  translator.translate(entry);

  // Verify unpackDimX/Y/Z correctness with known values
  if (g_last_block_x.load() != 64) {
    std::cerr << "FAIL: block_x should be 64, got " << g_last_block_x.load() << "\n";
    return 1;
  }
  if (g_last_block_y.load() != 8) {
    std::cerr << "FAIL: block_y should be 8, got " << g_last_block_y.load() << "\n";
    return 1;
  }
  if (g_last_block_z.load() != 4) {
    std::cerr << "FAIL: block_z should be 4, got " << g_last_block_z.load() << "\n";
    return 1;
  }

  std::cout << "PASS: test_translator_unpack_block\n";
  return 0;
}

int test_translator_invalid_entry() {
  ::usr_linux_emu::GpfifoToLaunchParamsTranslator translator;

  translator.setLaunchCallback([](const char*, uint32_t, uint32_t, uint32_t,
                                  uint32_t, uint32_t, uint32_t, uint32_t) {
    g_launch_count.fetch_add(1);
  });

  gpu_gpfifo_entry entry = {};
  entry.valid = 0;  // Invalid entry

  bool result = translator.translate(entry);

  if (result) {
    std::cerr << "FAIL: translate should return false for invalid entry\n";
    return 1;
  }

  if (g_launch_count.load() != 0) {
    std::cerr << "FAIL: callback should not be called for invalid entry\n";
    return 1;
  }

  std::cout << "PASS: test_translator_invalid_entry\n";
  return 0;
}

int test_translator_unknown_kernel() {
  ::usr_linux_emu::GpfifoToLaunchParamsTranslator translator;

  const char* captured_name = nullptr;
  translator.setLaunchCallback([&captured_name](const char* kernel_name,
                                               uint32_t, uint32_t, uint32_t,
                                               uint32_t, uint32_t, uint32_t,
                                               uint32_t) {
    captured_name = kernel_name;
  });

  // No kernels registered, use kernel_idx = 99
  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.payload[0] = 99;  // Unknown kernel
  entry.payload[1] = (1 | (1 << 16) | (0 << 24));
  entry.payload[2] = (256 | (1 << 8) | (1 << 16));

  translator.translate(entry);

  if (captured_name == nullptr) {
    std::cerr << "FAIL: kernel name should be captured\n";
    return 1;
  }
  if (strcmp(captured_name, "unknown") != 0) {
    std::cerr << "FAIL: unknown kernel should resolve to 'unknown', got '" << captured_name << "'\n";
    return 1;
  }

  std::cout << "PASS: test_translator_unknown_kernel\n";
  return 0;
}

int test_translator_no_callback() {
  ::usr_linux_emu::GpfifoToLaunchParamsTranslator translator;
  // Don't set callback - should not crash

  gpu_gpfifo_entry entry = {};
  entry.valid = 1;
  entry.payload[0] = 0;
  entry.payload[1] = (1 | (1 << 16));
  entry.payload[2] = (256 | (1 << 8));

  bool result = translator.translate(entry);

  if (!result) {
    std::cerr << "FAIL: translate should succeed without callback\n";
    return 1;
  }

  std::cout << "PASS: test_translator_no_callback\n";
  return 0;
}

int main() {
  std::cout << "=== GpfifoToLaunchParamsTranslator TDD Tests ===\n";

  int result = 0;

  reset_counters();
  result |= test_translator_basic();

  reset_counters();
  result |= test_translator_unpack_grid();

  reset_counters();
  result |= test_translator_unpack_block();

  reset_counters();
  result |= test_translator_invalid_entry();

  reset_counters();
  result |= test_translator_unknown_kernel();

  reset_counters();
  result |= test_translator_no_callback();

  if (result == 0) {
    std::cout << "\n=== ALL TESTS PASSED ===\n";
  } else {
    std::cout << "\n=== SOME TESTS FAILED ===\n";
  }

  return result;
}