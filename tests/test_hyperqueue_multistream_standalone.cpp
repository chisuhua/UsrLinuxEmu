#include <catch_amalgamated.hpp>
#include "sim/hardware/channel_manager.h"
#include "sim/hardware/hardware_puller_emu.h"
#include "sim/hardware/doorbell_emu.h"
#include "gpu_hal.h"
#include "fence_id.h"
#include <cstdint>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>

TEST_CASE("channel_register_and_submit", "[hyperqueue]") {
  ChannelManager mgr;
  REQUIRE(mgr.nextReadyChannel() == nullptr);
}

TEST_CASE("two_channel_round_robin_no_cross_contamination", "[hyperqueue]") {
  ChannelManager mgr;
  mgr.registerChannel(0, CHAN_PRIO_NORMAL, nullptr);
  mgr.registerChannel(1, CHAN_PRIO_NORMAL, nullptr);
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
  mgr.registerChannel(0, CHAN_PRIO_NORMAL, nullptr);
  mgr.submitBatch(0, 0x1000, 4, 100);
  mgr.submitBatch(0, 0x3000, 8, 300);
  auto* ch = mgr.nextReadyChannel();
  REQUIRE(ch->pending_fence_id == 300);
  REQUIRE(ch->total_entries == 8);
}

TEST_CASE("max_channels_enforced", "[hyperqueue]") {
  ChannelManager mgr;
  for (uint32_t i = 0; i < 32; i++) {
    REQUIRE(mgr.registerChannel(i, CHAN_PRIO_NORMAL, nullptr) == 0);
  }
  REQUIRE(mgr.registerChannel(32, CHAN_PRIO_NORMAL, nullptr) == -ENOSPC);
}

// ========== Task 2: Puller + ChannelManager Integration ==========

// Helper: mock HAL that returns zeroed gpfifo entries with release=1
static int puller_chmgr_hal_mem_read(void* ctx, uint64_t dev_addr,
                                      void* host_buf, uint64_t size) {
  (void)ctx; (void)dev_addr;
  std::memset(host_buf, 0, size);
  // Set release=1 so the Puller FSM goes through COMPLETE path
  if (size >= sizeof(gpu_gpfifo_entry)) {
    auto* e = reinterpret_cast<gpu_gpfifo_entry*>(host_buf);
    e->release = 1;
  }
  return 0;
}

static int noop_hal_register_read(void* c, uint64_t o, uint64_t* v) { (void)c;(void)o; *v=0; return 0; }
static int noop_hal_register_write(void* c, uint64_t o, uint64_t v) { (void)c;(void)o;(void)v; return 0; }
static int noop_hal_mem_write(void* c, uint64_t a, const void* b, uint64_t s) { (void)c;(void)a;(void)b;(void)s; return 0; }
static int noop_hal_mem_alloc(void* c, uint64_t s, uint64_t* o) { (void)c;(void)s; *o=0x100000; return 0; }
static int noop_hal_mem_free(void* c, uint64_t a) { (void)c;(void)a; return 0; }
static int noop_hal_fence_create(void* c, uint64_t* o) { (void)c; *o=1; return 0; }
static int noop_hal_fence_read(void* c, uint64_t i, uint64_t* o) { (void)c;(void)i; *o=1; return 0; }
static void noop_hal_doorbell_ring(void* c, uint32_t q) { (void)c;(void)q; }
static void noop_hal_interrupt_raise(void* c, uint32_t v) { (void)c;(void)v; }
static void noop_hal_time_wait(void* c, uint64_t u) { (void)c;(void)u; }

static struct gpu_hal_ops make_puller_chmgr_hal() {
  struct gpu_hal_ops hal;
  std::memset(&hal, 0, sizeof(hal));
  hal.ctx = nullptr;
  hal.register_read = noop_hal_register_read;
  hal.register_write = noop_hal_register_write;
  hal.mem_read = puller_chmgr_hal_mem_read;
  hal.mem_write = noop_hal_mem_write;
  hal.mem_alloc = noop_hal_mem_alloc;
  hal.mem_free = noop_hal_mem_free;
  hal.fence_create = noop_hal_fence_create;
  hal.fence_read = noop_hal_fence_read;
  hal.doorbell_ring = noop_hal_doorbell_ring;
  hal.interrupt_raise = noop_hal_interrupt_raise;
  hal.time_wait = noop_hal_time_wait;
  return hal;
}

template<typename Func>
static bool wait_for(Func&& pred, int timeout_ms = 200, int poll_ms = 1) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    if (pred()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
  }
  return pred();
}

TEST_CASE("puller_submit_routes_through_channel_manager", "[puller]") {
  struct gpu_hal_ops hal = make_puller_chmgr_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  ChannelManager mgr;
  puller.setChannelManager(&mgr);

  // Register a channel
  REQUIRE(mgr.registerChannel(0, CHAN_PRIO_NORMAL, nullptr) == 0);

  // Submit via Puller's submitBatch - should route to ChannelManager
  puller.submitBatch(0x1000, 2, 999);

  // ChannelManager should have the batch data
  ChannelState* ch = mgr.nextReadyChannel();
  REQUIRE(ch != nullptr);
  REQUIRE(ch->channel_id == 0);
  REQUIRE(ch->gpfifo_addr == 0x1000);
  REQUIRE(ch->total_entries == 2);
  REQUIRE(ch->pending_fence_id == 999);
  REQUIRE(ch->batch_in_flight == true);

  // Yield it back so it doesn't block subsequent nextReadyChannel() calls
  mgr.yieldChannel(0);
}

TEST_CASE("puller_runloop_processes_channel_manager_batch", "[puller]") {
  struct gpu_hal_ops hal = make_puller_chmgr_hal();
  DoorbellEmu doorbell;
  HardwarePullerEmu puller(&hal, &doorbell, nullptr);

  ChannelManager mgr;
  puller.setChannelManager(&mgr);

  sim_fence_id_reset_for_test();
  int64_t fence = sim_fence_id_alloc();
  REQUIRE(fence >= static_cast<int64_t>(SIM_FENCE_ID_BASE));
  u64 fence_id = static_cast<u64>(fence);

  // Pre-condition: fence not signaled
  bool pre_signaled = true;
  REQUIRE(sim_fence_id_check(fence_id, &pre_signaled) == 0);
  REQUIRE_FALSE(pre_signaled);

  REQUIRE(mgr.registerChannel(0, CHAN_PRIO_NORMAL, nullptr) == 0);

  puller.start();

  // Submit via Puller -> routes to ChannelManager
  puller.submitBatch(0x1000, 1, fence_id);

  // Ring doorbell to wake the runLoop
  doorbell.write(0);

  // Wait for processing: leave IDLE then return to IDLE
  wait_for([&puller]() {
    return puller.currentState() != HardwarePullerEmu::State::IDLE;
  }, 300);
  wait_for([&puller]() {
    return puller.currentState() == HardwarePullerEmu::State::IDLE;
  }, 300);

  // Fence should be signaled after batch completes through ChannelManager path
  bool post_signaled = false;
  REQUIRE(sim_fence_id_check(fence_id, &post_signaled) == 0);
  REQUIRE(post_signaled);

  puller.stop();
}
