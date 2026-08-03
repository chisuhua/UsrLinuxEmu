#pragma once
#include <cstdint>
#include <vector>
#include "shared/method_codec_types.h"  // ADR-072 §Decision 2 A-class: types in shared/

std::vector<uint32_t> method_codec_encode(const gpu_method_packet& pkt, const uint32_t* data);
gpu_method_packet method_codec_decode(const std::vector<uint32_t>& encoded);
