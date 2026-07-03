/*
 * errno_to_linux.cpp — byte-exact errno mapping for Linux 6.12 ABI
 *
 * Stage 1.2 task 2.5 (design.md Decision 1) — ensures UsrLinuxEmu
 * simulator errno returns are byte-identical to the Linux 6.12
 * kernel ABI (matches ADR-027 §Decision 3 "ABI consistency not
 * guaranteed but errno values match").
 *
 * In user-space, the libc errno constants are POSIX-defined and
 * already match the Linux kernel ABI for the standard codes we use
 * (EACCES, EFAULT, ENOMEM, EREMOTEIO, ENOSPC, EINVAL, EBUSY, etc.).
 * This function is therefore a 1:1 identity for the standard cases
 * and a re-encoder for non-POSIX negative values.
 *
 * Linkage: C++ (declaration in drm_ioctl.h has no extern "C"
 * wrapper; call sites are C++ Catch2 tests + C++ DRM handlers).
 */

#include "linux_compat/drm/drm_ioctl.h"

int errno_to_linux(int err)
{
  if (err >= 0) {
    return err;
  }
  return -err;
}
