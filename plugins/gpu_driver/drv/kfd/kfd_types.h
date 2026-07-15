/*
 * kfd_types.h — KFD common integer type aliases (C-12 SSOT)
 *
 * Per Oracle review (ses_09b3d22e3ffeaGZhjDAnM5F6eB):
 *   Consolidates u8/u16/u32/u64 typedefs that were previously
 *   duplicated in kfd_priv.h:52-54, kfd_topology.h:11-12, and
 *   kfd_svm.h:14-15. Duplicate typedefs are C-legal but cause
 *   -Werror=duplicate-decl-specifier failures under strict compilers
 *   (the #1 root cause of Stage 1.4 PoC commit 5341c3f's 8 iterations).
 *
 * Migration to real kernel:
 *   1. Delete this file
 *   2. Include <linux/types.h> from linux_compat instead
 *
 * All kfd_*.c and kfd_*.h files in this directory MUST include this
 * header before any type that uses u8/u16/u32/u64.
 */
#pragma once

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
