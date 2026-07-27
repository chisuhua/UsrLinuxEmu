#include <catch_amalgamated.hpp>
#include "sim/hardware/method_codec.h"
#include <vector>

TEST_CASE("pm4_encode_decode_roundtrip", "[pm4]") {
  gpu_method_packet pkt{};
  pkt.method_addr = 0x100;  // OP_LAUNCH_KERNEL
  pkt.engine = static_cast<uint8_t>(GpuEngineType::COMPUTE);
  pkt.data_count = 4;
  uint32_t data[] = {0xDEAD, 0xBEEF, 0xCAFE, 0xBAAB};

  auto encoded = method_codec_encode(pkt, data);
  REQUIRE(encoded.size() == 5);  // 1 header + 4 data

  auto decoded = method_codec_decode(encoded);
  REQUIRE(decoded.method_addr == 0x100);
  REQUIRE(decoded.engine == static_cast<uint8_t>(GpuEngineType::COMPUTE));
  REQUIRE(decoded.data_count == 4);
}

TEST_CASE("pm4_encode_decode_empty_data", "[pm4]") {
  gpu_method_packet pkt{0x200, 2, 0};
  auto encoded = method_codec_encode(pkt, nullptr);
  REQUIRE(encoded.size() == 1);
  auto decoded = method_codec_decode(encoded);
  REQUIRE(decoded.data_count == 0);
}

TEST_CASE("pm4_method_addr_preservation", "[pm4]") {
  gpu_method_packet pkt{0x100, 0, 2};
  uint32_t data[] = {42, 99};
  auto encoded = method_codec_encode(pkt, data);
  auto decoded = method_codec_decode(encoded);
  REQUIRE(decoded.method_addr == 0x100);
}
