#include <catch_amalgamated.hpp>
#include "sim/hardware/channel_manager.h"
#include <cstdint>

TEST_CASE("channel_register_and_submit", "[hyperqueue]") {
  ChannelManager mgr;
  REQUIRE(mgr.nextReadyChannel() == nullptr);
}

TEST_CASE("two_channel_round_robin_no_cross_contamination", "[hyperqueue]") {
  ChannelManager mgr;
  mgr.registerChannel(0, nullptr);
  mgr.registerChannel(1, nullptr);
  mgr.submitBatch(0, 0x1000, 4, 100);
  mgr.submitBatch(1, 0x2000, 4, 200);
  
  auto* ch0 = mgr.nextReadyChannel();
  REQUIRE(ch0 != nullptr);
  REQUIRE(ch0->pending_fence_id == 100);
  REQUIRE(ch0->channel_id == 0);
  
  mgr.yieldChannel(0);
  auto* ch1 = mgr.nextReadyChannel();
  REQUIRE(ch1 != nullptr);
  REQUIRE(ch1->pending_fence_id == 200);
  REQUIRE(ch1->channel_id == 1);
  
  mgr.yieldChannel(1);
  REQUIRE(mgr.nextReadyChannel() == nullptr);
}

TEST_CASE("channel_submit_overwrites_previous_batch", "[hyperqueue]") {
  ChannelManager mgr;
  mgr.registerChannel(0, nullptr);
  mgr.submitBatch(0, 0x1000, 4, 100);
  mgr.submitBatch(0, 0x3000, 8, 300);
  auto* ch = mgr.nextReadyChannel();
  REQUIRE(ch->pending_fence_id == 300);
  REQUIRE(ch->total_entries == 8);
}

TEST_CASE("max_channels_enforced", "[hyperqueue]") {
  ChannelManager mgr;
  for (uint32_t i = 0; i < 32; i++) {
    REQUIRE(mgr.registerChannel(i, nullptr) == 0);
  }
  REQUIRE(mgr.registerChannel(32, nullptr) == -ENOSPC);
}
