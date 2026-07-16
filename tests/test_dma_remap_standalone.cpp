// test_dma_remap_standalone.cpp — Stage 1.1 DMA remapping page-table correctness
// Per tasks.md §8.2: covers happy path + 1 error path; explicit error code mapping.
// Per spec Requirement: DMA remapping 页表实现 / 错误码语义与 Linux 内核一致.

#include <catch_amalgamated.hpp>

#include <cstdint>
#include <cstddef>
#include <cerrno>

extern "C" {
#include <linux_compat/iommu/iommu.h>
#include <linux_compat/iommu/iommu_domain.h>

/* H2: sim_pm + bridge declarations (C-linkage, resolved from gpu_sim + kernel).
 * Forward-declared here rather than including sim_proxy.h, which redeclares
 * iommu_map/unmap with a conflicting signature (unsigned long vs phys_addr_t). */
struct sim_page_migration;
struct sim_page_migration *sim_pm_create(unsigned long);
void sim_pm_destroy(struct sim_page_migration *);
int sim_pm_attach_domain(struct sim_page_migration *, void *);
int sim_pm_is_page_on_device(struct sim_page_migration *, unsigned long);
int sim_pm_migrate_to_device(struct sim_page_migration *, unsigned long,
                             const void *, unsigned long);
int iommu_domain_attach_sim_pm(struct iommu_domain *,
                               struct sim_page_migration *);
int iommu_flush_iotlb(struct iommu_domain *, unsigned long, size_t);
}

namespace {

/* Use Linux errno values to verify our constants are byte-exact. */
static_assert(IOMMU_ERR_EINVAL    == -EINVAL,    "IOMMU_ERR_EINVAL must match Linux -EINVAL");
static_assert(IOMMU_ERR_ENOMEM    == -ENOMEM,    "IOMMU_ERR_ENOMEM must match Linux -ENOMEM");
static_assert(IOMMU_ERR_ENOSPC    == -ENOSPC,    "IOMMU_ERR_ENOSPC must match Linux -ENOSPC");
static_assert(IOMMU_ERR_EREMOTEIO == -EREMOTEIO, "IOMMU_ERR_EREMOTEIO must match Linux -EREMOTEIO");
static_assert(IOMMU_ERR_ENOKEY    == -ENOKEY,    "IOMMU_ERR_ENOKEY must match Linux -ENOKEY");
static_assert(IOMMU_ERR_ETIMEDOUT == -ETIMEDOUT, "IOMMU_ERR_ETIMEDOUT must match Linux -ETIMEDOUT");
static_assert(IOMMU_ERR_ENOSYS    == -ENOSYS,    "IOMMU_ERR_ENOSYS must match Linux -ENOSYS");
static_assert(IOMMU_ERR_ENODEV    == -ENODEV,    "IOMMU_ERR_ENODEV must match Linux -ENODEV");
static_assert(IOMMU_ERR_EBUSY     == -EBUSY,     "IOMMU_ERR_EBUSY must match Linux -EBUSY");

/* T1+H2: file-static flush_iotlb call counter.  counting_flush_iotlb
 * increments the counter AND delegates to default_flush_iotlb so the
 * sim_pm_invalidate bridge stays live when state->sim_pm is attached
 * (counting-only would silently break the bridge under test). */
static int g_flush_call_count = 0;

static void counting_flush_iotlb(struct iommu_domain *d, unsigned long iova,
                                 size_t sz, void *flushed_entries) {
  g_flush_call_count++;
  const struct iommu_ops *def = iommu_default_ops_get();
  if (def && def->flush_iotlb)
    def->flush_iotlb(d, iova, sz, flushed_entries);
}

struct DmaRemapFixture {
  struct iommu_domain *d = nullptr;
  DmaRemapFixture() {
    REQUIRE(iommu_emu_init() == 0);
    d = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
    REQUIRE(d != nullptr);
  }
  ~DmaRemapFixture() {
    if (d) iommu_domain_free(d);
  }
};

}  // namespace

TEST_CASE_METHOD(DmaRemapFixture, "dma_remap_map_unmap_roundtrip", "[dma_remap]") {
  /* SPEC: Requirement: DMA remapping 页表实现 — Happy path DMA remap */
  CHECK(iommu_map(d, 0x1000, 0x100000, 0x1000, IOMMU_READ) == IOMMU_ERR_OK);
  CHECK(iommu_iova_to_phys(d, 0x1000) == 0x100000);

  long ret = iommu_unmap(d, 0x1000, 0x1000);
  CHECK(ret == 0x1000);  /* returns size unmapped on success */
  CHECK(iommu_iova_to_phys(d, 0x1000) == 0);
}

TEST_CASE_METHOD(DmaRemapFixture, "dma_remap_map_overlap_returns_eremoteio", "[dma_remap][error]") {
  /* SPEC: Requirement: DMA remapping 页表实现 — IOVA overlap returns EREMOTEIO
   *     / Requirement: 错误码语义与 Linux 内核一致 */
  CHECK(iommu_map(d, 0x1000, 0x100000, 0x1000, 0) == IOMMU_ERR_OK);
  int rc = iommu_map(d, 0x1000, 0x200000, 0x1000, 0);
  CHECK(rc == IOMMU_ERR_EREMOTEIO);
  CHECK(rc == -EREMOTEIO);     /* Linux kernel byte-exact verification */
  CHECK(rc == -121);           /* Numeric value documentation */
  /* Original mapping preserved per spec */
  CHECK(iommu_iova_to_phys(d, 0x1000) == 0x100000);
}

TEST_CASE_METHOD(DmaRemapFixture, "dma_remap_unmap_not_mapped_returns_enokey", "[dma_remap][error]") {
  /* SPEC: Requirement: 错误码语义与 Linux 内核一致 — unmap unmapped → -ENOKEY */
  long ret = iommu_unmap(d, 0x9999, 0x1000);
  CHECK(ret == (long)IOMMU_ERR_ENOKEY);
  CHECK(ret == -ENOKEY);
  CHECK(ret == -126);
}

TEST_CASE_METHOD(DmaRemapFixture, "dma_remap_invalid_size_returns_einval", "[dma_remap][error]") {
  /* SPEC: Requirement: DMA remapping 页表实现 — only 4KB mapping in stage-1.1 */
  int rc1 = iommu_map(d, 0x1000, 0x100000, 0x2000, 0);
  CHECK(rc1 == IOMMU_ERR_EINVAL);
  CHECK(rc1 == -EINVAL);
  CHECK(rc1 == -22);

  /* size=0 also invalid */
  int rc2 = iommu_map(d, 0x1000, 0x100000, 0, 0);
  CHECK(rc2 == IOMMU_ERR_EINVAL);

  long rc3 = iommu_unmap(d, 0x1000, 0x2000);
  CHECK(rc3 == (long)IOMMU_ERR_EINVAL);
}

TEST_CASE_METHOD(DmaRemapFixture, "dma_remap_misaligned_addresses_return_einval", "[dma_remap][error]") {
  /* SPEC: Requirement: DMA remapping 页表实现 — paddr not page-aligned → -EINVAL */
  CHECK(iommu_map(d, 0x1001, 0x100000, 0x1000, 0) == IOMMU_ERR_EINVAL);
  CHECK(iommu_map(d, 0x1000, 0x100001, 0x1000, 0) == IOMMU_ERR_EINVAL);
  CHECK(iommu_map(d, 0x1FFF, 0x1FFFFF, 0x1000, 0) == IOMMU_ERR_EINVAL);
}

TEST_CASE_METHOD(DmaRemapFixture, "dma_remap_iova_to_phys_zero_for_unmapped", "[dma_remap]") {
  /* SPEC: Requirement: 错误码语义与 Linux 内核一致 — iova_to_phys 0 = unmapped */
  CHECK(iommu_iova_to_phys(d, 0xDEAD) == 0);
  CHECK(iommu_iova_to_phys(nullptr, 0x1000) == 0);
}

TEST_CASE_METHOD(DmaRemapFixture, "dma_remap_default_ops_vtable_roundtrip", "[dma_remap][ops]") {
  /* SPEC: Requirement: IOMMU 数据结构定义 — iommu_ops hook completeness */
  const struct iommu_ops *ops = iommu_default_ops_get();
  REQUIRE(ops != nullptr);

  d->ops = ops;
  REQUIRE(d->ops->map_page != nullptr);
  REQUIRE(d->ops->unmap_page != nullptr);
  REQUIRE(d->ops->iova_to_phys != nullptr);

  /* Exercise ops vtable path (Linux kernel style) */
  CHECK(d->ops->map_page(d, 0x5000, 0x500000, 0x1000, 0) == IOMMU_ERR_OK);
  CHECK(d->ops->iova_to_phys(d, 0x5000) == 0x500000);
  CHECK(d->ops->unmap_page(d, 0x5000, 0x1000) == 0);  /* success → 0 */
  CHECK(d->ops->iova_to_phys(d, 0x5000) == 0);
}

TEST_CASE_METHOD(DmaRemapFixture, "dma_remap_ops_unmap_error_returns_negative", "[dma_remap][ops][error]") {
  /* SPEC: Requirement: 错误码语义与 Linux 内核一致 — unmap error via ops vtable */
  const struct iommu_ops *ops = iommu_default_ops_get();
  d->ops = ops;

  /* unmap_page returns int (not long), so error code must be < 0 */
  int rc = d->ops->unmap_page(d, 0x9999, 0x1000);
  CHECK(rc < 0);
  CHECK(rc == (int)IOMMU_ERR_ENOKEY);
}

TEST_CASE_METHOD(DmaRemapFixture, "dma_remap_flush_iotlb_invoked_on_unmap", "[dma_remap][ops]") {
  /* SPEC: Requirement: 错误码语义与 Linux 内核一致 - IOTLB flush on unmap.
   * T1 fix: the previous version built a tracing_ops with a no-op lambda
   * flush_iotlb but never assigned d->ops, so the hook was never exercised
   * and the test asserted nothing about the flush.  Now uses the
   * file-static counting_flush_iotlb and verifies the call count. */
  g_flush_call_count = 0;
  struct iommu_ops counting_ops = *iommu_default_ops_get();
  counting_ops.flush_iotlb = counting_flush_iotlb;
  d->ops = &counting_ops;

  CHECK(iommu_map(d, 0x6000, 0x600000, 0x1000, 0) == IOMMU_ERR_OK);
  CHECK(g_flush_call_count == 0);  /* map must not flush */
  CHECK(iommu_unmap(d, 0x6000, 0x1000) == 0x1000);
  CHECK(g_flush_call_count == 1);  /* unmap triggered flush_iotlb */
}

/* H2 (HIGH): iotlb_flush -> sim_pm_invalidate end-to-end bridge (SPEC §5).
 *
 * Verifies the full ①->③ invalidation path:
 *   iommu_unmap -> domain->ops->flush_iotlb (counting_flush_iotlb)
 *               -> default_flush_iotlb -> sim_pm_invalidate
 *               -> page evicted from sim_pm (PAGE_CLEAN -> PAGE_EVICTED).
 *
 * Setup: sim_pm is bound both ways - sim_pm_attach_domain(pm, d) so
 * sim_pm_migrate_to_device calls iommu_map internally, AND
 * iommu_domain_attach_sim_pm(d, pm) so default_flush_iotlb can reach
 * sim_pm_invalidate via state->sim_pm.  counting_flush_iotlb wraps the
 * default to count calls without breaking the bridge. */
TEST_CASE_METHOD(DmaRemapFixture, "dma_remap_flush_iotlb_tracking_via_sim_pm",
                 "[dma_remap][flush][iotlb][sim_pm][bridge][tier2]")
{
  g_flush_call_count = 0;

  struct iommu_ops counting_ops = *iommu_default_ops_get();
  counting_ops.flush_iotlb = counting_flush_iotlb;
  d->ops = &counting_ops;

  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);
  REQUIRE(sim_pm_attach_domain(pm, d) == 0);
  REQUIRE(iommu_domain_attach_sim_pm(d, pm) == 0);

  /* sim_pm_migrate_to_device internally calls iommu_map; no flush yet. */
  unsigned char src[4096];
  for (int i = 0; i < 4096; i++) src[i] = (unsigned char)(i & 0xff);
  REQUIRE(sim_pm_migrate_to_device(pm, 0x1000, src, 4096) == 0);
  CHECK(sim_pm_is_page_on_device(pm, 0x1000) == 1);
  CHECK(g_flush_call_count == 0);

  /* iommu_unmap -> flush_iotlb -> default_flush_iotlb -> sim_pm_invalidate.
   * flushed_entries snapshot (captured before erase) carries the evicted
   * iova so sim_pm_invalidate can evict the page from device state. */
  long unmap = iommu_unmap(d, 0x1000, 0x1000);
  REQUIRE(unmap == 0x1000);
  CHECK(g_flush_call_count == 1);
  CHECK(sim_pm_is_page_on_device(pm, 0x1000) == 0);

  /* Public iommu_flush_iotlb wrapper delegates through ops->flush_iotlb.
   * Passes nullptr for flushed_entries, so sim_pm_invalidate is skipped
   * (no entries to invalidate) - only the counter advances. */
  int rc = iommu_flush_iotlb(d, 0x2000, 0x1000);
  CHECK(rc == 0);
  CHECK(g_flush_call_count == 2);

  sim_pm_destroy(pm);
}
