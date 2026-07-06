/*
 * test_mmu_notifier_callback_runtime_standalone.cpp — Stage 1.4 Tier-2 §4
 *
 * Verifies that mmu_notifier callback body is actually wired up:
 * 1. Registering an mmu_notifier via iommu_invalidate_register_notifier_internal
 *    causes the callback to be invoked when fault_inject_page_fault triggers
 *    invalidation.
 * 2. This proves the full chain works:
 *      fault_inject -> mmu_notifier_dispatch_all_invalidate_start
 *      -> mn->ops->invalidate_range_start (user callback)
 * 3. Per design.md D2: minimal viable callback (no new sim primitives).
 *
 * Per kfd-portability-boundary.md §3.3 Tier-2 scope: callback body must
 * actually be invoked (was TODO stage-1.3, now Tier-2 penetrated).
 */

#include <catch_amalgamated.hpp>

extern "C" {
#include <linux_compat/mmu_notifier.h>
#include <linux_compat/iommu/iommu.h>
#include <linux_compat/iommu/iommu_domain.h>
#include <kernel/uvm/mmu_notifier_internal.h>
#include <kernel/uvm/mm_shim.h>

int fault_inject_page_fault(struct mm_struct *mm, unsigned long addr,
                             unsigned long *pfn_out);

int iommu_invalidate_register_notifier_internal(struct iommu_domain *d,
                                                 struct mmu_notifier *mnp);
int iommu_invalidate_unregister_notifier_internal(struct iommu_domain *d,
                                                   struct mmu_notifier *mnp);
}

namespace {

struct callback_log {
  int invalidate_start_calls = 0;
  unsigned long last_start = 0;
  unsigned long last_end = 0;
};

int counting_invalidate_start(struct mmu_notifier *mn,
                              struct mm_struct *mm,
                              unsigned long start, unsigned long end) {
  (void)mm;
  auto* log = static_cast<callback_log*>(mn->priv);
  if (log) {
    log->invalidate_start_calls++;
    log->last_start = start;
    log->last_end = end;
  }
  return 0;
}

void counting_invalidate_end(struct mmu_notifier*, struct mm_struct*,
                              unsigned long, unsigned long) {}
void counting_release(struct mmu_notifier*, struct mm_struct*) {}

const struct mmu_notifier_ops g_counting_ops = {
  .invalidate_range_start = counting_invalidate_start,
  .invalidate_range_end   = counting_invalidate_end,
  .release                = counting_release,
};

}  // namespace

TEST_CASE("mmu_notifier callback body — fires on fault_inject invalidation",
          "[kernel][mmu_notifier][callback][tier2]")
{
  struct mm_struct mm = { .id = 9001 };
  callback_log log = {};
  struct mmu_notifier mn = {};
  mn.ops = &g_counting_ops;
  mn.priv = &log;

  /* Register via the Tier-2 path: iommu_invalidate_register_notifier_internal
   * should call mmu_notifier_register under the hood.
   *
   * Issue #21 contract: use iommu_domain_alloc() to get a domain whose priv
   * is a real iommu_domain_state*, then iommu_domain_attach_mm() to wire the
   * mm into state->mm. The legacy "domain->priv = &mm" form is no longer
   * valid — Stage 2.1.2 reads state->mm_shim from priv and would dereference
   * past the 8-byte mm_struct, producing UB that compiles cleanly but
   * segfaults at runtime (clang+g++ exposed it first). */
  struct iommu_domain *domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(domain != nullptr);
  REQUIRE(iommu_domain_attach_mm(domain, &mm) == 0);
  /* Without iommu_domain_attach_mm_shim, state->mm_shim stays nullptr, so the
   * library preserves mn.priv (which is &log) — the user callback contract
   * is preserved. */

  int reg_ret = iommu_invalidate_register_notifier_internal(domain, &mn);
  if (reg_ret != 0) {
    /* If registration still doesn't bridge through the framework, mark
     * Tier-2 incomplete but still test the direct framework path. */
    WARN("iommu_invalidate_register_notifier_internal returned "
         << reg_ret << " (Tier-2 not fully wired)");
  }

  /* Trigger invalidation via fault_inject (the standard kernel path). */
  unsigned long pfn = 0;
  int fault_ret = fault_inject_page_fault(&mm, 0x10000, &pfn);
  CHECK(fault_ret == 0);

  /* If the Tier-2 wiring works, the callback fires.  Otherwise this is
   * a pre-existing Tier-1 limitation. */
  CHECK(log.invalidate_start_calls >= 1);

  /* Unregister to keep state clean for subsequent tests. */
  iommu_invalidate_unregister_notifier_internal(domain, &mn);
  iommu_domain_free(domain);
}

TEST_CASE("mmu_notifier — direct framework registration fires callback",
          "[kernel][mmu_notifier][callback][framework]")
{
  /* This test bypasses the iommu_ wrapper and directly registers via
   * the mmu_notifier framework, proving the dispatch chain works
   * independent of the Tier-2 wiring. */
  struct mm_struct mm = { .id = 9002 };
  callback_log log = {};
  struct mmu_notifier mn = {};
  mn.ops = &g_counting_ops;
  mn.priv = &log;

  int reg_ret = mmu_notifier_register(&mn, &mm);
  CHECK(reg_ret == 0);

  unsigned long pfn = 0;
  int fault_ret = fault_inject_page_fault(&mm, 0x20000, &pfn);
  CHECK(fault_ret == 0);

  CHECK(log.invalidate_start_calls == 1);
  CHECK(log.last_start == 0x20000);

  mmu_notifier_unregister(&mn);
}

TEST_CASE("us_mm_shim — PID + VMA list operations (Stage 2.1.2)",
          "[kernel][mm_shim][stage21][pid]")
{
  struct us_mm_shim shim;
  us_mm_shim_init(&shim, 0xCAFE);

  CHECK(shim.pid == 0xCAFE);
  CHECK(shim.vma_count == 0);

  CHECK(us_mm_shim_register_vma(&shim, 0x10000, 0x20000, 0x1) == 0);
  CHECK(us_mm_shim_register_vma(&shim, 0x30000, 0x40000, 0x3) == 0);
  CHECK(shim.vma_count == 2);

  unsigned long start = 0, end = 0;
  CHECK(us_mm_shim_find_vma(&shim, 0x15000, &start, &end) == 0);
  CHECK(start == 0x10000);
  CHECK(end == 0x20000);

  CHECK(us_mm_shim_find_vma(&shim, 0x99999, &start, &end) != 0);

  CHECK(us_mm_shim_unregister_vma(&shim, 0x10000, 0x20000) == 0);
  CHECK(shim.vma_count == 1);
  CHECK(us_mm_shim_unregister_vma(&shim, 0x10000, 0x20000) != 0);

  CHECK(us_mm_shim_register_vma(&shim, 0x1000, 0x1000, 0) != 0);
  CHECK(us_mm_shim_register_vma(nullptr, 0x1000, 0x2000, 0) != 0);
}