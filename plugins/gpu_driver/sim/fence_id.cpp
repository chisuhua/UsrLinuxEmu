/*
 * sim/fence_id.cpp — Sim 层 fence_id 分配器实现 (Fix-1 / Oracle H4)
 *
 * 设计：
 *  - next_sim_fence_id_ 原子计数器，从 SIM_FENCE_ID_BASE 开始
 *  - sim_fence_table_ std::map<id, signaled> 追踪已触发的 fence
 *  - Thread Safety §参见 design.md：本 change 不引入线程安全保证，
 *    但 fence_id 分配本身用 std::atomic 防御未来扩展
 *  - 单线程调用保证（与 Stage 1.4 一致）
 *
 * 架构：③ 硬件模拟层 (Hardware Simulation)
 * Per ADR-036 three-way separation.
 */

#include "fence_id.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>

namespace {

/* 内部计数器（C 链接 extern 不暴露） */
std::atomic<uint64_t> next_sim_fence_id_{SIM_FENCE_ID_BASE};

/* 已触发 fence 的状态表 */
std::map<uint64_t, bool> sim_fence_table_;

/* 保护 sim_fence_table_ 的互斥量（兼容未来多线程扩展） */
std::mutex sim_fence_table_mutex_;

}  // anonymous namespace

extern "C" {

int64_t sim_fence_id_alloc(void) {
  /* fetch_add 保证单调递增与原子性 */
  uint64_t id = next_sim_fence_id_.fetch_add(1, std::memory_order_relaxed);

  /* 初始化 entry 为未触发状态（记入表，未触发） */
  std::lock_guard<std::mutex> lock(sim_fence_table_mutex_);
  sim_fence_table_[id] = false;

  return static_cast<int64_t>(id);
}

int sim_fence_id_check(uint64_t fence_id, bool *signaled) {
  if (!signaled)
    return -EINVAL;

  /* 范围校验（防御越界调用） */
  if (fence_id < SIM_FENCE_ID_BASE || fence_id > static_cast<uint64_t>(SIM_FENCE_ID_MAX))
    return -EINVAL;

  std::lock_guard<std::mutex> lock(sim_fence_table_mutex_);
  auto it = sim_fence_table_.find(fence_id);
  if (it == sim_fence_table_.end()) {
    /* fence 未分配（无 sim_fence_id_alloc 调用记录） → 视为未触发 */
    *signaled = false;
    return 0;
  }

  *signaled = it->second;
  return 0;
}

void sim_fence_id_signal(uint64_t fence_id) {
  /* 越界静默忽略（与 driver 层 hal_fence 信号保持对称：silent-on-bad-id） */
  if (fence_id < SIM_FENCE_ID_BASE || fence_id > static_cast<uint64_t>(SIM_FENCE_ID_MAX))
    return;

  std::lock_guard<std::mutex> lock(sim_fence_table_mutex_);
  sim_fence_table_[fence_id] = true;
}

void sim_fence_id_reset_for_test(void) {
  std::lock_guard<std::mutex> lock(sim_fence_table_mutex_);
  sim_fence_table_.clear();
  next_sim_fence_id_.store(SIM_FENCE_ID_BASE, std::memory_order_relaxed);
}

}  // extern "C"
