#pragma once

#include <cstdint>

using pid_t = int;
using dev_t = uint32_t;
using mode_t = uint16_t;

constexpr int ENODEV = -1;  // No such device
constexpr int EAGAIN = -2;  // Try again (non-blocking)
