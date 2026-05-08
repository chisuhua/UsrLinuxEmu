#pragma once

#include <cstdint>

namespace usr_linux_emu {

using pid_t = int;
using dev_t = uint32_t;
using mode_t = uint16_t;

constexpr int ENODEV = -1;
constexpr int EAGAIN = -2;

}  // namespace usr_linux_emu