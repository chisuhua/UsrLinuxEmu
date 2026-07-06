/*
 * test_iommu_priv_contract_regression_standalone.cpp
 *
 * Issue #21 regression test — iommu priv contract lock-in.
 *
 * Background (per d09d6bf commit message):
 *   Stage 2.1.2 added mm_shim propagation in
 *   `iommu_invalidate_register_notifier_internal`.  The previous version of
 *   that function read domain->priv as `mm_struct*` (legacy 8-byte cast).
 *   Once mm_shim reinterpreted the same pointer as `iommu_domain_state*`,
 *   clang+g++ reliably surfaced the UB as a SEGFAULT inside test_mmu_notifier_
 *   callback_runtime_standalone (and intermittently in test_gpu_plugin via
 *   the next test on the CTest list).
 *
 *   The fix introduces a canonical contract:
 *     - `domain->priv` is `iommu_domain_state*` (always).
 *     - `iommu_domain_attach_mm(domain, mm)` puts `mm` into `state->mm`.
 *     - The mm_shim path only overwrites `mnp->priv` when `state->mm_shim`
 *       is non-null (so user-set `mn.priv` is preserved in the absence
 *       of mm_shim propagation).
 *
 * This test locks in that contract via targeted assertions:
 *   1. After `iommu_invalidate_register_notifier_internal` runs,
 *      `mn->priv` MUST equal the value we set (the legacy bug overwrites
 *      it with garbage read from past the 8-byte mm_struct boundary).
 *   2. `iommu_domain_attach_mm` returns 0 for valid inputs and -EINVAL
 *      for null inputs.
 *   3. Without `iommu_domain_attach_mm`, registration returns non-zero
 *      because `state->mm` is null (the legacy code skipped this check;
 *      the legacy read of `mm = static_cast<mm_struct*>(d->priv)` passed
 *      the null test because the pointer wasn't zero — it pointed into
 *      state memory).
 *   4. fault_inject still fires our callback with the attached mm.
 *
 * If this test ever SEGFAULTs or fails in clang+g++ builds, the iommu
 * priv contract has regressed — STOP and restore the Issue #21 fix
 * (see d09d6bf for the canonical patch).
 */

#include "catch_amalgamated.hpp"

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

TEST_CASE("Issue #21 lock-in: iommu priv contract preserves mn->priv when no mm_shim",
          "[kernel][iommu][regression][issue-21]")
{
  /*
   * The KEY invariant: the user's mn.priv pointer survives registration
   * unchanged when state->mm_shim is null.  The buggy code would write
   * a garbage pointer (whatever was past the 8-byte mm_struct boundary
   * in the caller-provided priv memory), turning downstream callbacks
   * that read mn->priv into SEGVs.
   */
  struct mm_struct mm = { .id = 0xABCDEF };
  callback_log log = {};
  struct mmu_notifier mn = {};
  mn.ops = &g_counting_ops;
  mn.priv = &log;

  struct iommu_domain* domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(domain != nullptr);
  REQUIRE(iommu_domain_attach_mm(domain, &mm) == 0);

  /* Pin mn.priv before registration so we can detect any post-call write. */
  void* priv_before = mn.priv;
  REQUIRE(priv_before == static_cast<void*>(&log));

  int reg_ret = iommu_invalidate_register_notifier_internal(domain, &mn);
  INFO("reg_ret = " << reg_ret);
  CHECK(reg_ret == 0);

  /* The fixed code only writes mn.priv when state->mm_shim != null.  Since
   * we did NOT call iommu_domain_attach_mm_shim, state->mm_shim is null,
   * so mn.priv must equal what we set.  The legacy code wrote whatever
   * `state->mm_shim` resolved to after a bogus reinterpret — a different
   * pointer. */
  CHECK(mn.priv == priv_before);
  CHECK(mn.priv == static_cast<void*>(&log));

  /* And the callback must fire when we trigger invalidation. */
  unsigned long pfn = 0;
  int fault_ret = fault_inject_page_fault(&mm, 0x4000, &pfn);
  CHECK(fault_ret == 0);
  CHECK(log.invalidate_start_calls == 1);
  CHECK(log.last_start == 0x4000);

  iommu_invalidate_unregister_notifier_internal(domain, &mn);
  iommu_domain_free(domain);
}

TEST_CASE("Issue #21 lock-in: register fails when state->mm is null",
          "[kernel][iommu][regression][issue-21]")
{
  /*
   * The fixed code returns EINVAL when state->mm is null because the
   * canonical contract requires callers to use iommu_domain_attach_mm().
   * The legacy code did `mm = static_cast<mm_struct*>(d->priv)` which
   * yielded a non-null but garbage pointer, then passed it to
   * mmu_notifier_register, segfaulting on first deref.  This test makes
   * that divergence observable: with the fix, registration cleanly
   * returns non-zero when state->mm is unset.
   */
  struct mmu_notifier mn = {};
  mn.ops = &g_counting_ops;
  /* mn.priv intentionally left null — anything would be clobbered in
   * the buggy variant before mmu_notifier_register gets a chance. */

  struct iommu_domain* domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(domain != nullptr);
  /* Deliberately skip iommu_domain_attach_mm. */

  int reg_ret = iommu_invalidate_register_notifier_internal(domain, &mn);
  CHECK(reg_ret != 0);  /* MUST refuse — no mm to register against. */

  iommu_domain_free(domain);
}

TEST_CASE("Issue #21 lock-in: iommu_domain_attach_mm input validation",
          "[kernel][iommu][regression][issue-21]")
{
  /*
   * The new helper must reject null inputs rather than crash.  Lock in
   * the contract that all iommu priv setters do null-checks.
   */
  struct iommu_domain* domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(domain != nullptr);

  struct mm_struct mm = { .id = 1 };

  CHECK(iommu_domain_attach_mm(domain, &mm) == 0);
  CHECK(iommu_domain_attach_mm(nullptr, &mm) != 0);
  CHECK(iommu_domain_attach_mm(domain, nullptr) != 0);
  /* Unallocated domain: priv is null */
  struct iommu_domain zero_domain = {};
  CHECK(iommu_domain_attach_mm(&zero_domain, &mm) != 0);

  iommu_domain_free(domain);
}
