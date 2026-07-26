#include <catch_amalgamated.hpp>

#include <cstring>
#include <sys/mman.h>

#include "sim/dma_coherent_pool.h"
#include "linux_compat/dma-mapping.h"

using namespace usr_linux_emu;

TEST_CASE("dma_coherent - init succeeds", "[sim][dma_coherent]") {
  DmaCoherentPool pool;
  REQUIRE(pool.init() == true);
  REQUIRE(pool.initialized == true);
  REQUIRE(pool.cpu_pool != nullptr);
  REQUIRE(pool.cpu_pool != MAP_FAILED);
}

TEST_CASE("dma_coherent - allocate returns valid cpu_addr and dma_addr", "[sim][dma_coherent]") {
  DmaCoherentPool pool;
  pool.init();
  uint64_t dma_addr = 0;
  void* cpu_addr = pool.allocate(4096, &dma_addr);
  REQUIRE(cpu_addr != nullptr);
  REQUIRE(dma_addr >= DMA_COHERENT_BASE);
  REQUIRE(dma_addr < DMA_COHERENT_BASE + DMA_COHERENT_SIZE);
}

TEST_CASE("dma_coherent - multiple allocations get unique dma_addrs", "[sim][dma_coherent]") {
  DmaCoherentPool pool;
  pool.init();
  uint64_t dma1 = 0, dma2 = 0, dma3 = 0;
  void* p1 = pool.allocate(4096, &dma1);
  void* p2 = pool.allocate(4096, &dma2);
  void* p3 = pool.allocate(4096, &dma3);
  REQUIRE(p1 != nullptr);
  REQUIRE(p2 != nullptr);
  REQUIRE(p3 != nullptr);
  REQUIRE(dma1 != dma2);
  REQUIRE(dma2 != dma3);
  REQUIRE(dma1 != dma3);
}

TEST_CASE("dma_coherent - dma_addr within range [0x1_0000_0000, 0x1_0FFF_FFFF]", "[sim][dma_coherent]") {
  DmaCoherentPool pool;
  pool.init();
  uint64_t dma_addr = 0;
  pool.allocate(4096, &dma_addr);
  REQUIRE(dma_addr >= 0x100000000ULL);
  REQUIRE(dma_addr < 0x100000000ULL + 0x10000000ULL);
}

TEST_CASE("dma_coherent - cpu_addr is writable and readable", "[sim][dma_coherent]") {
  DmaCoherentPool pool;
  pool.init();
  uint64_t dma_addr = 0;
  void* cpu_addr = pool.allocate(4096, &dma_addr);
  uint32_t* ptr = static_cast<uint32_t*>(cpu_addr);
  ptr[0] = 0xDEADBEEF;
  ptr[1] = 0xCAFEBABE;
  REQUIRE(ptr[0] == 0xDEADBEEF);
  REQUIRE(ptr[1] == 0xCAFEBABE);
}

TEST_CASE("dma_coherent - free removes allocation", "[sim][dma_coherent]") {
  DmaCoherentPool pool;
  pool.init();
  uint64_t dma_addr = 0;
  pool.allocate(4096, &dma_addr);
  REQUIRE(pool.allocations.size() == 1);
  pool.free(dma_addr);
  REQUIRE(pool.allocations.empty());
}

TEST_CASE("dma_coherent - allocate without init returns nullptr", "[sim][dma_coherent]") {
  DmaCoherentPool pool;
  uint64_t dma_addr = 0;
  void* result = pool.allocate(4096, &dma_addr);
  REQUIRE(result == nullptr);
}

TEST_CASE("dma_coherent - consecutive allocations are contiguous", "[sim][dma_coherent]") {
  DmaCoherentPool pool;
  pool.init();
  uint64_t dma1 = 0, dma2 = 0;
  void* p1 = pool.allocate(4096, &dma1);
  void* p2 = pool.allocate(4096, &dma2);
  // Bump allocator: dma2 should be dma1 + 4096
  REQUIRE(dma2 == dma1 + 4096);
  // cpu pointers should also be contiguous
  uint8_t* b1 = static_cast<uint8_t*>(p1);
  uint8_t* b2 = static_cast<uint8_t*>(p2);
  REQUIRE(b2 == b1 + 4096);
}

// ============================================================================
// Stage 4.1 Phase 2 - ① COMPAT layer API tests (dma_alloc_coherent etc.)
// These test the Linux-compat API which delegates to the global g_dma_pool
// ============================================================================

TEST_CASE("compat dma_alloc_coherent - allocates and returns valid cpu_addr", "[compat][dma]") {
  // Initialize the global pool
  REQUIRE(g_dma_pool.init() == true);

  dma_addr_t dma_handle = 0;
  void* cpu_addr = dma_alloc_coherent(nullptr, 4096, &dma_handle, GFP_KERNEL);
  REQUIRE(cpu_addr != nullptr);
  REQUIRE(dma_handle >= DMA_COHERENT_BASE);
  REQUIRE(dma_handle < DMA_COHERENT_BASE + DMA_COHERENT_SIZE);
}

TEST_CASE("compat dma_alloc_coherent - cpu_addr is writable and readable", "[compat][dma]") {
  dma_addr_t dma_handle = 0;
  void* cpu_addr = dma_alloc_coherent(nullptr, 4096, &dma_handle, GFP_KERNEL);
  REQUIRE(cpu_addr != nullptr);

  uint32_t* ptr = static_cast<uint32_t*>(cpu_addr);
  ptr[0] = 0xDEADBEEF;
  ptr[1] = 0xCAFEBABE;
  REQUIRE(ptr[0] == 0xDEADBEEF);
  REQUIRE(ptr[1] == 0xCAFEBABE);
}

TEST_CASE("compat dma_alloc_coherent - multiple allocations get unique dma_addrs", "[compat][dma]") {
  dma_addr_t dma1 = 0, dma2 = 0, dma3 = 0;
  void* p1 = dma_alloc_coherent(nullptr, 4096, &dma1, GFP_KERNEL);
  void* p2 = dma_alloc_coherent(nullptr, 4096, &dma2, GFP_KERNEL);
  void* p3 = dma_alloc_coherent(nullptr, 4096, &dma3, GFP_KERNEL);
  REQUIRE(p1 != nullptr);
  REQUIRE(p2 != nullptr);
  REQUIRE(p3 != nullptr);
  REQUIRE(dma1 != dma2);
  REQUIRE(dma2 != dma3);
  REQUIRE(dma1 != dma3);
}

TEST_CASE("compat dma_free_coherent - frees allocation without crash", "[compat][dma]") {
  dma_addr_t dma_handle = 0;
  void* cpu_addr = dma_alloc_coherent(nullptr, 4096, &dma_handle, GFP_KERNEL);
  REQUIRE(cpu_addr != nullptr);
  REQUIRE(g_dma_pool.allocations.count(dma_handle) == 1);

  dma_free_coherent(nullptr, 4096, cpu_addr, dma_handle);
  REQUIRE(g_dma_pool.allocations.count(dma_handle) == 0);
}

TEST_CASE("compat dma_map_single - stub returns 0", "[compat][dma]") {
  // Per ADR-073 D6: streaming deferred, returns 0
  dma_addr_t result = dma_map_single(nullptr, nullptr, 4096, DMA_BIDIRECTIONAL);
  REQUIRE(result == 0);
}

TEST_CASE("compat dma_alloc_coherent - dma_addr in valid range", "[compat][dma]") {
  dma_addr_t dma_handle = 0;
  dma_alloc_coherent(nullptr, 4096, &dma_handle, GFP_KERNEL);
  REQUIRE(dma_handle >= 0x100000000ULL);
  REQUIRE(dma_handle < 0x100000000ULL + 0x10000000ULL);
}
