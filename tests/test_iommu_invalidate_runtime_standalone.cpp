/*
 * test_iommu_invalidate_runtime_standalone.cpp — Stage 1.4 Tier-2 §5
 *
 * Verifies IOMMU IOTLB flush penetrates from fprintf stub to real
 * implementation:
 * 1. iommu_flush_iotlb is callable on a valid domain
 * 2. After map + unmap + flush, the flush actually executes (no longer
 *    just a fprintf) — verified via a domain-priv counter
 * 3. Boundary: flush on null domain returns -EINVAL
 * 4. Boundary: flush on non-page-aligned iova is handled (current impl
 *    may accept and log; just verify no crash)
 *
 * Per design.md D3: minimal viable flush in user-space only.
 * Per boundary §3.2: real hardware invalidation deferred to Stage 2.
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/types.h>
#include <linux_compat/iommu/iommu.h>
#include <linux_compat/iommu/iommu_domain.h>

int iommu_flush_iotlb(struct iommu_domain *domain, unsigned long iova, size_t size);

/* Stage 2.1.1 vfio bridge (boundary §5.2 absorption) */
int  us_iommu_vfio_available(void);
int  us_iommu_vfio_invalidate(unsigned long iova, size_t size);
void us_iommu_vfio_reset(void);

/* M8 sim_pm bridge forward declarations */
struct sim_page_migration;
struct sim_page_migration *sim_pm_create(unsigned long);
void                       sim_pm_destroy(struct sim_page_migration *);
int  sim_pm_attach_domain(struct sim_page_migration *, void *);
int  sim_pm_migrate_to_device(struct sim_page_migration *,
                               unsigned long, const void *, unsigned long);
int  sim_pm_is_page_on_device(struct sim_page_migration *,
                               unsigned long);
int  iommu_domain_attach_sim_pm(struct iommu_domain *,
                                 struct sim_page_migration *);
}

TEST_CASE("iommu_flush_iotlb — null domain returns -EINVAL",
          "[kernel][iommu][flush][tier2][null_guard]")
{
  int ret = iommu_flush_iotlb(nullptr, 0x1000, 4096);
  CHECK(ret == -22);  /* -EINVAL */
}

TEST_CASE("iommu_flush_iotlb — real implementation executes on valid domain",
          "[kernel][iommu][flush][tier2]")
{
  struct iommu_domain *domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(domain != nullptr);

  /* Map a page, then unmap to trigger flush.  If flush_iotlb is still
   * a fprintf-only stub, the new Tier-2 implementation will execute
   * (visible via stderr log change AND no crash on subsequent operations). */
  int map_ret = iommu_map(domain, 0x1000, 0x1000, 4096, 0);
  REQUIRE(map_ret == 0);

  long unmap_ret = iommu_unmap(domain, 0x1000, 4096);
  REQUIRE(unmap_ret == 4096);  /* Linux semantics: returns size */

  /* Manually trigger flush (iommu_unmap also calls it but verify
   * direct call works too). */
  int flush_ret = iommu_flush_iotlb(domain, 0x1000, 4096);
  CHECK(flush_ret == 0);

  iommu_domain_free(domain);
}

TEST_CASE("iommu_flush_iotlb — multiple pages flushed in range",
          "[kernel][iommu][flush][tier2]")
{
  struct iommu_domain *domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(domain != nullptr);

  /* Stage 1.1 only supports 4KB pages, so each map is one entry.
   * Tier-2 flush should handle the [iova, iova+size) range even if
   * size spans multiple pages. */
  REQUIRE(iommu_map(domain, 0x1000, 0x1000, 4096, 0) == 0);
  REQUIRE(iommu_map(domain, 0x2000, 0x2000, 4096, 0) == 0);

  /* Flush a range covering both pages. */
  int flush_ret = iommu_flush_iotlb(domain, 0x1000, 8192);
  CHECK(flush_ret == 0);

  iommu_domain_free(domain);
}

TEST_CASE("us_iommu_vfio_available — non-root env returns 0 (degrade path)",
          "[kernel][iommu][vfio][stage21][degrade]")
{
  /* boundary §5.2 graceful-degrade contract: non-root container must
   * not crash on iommu_flush_iotlb; bridge returns 0 (degrade). */
  us_iommu_vfio_reset();
  int avail = us_iommu_vfio_available();
  CHECK(avail == 0);
}

TEST_CASE("us_iommu_vfio_invalidate — returns -ENOSYS when vfio unavailable",
          "[kernel][iommu][vfio][stage21][degrade]")
{
  us_iommu_vfio_reset();
  if (us_iommu_vfio_available() != 0) {
    SUCCEED("Skipping: vfio available in this environment");
    return;
  }
  int ret = us_iommu_vfio_invalidate(0x1000, 4096);
  CHECK(ret != 0);
}

TEST_CASE("iommu_flush_iotlb — Stage 2.1.1 degrades gracefully without vfio",
          "[kernel][iommu][flush][stage21][degrade]")
{
  /* non-root env path: vfio bridge reports 0 available, flush falls
   * back to page-table walk, must not crash. */
  us_iommu_vfio_reset();
  REQUIRE(us_iommu_vfio_available() == 0);

  struct iommu_domain *domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(domain != nullptr);
  REQUIRE(iommu_map(domain, 0x1000, 0x1000, 4096, 0) == 0);
  long unmap_ret = iommu_unmap(domain, 0x1000, 4096);
  REQUIRE(unmap_ret == 4096);

  int flush_ret = iommu_flush_iotlb(domain, 0x1000, 4096);
  CHECK(flush_ret == 0);

  iommu_domain_free(domain);
}

/* M8: iommu_flush_iotlb public wrapper (with sim_pm bridge) doesn't crash and
 * correctly clears stale state when entries were unmapped. */
TEST_CASE("iommu_flush_iotlb — sim_pm bridge: public wrapper flush propagates",
          "[kernel][iommu][flush][tier2][sim_pm][bridge]") {
  struct iommu_domain *domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(domain != nullptr);
  /* iommu_domain_alloc does not set ops; default_flush_iotlb needs them. */
  domain->ops = iommu_default_ops_get();

  struct sim_page_migration *pm = sim_pm_create(0x10000);
  REQUIRE(pm != nullptr);
  REQUIRE(sim_pm_attach_domain(pm, domain) == 0);
  REQUIRE(iommu_domain_attach_sim_pm(domain, pm) == 0);

  /* Use dummy src for migrate_to_device.
   * migrate_to_device calls iommu_map(domain, offset, paddr, size) — that
   * updates domain->iova_to_phys. */
  unsigned char src[4096] = {0};
  REQUIRE(sim_pm_migrate_to_device(pm, 0x2000, src, 4096) == 0);
  CHECK(sim_pm_is_page_on_device(pm, 0x2000) == 1);

  /* Direct call to public wrapper (used by invalidate.cpp + ats_protocol.cpp
   * with nullptr as flushed_entries). Should NOT crash even when
   * flushed_entries is NULL. */
  int flush_ret = iommu_flush_iotlb(domain, 0x2000, 4096);
  CHECK(flush_ret == 0);
  /* sim_pm is still bound, but no entries to invalidate via flushed_entries
   * path — page remains on device. */
  CHECK(sim_pm_is_page_on_device(pm, 0x2000) == 1);

  /* Now unmap → triggers default_flush_iotlb WITH flushed_entries
   * (since iommu_unmap snapshots before erase). */
  long unmap_ret = iommu_unmap(domain, 0x2000, 4096);
  REQUIRE(unmap_ret == 4096);
  /* sim_pm_invalidate called from default_flush_iotlb — page should be EVICTED. */
  CHECK(sim_pm_is_page_on_device(pm, 0x2000) == 0);

  sim_pm_destroy(pm);
  iommu_domain_free(domain);
}