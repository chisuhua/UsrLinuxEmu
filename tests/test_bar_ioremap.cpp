#include <catch_amalgamated.hpp>

#include <cstdint>
#include <cstring>
#include <sys/mman.h>

// Include the compat layer header under test
#include "linux_compat/io.h"

// Sim layer: needed to initialize g_vram_store so ioremap has BAR backing
#include "sim/vram_store.h"
#include "sim/bar_sim.h"
#include "sim/hardware/mqd_state.h"

using namespace usr_linux_emu;

// HQD register constants (must match bar_sim.h / shared/mqd.h)
#ifndef HQD_BASE
#define HQD_BASE 0x4000
#endif
#ifndef HQD_STRIDE
#define HQD_STRIDE 64
#endif

// Helper: set up BAR0 with a known phys_base and return it
static uint64_t setup_bar0(uint64_t phys_base, uint64_t size) {
  GpuVramStore& store = g_vram_store;
  store.init(64);
  PciBarSim* bar0 = store.get_bar(0);
  bar0->phys_base = phys_base;
  bar0->size = size;
  return phys_base;
}

TEST_CASE("ioremap - maps known BAR and returns non-null", "[compat][io]") {
  uint64_t bar0_phys = setup_bar0(0x10000000ULL, 0x10000);

  void* mapped = ioremap(bar0_phys, 0x10000);
  REQUIRE(mapped != nullptr);
  REQUIRE(mapped != MAP_FAILED);

  iounmap(mapped);
}

TEST_CASE("ioremap - writel/readl roundtrip single register", "[compat][io]") {
  uint64_t bar0_phys = setup_bar0(0x20000000ULL, 0x10000);

  void __iomem* base = ioremap(bar0_phys, 0x10000);
  REQUIRE(base != nullptr);

  // Write a value and read it back
  writel(0xDEADBEEF, base);
  uint32_t val = readl(base);
  REQUIRE(val == 0xDEADBEEF);

  iounmap(base);
}

TEST_CASE("ioremap - writel/readl at offset", "[compat][io]") {
  uint64_t bar0_phys = setup_bar0(0x30000000ULL, 0x10000);

  void __iomem* base = ioremap(bar0_phys, 0x10000);
  REQUIRE(base != nullptr);

  // Write at offset 4 and 8
  writel(0xCAFEBABE, (volatile void __iomem*)((char*)base + 4));
  writel(0x12345678, (volatile void __iomem*)((char*)base + 8));

  REQUIRE(readl((const volatile void __iomem*)((char*)base + 4)) == 0xCAFEBABE);
  REQUIRE(readl((const volatile void __iomem*)((char*)base + 8)) == 0x12345678);

  // Offset 0 should still be zero (or whatever it was)
  iounmap(base);
}

TEST_CASE("ioremap - ioread32/iowrite32 alias works", "[compat][io]") {
  uint64_t bar0_phys = setup_bar0(0x40000000ULL, 0x10000);

  void __iomem* base = ioremap(bar0_phys, 0x10000);
  REQUIRE(base != nullptr);

  iowrite32(0xBEEF1234, base);
  REQUIRE(ioread32(base) == 0xBEEF1234);

  iounmap(base);
}

TEST_CASE("ioremap - unknown phys_addr returns null", "[compat][io]") {
  // Don't set up any BAR with this address
  g_vram_store.init(64);

  void* mapped = ioremap(0xDEADBEEFDEAD, 0x1000);
  REQUIRE(mapped == nullptr);
}

TEST_CASE("ioremap - idempotent mapping returns same pointer", "[compat][io]") {
  uint64_t bar0_phys = setup_bar0(0x50000000ULL, 0x10000);

  void* first = ioremap(bar0_phys, 0x10000);
  void* second = ioremap(bar0_phys, 0x10000);
  REQUIRE(first != nullptr);
  REQUIRE(first == second);

  iounmap(first);
}

TEST_CASE("bar0_hqd_activate_and_read_status", "[bar0_hqd]") {
  setup_bar0(0x00000000ULL, 0x8000);

  void* bar0 = sim_bar_ioremap(0x0000, 0x8000);
  REQUIRE(bar0 != nullptr);

  uint32_t channel = 0;
  uint64_t ctl_offset   = HQD_BASE + channel * HQD_STRIDE + 0x00;
  uint64_t status_offset = HQD_BASE + channel * HQD_STRIDE + 0x04;

  sim_bar0_writel(ctl_offset, 0x00000001);

  uint32_t status = sim_bar0_readl(status_offset);
  REQUIRE(status == MQD_STATE_ACTIVE);

  sim_bar_iounmap(bar0, 0x8000);
}
