/*
 * test_aql_standalone.cpp - AQL packet parser + format dispatch tests
 *
 * Stage 4.5 Phase 6 (ADR-052): AQL format support in
 * GpfifoToLaunchParamsTranslator.
 *
 * Tests:
 * 1. AQL format packet launches via parseAqlPacket
 * 2. PM4 format returns false (stub)
 * 3. UsrNative format delegates to existing path (unchanged behavior)
 */

#include "catch_amalgamated.hpp"

#include "scheduler/translator/gpfifo_translator.h"

#include <cstring>

using usr_linux_emu::GpfifoToLaunchParamsTranslator;

TEST_CASE("AQL: parseAqlPacket maps kernel_object->kernel_addr", "[aql]") {
  GpfifoToLaunchParamsTranslator translator;

  // Track callback invocations
  bool callback_called = false;
  const char* captured_name = nullptr;
  uint32_t captured_gx = 0, captured_gy = 0, captured_gz = 0;
  uint32_t captured_bx = 0, captured_by = 0, captured_bz = 0;
  uint32_t captured_smem = 0;

  translator.setLaunchCallback([&](const char* kernel_name,
                                   uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
                                   uint32_t block_x, uint32_t block_y, uint32_t block_z,
                                   uint32_t shared_mem) {
    callback_called = true;
    captured_name = kernel_name;
    captured_gx = grid_x;
    captured_gy = grid_y;
    captured_gz = grid_z;
    captured_bx = block_x;
    captured_by = block_y;
    captured_bz = block_z;
    captured_smem = shared_mem;
  });

  // Register a kernel so the translator can resolve the index
  translator.registerKernel(42, "test_kernel");

  // Build an AQL-format GPFIFO entry
  gpu_gpfifo_entry entry{};
  entry.valid = 1;
  entry.format = FORMAT_AQL;
  // payload[0] = kernel_object (kernel index)
  entry.payload[0] = 42;
  // payload[1] = grid dims (packed: x | (y << 16) | (z << 24))
  entry.payload[1] = 4 | (2 << 16) | (1 << 24);
  // payload[2] = block dims (packed: x[15:0] | y[23:16] | z[31:24])
  entry.payload[2] = 64 | (1 << 16) | (1 << 24);
  // payload[3] = shared_mem
  entry.payload[3] = 256;

  bool result = translator.translateForTest(entry);

  REQUIRE(result == true);
  REQUIRE(callback_called == true);
  REQUIRE(captured_name != nullptr);
  REQUIRE(std::strcmp(captured_name, "test_kernel") == 0);
  REQUIRE(captured_gx == 4);
  REQUIRE(captured_gy == 2);
  REQUIRE(captured_gz == 1);
  REQUIRE(captured_bx == 64);
  REQUIRE(captured_by == 1);
  REQUIRE(captured_bz == 1);
  REQUIRE(captured_smem == 256);
}

TEST_CASE("AQL: PM4 format returns false", "[aql]") {
  GpfifoToLaunchParamsTranslator translator;

  translator.setLaunchCallback([](const char*, uint32_t, uint32_t, uint32_t,
                                   uint32_t, uint32_t, uint32_t, uint32_t) {
    FAIL("Launch callback should not be called for PM4 format");
  });

  gpu_gpfifo_entry entry{};
  entry.valid = 1;
  entry.format = FORMAT_PM4;

  bool result = translator.translateForTest(entry);

  REQUIRE(result == false);
}

TEST_CASE("AQL: UsrNative format delegates to existing path", "[aql]") {
  GpfifoToLaunchParamsTranslator translator;

  bool callback_called = false;
  const char* captured_name = nullptr;
  uint32_t captured_gx = 0;

  translator.setLaunchCallback([&](const char* kernel_name,
                                   uint32_t grid_x, uint32_t, uint32_t,
                                   uint32_t, uint32_t, uint32_t, uint32_t) {
    callback_called = true;
    captured_name = kernel_name;
    captured_gx = grid_x;
  });

  translator.registerKernel(1, "native_kernel");

  // UsrNative format (default = 0)
  gpu_gpfifo_entry entry{};
  entry.valid = 1;
  entry.format = FORMAT_USR_NATIVE;
  entry.payload[0] = 1;  // kernel index
  entry.payload[1] = 8;  // grid_x = 8
  entry.payload[2] = 32; // block_x = 32

  bool result = translator.translateForTest(entry);

  REQUIRE(result == true);
  REQUIRE(callback_called == true);
  REQUIRE(captured_name != nullptr);
  REQUIRE(std::strcmp(captured_name, "native_kernel") == 0);
  REQUIRE(captured_gx == 8);
}

TEST_CASE("AQL: completion_signal triggers timeline semaphore signal on completion", "[aql]") {
  GpfifoToLaunchParamsTranslator translator;

  translator.setLaunchCallback([](const char*, uint32_t, uint32_t, uint32_t,
                                  uint32_t, uint32_t, uint32_t, uint32_t) {
    // Launch callback may fire; we only care about the signal hook below.
  });

  gpu_gpfifo_entry entry{};
  entry.format = FORMAT_AQL;
  entry.valid = 1;
  entry.payload[0] = 0x1000;  /* kernel_object */
  entry.payload[1] = 0x2000;  /* kernarg_address */
  entry.payload[4] = 42;      /* completion_signal = sem handle 42 */

  uint64_t captured_handle = 0;
  uint64_t captured_value = 0;
  translator.setCompletionSignalHookForTest([&](uint64_t h, uint64_t v) {
    captured_handle = h;
    captured_value = v;
  });

  REQUIRE(translator.translateForTest(entry) == true);
  REQUIRE(captured_handle == 42);
  REQUIRE(captured_value == 1);  /* Default signal value = batch completion count */
}

TEST_CASE("AQL: completion_signal=0 is a no-op (no signal fired)", "[aql]") {
  GpfifoToLaunchParamsTranslator translator;

  translator.setLaunchCallback([](const char*, uint32_t, uint32_t, uint32_t,
                                  uint32_t, uint32_t, uint32_t, uint32_t) {
  });

  gpu_gpfifo_entry entry{};
  entry.format = FORMAT_AQL;
  entry.valid = 1;
  entry.payload[0] = 0x1000;
  entry.payload[1] = 0x2000;
  entry.payload[4] = 0;  /* completion_signal = 0 -> no-op */

  bool hook_fired = false;
  translator.setCompletionSignalHookForTest([&](uint64_t, uint64_t) {
    hook_fired = true;
  });

  REQUIRE(translator.translateForTest(entry) == true);
  REQUIRE(hook_fired == false);
}
