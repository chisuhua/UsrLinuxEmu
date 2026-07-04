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

#include <cstring>
#include <vector>
#include <map>

namespace {

constexpr unsigned long INVALID_PFN_VALUE = ~0UL;

struct SimPageMigration {
  unsigned long device_mem_size;
  std::vector<unsigned char> device_memory;
  std::map<unsigned long, bool> page_on_device;
  std::map<unsigned long, unsigned long> page_table;
  int migration_count = 0;
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
  p->page_on_device[offset] = true;
  p->page_table[offset] = offset / 4096;
  p->migration_count++;
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

  memcpy(dst, p->device_memory.data() + offset, size);
  p->page_on_device[offset] = false;
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
  auto it = p->page_on_device.find(offset);
  return (it != p->page_on_device.end() && it->second) ? 1 : 0;
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

} // extern "C"