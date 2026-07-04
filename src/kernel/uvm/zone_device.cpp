/*
 * zone_device.cpp — Device zone (SPM VMA) minimal implementation
 *
 * Stage 1.3 UVM/HMM §3.5: spm vma + page state machine.
 * Per design.md Decision 5: minimal zone_device.
 *
 * Architecture: ① 内核环境模拟层 (Kernel Environment Simulation)
 */

#include <linux_compat/mmu_notifier.h>
#include <kernel/uvm/mmu_notifier_internal.h>

#include <map>

/* Forward: page_state_machine API */
extern "C" {
enum page_state {
  PAGE_STATE_CPU       = 0,
  PAGE_STATE_GPU       = 1,
  PAGE_STATE_MIGRATING = 2,
};
int page_state_transition(enum page_state *current, enum page_state target);
const char *page_state_name(enum page_state s);
}

namespace {

struct ZoneDevice {
  struct mm_struct *mm;
  unsigned long start;
  unsigned long end;
  std::map<unsigned long, enum page_state> pages;
};

} // anonymous namespace

extern "C" {

struct zone_device *zone_device_create(struct mm_struct *mm,
                                        unsigned long start,
                                        unsigned long end) {
  if (!mm)
    return nullptr;
  if (start >= end)
    return nullptr;

  auto *zd = new ZoneDevice{};
  zd->mm    = mm;
  zd->start = start;
  zd->end   = end;
  return reinterpret_cast<struct zone_device *>(zd);
}

void zone_device_destroy(struct zone_device *zd) {
  if (zd)
    delete reinterpret_cast<ZoneDevice *>(zd);
}

int zone_device_get_page_state(struct zone_device *zd,
                                unsigned long addr,
                                enum page_state *out) {
  if (!zd || !out)
    return -EINVAL;

  auto *z = reinterpret_cast<ZoneDevice *>(zd);
  if (addr < z->start || addr >= z->end)
    return -EFAULT;

  auto it = z->pages.find(addr);
  if (it == z->pages.end())
    return -EFAULT;

  *out = it->second;
  return 0;
}

int zone_device_set_page_state(struct zone_device *zd,
                                unsigned long addr,
                                enum page_state target) {
  if (!zd)
    return -EINVAL;

  auto *z = reinterpret_cast<ZoneDevice *>(zd);
  if (addr < z->start || addr >= z->end)
    return -EFAULT;

  /* Get current state; if not tracked yet, allow any initial state */
  auto it = z->pages.find(addr);
  if (it == z->pages.end()) {
    z->pages[addr] = target;
    return 0;
  }

  int ret = page_state_transition(&it->second, target);
  if (ret != 0)
    return ret;

  return 0;
}

} // extern "C"