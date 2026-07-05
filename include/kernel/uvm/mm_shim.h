#ifndef USR_LINUX_EMU_UVM_MM_SHIM_H
#define USR_LINUX_EMU_UVM_MM_SHIM_H

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define US_MM_SHIM_VMA_CAPACITY 16

struct us_mm_vma {
    unsigned long start;
    unsigned long end;
    unsigned long flags;
};

struct us_mm_shim {
    unsigned long pid;
    unsigned int vma_count;
    unsigned int vma_capacity;
    struct us_mm_vma vmas[US_MM_SHIM_VMA_CAPACITY];
};

void us_mm_shim_init(struct us_mm_shim* m, unsigned long pid);
int  us_mm_shim_register_vma(struct us_mm_shim* m,
                             unsigned long start,
                             unsigned long end,
                             unsigned long flags);
int  us_mm_shim_unregister_vma(struct us_mm_shim* m,
                               unsigned long start,
                               unsigned long end);
int  us_mm_shim_find_vma(const struct us_mm_shim* m,
                         unsigned long addr,
                         unsigned long* out_start,
                         unsigned long* out_end);
int  us_mm_shim_foreach_in_range(const struct us_mm_shim* m,
                                 unsigned long start,
                                 unsigned long end,
                                 int (*cb)(const struct us_mm_vma*, void*),
                                 void* user);

#ifdef __cplusplus
}
#endif

#endif