/*
 * Stage 1.2 task group 2.3 — drm_prime.cpp (dma_buf attach/detach/map/unmap/pin/unpin)
 *
 * Aligned with Linux 6.12 LTS dma_buf semantics.
 * Key correction (librarian): amdgpu uses dma_buf_dynamic_attach().
 */

#include "catch_amalgamated.hpp"
#include "linux_compat/drm/drm_prime.h"
#include "linux_compat/drm/drm_device.h"
#include <cstring>

TEST_CASE("dma_buf_dynamic_attach returns non-null attachment", "[drm][prime][attach]")
{
  struct dma_buf buf;
  std::memset(&buf, 0, sizeof(buf));

  struct dma_buf_attach_ops ops{};
  ops.allow_peer2peer = false;
  ops.move_notify = nullptr;

  struct dma_buf_attachment *attach = dma_buf_dynamic_attach(&buf, nullptr, &ops, nullptr);
  REQUIRE(attach != nullptr);
  REQUIRE(attach->dmabuf == &buf);

  dma_buf_detach(&buf, attach);
  /* attach freed — no further access */
}

TEST_CASE("dma_buf_map_attachment returns valid sg_table", "[drm][prime][map]")
{
  struct dma_buf buf;
  std::memset(&buf, 0, sizeof(buf));

  struct dma_buf_attach_ops ops{};
  struct dma_buf_attachment *attach = dma_buf_dynamic_attach(&buf, nullptr, &ops, nullptr);
  REQUIRE(attach != nullptr);

  struct sg_table *sg = dma_buf_map_attachment(attach, 0);
  REQUIRE(sg != nullptr);
  REQUIRE(sg->nents == 1);
  REQUIRE(sg->sgl != nullptr);

  dma_buf_unmap_attachment(attach, sg, 0);
  dma_buf_detach(&buf, attach);
}

TEST_CASE("dma_buf_pin/unpin lifecycle", "[drm][prime][pin]")
{
  struct dma_buf buf;
  std::memset(&buf, 0, sizeof(buf));

  struct dma_buf_attach_ops ops{};
  struct dma_buf_attachment *attach = dma_buf_dynamic_attach(&buf, nullptr, &ops, nullptr);

  int ret = dma_buf_pin(attach);
  REQUIRE(ret == 0);

  dma_buf_unpin(attach);
  dma_buf_detach(&buf, attach);
}

TEST_CASE("dma_buf_get/put lifecycle", "[drm][prime][getput]")
{
  struct dma_buf buf;
  std::memset(&buf, 0, sizeof(buf));

  /* Simulate: get then put */
  struct dma_buf *got = dma_buf_get(42);  /* fd=42, simulated */
  REQUIRE(got != nullptr);

  dma_buf_put(got);
}