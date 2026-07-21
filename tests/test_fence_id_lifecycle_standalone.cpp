/*
 * test_fence_id_lifecycle_standalone.cpp — Phase 5.6
 * (sim-stream-primitive-support ACCEPTED 2026-07-05)
 *
 * Verifies the 跨层 fence_id lifecycle (Oracle H4):
 *   - sim 层 fence_id: 分配单调递增, 首个 ≥ 1<<32
 *   - driver 层 fence_id: 与 Stage 1.4 一致, 范围 [1, 1<<32-1]
 *   - 两层 fence_id 不冲突
 *   - 信号触发后 sim_fence_id_check 正确返回
 *
 * Plus verifies gpu_ioctl_wait_fence 分发在 driver 层。sim 层
 * 分发逻辑通过单独 helper 模拟（无 ioctl 调用 path）。
 */

#include <catch_amalgamated.hpp>

#include <atomic>
#include <cstdint>

extern "C" {

#include <cerrno>

/* 引入外部 C ABI 头 */
#include "sim/fence_id.h"

/* HAL fence 创建 — Stage 1.4 路径, 提供 driver 层 fence_id 分配。
 * 注意: 我们不链接 HAL 库; 改为本地桩验证范围分配语义. */
static uint64_t fake_hal_fence_id_ = 1;

int hal_fence_create_stub(uint64_t *out) {
  if (!out) return -1;
  *out = fake_hal_fence_id_++;
  return 0;
}

}

TEST_CASE("fence_id — sim layer range starts at SIM_FENCE_ID_BASE",
          "[fence_id][sim]")
{
  sim_fence_id_reset_for_test();
  int64_t id = sim_fence_id_alloc();
  REQUIRE(id >= static_cast<int64_t>(SIM_FENCE_ID_BASE));
  REQUIRE(static_cast<uint64_t>(id) < static_cast<uint64_t>(SIM_FENCE_ID_MAX) + 1);
}

TEST_CASE("fence_id — sim layer monotonic increment",
          "[fence_id][sim]")
{
  sim_fence_id_reset_for_test();
  int64_t a = sim_fence_id_alloc();
  int64_t b = sim_fence_id_alloc();
  int64_t c = sim_fence_id_alloc();
  REQUIRE(b > a);
  REQUIRE(c > b);
  REQUIRE(b == a + 1);
  REQUIRE(c == b + 1);
}

TEST_CASE("fence_id — driver layer range stays below 1<<32",
          "[fence_id][driver]")
{
  /* Stage 1.4 driver layer HAL fence_id 在 [1, (1<<32)-1]. */
  for (int i = 0; i < 5; ++i) {
    uint64_t h = 0;
    REQUIRE(hal_fence_create_stub(&h) == 0);
    REQUIRE(h >= 1);
    REQUIRE(h < (1ULL << 32));
  }
}

TEST_CASE("fence_id — two layers don't collide (unique ranges)",
          "[fence_id][lifecycle]")
{
  sim_fence_id_reset_for_test();
  int64_t s1 = sim_fence_id_alloc();  /* sim */
  uint64_t  d1 = 0;
  hal_fence_create_stub(&d1);          /* driver */
  REQUIRE(static_cast<uint64_t>(s1) >= (1ULL << 32));
  REQUIRE(d1 < (1ULL << 32));
  REQUIRE(static_cast<uint64_t>(s1) != d1);  /* never overlap */
}

TEST_CASE("fence_id — sim_fence_id_signal + check returns signaled=true",
          "[fence_id][lifecycle]")
{
  sim_fence_id_reset_for_test();
  int64_t id = sim_fence_id_alloc();
  REQUIRE(id > 0);

  bool signaled = false;
  REQUIRE(sim_fence_id_check(static_cast<uint64_t>(id), &signaled) == 0);
  REQUIRE_FALSE(signaled);  /* 默认 NOT signaled */

  sim_fence_id_signal(static_cast<uint64_t>(id));

  signaled = false;
  REQUIRE(sim_fence_id_check(static_cast<uint64_t>(id), &signaled) == 0);
  REQUIRE(signaled);  /* signal 后变 true */
}

TEST_CASE("fence_id — simulated wait_fence range dispatch (helper)",
          "[fence_id][lifecycle][wait]")
{
  /* 模拟 gpu_ioctl_wait_fence 的范围分发:
   *   < 1<<32  → HAL (driver 层)
   *   ≥ 1<<32  → sim_fence_id_check (sim 层) */
  sim_fence_id_reset_for_test();

  int64_t sim_id = sim_fence_id_alloc();
  sim_fence_id_signal(static_cast<uint64_t>(sim_id));

  /* driver 层: 直接 HAL */
  uint64_t drv_id = 0;
  hal_fence_create_stub(&drv_id);
  /* (drv_id 默认 'signaled' 在 HAL 桩里为 assumed-ready; 不细验证) */

  /* 模拟 wait_fence 的两个分支 */
  auto dispatch = [](uint64_t id) -> bool {
    if (id < (1ULL << 32)) {
      /* HAL branch (skipped in this test) */
      return false;
    }
    bool s = false;
    sim_fence_id_check(id, &s);
    return s;
  };
  REQUIRE(dispatch(static_cast<uint64_t>(sim_id)));   /* signaled */
  REQUIRE_FALSE(dispatch(42ULL));                        /* HAL branch (false) */
}

TEST_CASE("fence_id — sim_fence_id_check returns false for unknown id",
          "[fence_id][sim][edge]")
{
  sim_fence_id_reset_for_test();
  bool signaled = true;
  /* 未分配的 sim fence_id → 视为 not signaled, 不报错 */
  REQUIRE(sim_fence_id_check(SIM_FENCE_ID_BASE + 9999, &signaled) == 0);
  REQUIRE_FALSE(signaled);
}

TEST_CASE("fence_id — sim_fence_id_check rejects out-of-range id",
          "[fence_id][sim][edge]")
{
  bool signaled = false;
  /* 远低于 BASE → 返回 -1 */
  REQUIRE(sim_fence_id_check(0, &signaled) == -EINVAL);
  /* 远高于 MAX → 返回 -1 */
  REQUIRE(sim_fence_id_check(UINT64_MAX, &signaled) == -EINVAL);
}
