/*
 * page_migration.cpp — Simulated device ↔ system memory migration
 *
 * Stage 1.3 UVM/HMM §4.2: simulates device memory ↔ system memory
 * page migration using kernel layer migrate_to_ram/migrate_to_dev.
 *
 * Architecture: ③ 硬件模拟层 (Hardware Simulation)
 * Per ADR-036 three-way separation.
 */

#include <linux_compat/mmu_notifier.h>
#include <kernel/sim_proxy.h>

#include <cerrno>
#include <cstring>
#include <vector>
#include <map>

namespace {

constexpr unsigned long INVALID_PFN_VALUE = ~0UL;

enum PageState { PAGE_CLEAN = 0, PAGE_DIRTY = 1, PAGE_EVICTED = 2 };

struct SimPageMigration {
  unsigned long device_mem_size;
  std::vector<unsigned char> device_memory;
  std::map<unsigned long, PageState> page_state;
  std::map<unsigned long, unsigned long> page_table;
  int migration_count = 0;
  void *iommu_domain = nullptr;
  std::map<unsigned long, bool> page_dirty;
};

} // anonymous namespace

extern "C" {

struct sim_page_migration *sim_pm_create(unsigned long device_mem_size) {
  if (device_mem_size == 0)
    return nullptr;

  auto *pm = new SimPageMigration{};
  pm->device_mem_size = device_mem_size;
  pm->device_memory.resize(device_mem_size, 0);
  return reinterpret_cast<struct sim_page_migration *>(pm);
}

void sim_pm_destroy(struct sim_page_migration *pm) {
  delete reinterpret_cast<SimPageMigration *>(pm);
}

int sim_pm_migrate_to_device(struct sim_page_migration *pm,
                              unsigned long offset,
                              const void *src, unsigned long size) {
  if (!pm || !src)
    return -EINVAL;

  auto *p = reinterpret_cast<SimPageMigration *>(pm);
  if (offset + size > p->device_mem_size)
    return -EFAULT;

  memcpy(p->device_memory.data() + offset, src, size);
  p->page_state[offset] = PAGE_CLEAN;
  p->page_table[offset] = offset / 4096;
  p->migration_count++;
  if (p->iommu_domain) {
    iommu_map(static_cast<struct iommu_domain*>(p->iommu_domain), offset,
              p->page_table[offset] << 12, size, 0);
  }
  return 0;
}

int sim_pm_migrate_to_system(struct sim_page_migration *pm,
                              unsigned long offset,
                              void *dst, unsigned long size) {
  if (!pm || !dst)
    return -EINVAL;

  auto *p = reinterpret_cast<SimPageMigration *>(pm);
  if (offset + size > p->device_mem_size)
    return -EFAULT;

  if (p->iommu_domain)
    iommu_unmap(static_cast<struct iommu_domain*>(p->iommu_domain), offset, size);

  memcpy(dst, p->device_memory.data() + offset, size);
  p->page_state[offset] = PAGE_EVICTED;
  p->page_table.erase(offset);
  p->migration_count++;
  return 0;
}

int sim_pm_get_migration_count(struct sim_page_migration *pm) {
  if (!pm)
    return 0;
  return reinterpret_cast<SimPageMigration *>(pm)->migration_count;
}

int sim_pm_is_page_on_device(struct sim_page_migration *pm,
                              unsigned long offset) {
  if (!pm)
    return 0;
  auto *p = reinterpret_cast<SimPageMigration *>(pm);
  auto it = p->page_state.find(offset);
  return (it != p->page_state.end() && it->second != PAGE_EVICTED) ? 1 : 0;
}

unsigned long sim_pm_lookup_pfn(struct sim_page_migration *pm,
                                 unsigned long offset) {
  if (!pm)
    return INVALID_PFN_VALUE;
  auto *p = reinterpret_cast<SimPageMigration *>(pm);
  auto it = p->page_table.find(offset);
  if (it == p->page_table.end())
    return INVALID_PFN_VALUE;
  return it->second;
}

/* ADR-063 D2: 4 new APIs for 3-state page migration + IOMMU integration */

int sim_pm_attach_domain(struct sim_page_migration *pm, void *domain) {
  if (!pm)
    return -EINVAL;
  reinterpret_cast<SimPageMigration *>(pm)->iommu_domain = domain;
  return 0;
}

void sim_pm_invalidate(struct sim_page_migration *pm, unsigned long offset) {
  if (!pm)
    return;
  auto *p = reinterpret_cast<SimPageMigration *>(pm);
  auto it = p->page_state.find(offset);
  if (it != p->page_state.end() && it->second != PAGE_EVICTED) {
    it->second = PAGE_EVICTED;
    p->page_table.erase(offset);
    p->page_dirty.erase(offset);
  }
}

int sim_pm_is_page_dirty(struct sim_page_migration *pm, unsigned long offset) {
  if (!pm)
    return 0;
  auto *p = reinterpret_cast<SimPageMigration *>(pm);
  auto it = p->page_dirty.find(offset);
  return (it != p->page_dirty.end() && it->second) ? 1 : 0;
}

void sim_pm_mark_dirty(struct sim_page_migration *pm, unsigned long offset) {
  if (!pm)
    return;
  auto *p = reinterpret_cast<SimPageMigration *>(pm);
  auto it = p->page_state.find(offset);
  if (it != p->page_state.end() && it->second != PAGE_EVICTED) {
    it->second = PAGE_DIRTY;
    p->page_dirty[offset] = true;
  }
}

} // extern "C"