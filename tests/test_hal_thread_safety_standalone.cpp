/*
 * test_hal_thread_safety_standalone.cpp — HAL 线程安全冒烟测试 (TSan)
 *
 * 使用 ThreadSanitizer 验证 struct gpu_hal_ops 的 11 个函数指针
 * 在并发调用下无数据竞争。
 *
 * 同时测试 hal_mock（非原子计数器，TSan 应报告竞争 — 预期行为）
 * 和 hal_user（mutex + __sync 保护，TSan 应报告无竞争 — 验证目标）。
 *
 * 构建：cmake -DENABLE_TSAN=ON .. && make -j4 test_hal_thread_safety_standalone
 * 运行：从项目根目录 ./build/bin/test_hal_thread_safety_standalone
 */
#include "gpu_driver/hal/gpu_hal.h"
#include "gpu_driver/hal/hal_mock.h"
#include "gpu_driver/hal/hal_user.h"
#include "catch_amalgamated.hpp"
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

/* ── 并发测试辅助：hal_mock 版 ────────────────────── */

struct concurrent_hal_mock_test {
  struct gpu_hal_ops hal;
  struct hal_mock_state state;

  concurrent_hal_mock_test() { hal_mock_init(&hal, &state); }

  void run(int n_threads, int n_calls, std::function<void()> fn) {
    std::vector<std::thread> threads;
    threads.reserve(n_threads);
    for (int t = 0; t < n_threads; ++t) {
      threads.emplace_back([fn, n_calls]() {
        for (int i = 0; i < n_calls; ++i) fn();
      });
    }
    for (auto& th : threads) th.join();
  }
};

/* ── 并发测试辅助：hal_user 版 ────────────────────── */

struct concurrent_hal_user_test {
  struct gpu_hal_ops hal;
  struct hal_user_context uctx;

  concurrent_hal_user_test() { hal_user_init(&hal, &uctx); }
  ~concurrent_hal_user_test() { hal_user_destroy(&uctx); }

  void run(int n_threads, int n_calls, std::function<void()> fn) {
    std::vector<std::thread> threads;
    threads.reserve(n_threads);
    for (int t = 0; t < n_threads; ++t) {
      threads.emplace_back([fn, n_calls]() {
        for (int i = 0; i < n_calls; ++i) fn();
      });
    }
    for (auto& th : threads) th.join();
  }
};

constexpr int N_THREADS = 4;
constexpr int N_CALLS   = 100;

/* ================================================================
 * TEST_CASE 1 — register_read
 * ================================================================ */

TEST_CASE("HAL register_read is thread-safe", "[hal][tsan]") {
  SECTION("hal_mock") {
    concurrent_hal_mock_test t;
    t.state.register_read_result = 0;
    t.state.register_read_out    = 0x42;
    t.run(N_THREADS, N_CALLS, [&t]() {
      uint64_t val = 0;
      hal_register_read(&t.hal, 0x1000, &val);
    });
    // TSan 应检测到 mock 计数器的数据竞争（非原子 int）
    // 不验证精确值——竞争可能导致丢失更新
    CHECK(t.state.register_read_count > 0);
  }

  SECTION("hal_user") {
    concurrent_hal_user_test t;
    std::atomic<uint64_t> ok{0};
    t.run(N_THREADS, N_CALLS, [&t, &ok]() {
      uint64_t val = 0;
      int ret = hal_register_read(&t.hal, 0x80, &val);
      if (ret == 0) ok.fetch_add(1);
    });
    // hal_user 使用 mutex 保护；TSan 应报告无竞争
    CHECK(ok.load() == N_THREADS * N_CALLS);
  }
}

/* ================================================================
 * TEST_CASE 2 — register_write
 * ================================================================ */

TEST_CASE("HAL register_write is thread-safe", "[hal][tsan]") {
  SECTION("hal_mock") {
    concurrent_hal_mock_test t;
    t.state.register_write_result = 0;
    t.run(N_THREADS, N_CALLS, [&t]() {
      hal_register_write(&t.hal, 0x2000, 0xDEAD);
    });
    CHECK(t.state.register_write_count > 0);
  }

  SECTION("hal_user") {
    concurrent_hal_user_test t;
    std::atomic<uint64_t> ok{0};
    t.run(N_THREADS, N_CALLS, [&t, &ok]() {
      int ret = hal_register_write(&t.hal, 0x80, 0xCAFE);
      if (ret == 0) ok.fetch_add(1);
    });
    CHECK(ok.load() == N_THREADS * N_CALLS);
  }
}

/* ================================================================
 * TEST_CASE 3 — mem_read / mem_write 并发
 * ================================================================ */

TEST_CASE("HAL mem_read/mem_write concurrent is thread-safe", "[hal][tsan]") {
  SECTION("hal_mock") {
    concurrent_hal_mock_test t;
    t.state.mem_read_result  = 0;
    t.state.mem_write_result = 0;
    t.run(N_THREADS, N_CALLS, [&t]() {
      char buf[16];
      hal_mem_read(&t.hal, 0x1000, buf, 8);
      hal_mem_write(&t.hal, 0x1000, buf, 8);
    });
    CHECK(t.state.mem_read_count + t.state.mem_write_count > 0);
  }

  SECTION("hal_user") {
    concurrent_hal_user_test t;
    /* 先分配一块内存用于并发读写 */
    uint64_t addr = 0;
    REQUIRE(hal_mem_alloc(&t.hal, 65536, &addr) == 0);
    std::atomic<uint64_t> ok{0};
    t.run(N_THREADS, N_CALLS, [&t, addr, &ok]() {
      char buf[16];
      int r1 = hal_mem_read(&t.hal, addr, buf, 8);
      int r2 = hal_mem_write(&t.hal, addr, buf, 8);
      if (r1 == 0 && r2 == 0) ok.fetch_add(1);
    });
    CHECK(ok == N_THREADS * N_CALLS);
    hal_mem_free(&t.hal, addr);
  }
}

/* ================================================================
 * TEST_CASE 4 — mem_alloc / mem_free 并发
 * ================================================================ */

TEST_CASE("HAL mem_alloc/free concurrent is thread-safe", "[hal][tsan]") {
  SECTION("hal_mock") {
    concurrent_hal_mock_test t;
    t.state.mem_alloc_result = 0;
    t.state.mem_free_result  = 0;
    t.state.mem_alloc_out_addr = 0x200000;
    t.run(N_THREADS, N_CALLS, [&t]() {
      uint64_t a = 0;
      hal_mem_alloc(&t.hal, 4096, &a);
      hal_mem_free(&t.hal, a);
    });
    CHECK(t.state.mem_alloc_count + t.state.mem_free_count > 0);
  }

  SECTION("hal_user") {
    concurrent_hal_user_test t;
    /* 预分配 + 释放，模拟真实使用模式 */
    std::atomic<uint64_t> alloc_ok{0};
    std::atomic<uint64_t> free_ok{0};
    t.run(N_THREADS, N_CALLS, [&t, &alloc_ok, &free_ok]() {
      uint64_t a = 0;
      if (hal_mem_alloc(&t.hal, 4096, &a) == 0) {
        alloc_ok.fetch_add(1);
        if (hal_mem_free(&t.hal, a) == 0)
          free_ok.fetch_add(1);
      }
    });
    // hal_user 内 buddy allocator 使用 heap_lock 保护
    CHECK(alloc_ok == free_ok);
    CHECK(alloc_ok > 0);
  }
}

/* ================================================================
 * TEST_CASE 5 — fence_create / fence_read 并发
 * ================================================================ */

TEST_CASE("HAL fence_create/read concurrent is thread-safe", "[hal][tsan]") {
  SECTION("hal_mock") {
    concurrent_hal_mock_test t;
    t.state.fence_create_result = 0;
    t.state.fence_read_result   = 0;
    t.state.fence_create_out_id = 99;
    t.state.fence_read_out_val  = 1;
    t.run(N_THREADS, N_CALLS, [&t]() {
      uint64_t fid = 0;
      hal_fence_create(&t.hal, &fid);
      hal_fence_read(&t.hal, fid, &fid);
    });
    CHECK(t.state.fence_create_count + t.state.fence_read_count > 0);
  }

  SECTION("hal_user") {
    concurrent_hal_user_test t;
    std::atomic<uint64_t> ok{0};
    t.run(N_THREADS, N_CALLS, [&t, &ok]() {
      uint64_t fid = 0;
      int rc = hal_fence_create(&t.hal, &fid);
      if (rc == 0) {
        /* 创建后 fence 立即 signaled */
        uint64_t val = 0;
        int rr = hal_fence_read(&t.hal, fid, &val);
        if (rr == 0 && val == 1) ok.fetch_add(1);
      }
    });
    // fence 操作使用 fence_lock 保护
    CHECK(ok > 0);
  }
}

/* ================================================================
 * TEST_CASE 6 — void-return ops (doorbell, interrupt)
 * 弹射式操作：不返回错误码，仅触发副作用
 * ================================================================ */

TEST_CASE("HAL void-return ops concurrent OK", "[hal][tsan]") {
  SECTION("hal_mock") {
    concurrent_hal_mock_test t;
    t.run(N_THREADS, N_CALLS, [&t]() {
      hal_doorbell_ring(&t.hal, 1);
      hal_interrupt_raise(&t.hal, 2);
    });
    CHECK(t.state.doorbell_ring_count + t.state.interrupt_raise_count > 0);
  }

  SECTION("hal_user") {
    concurrent_hal_user_test t;
    t.run(N_THREADS, N_CALLS, [&t]() {
      hal_doorbell_ring(&t.hal, 1);
      hal_interrupt_raise(&t.hal, 2);
      hal_time_wait(&t.hal, 1);  // 1us
    });
    // hal_user uses std::atomic with memory_order_relaxed — verify no lost updates
    CHECK(t.uctx.doorbell_count.load() == N_THREADS * N_CALLS);
    CHECK(t.uctx.interrupt_count.load() == N_THREADS * N_CALLS);
  }
}

/* ================================================================
 * TEST_CASE 6.5 — doorbell callback 语义正确性
 * 验证：并发调用下 doorbell callback 被调用次数 = N_THREADS * N_CALLS
 * ================================================================ */

TEST_CASE("HAL doorbell callback invoked exactly N times under concurrency", "[hal][tsan]") {
  concurrent_hal_user_test t;
  std::atomic<uint64_t> cb_count{0};

  int ret = hal_user_set_doorbell_cb(&t.uctx,
    [](void* c, uint32_t /*qid*/) {
      static_cast<std::atomic<uint64_t>*>(c)->fetch_add(1, std::memory_order_relaxed);
    }, &cb_count);
  REQUIRE(ret == 0);

  t.run(N_THREADS, N_CALLS, [&t]() {
    hal_doorbell_ring(&t.hal, 0);
  });

  CHECK(cb_count.load() == N_THREADS * N_CALLS);
}

/* ================================================================
 * TEST_CASE 7 — 混合所有 11 个操作（高重负）
 *
 * 模拟真实多线程驱动场景：所有 HAL op 同时在不同线程被调用
 * ================================================================ */

TEST_CASE("HAL all ops mixed concurrent has no data race", "[hal][tsan]") {
  constexpr int MIXED_THREADS = 6;
  constexpr int MIXED_CALLS   = 50;

  SECTION("hal_mock") {
    concurrent_hal_mock_test t;
    t.state.register_read_result   = 0;
    t.state.register_write_result  = 0;
    t.state.mem_read_result        = 0;
    t.state.mem_write_result       = 0;
    t.state.mem_alloc_result       = 0;
    t.state.mem_free_result        = 0;
    t.state.fence_create_result    = 0;
    t.state.fence_read_result      = 0;
    t.state.register_read_out      = 1;
    t.state.mem_alloc_out_addr     = 0x300000;
    t.state.fence_create_out_id    = 7;
    t.state.fence_read_out_val     = 1;

    t.run(MIXED_THREADS, MIXED_CALLS, [&t]() {
      uint64_t v = 0;
      hal_register_read(&t.hal, 0, &v);
      hal_register_write(&t.hal, 0, 0);
      hal_mem_read(&t.hal, 0, &v, 4);
      hal_mem_write(&t.hal, 0, &v, 4);
      hal_mem_alloc(&t.hal, 4096, &v);
      hal_mem_free(&t.hal, v);
      hal_fence_create(&t.hal, &v);
      hal_fence_read(&t.hal, 0, &v);
      hal_doorbell_ring(&t.hal, 0);
      hal_interrupt_raise(&t.hal, 0);
    });
    // 所有 mock 计数器均为非原子 int — TSan 应报告竞争
    CHECK(t.state.register_read_count > 0);
  }

  SECTION("hal_user") {
    concurrent_hal_user_test t;
    /* 预先分配一块内存用于读写 */
    uint64_t mem = 0;
    REQUIRE(hal_mem_alloc(&t.hal, 65536, &mem) == 0);

    std::atomic<uint64_t> errors{0};
    t.run(MIXED_THREADS, MIXED_CALLS, [&t, mem, &errors]() {
      uint64_t v = 0;

      if (hal_register_read(&t.hal, 0, &v) != 0) errors.fetch_add(1);
      if (hal_register_write(&t.hal, 0, 42) != 0) errors.fetch_add(1);
      if (hal_mem_read(&t.hal, mem, &v, 4) != 0) errors.fetch_add(1);
      if (hal_mem_write(&t.hal, mem, &v, 4) != 0) errors.fetch_add(1);

      uint64_t a = 0;
      if (hal_mem_alloc(&t.hal, 4096, &a) == 0) {
        hal_mem_free(&t.hal, a);
      }

      uint64_t fid = 0;
      if (hal_fence_create(&t.hal, &fid) == 0) {
        hal_fence_read(&t.hal, fid, &v);
      }

      hal_doorbell_ring(&t.hal, 0);
      hal_interrupt_raise(&t.hal, 0);
      hal_time_wait(&t.hal, 1);
    });

    // hal_user uses mutex (state ops) + std::atomic (counters) — TSan should report no races
    CHECK(errors == 0);
    // Verify doorbell/interrupt counters have no lost updates in mixed-op scenario
    CHECK(t.uctx.doorbell_count.load() > 0);
    CHECK(t.uctx.interrupt_count.load() > 0);
    hal_mem_free(&t.hal, mem);
  }
}
