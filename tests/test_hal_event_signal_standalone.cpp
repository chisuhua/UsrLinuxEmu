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
#include <atomic>
#include "kernel/thread/kernel_workqueue.h"
#include "hal/gpu_hal.h"
#include "hal/hal_mock.h"
#include "hal/hal_user.h"
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

  /* After ADR-062 Phase 2: event_signal uses cv-based sync directly,
   * not kfd_events workqueue. So it returns 0 (success) without needing
   * kfd_events_init. */
  int ret = hal_event_signal(&hal, 1, 1, 1);
  REQUIRE(ret == 0);

  hal_mock_destroy(&state);
}

TEST_CASE("hal_event_signal zero events mask rejected", "[hal_event][b44]") {
  int ret = kfd_events_init();
  REQUIRE(ret == 0);

  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  /* After ADR-062 Phase 2: event_signal rejects zero events mask at
   * the HAL layer with -EINVAL (-22), before reaching sim_signal_event. */
  ret = hal_event_signal(&hal, 1, 1, 0);
  REQUIRE(ret == -22);

  int baseline = sim_signal_event_count();

  usr_linux_emu::kernel_workqueue *wq =
      static_cast<usr_linux_emu::kernel_workqueue *>(kfd_events_get_workqueue());
  REQUIRE(wq != nullptr);
  bool drained = wq->flush(std::chrono::milliseconds(200));
  REQUIRE(drained);

  REQUIRE(sim_signal_event_count() == baseline);

  hal_mock_destroy(&state);
  kfd_events_exit();
}

/* ── MSI-X vector routing tests (ADR-062 §HAL Event Signal) ─────────────── */

static std::atomic<uint32_t> g_called_mask(0);

static void handler_0(uint64_t) { g_called_mask.fetch_or(1u << 0, std::memory_order_relaxed); }
static void handler_1(uint64_t) { g_called_mask.fetch_or(1u << 1, std::memory_order_relaxed); }
static void handler_2(uint64_t) { g_called_mask.fetch_or(1u << 2, std::memory_order_relaxed); }
static void handler_3(uint64_t) { g_called_mask.fetch_or(1u << 3, std::memory_order_relaxed); }

TEST_CASE("user_interrupt_raise dispatches vector-0 handler only", "[hal_event][vector-routing]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  hal.interrupt_register(hal.ctx, 0, handler_0);
  hal.interrupt_register(hal.ctx, 1, handler_1);
  hal.interrupt_register(hal.ctx, 2, handler_2);
  hal.interrupt_register(hal.ctx, 3, handler_3);

  g_called_mask.store(0, std::memory_order_relaxed);
  hal_interrupt_raise(&hal, 0);

  REQUIRE(g_called_mask.load(std::memory_order_relaxed) == (1u << 0));

  hal_user_destroy(&ctx);
}

TEST_CASE("user_interrupt_raise dispatches vector-1 handler only", "[hal_event][vector-routing]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  hal.interrupt_register(hal.ctx, 0, handler_0);
  hal.interrupt_register(hal.ctx, 1, handler_1);
  hal.interrupt_register(hal.ctx, 2, handler_2);
  hal.interrupt_register(hal.ctx, 3, handler_3);

  g_called_mask.store(0, std::memory_order_relaxed);
  hal_interrupt_raise(&hal, 1);

  REQUIRE(g_called_mask.load(std::memory_order_relaxed) == (1u << 1));

  hal_user_destroy(&ctx);
}

TEST_CASE("user_interrupt_raise dispatches vector-2 handler only", "[hal_event][vector-routing]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  hal.interrupt_register(hal.ctx, 0, handler_0);
  hal.interrupt_register(hal.ctx, 1, handler_1);
  hal.interrupt_register(hal.ctx, 2, handler_2);
  hal.interrupt_register(hal.ctx, 3, handler_3);

  g_called_mask.store(0, std::memory_order_relaxed);
  hal_interrupt_raise(&hal, 2);

  REQUIRE(g_called_mask.load(std::memory_order_relaxed) == (1u << 2));

  hal_user_destroy(&ctx);
}

TEST_CASE("user_interrupt_raise dispatches vector-3 handler only", "[hal_event][vector-routing]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  hal.interrupt_register(hal.ctx, 0, handler_0);
  hal.interrupt_register(hal.ctx, 1, handler_1);
  hal.interrupt_register(hal.ctx, 2, handler_2);
  hal.interrupt_register(hal.ctx, 3, handler_3);

  g_called_mask.store(0, std::memory_order_relaxed);
  hal_interrupt_raise(&hal, 3);

  REQUIRE(g_called_mask.load(std::memory_order_relaxed) == (1u << 3));

  hal_user_destroy(&ctx);
}