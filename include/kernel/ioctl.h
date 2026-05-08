#pragma once

#include <cstdint>

namespace usr_linux_emu {

#define _IOC_NONE 0U
#define _IOC_WRITE 1U
#define _IOC_READ 2U

#define _IOC(dir, type, nr, size) (((dir) << 30) | ((type) << 8) | (nr) | ((size) << 16))

#define _IO(type, nr) _IOC(_IOC_NONE, (type), (nr), 0)
#define _IOR(type, nr, size) _IOC(_IOC_READ, (type), (nr), sizeof(size))
#define _IOW(type, nr, size) _IOC(_IOC_WRITE, (type), (nr), sizeof(size))
#define _IOWR(type, nr, size) _IOC(_IOC_READ | _IOC_WRITE, (type), (nr), sizeof(size))

}  // namespace usr_linux_emu