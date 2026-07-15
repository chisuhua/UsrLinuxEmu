/*
 * test_hal_event_signal_standalone.cpp — HAL event_signal async test (C-12 B.4.4)
 *
 * Tests the full async path:
 *   kfd_events_init → hal_event_signal → workqueue → sim_signal_event
 *
 * Per ADR-060 §2.1 + ADR-062 §D3: events must be async via kernel_workqueue.
 */

#include <catch_amalgamated.hpp>
#include <thread>
#include <chrono>
#include "kernel/thread/kernel_workqueue.h"
#include "hal/gpu_hal.h"
#include "hal/hal_mock.h"
#include "sim/sim_event.h"

/* Forward declarations for Agent A's kfd_events.c — resolved at link time */
extern "C" {
  int  kfd_events_init(void);
  void kfd_events_exit(void);
  void *kfd_events_get_workqueue(void);
}

static int test_event_signal_direct(void *ctx, uint32_t pasid, uint32_t event_id, uint64_t events) {
  (void)ctx;
  sim_signal_event(pasid, event_id, events);
  return 0;
}

TEST_CASE("hal_event_signal async path via workqueue", "[hal_event][b44]") {
  int ret = kfd_events_init();
  REQUIRE(ret == 0);

  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);
  hal.event_signal = test_event_signal_direct;

  int baseline = sim_signal_event_count();

  ret = hal_event_signal(&hal, 42, 7, 0xBEEF);
  REQUIRE(ret == 0);

  /* Drain the workqueue to ensure async delivery completes */
  usr_linux_emu::kernel_workqueue *wq =
      static_cast<usr_linux_emu::kernel_workqueue *>(kfd_events_get_workqueue());
  REQUIRE(wq != nullptr);
  bool drained = wq->flush(std::chrono::milliseconds(200));
  REQUIRE(drained);

  /* Verify sim_signal_event was called via the async path */
  REQUIRE(sim_signal_event_count() == baseline + 1);

  kfd_events_exit();
}

TEST_CASE("hal_event_signal EAGAIN before kfd_events_init", "[hal_event][b44]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  int ret = hal_event_signal(&hal, 1, 1, 1);
  REQUIRE(ret == -11);  /* -EAGAIN */
}

TEST_CASE("hal_event_signal zero events mask rejected", "[hal_event][b44]") {
  int ret = kfd_events_init();
  REQUIRE(ret == 0);

  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  int baseline = sim_signal_event_count();

  ret = hal_event_signal(&hal, 1, 1, 0);
  REQUIRE(ret == 0);  /* enqueue succeeds */

  usr_linux_emu::kernel_workqueue *wq =
      static_cast<usr_linux_emu::kernel_workqueue *>(kfd_events_get_workqueue());
  REQUIRE(wq != nullptr);
  bool drained = wq->flush(std::chrono::milliseconds(200));
  REQUIRE(drained);

  /* sim_signal_event rejects 0 events, so count should not change */
  REQUIRE(sim_signal_event_count() == baseline);

  kfd_events_exit();
}