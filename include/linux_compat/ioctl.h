#pragma once

#include <sys/ioctl.h>

// Linux kernel ioctl-compatible macro definitions.
//
// The system <sys/ioctl.h> already defines _IOC_*, _IO, _IOR, _IOW,
// _IOWR, FIONREAD, FIONBIO etc. for the host platform. We re-define
// them only when missing so include order (this header before or
// after system) doesn't trigger redefinition warnings.

// ioctl command type widths
#ifndef _IOC_NRBITS
#define _IOC_NRBITS 8
#endif
#ifndef _IOC_TYPEBITS
#define _IOC_TYPEBITS 8
#endif
#ifndef _IOC_SIZEBITS
#define _IOC_SIZEBITS 14
#endif
#ifndef _IOC_DIRBITS
#define _IOC_DIRBITS 2
#endif

#ifndef _IOC_NRMASK
#define _IOC_NRMASK ((1 << _IOC_NRBITS) - 1)
#endif
#ifndef _IOC_TYPEMASK
#define _IOC_TYPEMASK ((1 << _IOC_TYPEBITS) - 1)
#endif
#ifndef _IOC_SIZEMASK
#define _IOC_SIZEMASK ((1 << _IOC_SIZEBITS) - 1)
#endif
#ifndef _IOC_DIRMASK
#define _IOC_DIRMASK ((1 << _IOC_DIRBITS) - 1)
#endif

#ifndef _IOC_NRSHIFT
#define _IOC_NRSHIFT 0
#endif
#ifndef _IOC_TYPESHIFT
#define _IOC_TYPESHIFT (_IOC_NRSHIFT + _IOC_NRBITS)
#endif
#ifndef _IOC_SIZESHIFT
#define _IOC_SIZESHIFT (_IOC_TYPESHIFT + _IOC_TYPEBITS)
#endif
#ifndef _IOC_DIRSHIFT
#define _IOC_DIRSHIFT (_IOC_SIZESHIFT + _IOC_SIZEBITS)
#endif

// Direction flags
#ifndef _IOC_NONE
#define _IOC_NONE 0U
#endif
#ifndef _IOC_WRITE
#define _IOC_WRITE 1U
#endif
#ifndef _IOC_READ
#define _IOC_READ 2U
#endif

#ifndef _IOC
#define _IOC(dir, type, nr, size)                                                   \
  (((dir) << _IOC_DIRSHIFT) | ((type) << _IOC_TYPESHIFT) | ((nr) << _IOC_NRSHIFT) | \
   ((size) << _IOC_SIZESHIFT))
#endif

// High-level ioctl helpers
#ifndef _IO
#define _IO(type, nr) _IOC(_IOC_NONE, (type), (nr), 0)
#endif
#ifndef _IOR
#define _IOR(type, nr, size) _IOC(_IOC_READ, (type), (nr), sizeof(size))
#endif
#ifndef _IOR_BAD
#define _IOR_BAD(type, nr, size) _IOC(_IOC_READ, (type), (nr), sizeof(size))
#endif
#ifndef _IOW
#define _IOW(type, nr, size) _IOC(_IOC_WRITE, (type), (nr), sizeof(size))
#endif
#ifndef _IOWR
#define _IOWR(type, nr, size) _IOC(_IOC_READ | _IOC_WRITE, (type), (nr), sizeof(size))
#endif

// Field extractors
#ifndef _IOC_DIR
#define _IOC_DIR(nr) (((nr) >> _IOC_DIRSHIFT) & _IOC_DIRMASK)
#endif
#ifndef _IOC_TYPE
#define _IOC_TYPE(nr) (((nr) >> _IOC_TYPESHIFT) & _IOC_TYPEMASK)
#endif
#ifndef _IOC_NR
#define _IOC_NR(nr) (((nr) >> _IOC_NRSHIFT) & _IOC_NRMASK)
#endif
#ifndef _IOC_SIZE
#define _IOC_SIZE(nr) (((nr) >> _IOC_SIZESHIFT) & _IOC_SIZEMASK)
#endif

// Generic ioctl command numbers (FION* family). System headers usually
// provide these; guard for the case where they don't.
#ifndef FIONREAD
#define FIONREAD 0x541B
#endif
#ifndef TIOCINQ
#define TIOCINQ FIONREAD
#endif
#ifndef FIONBIO
#define FIONBIO 0x5421
#endif
#ifndef FIOASYNC
#define FIOASYNC 0x5452
#endif
#ifndef FIOCLEX
#define FIOCLEX 0x5451
#endif
#ifndef FIONCLEX
#define FIONCLEX 0x5450
#endif
