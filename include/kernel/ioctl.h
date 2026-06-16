#pragma once

#include <cstdint>
#include <sys/ioctl.h>

namespace usr_linux_emu {

// Direction flags. Guarded so that if <sys/ioctl.h> is included
// transitively, we don't redefine these.
#ifndef _IOC_NONE
#define _IOC_NONE 0U
#endif
#ifndef _IOC_WRITE
#define _IOC_WRITE 1U
#endif
#ifndef _IOC_READ
#define _IOC_READ 2U
#endif

// ioctl command encoder. The hardcoded 30/8/16/0 shifts here are
// intentionally different from the kernel's configurable
// _IOC_DIRBITS / _IOC_TYPEBITS etc. so this header can compile in
// isolation (no kernel header needed). Project callers that need
// kernel-exact shifts should use linux_compat/ioctl.h.
#ifndef _IOC
#define _IOC(dir, type, nr, size) (((dir) << 30) | ((type) << 8) | (nr) | ((size) << 16))
#endif

// High-level ioctl helpers
#ifndef _IO
#define _IO(type, nr) _IOC(_IOC_NONE, (type), (nr), 0)
#endif
#ifndef _IOR
#define _IOR(type, nr, size) _IOC(_IOC_READ, (type), (nr), sizeof(size))
#endif
#ifndef _IOW
#define _IOW(type, nr, size) _IOC(_IOC_WRITE, (type), (nr), sizeof(size))
#endif
#ifndef _IOWR
#define _IOWR(type, nr, size) _IOC(_IOC_READ | _IOC_WRITE, (type), (nr), sizeof(size))
#endif

}  // namespace usr_linux_emu
