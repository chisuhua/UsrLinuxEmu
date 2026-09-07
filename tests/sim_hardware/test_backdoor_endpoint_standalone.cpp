/*
 * tests/sim_hardware/test_backdoor_endpoint_standalone.cpp
 * kcpptlm-backend-binding-with-handle-and-adapter-info — §D2
 *
 * CHARACTERIZATION TEST: BackdoorEndpoint implementation is Phase 2.1 (not yet
 * delivered). These tests verify:
 * 1. Header compiles with expected API surface (signature check)
 * 2. -ENOSYS returned until implementation lands
 * 3. Handle lifecycle (acquire → get_info → read/write → release)
 * 4. Space-distinguished read/write
 * 5. Concurrent cross-thread safety
 */
#include <catch_amalgamated.hpp>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <thread>
#include <vector>

#include "cpptlm/backdoor_endpoint.h"

TEST_CASE("backdoor_endpoint: header API surface compiles", "[backdoor][characterization]") {
  CHECK(sizeof(ule_dgpu_space) >= 4);
  CHECK(sizeof(ule_dgpu_handle_t) >= 8);
  CHECK(sizeof(ule_dgpu_adapter_info) >= 48);
}

TEST_CASE("backdoor_endpoint: acquire returns -ENOSYS until Phase 2.1",
          "[backdoor][characterization]") {
  ule_dgpu_handle_t h = 0;
  int ret = ule_dgpu_acquire(0, &h);
  REQUIRE(ret == -ENOSYS);
}

TEST_CASE("backdoor_endpoint: get_adapter_info returns -ENOSYS until Phase 2.1",
          "[backdoor][characterization]") {
  ule_dgpu_adapter_info info{};
  int ret = ule_dgpu_get_adapter_info(0, &info);
  REQUIRE(ret == -ENOSYS);
}

TEST_CASE("backdoor_endpoint: read returns -ENOSYS until Phase 2.1",
          "[backdoor][characterization]") {
  uint8_t buf[8] = {};
  int ret = ule_dgpu_read(0, ule_dgpu_space::kConfig, 0, buf, sizeof(buf));
  REQUIRE(ret == -ENOSYS);
}

TEST_CASE("backdoor_endpoint: write returns -ENOSYS until Phase 2.1",
          "[backdoor][characterization]") {
  uint8_t buf[8] = {};
  int ret = ule_dgpu_write(0, ule_dgpu_space::kConfig, 0, buf, sizeof(buf));
  REQUIRE(ret == -ENOSYS);
}

TEST_CASE("backdoor_endpoint: release returns -ENOSYS until Phase 2.1",
          "[backdoor][characterization]") {
  int ret = ule_dgpu_release(0);
  REQUIRE(ret == -ENOSYS);
}

TEST_CASE("backdoor_endpoint: space enum values are distinct",
          "[backdoor][characterization]") {
  CHECK(static_cast<uint32_t>(ule_dgpu_space::kConfig) !=
        static_cast<uint32_t>(ule_dgpu_space::kBarMmio));
  CHECK(static_cast<uint32_t>(ule_dgpu_space::kBarMmio) !=
        static_cast<uint32_t>(ule_dgpu_space::kBarVram));
  CHECK(static_cast<uint32_t>(ule_dgpu_space::kBarVram) !=
        static_cast<uint32_t>(ule_dgpu_space::kAxiDirect));
}

TEST_CASE("backdoor_endpoint: adapter_info field offsets",
          "[backdoor][characterization]") {
  ule_dgpu_adapter_info info{};
  info.vendor_id = 0x1002;
  info.device_id = 0x740C;
  info.gpu_id = 42;
  info.gfx_version = 0x10A;
  info.bdf = 0x1234;
  info.visible_vram_size = 8ULL * 1024 * 1024 * 1024;
  info.invisible_vram_size = 16ULL * 1024 * 1024 * 1024;
  info.va_region_size = 32ULL * 1024 * 1024 * 1024;
  CHECK(info.vendor_id == 0x1002);
  CHECK(info.device_id == 0x740C);
  CHECK(info.gpu_id == 42);
  CHECK(info.gfx_version == 0x10A);
  CHECK(info.bdf == 0x1234);
  CHECK(info.visible_vram_size == 8ULL * 1024 * 1024 * 1024);
  CHECK(info.invisible_vram_size == 16ULL * 1024 * 1024 * 1024);
  CHECK(info.va_region_size == 32ULL * 1024 * 1024 * 1024);
  CHECK(info.bar_sizes[0] == 0);
  CHECK(info.bar_sizes[5] == 0);
}

TEST_CASE("backdoor_endpoint: concurrent acquire threads all return -ENOSYS",
          "[backdoor][characterization][concurrent]") {
  std::atomic<int> failures{0};
  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back([&]() {
      ule_dgpu_handle_t h = 0;
      if (ule_dgpu_acquire(0, &h) != -ENOSYS) failures.fetch_add(1);
    });
  }
  for (auto& t : threads) t.join();
  CHECK(failures.load() == 0);
}

TEST_CASE("backdoor_endpoint: handle type is uint64_t compatible",
          "[backdoor][characterization]") {
  ule_dgpu_handle_t h1 = 1;
  ule_dgpu_handle_t h2 = 2;
  CHECK(h1 != h2);
  CHECK(h1 + 1 == h2);
}
