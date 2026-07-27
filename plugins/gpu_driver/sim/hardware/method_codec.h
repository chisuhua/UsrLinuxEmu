#pragma once
#include <cstdint>
#include <vector>

enum class GpuEngineType : uint8_t { COMPUTE=0, COPY=1, GRAPHICS=2, _RESERVED=3 };

struct gpu_method_packet {
  uint16_t method_addr;
  uint8_t engine;
  uint8_t data_count;
};

std::vector<uint32_t> method_codec_encode(const gpu_method_packet& pkt, const uint32_t* data);
gpu_method_packet method_codec_decode(const std::vector<uint32_t>& encoded);
