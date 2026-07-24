#include <catch_amalgamated.hpp>
#include "gpu_driver/hal/gpu_hal.h"
#include "gpu_driver/hal/hal_mock.h"
#include "gpu_driver/hal/hal_user.h"
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>

TEST_CASE("hal_event_signal fn-ptr exists in gpu_hal_ops", "[hal_event]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  REQUIRE(hal.event_signal != nullptr);
  REQUIRE(hal.event_wait != nullptr);
  REQUIRE(hal.event_notify != nullptr);

  hal_mock_destroy(&state);
}

TEST_CASE("hal_event_signal then wait returns immediately (single-thread)", "[hal_event]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  int ret = hal_event_signal(&hal, 42, 7, 0xBEEF);
  REQUIRE(ret == 0);
  REQUIRE(state.event_signal_count == 1);

  ret = hal_event_wait(&hal, 7, 0);
  REQUIRE(ret == 0);
  REQUIRE(state.event_wait_count == 1);

  hal_mock_destroy(&state);
}

TEST_CASE("hal_event_wait timeout returns -ETIMEDOUT", "[hal_event]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  int ret = hal_event_wait(&hal, 99, 1000);
  REQUIRE(ret == -110);
  REQUIRE(state.event_wait_count == 1);

  hal_mock_destroy(&state);
}

TEST_CASE("hal_event_notify broadcasts to all waiters", "[hal_event]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  int ret = hal_event_notify(&hal, 55);
  REQUIRE(ret == 0);
  REQUIRE(state.event_notify_count == 1);
  REQUIRE(state.last_event_id == 55);

  hal_mock_destroy(&state);
}

TEST_CASE("hal_user event_signal/wait single-thread", "[hal_event][hal_user]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context uctx;
  hal_user_init(&hal, &uctx);

  SECTION("signal then poll-wait returns 0") {
    int ret = hal_event_signal(&hal, 10, 5, 0xFF);
    REQUIRE(ret == 0);

    ret = hal_event_wait(&hal, 5, 0);
    REQUIRE(ret == 0);
  }

  SECTION("poll-wait without signal returns -ETIMEDOUT") {
    int ret = hal_event_wait(&hal, 99, 0);
    REQUIRE(ret == -110);
  }

  SECTION("notify broadcasts") {
    int ret = hal_event_notify(&hal, 7);
    REQUIRE(ret == 0);

    ret = hal_event_wait(&hal, 7, 0);
    REQUIRE(ret == 0);
  }

  hal_user_destroy(&uctx);
}

TEST_CASE("hal_mock event concurrent signal/wait (multi-thread)", "[hal_event][concurrent]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  constexpr int N_WAITERS = 4;
  constexpr int N_SIGNALS = 10;
  std::atomic<int> wait_success{0};
  std::atomic<int> wait_timeout{0};
  std::atomic<bool> done{false};

  std::vector<std::thread> waiters;
  waiters.reserve(N_WAITERS);
  for (int i = 0; i < N_WAITERS; ++i) {
    waiters.emplace_back([&]() {
      while (!done.load()) {
        int ret = hal_event_wait(&hal, 42, 100000);
        if (ret == 0) wait_success.fetch_add(1);
        else if (ret == -110) wait_timeout.fetch_add(1);
      }
    });
  }

  for (int i = 0; i < N_SIGNALS; ++i) {
    int ret = hal_event_signal(&hal, 0, 42, 0x1);
    REQUIRE(ret == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  done.store(true);

  hal_event_notify(&hal, 42);

  for (auto &t : waiters) t.join();

  CHECK(wait_success.load() > 0);
  CHECK((wait_success.load() + wait_timeout.load()) > 0);

  hal_mock_destroy(&state);
}

TEST_CASE("hal_user event concurrent signal/wait (multi-thread)", "[hal_event][concurrent][hal_user]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context uctx;
  hal_user_init(&hal, &uctx);

  constexpr int N_THREADS = 4;
  constexpr int N_OPS = 50;
  std::atomic<int> signal_ok{0};
  std::atomic<int> wait_ok{0};
  std::atomic<int> wait_timeout{0};

  std::vector<std::thread> threads;
  threads.reserve(N_THREADS);

  for (int t = 0; t < N_THREADS; ++t) {
    threads.emplace_back([&hal, &signal_ok, &wait_ok, &wait_timeout, t, N_OPS]() {
      if (t % 2 == 0) {
        for (int i = 0; i < N_OPS; ++i) {
          if (hal_event_signal(&hal, 0, 100, 0x1) == 0)
            signal_ok.fetch_add(1);
          std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
      } else {
        for (int i = 0; i < N_OPS; ++i) {
          int ret = hal_event_wait(&hal, 100, 10000);
          if (ret == 0) wait_ok.fetch_add(1);
          else if (ret == -110) wait_timeout.fetch_add(1);
        }
      }
    });
  }

  for (auto &th : threads) th.join();

  CHECK(signal_ok.load() > 0);
  CHECK(wait_ok.load() + wait_timeout.load() == (N_THREADS / 2) * N_OPS);

  hal_user_destroy(&uctx);
}

TEST_CASE("hal_mock event_notify wakes all blocked waiters", "[hal_event][concurrent]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  constexpr int N_WAITERS = 5;
  std::atomic<int> woken{0};
  std::vector<std::thread> waiters;
  waiters.reserve(N_WAITERS);

  for (int i = 0; i < N_WAITERS; ++i) {
    waiters.emplace_back([&hal, &woken]() {
      int ret = hal_event_wait(&hal, 77, UINT64_MAX);
      if (ret == 0) woken.fetch_add(1);
    });
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  int ret = hal_event_notify(&hal, 77);
  REQUIRE(ret == 0);

  for (auto &t : waiters) t.join();

  REQUIRE(woken.load() == N_WAITERS);

  hal_mock_destroy(&state);
}
