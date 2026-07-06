/*
 * test_kfd_portability_phase31_standalone.cpp — Phase 5.3.1
 * (sim-stream-primitive-support ACCEPTED 2026-07-05)
 *
 * Verifies 18 new IOCTL (0x50-0x67) #define 编号注册到
 * shared/gpu_ioctl.h — 实际 handler 行为在 E2E 集成测试中验证。
 * 这里只验证:
 *   1. 18 个 IOCTL #define 编译期存在
 *   2. #define 编号递增 (0x50..0x67)
 *   3. struct 布局对齐 (sizeof checks)
 */

#include <catch_amalgamated.hpp>

#include <cstdint>

extern "C" {
#include "shared/gpu_ioctl.h"
}

TEST_CASE("kfd_portability_phase31 — 0x50 STREAM_CAPTURE_BEGIN defined",
          "[kfd_portability][ioctl][phase31]")
{
  /* 编译期只需 #define 存在 (引用即可) */
  u32 cmd = GPU_IOCTL_STREAM_CAPTURE_BEGIN;
  REQUIRE(cmd != 0);
}

TEST_CASE("kfd_portability_phase31 — 18 IOCTLs registered (0x50-0x67)",
          "[kfd_portability][ioctl][phase31][count]")
{
  /* 抽样验证递增关系 */
  REQUIRE(GPU_IOCTL_STREAM_CAPTURE_END != GPU_IOCTL_STREAM_CAPTURE_BEGIN);
  REQUIRE(GPU_IOCTL_STREAM_CAPTURE_STATUS != GPU_IOCTL_STREAM_CAPTURE_END);
  REQUIRE(GPU_IOCTL_GRAPH_CREATE != GPU_IOCTL_STREAM_CAPTURE_STATUS);
  REQUIRE(GPU_IOCTL_GRAPH_DESTROY != GPU_IOCTL_GRAPH_CREATE);
  REQUIRE(GPU_IOCTL_GRAPH_ADD_KERNEL_NODE != GPU_IOCTL_GRAPH_DESTROY);
  REQUIRE(GPU_IOCTL_GRAPH_ADD_MEMCPY_NODE != GPU_IOCTL_GRAPH_ADD_KERNEL_NODE);
  REQUIRE(GPU_IOCTL_GRAPH_INSTANTIATE != GPU_IOCTL_GRAPH_ADD_MEMCPY_NODE);
  REQUIRE(GPU_IOCTL_GRAPH_LAUNCH != GPU_IOCTL_GRAPH_INSTANTIATE);
  REQUIRE(GPU_IOCTL_GRAPH_DESTROY_EXEC != GPU_IOCTL_GRAPH_LAUNCH);
  REQUIRE(GPU_IOCTL_MEM_POOL_CREATE != GPU_IOCTL_GRAPH_DESTROY_EXEC);
  REQUIRE(GPU_IOCTL_MEM_POOL_DESTROY != GPU_IOCTL_MEM_POOL_CREATE);
  REQUIRE(GPU_IOCTL_MEM_POOL_ALLOC != GPU_IOCTL_MEM_POOL_DESTROY);
  REQUIRE(GPU_IOCTL_MEM_POOL_ALLOC_ASYNC != GPU_IOCTL_MEM_POOL_ALLOC);
  REQUIRE(GPU_IOCTL_MEM_POOL_FREE_ASYNC != GPU_IOCTL_MEM_POOL_ALLOC_ASYNC);
  REQUIRE(GPU_IOCTL_MEM_POOL_SET_ATTR != GPU_IOCTL_MEM_POOL_FREE_ASYNC);
  REQUIRE(GPU_IOCTL_MEM_POOL_GET_ATTR != GPU_IOCTL_MEM_POOL_SET_ATTR);
  REQUIRE(GPU_IOCTL_MEM_POOL_TRIM != GPU_IOCTL_MEM_POOL_GET_ATTR);
}

TEST_CASE("kfd_portability_phase31 — struct layouts are sane",
          "[kfd_portability][ioctl][phase31][struct]")
{
  REQUIRE(sizeof(struct gpu_stream_capture_args) >= 16);
  REQUIRE(sizeof(struct gpu_graph_create_args) >= 8);
  REQUIRE(sizeof(struct gpu_graph_launch_args) >= 24);  /* exec_handle + stream_id + fence_id_out */
  REQUIRE(sizeof(struct gpu_mem_pool_props) >= 24);    /* va_space + size + va_base + va_limit + flags + pad */
  REQUIRE(sizeof(struct gpu_mem_pool_attr_args) >= 48); /* pool_handle + attr + pad + value[4] */
}

TEST_CASE("kfd_portability_phase31 — all 0x50-0x67 #defines unique (single sample via dir)",
          "[kfd_portability][ioctl][phase31]")
{
  /* numbering reservation comment: 0x50-0x59 stream+graph (10), 0x60-0x67 mempool (8)
   * 通过 _IO* macro 解码 (Linux ioctl encoding: dir=bits, type=byte, nr=byte)
   * '_IOW' macro 解码返回的 cmd 中 nr 字段位于低位字节 */
  auto extract_nr = [](u32 cmd) -> u32 { return cmd & 0xFFu; };

  REQUIRE(extract_nr(GPU_IOCTL_STREAM_CAPTURE_BEGIN) == 0x50);
  REQUIRE(extract_nr(GPU_IOCTL_STREAM_CAPTURE_END)   == 0x51);
  REQUIRE(extract_nr(GPU_IOCTL_STREAM_CAPTURE_STATUS) == 0x52);
  REQUIRE(extract_nr(GPU_IOCTL_GRAPH_CREATE)         == 0x53);
  REQUIRE(extract_nr(GPU_IOCTL_GRAPH_DESTROY)        == 0x54);
  REQUIRE(extract_nr(GPU_IOCTL_GRAPH_ADD_KERNEL_NODE)== 0x55);
  REQUIRE(extract_nr(GPU_IOCTL_GRAPH_ADD_MEMCPY_NODE)== 0x56);
  REQUIRE(extract_nr(GPU_IOCTL_GRAPH_INSTANTIATE)    == 0x57);
  REQUIRE(extract_nr(GPU_IOCTL_GRAPH_LAUNCH)         == 0x58);
  REQUIRE(extract_nr(GPU_IOCTL_GRAPH_DESTROY_EXEC)   == 0x59);
  REQUIRE(extract_nr(GPU_IOCTL_MEM_POOL_CREATE)      == 0x60);
  REQUIRE(extract_nr(GPU_IOCTL_MEM_POOL_DESTROY)     == 0x61);
  REQUIRE(extract_nr(GPU_IOCTL_MEM_POOL_ALLOC)       == 0x62);
  REQUIRE(extract_nr(GPU_IOCTL_MEM_POOL_ALLOC_ASYNC) == 0x63);
  REQUIRE(extract_nr(GPU_IOCTL_MEM_POOL_FREE_ASYNC)  == 0x64);
  REQUIRE(extract_nr(GPU_IOCTL_MEM_POOL_SET_ATTR)    == 0x65);
  REQUIRE(extract_nr(GPU_IOCTL_MEM_POOL_GET_ATTR)    == 0x66);
  REQUIRE(extract_nr(GPU_IOCTL_MEM_POOL_TRIM)        == 0x67);
}
