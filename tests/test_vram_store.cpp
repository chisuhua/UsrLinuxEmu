#include <catch_amalgamated.hpp>

#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

#include "sim/vram_store.h"

using namespace usr_linux_emu;

TEST_CASE("vram_store - init succeeds with default 256MB", "[sim][vram_store]") {
  GpuVramStore store;
  REQUIRE(store.init(256) == true);
  REQUIRE(store.initialized == true);
  REQUIRE(store.vram_size == 256ULL * 1024 * 1024);
  REQUIRE(store.pool_backing != nullptr);
  REQUIRE(store.pool_backing != MAP_FAILED);
}

TEST_CASE("vram_store - init with custom size", "[sim][vram_store]") {
  GpuVramStore store;
  REQUIRE(store.init(64) == true);
  REQUIRE(store.vram_size == 64ULL * 1024 * 1024);
}

TEST_CASE("vram_store - get_bar returns valid pointers", "[sim][vram_store]") {
  GpuVramStore store;
  store.init(64);
  for (int i = 0; i < 6; i++) {
    PciBarSim* bar = store.get_bar(i);
    REQUIRE(bar != nullptr);
    REQUIRE(bar->phys_base == 0);
    REQUIRE(bar->backing == nullptr);
  }
}

TEST_CASE("vram_store - get_bar out of range returns nullptr", "[sim][vram_store]") {
  GpuVramStore store;
  store.init(64);
  REQUIRE(store.get_bar(-1) == nullptr);
  REQUIRE(store.get_bar(6) == nullptr);
  REQUIRE(store.get_bar(99) == nullptr);
}

TEST_CASE("vram_store - bar_ioremap maps and returns backing", "[sim][vram_store]") {
  GpuVramStore store;
  store.init(64);
  // Set BAR0 phys_base before ioremap
  PciBarSim* bar0 = store.get_bar(0);
  bar0->phys_base = 0x10000000ULL;
  bar0->size = 0x10000; // 64KB

  void* mapped = store.bar_ioremap(0x10000000ULL, 0x10000);
  REQUIRE(mapped != nullptr);
  REQUIRE(mapped != MAP_FAILED);

  // Verify we can write/read to the mapping
  uint32_t* ptr = static_cast<uint32_t*>(mapped);
  ptr[0] = 0xDEADBEEF;
  REQUIRE(ptr[0] == 0xDEADBEEF);
}

TEST_CASE("vram_store - bar_ioremap idempotent returns same pointer", "[sim][vram_store]") {
  GpuVramStore store;
  store.init(64);
  PciBarSim* bar1 = store.get_bar(1);
  bar1->phys_base = 0x20000000ULL;
  bar1->size = 0x10000;

  void* first = store.bar_ioremap(0x20000000ULL, 0x10000);
  void* second = store.bar_ioremap(0x20000000ULL, 0x10000);
  REQUIRE(first != nullptr);
  REQUIRE(first == second);
}

TEST_CASE("vram_store - bar_ioremap unknown phys returns nullptr", "[sim][vram_store]") {
  GpuVramStore store;
  store.init(64);
  // No BAR has this phys_base
  void* mapped = store.bar_ioremap(0xDEADBEEF, 0x1000);
  REQUIRE(mapped == nullptr);
}

TEST_CASE("vram_store - bar_iounmap clears BAR backing", "[sim][vram_store]") {
  GpuVramStore store;
  store.init(64);
  PciBarSim* bar2 = store.get_bar(2);
  bar2->phys_base = 0x30000000ULL;
  bar2->size = 0x10000;

  void* mapped = store.bar_ioremap(0x30000000ULL, 0x10000);
  REQUIRE(mapped != nullptr);

  store.bar_iounmap(mapped, 0x10000);
  REQUIRE(bar2->backing == nullptr);
}

TEST_CASE("vram_store - data persists across BAR access", "[sim][vram_store]") {
  GpuVramStore store;
  store.init(64);
  PciBarSim* bar0 = store.get_bar(0);
  bar0->phys_base = 0x40000000ULL;
  bar0->size = 0x1000;

  void* mapped = store.bar_ioremap(0x40000000ULL, 0x1000);
  uint32_t* ptr = static_cast<uint32_t*>(mapped);
  ptr[10] = 0xCAFEBABE;
  ptr[20] = 0xBEEF1234;

  // Re-read via same pointer
  REQUIRE(ptr[10] == 0xCAFEBABE);
  REQUIRE(ptr[20] == 0xBEEF1234);
}

TEST_CASE("vram_store - multiple BARs independent", "[sim][vram_store]") {
  GpuVramStore store;
  store.init(64);
  PciBarSim* bar0 = store.get_bar(0);
  PciBarSim* bar1 = store.get_bar(1);
  bar0->phys_base = 0x50000000ULL;
  bar0->size = 0x1000;
  bar1->phys_base = 0x60000000ULL;
  bar1->size = 0x1000;

  void* m0 = store.bar_ioremap(0x50000000ULL, 0x1000);
  void* m1 = store.bar_ioremap(0x60000000ULL, 0x1000);
  REQUIRE(m0 != nullptr);
  REQUIRE(m1 != nullptr);
  REQUIRE(m0 != m1);

  // Write to BAR0 doesn't affect BAR1
  uint32_t* p0 = static_cast<uint32_t*>(m0);
  uint32_t* p1 = static_cast<uint32_t*>(m1);
  p0[0] = 0xAAAA;
  p1[0] = 0xBBBB;
  REQUIRE(p0[0] == 0xAAAA);
  REQUIRE(p1[0] == 0xBBBB);
}
