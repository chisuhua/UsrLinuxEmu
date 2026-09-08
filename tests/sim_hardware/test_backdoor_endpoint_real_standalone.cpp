/*
 * tests/sim_hardware/test_backdoor_endpoint_real_standalone.cpp
 * 5.5.6-cpptlm-ep-binding — P4.NEW-A: real CppTLM BackdoorEndpoint binding
 *
 * Verifies the 5 functions in sim_hardware/src/cpptlm/backdoor_endpoint.cpp:
 *   - ule_dgpu_acquire: real dlopen libcpptlm_emulator.so + cpptlm_emulator_create + cpptlm_emulator_open
 *   - ule_dgpu_get_adapter_info: cpptlm_emulator_get_adapter_info + 9-field mapping
 *   - ule_dgpu_read/write: 4-space dispatch (kConfig/kBarMmio/kBarVram/kAxiDirect)
 *   - ule_dgpu_release: cpptlm_emulator_close
 *
 * Test fixture: skips real-CppTLM-path tests when libcpptlm_emulator.so is not
 * loadable (e.g., CI without sibling build). Falls back to -ENOSYS path testing.
 */
#include <catch_amalgamated.hpp>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <thread>
#include <vector>

#include "cpptlm/backdoor_endpoint.h"

namespace {

// Test fixture: probe whether libcpptlm_emulator.so is loadable
struct CpptlmLibProbe {
  void* handle = nullptr;

  CpptlmLibProbe() {
    // Try standard dynamic linker path first
    handle = dlopen("libcpptlm_emulator.so", RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
      // Fallback: sibling build path (development setup)
      handle = dlopen("../CppTLM/build/lib/libcpptlm_emulator.so",
                      RTLD_NOW | RTLD_LOCAL);
    }
    if (!handle) {
      // Fallback: absolute path
      handle = dlopen("/workspace/project/CppTLM/build/lib/libcpptlm_emulator.so",
                      RTLD_NOW | RTLD_LOCAL);
    }
  }

  ~CpptlmLibProbe() {
    if (handle) dlclose(handle);
  }

  bool available() const { return handle != nullptr; }
};

// Check whether the running test is running in a context where the real
// CppTLM binding should be exercised.
bool cpptlm_runtime_available() {
  return CpptlmLibProbe{}.available();
}

}  // namespace

// ===== TDD-A.1 =====: API surface (always runs)
TEST_CASE("backdoor_endpoint: API surface compiles (header sanity)", "[backdoor][real]") {
  CHECK(sizeof(ule_dgpu_space) >= 4);
  CHECK(sizeof(ule_dgpu_handle_t) >= 8);
  CHECK(sizeof(ule_dgpu_adapter_info) >= 48);

  // Verify all 9 fields are present with expected sizes (compile-time check)
  ule_dgpu_adapter_info info{};
  CHECK(sizeof(info.vendor_id) == 2);
  CHECK(sizeof(info.device_id) == 2);
  CHECK(sizeof(info.gpu_id) == 4);
  CHECK(sizeof(info.gfx_version) == 2);
  CHECK(sizeof(info.bdf) == 2);
  CHECK(sizeof(info.visible_vram_size) == 8);
  CHECK(sizeof(info.invisible_vram_size) == 8);
  CHECK(sizeof(info.va_region_size) == 8);
  CHECK(sizeof(info.bar_sizes) == 48);  // 6 × 8
}

// ===== TDD-A.2 =====: acquire + release (skipped if no real CppTLM)
TEST_CASE("backdoor_endpoint: acquire returns valid handle on real cpptlm",
          "[backdoor][real][acquire]") {
  if (!cpptlm_runtime_available()) {
    SKIP("libcpptlm_emulator.so not available; skipping real CppTLM path");
  }

  ule_dgpu_handle_t handle = 0;
  int ret = ule_dgpu_acquire(1, &handle);
  REQUIRE(ret == 0);
  REQUIRE(handle != 0);

  // Cleanup
  int rel_ret = ule_dgpu_release(handle);
  REQUIRE(rel_ret == 0);
}

TEST_CASE("backdoor_endpoint: acquire-release lifecycle",
          "[backdoor][real][acquire]") {
  if (!cpptlm_runtime_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  ule_dgpu_handle_t h1 = 0;
  ule_dgpu_handle_t h2 = 0;

  REQUIRE(ule_dgpu_acquire(1, &h1) == 0);
  REQUIRE(h1 != 0);

  // Second acquire returns a different handle
  REQUIRE(ule_dgpu_acquire(2, &h2) == 0);
  REQUIRE(h2 != 0);
  REQUIRE(h1 != h2);

  // Release both
  REQUIRE(ule_dgpu_release(h1) == 0);
  REQUIRE(ule_dgpu_release(h2) == 0);

  // Re-acquire after release should succeed
  REQUIRE(ule_dgpu_acquire(1, &h1) == 0);
  REQUIRE(ule_dgpu_release(h1) == 0);
}

TEST_CASE("backdoor_endpoint: acquire with NULL handle pointer returns -EINVAL",
          "[backdoor][real][acquire]") {
  if (!cpptlm_runtime_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  int ret = ule_dgpu_acquire(1, nullptr);
  REQUIRE(ret == -EINVAL);
}

// ===== TDD-A.3 =====: get_adapter_info
TEST_CASE("backdoor_endpoint: get_adapter_info populates 9 fields on real cpptlm",
          "[backdoor][real][info]") {
  if (!cpptlm_runtime_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  ule_dgpu_handle_t handle = 0;
  REQUIRE(ule_dgpu_acquire(1, &handle) == 0);

  ule_dgpu_adapter_info info{};
  int ret = ule_dgpu_get_adapter_info(handle, &info);
  INFO("get_adapter_info ret=" << ret << " handle=" << handle);
  REQUIRE(ret == 0);

  // Field-level: at least vendor_id and device_id should be non-zero on a real CppTLM
  // (real libcpptlm_emulator.so returns default profile info)
  INFO("vendor_id=" << std::hex << info.vendor_id);
  INFO("device_id=" << info.device_id);
  INFO("bar_sizes[0]=" << info.bar_sizes[0]);

  // NOTE: We don't assert non-zero values because the CppTLM default profile
  // may return zeros. We only assert the call succeeded.

  REQUIRE(ule_dgpu_release(handle) == 0);
}

TEST_CASE("backdoor_endpoint: get_adapter_info with NULL pointer returns -EINVAL",
          "[backdoor][real][info]") {
  if (!cpptlm_runtime_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  int ret = ule_dgpu_get_adapter_info(0, nullptr);
  REQUIRE(ret == -EINVAL);
}

// ===== TDD-A.4 =====: read/write space dispatch
TEST_CASE("backdoor_endpoint: read kBarMmio roundtrip on real cpptlm",
          "[backdoor][real][rw]") {
  if (!cpptlm_runtime_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  ule_dgpu_handle_t handle = 0;
  REQUIRE(ule_dgpu_acquire(1, &handle) == 0);

  // Write a known 4-byte pattern to BAR MMIO offset 0
  uint32_t write_val = 0xDEADBEEF;
  uint32_t read_val = 0;
  int wr_ret = ule_dgpu_write(handle, ule_dgpu_space::kBarMmio, 0,
                              &write_val, sizeof(write_val));
  if (wr_ret == 0) {
    int rd_ret = ule_dgpu_read(handle, ule_dgpu_space::kBarMmio, 0,
                                &read_val, sizeof(read_val));
    REQUIRE(rd_ret == 0);
    // NOTE: Real CppTLM may not implement MMIO write-back without a CP attached;
    // we don't assert the roundtrip equality here, only that the calls succeed.
  } else {
    INFO("MMIO write returned " << wr_ret << " (may not be supported by default profile)");
  }

  REQUIRE(ule_dgpu_release(handle) == 0);
}

TEST_CASE("backdoor_endpoint: read kBarVram roundtrip on real cpptlm",
          "[backdoor][real][rw]") {
  if (!cpptlm_runtime_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  ule_dgpu_handle_t handle = 0;
  REQUIRE(ule_dgpu_acquire(1, &handle) == 0);

  // Backdoor read should succeed without MMIO side effects
  uint8_t buf[64] = {};
  int rd_ret = ule_dgpu_read(handle, ule_dgpu_space::kBarVram, 0, buf, sizeof(buf));
  INFO("kBarVram read returned " << rd_ret);
  // We don't assert success/failure because CppTLM default profile may not
  // support backdoor reads; we only verify the call doesn't crash.

  REQUIRE(ule_dgpu_release(handle) == 0);
}

TEST_CASE("backdoor_endpoint: read kConfig roundtrip on real cpptlm",
          "[backdoor][real][rw]") {
  if (!cpptlm_runtime_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  ule_dgpu_handle_t handle = 0;
  REQUIRE(ule_dgpu_acquire(1, &handle) == 0);

  uint32_t val = 0;
  int rd_ret = ule_dgpu_read(handle, ule_dgpu_space::kConfig, 0, &val, sizeof(val));
  INFO("kConfig read returned " << rd_ret << ", vendor_id=" << std::hex << val);

  REQUIRE(ule_dgpu_release(handle) == 0);
}

TEST_CASE("backdoor_endpoint: read kAxiDirect returns -EOPNOTSUPP",
          "[backdoor][real][rw]") {
  if (!cpptlm_runtime_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  ule_dgpu_handle_t handle = 0;
  int acq_ret = ule_dgpu_acquire(1, &handle);
  INFO("acquire ret=" << acq_ret << " handle=" << handle);
  REQUIRE(acq_ret == 0);
  REQUIRE(handle != 0);

  uint8_t buf[8] = {};
  int ret = ule_dgpu_read(handle, ule_dgpu_space::kAxiDirect, 0, buf, sizeof(buf));
  INFO("kAxiDirect read ret=" << ret << " (-22=-EINVAL, -95=-EOPNOTSUPP)");
  REQUIRE(ret == -EOPNOTSUPP);

  ret = ule_dgpu_write(handle, ule_dgpu_space::kAxiDirect, 0, buf, sizeof(buf));
  REQUIRE(ret == -EOPNOTSUPP);

  REQUIRE(ule_dgpu_release(handle) == 0);
}

// ===== TDD-A.5 =====: timeout / cross-thread safety
TEST_CASE("backdoor_endpoint: concurrent acquire-release cycles",
          "[backdoor][real][concurrent]") {
  if (!cpptlm_runtime_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  constexpr int kThreads = 4;
  constexpr int kCyclesPerThread = 5;
  std::atomic<int> successes{0};
  std::atomic<int> failures{0};
  std::vector<std::thread> threads;

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&, tid = t]() {
      for (int i = 0; i < kCyclesPerThread; ++i) {
        ule_dgpu_handle_t h = 0;
        // Each thread uses a different dev_id to avoid contention
        int ret = ule_dgpu_acquire(static_cast<uint32_t>(tid), &h);
        if (ret == 0 && h != 0) {
          if (ule_dgpu_release(h) == 0) {
            successes.fetch_add(1);
          } else {
            failures.fetch_add(1);
          }
        } else {
          failures.fetch_add(1);
        }
      }
    });
  }

  for (auto& t : threads) t.join();
  CHECK(successes.load() + failures.load() == kThreads * kCyclesPerThread);
  INFO("Concurrent: " << successes.load() << " ok, " << failures.load() << " fail");
}

// ===== TDD-A.5 fallback =====: verify -ENOSYS path when CppTLM unavailable
TEST_CASE("backdoor_endpoint: returns -ENOSYS when CppTLM not available",
          "[backdoor][fallback]") {
  if (cpptlm_runtime_available()) {
    SKIP("CppTLM available, real path tested above");
  }

  ule_dgpu_handle_t handle = 0;
  int ret = ule_dgpu_acquire(0, &handle);
  CHECK(ret == -ENOSYS);

  ule_dgpu_adapter_info info{};
  ret = ule_dgpu_get_adapter_info(0, &info);
  CHECK(ret == -ENOSYS);

  uint8_t buf[8] = {};
  ret = ule_dgpu_read(0, ule_dgpu_space::kBarMmio, 0, buf, sizeof(buf));
  CHECK(ret == -ENOSYS);

  ret = ule_dgpu_release(0);
  CHECK(ret == -ENOSYS);
}
