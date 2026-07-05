#include <kernel/uvm/mm_shim.h>

#include <linux_compat/types.h>
#include <cerrno>

extern "C" {

void us_mm_shim_init(struct us_mm_shim* m, unsigned long pid) {
  if (!m) return;
  m->pid = pid;
  m->vma_count = 0;
  m->vma_capacity = US_MM_SHIM_VMA_CAPACITY;
  for (unsigned int i = 0; i < US_MM_SHIM_VMA_CAPACITY; ++i) {
    m->vmas[i].start = 0;
    m->vmas[i].end = 0;
    m->vmas[i].flags = 0;
  }
}

int us_mm_shim_register_vma(struct us_mm_shim* m,
                            unsigned long start,
                            unsigned long end,
                            unsigned long flags) {
  if (!m) return -EINVAL;
  if (end <= start) return -EINVAL;
  if (m->vma_count >= m->vma_capacity) return -ENOSPC;

  for (unsigned int i = 0; i < m->vma_count; ++i) {
    if (m->vmas[i].start == start && m->vmas[i].end == end) {
      m->vmas[i].flags = flags;
      return 0;
    }
  }

  m->vmas[m->vma_count].start = start;
  m->vmas[m->vma_count].end = end;
  m->vmas[m->vma_count].flags = flags;
  m->vma_count++;
  return 0;
}

int us_mm_shim_unregister_vma(struct us_mm_shim* m,
                              unsigned long start,
                              unsigned long end) {
  if (!m) return -EINVAL;
  for (unsigned int i = 0; i < m->vma_count; ++i) {
    if (m->vmas[i].start == start && m->vmas[i].end == end) {
      for (unsigned int j = i; j + 1 < m->vma_count; ++j) {
        m->vmas[j] = m->vmas[j + 1];
      }
      m->vma_count--;
      m->vmas[m->vma_count].start = 0;
      m->vmas[m->vma_count].end = 0;
      m->vmas[m->vma_count].flags = 0;
      return 0;
    }
  }
  return -ENOENT;
}

int us_mm_shim_find_vma(const struct us_mm_shim* m,
                        unsigned long addr,
                        unsigned long* out_start,
                        unsigned long* out_end) {
  if (!m) return -EINVAL;
  for (unsigned int i = 0; i < m->vma_count; ++i) {
    if (addr >= m->vmas[i].start && addr < m->vmas[i].end) {
      if (out_start) *out_start = m->vmas[i].start;
      if (out_end)   *out_end   = m->vmas[i].end;
      return 0;
    }
  }
  return -ENOENT;
}

int us_mm_shim_foreach_in_range(const struct us_mm_shim* m,
                                unsigned long start,
                                unsigned long end,
                                int (*cb)(const struct us_mm_vma*, void*),
                                void* user) {
  if (!m || !cb) return -EINVAL;
  int count = 0;
  for (unsigned int i = 0; i < m->vma_count; ++i) {
    if (m->vmas[i].start < end && m->vmas[i].end > start) {
      int rc = cb(&m->vmas[i], user);
      if (rc != 0) return rc;
      count++;
    }
  }
  return count;
}

}  // extern "C"