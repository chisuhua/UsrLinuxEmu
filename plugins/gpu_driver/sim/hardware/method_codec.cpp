#include "method_codec.h"

std::vector<uint32_t> method_codec_encode(const gpu_method_packet& pkt, const uint32_t* data) {
  std::vector<uint32_t> buf;
  uint32_t header = (static_cast<uint32_t>(pkt.method_addr) << 16) |
                    (static_cast<uint32_t>(pkt.engine) << 8) |
                    pkt.data_count;
  buf.push_back(header);
  for (uint8_t i = 0; i < pkt.data_count; ++i) {
    buf.push_back(data ? data[i] : 0);
  }
  return buf;
}

gpu_method_packet method_codec_decode(const std::vector<uint32_t>& encoded) {
  gpu_method_packet pkt{};
  if (encoded.empty()) return pkt;
  uint32_t header = encoded[0];
  pkt.method_addr = static_cast<uint16_t>((header >> 16) & 0xFFFF);
  pkt.engine = static_cast<uint8_t>((header >> 8) & 0xFF);
  pkt.data_count = static_cast<uint8_t>(header & 0xFF);
  return pkt;
}
