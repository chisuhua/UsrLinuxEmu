/*
 * kfd_pasid.c — KFD PASID bitmap allocator (C-12 B.1.3)
 *
 * Source: linux/drivers/gpu/drm/amd/amdkfd/kfd_pasid.c (Linux 6.12 LTS)
 *   Real implementation: idr_alloc() / idr_remove() from <linux/idr.h>.
 *   UsrLinuxEmu replacement: simple bitmap (no idr in linux_compat yet).
 *
 * Design:
 *   - Bitmap: 1024 unsigned long words = 8KB (covers PASID [0..0xFFFF])
 *   - PASID 0 reserved (never allocated)
 *   - Next-free hint pointer for O(n total) scan over all allocations
 *   - pthread_mutex_t for thread safety (standalone, no cross-deps)
 *   - Lazy init: kfd_pasid_init() idempotent; allocate auto-inits
 *   - Static state via file-static variables
 *
 * Migration to real kernel:
 *   1. Replace this entire file with upstream idr-based implementation
 *   2. Public API (kfd_pasid.h) signatures match upstream
 */
#include "kfd_pasid.h"  /* first — own header */
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#define MAX_PASID      0xFFFF          /* 65535 */
#define BITMAP_WORDS   ((MAX_PASID + 1 + 63) / 64)  /* 1024 */
#define BITMAP_BITS    (MAX_PASID + 1)

static unsigned long pasid_bitmap[BITMAP_WORDS];
static pthread_mutex_t pasid_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool pasid_initialized = false;
static bool pasid_exited = false;        /* exit() called — block alloc */
static int pasid_count = 0;              /* track allocated count */
static int next_free_hint = 1;           /* next candidate start position */

/* --- internal helpers (pasid_mutex MUST be held) --- */

static inline int test_bit(int bit) {
  int word = bit / 64;
  int off  = bit % 64;
  return (pasid_bitmap[word] >> off) & 1UL;
}

static inline void set_bit(int bit) {
  int word = bit / 64;
  int off  = bit % 64;
  pasid_bitmap[word] |= (1UL << off);
}

static inline void clear_bit(int bit) {
  int word = bit / 64;
  int off  = bit % 64;
  pasid_bitmap[word] &= ~(1UL << off);
}

/* lazily initialize bitmap state */
static void lazy_init(void) {
  if (pasid_exited)
    return;  /* exit() was called — do not reinitialize */
  if (!pasid_initialized) {
    memset(pasid_bitmap, 0, sizeof(pasid_bitmap));
    set_bit(0);              /* PASID 0 reserved */
    pasid_count = 0;
    next_free_hint = 1;
    pasid_initialized = true;
  }
}

/* --- public API --- */

int kfd_pasid_init(void) {
  pthread_mutex_lock(&pasid_mutex);
  pasid_exited = false;
  lazy_init();
  pthread_mutex_unlock(&pasid_mutex);
  return 0;
}

void kfd_pasid_exit(void) {
  pthread_mutex_lock(&pasid_mutex);
  pasid_initialized = false;
  pasid_exited = true;
  pasid_count = 0;
  next_free_hint = 1;
  pthread_mutex_unlock(&pasid_mutex);
}

int kfd_allocate_pasid(u32 *out_pasid) {
  if (!out_pasid)
    return -EINVAL;

  pthread_mutex_lock(&pasid_mutex);
  lazy_init();

  /* after kfd_pasid_exit(), allocation is permanently disabled */
  if (pasid_exited) {
    pthread_mutex_unlock(&pasid_mutex);
    return -ENOSPC;
  }

  /* scan from hint with wrap-around; at most BITMAP_BITS probes */
  int hint = next_free_hint;
  for (int i = 0; i < BITMAP_BITS; i++) {
    int idx = hint;
    if (idx > MAX_PASID)
      idx = 1;
    if (idx == 0)
      idx = 1;

    if (!test_bit(idx)) {
      set_bit(idx);
      *out_pasid = (u32)idx;
      pasid_count++;
      next_free_hint = idx + 1;
      if (next_free_hint > MAX_PASID)
        next_free_hint = 1;
      pthread_mutex_unlock(&pasid_mutex);
      return 0;
    }

    hint++;
    if (hint > MAX_PASID)
      hint = 1;
  }

  /* exhausted */
  pthread_mutex_unlock(&pasid_mutex);
  return -ENOSPC;
}

int kfd_free_pasid(u32 pasid) {
  /* 0 reserved, > MAX_PASID out of range */
  if (pasid == 0 || pasid > MAX_PASID)
    return -EINVAL;

  pthread_mutex_lock(&pasid_mutex);

  if (!pasid_initialized || !test_bit((int)pasid)) {
    /* not allocated */
    pthread_mutex_unlock(&pasid_mutex);
    return -EINVAL;
  }

  clear_bit((int)pasid);
  pasid_count--;

  /* update hint so next alloc can reclaim this slot immediately */
  if ((int)pasid < next_free_hint)
    next_free_hint = (int)pasid;

  pthread_mutex_unlock(&pasid_mutex);
  return 0;
}

int kfd_pasid_allocated_count(void) {
  pthread_mutex_lock(&pasid_mutex);
  int c = pasid_count;
  pthread_mutex_unlock(&pasid_mutex);
  return c;
}