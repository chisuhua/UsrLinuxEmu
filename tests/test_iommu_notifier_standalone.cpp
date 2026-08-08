/*
 * test_iommu_notifier_standalone.cpp — complete-mmu-notifier-callback
 *
 * Acceptance criteria (per proposal.md):
 *   1. register + trigger invalidate_range
 *   2. multiple notifiers registered
 *   3. release triggers cleanup
 *   4. unregister correctly removes
 */

#include <catch_amalgamated.hpp>
#include <cstring>
#include <linux_compat/types.h>
#include <linux_compat/iommu/iommu.h>
#include <linux_compat/iommu/iommu_domain.h>
#include <linux_compat/mmu_notifier.h>

extern "C" {
struct iommu_domain *iommu_domain_alloc(enum iommu_domain_type type);
void iommu_domain_free(struct iommu_domain *domain);
int  iommu_domain_attach_mm(struct iommu_domain *domain, struct mm_struct *mm);
int  iommu_map(struct iommu_domain *domain, unsigned long iova, phys_addr_t paddr, size_t size, int prot);
long iommu_unmap(struct iommu_domain *domain, unsigned long iova, size_t size);
int  mmu_notifier_register(struct mmu_notifier *mnp, struct mm_struct *mm);
void mmu_notifier_unregister(struct mmu_notifier *mnp);

int  iommu_invalidate_register_notifier_internal(struct iommu_domain *d, struct mmu_notifier *mnp);
int  iommu_invalidate_unregister_notifier_internal(struct iommu_domain *d, struct mmu_notifier *mnp);
void mmu_notifier_dispatch_release(struct mmu_notifier *mnp, struct mm_struct *mm);
}

struct callback_count {
  int invalidate_start_count;
  int invalidate_end_count;
  int release_count;
};

static int cb_invalidate_start(struct mmu_notifier *mn, struct mm_struct *mm,
                               unsigned long start, unsigned long end) {
  auto* c = static_cast<callback_count*>(mn->priv);
  if (c) c->invalidate_start_count++;
  return 0;
}
static void cb_invalidate_end(struct mmu_notifier *mn, struct mm_struct *mm,
                              unsigned long start, unsigned long end) {
  auto* c = static_cast<callback_count*>(mn->priv);
  if (c) c->invalidate_end_count++;
}
static void cb_release(struct mmu_notifier *mn, struct mm_struct *mm) {
  auto* c = static_cast<callback_count*>(mn->priv);
  if (c) c->release_count++;
}

static struct mmu_notifier_ops test_ops = {
  cb_invalidate_start,
  cb_invalidate_end,
  cb_release,
  nullptr,
  nullptr
};

TEST_CASE("iommu_unmap triggers invalidate_range on registered notifier",
          "[iommu_notifier]") {
  callback_count cnt = {0, 0, 0};
  struct mm_struct mm = { .id = 1 };

  struct iommu_domain *domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(domain != nullptr);
  REQUIRE(iommu_domain_attach_mm(domain, &mm) == 0);

  struct mmu_notifier mnp = {};
  mnp.ops = &test_ops;
  mnp.priv = &cnt;

  REQUIRE(iommu_invalidate_register_notifier_internal(domain, &mnp) == 0);

  REQUIRE(iommu_map(domain, 0x1000, 0x1000, 0x1000, 0) == 0);
  REQUIRE(iommu_unmap(domain, 0x1000, 0x1000) >= 0);
  REQUIRE(cnt.invalidate_start_count == 1);
  REQUIRE(cnt.invalidate_end_count == 1);

  iommu_invalidate_unregister_notifier_internal(domain, &mnp);
  iommu_domain_free(domain);
}

TEST_CASE("multiple notifiers registered — all receive invalidate_range",
          "[iommu_notifier]") {
  callback_count cnt1 = {0, 0, 0};
  callback_count cnt2 = {0, 0, 0};
  struct mm_struct mm = { .id = 2 };

  struct iommu_domain *domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(domain != nullptr);
  REQUIRE(iommu_domain_attach_mm(domain, &mm) == 0);

  struct mmu_notifier mnp1 = {};
  mnp1.ops = &test_ops;
  mnp1.priv = &cnt1;

  struct mmu_notifier mnp2 = {};
  mnp2.ops = &test_ops;
  mnp2.priv = &cnt2;

  REQUIRE(iommu_invalidate_register_notifier_internal(domain, &mnp1) == 0);
  REQUIRE(iommu_invalidate_register_notifier_internal(domain, &mnp2) == 0);

  REQUIRE(iommu_map(domain, 0x2000, 0x2000, 0x1000, 0) == 0);
  REQUIRE(iommu_unmap(domain, 0x2000, 0x1000) >= 0);
  REQUIRE(cnt1.invalidate_start_count == 1);
  REQUIRE(cnt1.invalidate_end_count == 1);
  REQUIRE(cnt2.invalidate_start_count == 1);
  REQUIRE(cnt2.invalidate_end_count == 1);

  iommu_invalidate_unregister_notifier_internal(domain, &mnp1);
  iommu_invalidate_unregister_notifier_internal(domain, &mnp2);
  iommu_domain_free(domain);
}

TEST_CASE("release callback fires on domain free",
          "[iommu_notifier]") {
  callback_count cnt = {0, 0, 0};
  struct mm_struct mm = { .id = 3 };

  struct iommu_domain *domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(domain != nullptr);
  REQUIRE(iommu_domain_attach_mm(domain, &mm) == 0);

  struct mmu_notifier mnp = {};
  mnp.ops = &test_ops;
  mnp.priv = &cnt;

  REQUIRE(iommu_invalidate_register_notifier_internal(domain, &mnp) == 0);

  /*
   * Explicit release dispatch: simulates the MM subsystem tearing down
   * the mm (e.g., process exit). The release callback must fire.
   * Then unregister to clean up g_registry before the next test case.
   */
  mmu_notifier_dispatch_release(&mnp, &mm);
  REQUIRE(cnt.release_count == 1);

  iommu_invalidate_unregister_notifier_internal(domain, &mnp);
  iommu_domain_free(domain);
}

TEST_CASE("unregister removes notifier from domain list",
          "[iommu_notifier]") {
  callback_count cnt1 = {0, 0, 0};
  callback_count cnt2 = {0, 0, 0};
  struct mm_struct mm = { .id = 4 };

  struct iommu_domain *domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(domain != nullptr);
  REQUIRE(iommu_domain_attach_mm(domain, &mm) == 0);

  struct mmu_notifier mnp1 = {};
  mnp1.ops = &test_ops;
  mnp1.priv = &cnt1;

  struct mmu_notifier mnp2 = {};
  mnp2.ops = &test_ops;
  mnp2.priv = &cnt2;

  REQUIRE(iommu_invalidate_register_notifier_internal(domain, &mnp1) == 0);
  REQUIRE(iommu_invalidate_register_notifier_internal(domain, &mnp2) == 0);

  iommu_invalidate_unregister_notifier_internal(domain, &mnp1);

  REQUIRE(iommu_map(domain, 0x3000, 0x3000, 0x1000, 0) == 0);
  REQUIRE(iommu_unmap(domain, 0x3000, 0x1000) >= 0);

  REQUIRE(cnt1.invalidate_start_count == 0);
  REQUIRE(cnt1.invalidate_end_count == 0);
  REQUIRE(cnt2.invalidate_start_count == 1);
  REQUIRE(cnt2.invalidate_end_count == 1);

  iommu_invalidate_unregister_notifier_internal(domain, &mnp2);
  iommu_domain_free(domain);
}
