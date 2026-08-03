// include/shared/method_codec_types.h
//
// Public type definitions for the method_codec HAL contract.
// Per ADR-072 §Decision 2 A-class: type/constant references belong in shared/.
// drv/ includes this to construct gpu_method_packet without depending on
// sim/hardware/method_codec.h (the function declarations stay in sim/).
//
// Per ADR-042 (Method Encoding Format).

#pragma once

#include <cstdint>

/* Stage 4.3 (ADR-042): engine type discriminator for method packets. */
enum class GpuEngineType : uint8_t {
  COMPUTE  = 0,
  COPY      = 1,
  GRAPHICS  = 2,
  _RESERVED = 3,
};

/* Stage 4.3 (ADR-042): GPFIFO method packet descriptor.
 * 4 bytes total: method_addr (2) + engine (1) + data_count (1).
 * ABI-stable: shared between drv/ (caller) and sim/ (callee). */
struct gpu_method_packet {
  uint16_t method_addr;
  uint8_t  engine;
  uint8_t  data_count;
};
