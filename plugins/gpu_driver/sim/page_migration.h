/*
 * page_migration.h — Simulated device ↔ system memory migration (C ABI)
 *
 * Stage 1.3 UVM/HMM §4.2: simulates device memory ↔ system memory
 * page migration using kernel layer migrate_to_ram/migrate_to_dev.
 *
 * Architecture: ③ 硬件模拟层 (Hardware Simulation)
 * Per ADR-036 three-way separation.
 *
 * This header exposes the C ABI used by tests and downstream Tier-2
 * handlers. The C++ implementation lives in page_migration.cpp.
 */

#ifndef SIM_PAGE_MIGRATION_H
#define SIM_PAGE_MIGRATION_H

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle — do not access fields directly */
struct sim_page_migration;

#define INVALID_PFN (~0UL)

struct sim_page_migration *sim_pm_create(unsigned long device_mem_size);
void                       sim_pm_destroy(struct sim_page_migration *pm);
int                        sim_pm_migrate_to_device(struct sim_page_migration *pm,
                                                    unsigned long offset,
                                                    const void *src,
                                                    unsigned long size);
int                        sim_pm_migrate_to_system(struct sim_page_migration *pm,
                                                    unsigned long offset,
                                                    void *dst,
                                                    unsigned long size);
int                        sim_pm_get_migration_count(struct sim_page_migration *pm);
int                        sim_pm_is_page_on_device(struct sim_page_migration *pm,
                                                  unsigned long offset);
unsigned long              sim_pm_lookup_pfn(struct sim_page_migration *pm,
                                              unsigned long offset);

int                        sim_pm_attach_domain(struct sim_page_migration *pm,
                                               void *domain);
void                       sim_pm_invalidate(struct sim_page_migration *pm,
                                             unsigned long offset);
int                        sim_pm_is_page_dirty(struct sim_page_migration *pm,
                                                unsigned long offset);
void                       sim_pm_mark_dirty(struct sim_page_migration *pm,
                                             unsigned long offset);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* SIM_PAGE_MIGRATION_H */