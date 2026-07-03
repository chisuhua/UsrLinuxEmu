#pragma once
// Stage 1.2 PoC: minimal Linux slab compat stubs for KFD compilation

#ifdef __cplusplus
#include <cstdlib>
#include <cstring>
#else
#include <stdlib.h>
#include <string.h>
#endif

#ifndef GFP_KERNEL
#define GFP_KERNEL 0
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif
#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#ifndef PAGE_SHIFT
#define PAGE_SHIFT 12
#endif

#define pr_debug(fmt, ...) ((void)0)

#define kzalloc(size, flags) calloc(1, (size))
#define kfree(p) free(p)
#define kmalloc(size, flags) malloc((size))