/**
 * @file sim_proxy.h
 * @brief µ-thin forward-prototype header for ①→③ sim C ABI bridging
 *
 * This header exists per ADR-063 D4 Path X to prevent ADR-027 violations:
 * - ① (kernel env) code includes THIS header, NOT ③'s page_migration.h
 * - Only extern "C" function prototypes + forward struct decls are exposed
 * - No implementation details of sim layer are leaked
 *
 * Linker resolves: sim_pm_* symbols come from plugins/gpu_driver/sim/ at link time.
 */

#ifndef KERNEL_SIM_PROXY_H
#define KERNEL_SIM_PROXY_H

#include <stddef.h>  /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration — opaque, no internals exposed */
struct sim_page_migration;

/* Forward declaration for IOMMU integration (opaque, ADR-063 D2) */
struct iommu_domain;

/* IOMMU map/unmap — extern C prototypes for ③→① IOMMU sync (per ADR-063 D4) */
int  iommu_map(struct iommu_domain *domain, unsigned long iova,
               unsigned long paddr, unsigned long size, int prot);
long iommu_unmap(struct iommu_domain *domain, unsigned long iova,
                 unsigned long size);

/**
 * Bind an IOMMU domain to a sim page migration instance.
 * After attachment, migrate_to_device/system will synchronize with
 * the domain's IOMMU page table via iommu_map/iommu_unmap.
 */
int  sim_pm_attach_domain(struct sim_page_migration *pm, void *domain);

/**
 * Invalidate a page in the sim page migration cache.
 * Transitions PAGE_DIRTY→PAGE_EVICTED or PAGE_CLEAN→PAGE_EVICTED.
 * Called from IOTLB flush path (default_flush_iotlb) after each evicted IOVA.
 */
void sim_pm_invalidate(struct sim_page_migration *pm, unsigned long offset);

/**
 * Returns non-zero if the page at @p offset is marked dirty.
 */
int  sim_pm_is_page_dirty(struct sim_page_migration *pm, unsigned long offset);

/**
 * Mark a page as dirty (caller-injected callback path).
 * Page must be currently on device (PAGE_CLEAN or PAGE_DIRTY state).
 */
void sim_pm_mark_dirty(struct sim_page_migration *pm, unsigned long offset);

#ifdef __cplusplus
}
#endif

#endif /* KERNEL_SIM_PROXY_H */
