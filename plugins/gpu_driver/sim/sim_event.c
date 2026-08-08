/*
 * sim_event.c — Sim-layer KFD event signaling (C implementation)
 *
 * Phase B.4 day-1 stub: atomic counter + validation.
 * Phase C/E: real event page write (amdgpu_kfd_event_page_set).
 *
 * Naming conventions per design.md §Naming:
 *   - Functions:  sim_<feature>_<verb>
 *   - No STL — pure C11 with stdatomic.h
 */

#include "sim_event.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ==== Per-process event page registry ==== */
#define SIM_EVENT_PAGE_MAX_PIDS  256

typedef struct {
  u32     pid;
  void*   page;
  int     in_use;
} sim_event_page_entry_t;

static sim_event_page_entry_t sim_event_pages_[SIM_EVENT_PAGE_MAX_PIDS];
static atomic_int sim_event_page_count_ = 0;

static sim_event_page_entry_t* sim_event_page_find_lockless_(u32 pid) {
  for (int i = 0; i < SIM_EVENT_PAGE_MAX_PIDS; i++) {
    if (sim_event_pages_[i].in_use && sim_event_pages_[i].pid == pid) {
      return &sim_event_pages_[i];
    }
  }
  return NULL;
}

int sim_event_page_alloc(u32 pid, void** page_ptr) {
  if (pid == 0) return -EINVAL;
  if (page_ptr == NULL) return -EINVAL;
  if (sim_event_page_find_lockless_(pid) != NULL) return -EEXIST;

  void* page = aligned_alloc(8, SIM_EVENT_PAGE_SIZE);
  if (page == NULL) return -ENOMEM;
  memset(page, 0, SIM_EVENT_PAGE_SIZE);

  for (int i = 0; i < SIM_EVENT_PAGE_MAX_PIDS; i++) {
    if (!sim_event_pages_[i].in_use) {
      sim_event_pages_[i].pid = pid;
      sim_event_pages_[i].page = page;
      sim_event_pages_[i].in_use = 1;
      atomic_fetch_add(&sim_event_page_count_, 1);
      *page_ptr = page;
      return 0;
    }
  }
  free(page);
  return -ENOMEM;
}

int sim_event_page_free(u32 pid) {
  sim_event_page_entry_t* e = sim_event_page_find_lockless_(pid);
  if (e == NULL) return -ENOENT;
  free(e->page);
  e->page = NULL;
  e->pid = 0;
  e->in_use = 0;
  atomic_fetch_sub(&sim_event_page_count_, 1);
  return 0;
}

int sim_event_page_get(u32 pid, void** page_ptr) {
  if (page_ptr == NULL) return -EINVAL;
  sim_event_page_entry_t* e = sim_event_page_find_lockless_(pid);
  *page_ptr = (e != NULL) ? e->page : NULL;
  return 0;
}

/* ==== Original sim_signal_event ==== */
static atomic_int sim_signal_count_ = 0;

int sim_signal_event(u32 pasid, u32 event_id, u64 events) {
  if (pasid > 0xFFFF) return -EINVAL;
  if (event_id > SIM_EVENT_SLOTS) return -EINVAL;
  if (events == 0) return -EINVAL;
  atomic_fetch_add(&sim_signal_count_, 1);

  if (pasid != 0) {
    void* page = NULL;
    if (sim_event_page_get(pasid, &page) == 0 && page != NULL) {
      uint64_t* slots = (uint64_t*)page;
      uint32_t slot_idx = event_id / 64;
      uint32_t bit_off  = event_id % 64;
      if (slot_idx < SIM_EVENT_PAGE_SLOTS) {
        slots[slot_idx] |= (events << bit_off);
      }
    }
  }
  return 0;
}

int sim_signal_event_count(void) {
  return atomic_load(&sim_signal_count_);
}
