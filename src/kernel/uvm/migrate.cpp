/*
 * migrate.cpp — Page migration user-space simulation
 *
 * Stage 1.3 UVM/HMM §3.3: migrate_to_ram / migrate_to_dev.
 * Mirrors Linux kernel mm/migrate.c behavior for device memory migration.
 *
 * Architecture: ① 内核环境模拟层 (Kernel Environment Simulation)
 * Per ADR-036 three-way separation.
 */

#include <linux_compat/mmu_notifier.h>

#include <cstring>

extern "C" {

int migrate_to_ram(void *dev_page, void *sys_page, unsigned long size) {
  if (!dev_page || !sys_page)
    return -EINVAL;
  if (size > 0)
    memcpy(sys_page, dev_page, size);
  return 0;
}

int migrate_to_dev(void *sys_page, void *dev_page, unsigned long size) {
  if (!sys_page || !dev_page)
    return -EINVAL;
  if (size > 0)
    memcpy(dev_page, sys_page, size);
  return 0;
}

} // extern "C"