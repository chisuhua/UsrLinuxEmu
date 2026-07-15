/*
 * test_kfd_mmu_standalone.cpp — C-12 B.3.6: kfd_mmu 单元测试
 *
 * 测试范围（per tasks.md §B.3.6）:
 *   - kfd_mmu init/exit 生命周期
 *   - kfd_mmu_map/unmap 基本调用（day-1: B.3.4 未落地，返回 -ENODEV）
 *   - kfd_mmu_get_workqueue 在 init 后返回非 NULL
 *   - kfd_mmu_map 对 NULL process 返回 -EINVAL
 *   - kfd_mmu 并发 map 基础锁测试
 *
 * 已知限制:
 *   - kfd_mmu_map/unmap 需要 HAL ops (B.3.4, Agent B) 才能实际执行
 *   - 在 HAL 未就绪时，map/unmap 返回 -ENODEV（符合 day-1 预期）
 *   - test_map_basic / test_unmap_basic 已验证 -ENODEV 路径
 *
 * 链接: kfd_mmu.c（独立编译，无 HAL mock）
 */
#include <catch_amalgamated.hpp>

#include <thread>
#include <vector>

extern "C" {
#include "kfd_mmu.h"
/* kfd_priv.h cannot be included from C++ context (atomic_int from <stdatomic.h>
 * is C11-only). Forward-declare struct kfd_process — kfd_mmu only uses it as a
 * pointer (validate non-NULL), so complete type is not needed here. */
struct kfd_process {
  int pid;
  unsigned int pasid;
};
}

/* ── helper: minimal process stub for test ──────────────────────────── */

static struct kfd_process make_test_process() {
  struct kfd_process p{};
  p.pid = 1000;
  p.pasid = 1;
  return p;
}

/* ── test cases ─────────────────────────────────────────────────────── */

TEST_CASE("kfd_mmu init/exit lifecycle", "[kfd][mmu][lifecycle]") {
  /* init */
  int ret = kfd_mmu_init();
  REQUIRE(ret == 0);

  /* idempotent: second init returns 0 */
  ret = kfd_mmu_init();
  REQUIRE(ret == 0);

  /* exit */
  kfd_mmu_exit();
  SUCCEED("init/exit cycle completed");
}

TEST_CASE("kfd_mmu exit is idempotent", "[kfd][mmu][exit_idem]") {
  kfd_mmu_init();
  kfd_mmu_exit();
  kfd_mmu_exit();  /* double exit should not crash */
  SUCCEED("double exit is safe");
}

TEST_CASE("kfd_mmu_get_workqueue returns non-NULL after init",
          "[kfd][mmu][workqueue]") {
  kfd_mmu_init();

void *wq = kfd_mmu_get_workqueue();
    REQUIRE(wq != nullptr);

  kfd_mmu_exit();
}

TEST_CASE("kfd_mmu_get_workqueue returns NULL before init",
          "[kfd][mmu][workqueue_null]") {
  /* ensure clean state */
  kfd_mmu_exit();

void *wq = kfd_mmu_get_workqueue();
    REQUIRE(wq == nullptr);
}

TEST_CASE("kfd_mmu_map with null process returns EINVAL",
          "[kfd][mmu][null_process]") {
  kfd_mmu_init();

  int ret = kfd_mmu_map(nullptr, 0x1000, 4096, 0);
  REQUIRE(ret == -EINVAL);  /* EINVAL = 22 */

  kfd_mmu_exit();
}

TEST_CASE("kfd_mmu_unmap with null process returns EINVAL",
          "[kfd][mmu][null_process]") {
  kfd_mmu_init();

  int ret = kfd_mmu_unmap(nullptr, 0x1000, 4096);
  REQUIRE(ret == -EINVAL);

  kfd_mmu_exit();
}

TEST_CASE("kfd_mmu_map with zero size returns EINVAL",
          "[kfd][mmu][zero_size]") {
  kfd_mmu_init();

  struct kfd_process p = make_test_process();
  int ret = kfd_mmu_map(&p, 0x1000, 0, 0);
  REQUIRE(ret == -EINVAL);

  kfd_mmu_exit();
}

/*
 * B.3.4 GATED: kfd_mmu_map/unmap require HAL ops (Agent B).
 * When HAL is not initialized, these return -ENODEV.
 * Verified here: the -ENODEV path is correct for day-1.
 */
TEST_CASE("kfd_mmu_map returns ENODEV when HAL not registered (day-1 gate)",
          "[kfd][mmu][map_nodev]") {
  kfd_mmu_init();

  struct kfd_process p = make_test_process();
  int ret = kfd_mmu_map(&p, 0x1000, 4096, 0);
  REQUIRE(ret == -ENODEV);  /* 19 (ENODEV) — HAL not registered yet */

  kfd_mmu_exit();
}

TEST_CASE("kfd_mmu_unmap returns ENODEV when HAL not registered (day-1 gate)",
          "[kfd][mmu][unmap_nodev]") {
  kfd_mmu_init();

  struct kfd_process p = make_test_process();
  int ret = kfd_mmu_unmap(&p, 0x1000, 4096);
  REQUIRE(ret == -ENODEV);  /* 19 (ENODEV) — HAL not registered yet */

  kfd_mmu_exit();
}

/*
 * B.3.6 §5: 并发 map 基础锁测试。
 * 测试多个线程并发调用 kfd_mmu_map，验证不会崩溃或死锁。
 * 在 HAL 未注册时，所有调用都返回 -ENODEV；锁通过 HAL 内部实现。
 */
TEST_CASE("kfd_mmu concurrent map (basic lock test)",
          "[kfd][mmu][concurrent]") {
  kfd_mmu_init();

  constexpr int kNumThreads = 8;
  struct kfd_process p = make_test_process();
  std::vector<std::thread> threads;

  for (int i = 0; i < kNumThreads; i++) {
    threads.emplace_back([&p]() {
      int ret = kfd_mmu_map(&p, 0x1000 * (1 + (ret & 0xf)), 4096, 0);
      /* day-1: -ENODEV, no crash */
      (void)ret;
    });
  }

  for (auto &t : threads)
    t.join();

  SUCCEED("concurrent map calls completed without crash");
  kfd_mmu_exit();
}

TEST_CASE("kfd_mmu re-init after full cycle", "[kfd][mmu][reinit]") {
  for (int i = 0; i < 3; i++) {
    int ret = kfd_mmu_init();
    REQUIRE(ret == 0);
    kfd_mmu_exit();
  }
  SUCCEED("init/exit cycle repeated 3 times");
}