/*
 * test_kfd_events_standalone.cpp — C-12 B.4.5: kfd_events 单元测试
 *
 * 测试范围（per tasks.md §B.4.5）:
 *   - kfd_events init/exit 生命周期
 *   - kfd_events_signal 在 init 前返回 -EAGAIN
 *   - kfd_events_signal 在 init 后入队成功返回 0
 *   - kfd_events_signal 对无效参数返回 -EINVAL
 *   - kfd_events 并发 signal 基础竞争测试
 *   - kfd_events_get_workqueue 在 init 后返回非 NULL
 *
 * 已知限制:
 *   - 实际事件页面写入由 B.4.6 (Phase C/E, Agent B) 实现
 *   - day-1 仅验证入队管道 + 错误路径
 *
 * 链接: kfd_events.c（独立编译）
 */
#include <catch_amalgamated.hpp>

#include <chrono>
#include <thread>
#include <vector>

#include "kernel/thread/kernel_workqueue.h"

extern "C" {
#include "kfd_events.h"
}

TEST_CASE("kernel_workqueue repeatedly stops idle worker",
          "[kfd][events][workqueue][regression]") {
  for (int iteration = 0; iteration < 256; ++iteration) {
    usr_linux_emu::kernel_workqueue workqueue;
    workqueue.start();

    REQUIRE(workqueue.flush(std::chrono::milliseconds(100)));
    workqueue.stop();
  }
}

TEST_CASE("kfd_events init/exit lifecycle", "[kfd][events][lifecycle]") {
  /* init */
  int ret = kfd_events_init();
  REQUIRE(ret == 0);

  /* idempotent: second init returns 0 */
  ret = kfd_events_init();
  REQUIRE(ret == 0);

  /* exit */
  kfd_events_exit();
  SUCCEED("init/exit cycle completed");
}

TEST_CASE("kfd_events exit is idempotent", "[kfd][events][exit_idem]") {
  kfd_events_init();
  kfd_events_exit();
  kfd_events_exit();  /* double exit should not crash */
  SUCCEED("double exit is safe");
}

TEST_CASE("kfd_events_signal returns EAGAIN before init",
          "[kfd][events][pre_init]") {
  /* ensure clean state */
  kfd_events_exit();

  int ret = kfd_events_signal(1, 0, 1);
  REQUIRE(ret == -EAGAIN);  /* 11 (EAGAIN) */
}

TEST_CASE("kfd_events_signal basic enqueue succeeds after init",
          "[kfd][events][signal_ok]") {
  kfd_events_init();

  int ret = kfd_events_signal(1, 0, 1ULL << 0);
  REQUIRE(ret == 0);  /* enqueue success */

  kfd_events_exit();
}

TEST_CASE("kfd_events_signal returns EINVAL on event_id >= 8",
          "[kfd][events][bad_event_id]") {
  kfd_events_init();

  int ret = kfd_events_signal(1, 8, 1);
  REQUIRE(ret == -EINVAL);  /* 22 (EINVAL) */

  ret = kfd_events_signal(1, 255, 1);
  REQUIRE(ret == -EINVAL);

  kfd_events_exit();
}

TEST_CASE("kfd_events_signal returns EINVAL on zero events mask",
          "[kfd][events][zero_mask]") {
  kfd_events_init();

  int ret = kfd_events_signal(1, 0, 0);
  REQUIRE(ret == -EINVAL);

  kfd_events_exit();
}

TEST_CASE("kfd_events_signal returns EAGAIN after exit",
          "[kfd][events][after_exit]") {
  kfd_events_init();
  kfd_events_exit();

  int ret = kfd_events_signal(1, 0, 1);
  REQUIRE(ret == -EAGAIN);
}

TEST_CASE("kfd_events_signal after re-init works",
          "[kfd][events][reinit]") {
  kfd_events_init();
  kfd_events_exit();
  kfd_events_init();

  int ret = kfd_events_signal(2, 3, 0xdeadULL);
  REQUIRE(ret == 0);

  kfd_events_exit();
}

/*
 * B.4.5 §6: 并发 signal 基础竞争测试。
 * 多个线程并发调用 kfd_events_signal，验证 workqueue 的 mutex
 * 能正确处理并发入队（不崩溃、不死锁）。
 */
TEST_CASE("kfd_events concurrent signal (basic race test)",
          "[kfd][events][concurrent]") {
  kfd_events_init();

  constexpr int kNumThreads = 8;
  constexpr int kRounds = 50;
  std::vector<std::thread> threads;

  for (int i = 0; i < kNumThreads; i++) {
    threads.emplace_back([i]() {
      for (int r = 0; r < kRounds; r++) {
        int ret = kfd_events_signal((u32)(i + 1), (u32)(r % 8),
                                    (u64)(1ULL << (r % 64)));
        (void)ret;  /* enqueue success expected */
      }
    });
  }

  for (auto &t : threads)
    t.join();

  SUCCEED("concurrent signal calls completed without crash");
  kfd_events_exit();
}

TEST_CASE("kfd_events_get_workqueue returns non-NULL after init",
          "[kfd][events][workqueue]") {
  kfd_events_init();

void *wq = kfd_events_get_workqueue();
    REQUIRE(wq != nullptr);

  kfd_events_exit();
}

TEST_CASE("kfd_events_get_workqueue returns NULL before init",
          "[kfd][events][workqueue_null]") {
  kfd_events_exit();  /* ensure clean state */

void *wq = kfd_events_get_workqueue();
    REQUIRE(wq == nullptr);
}

TEST_CASE("kfd_events signal with PASID 0 (broadcast) is accepted",
          "[kfd][events][pasid0]") {
  kfd_events_init();

  /* PASID 0 = broadcast, should be accepted */
  int ret = kfd_events_signal(0, 0, 1);
  REQUIRE(ret == 0);

  kfd_events_exit();
}