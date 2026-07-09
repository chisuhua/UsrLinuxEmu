/*
 * sim/fence_id.h — Sim 层 fence_id 分配器 (Fix-1 / Oracle H4)
 *
 * 背景：现有 HAL 层 (hal_fence_create) 分配 driver 层 fence_id，范围
 *       [1, SIM_FENCE_ID_BASE - 1]。本 change 新增的 sim 原语
 *       (sim_graph_launch / sim_mem_pool_alloc_async / sim_mem_pool_free_async)
 *       也返回 fence_id，但属于 sim 层分配，范围 [SIM_FENCE_ID_BASE, SIM_FENCE_ID_MAX]。
 *       两层 fence_id 不冲突，由 gpu_ioctl_wait_fence 按值范围分发。
 *
 * 架构：③ 硬件模拟层 (Hardware Simulation)
 * Per ADR-036 three-way separation.
 *
 * C ABI 用法：见 tests/test_fence_id_lifecycle_standalone.cpp
 */

#ifndef SIM_FENCE_ID_H
#define SIM_FENCE_ID_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Range constants (Fix-1: sim 层 fence_id 范围 [SIM_FENCE_ID_BASE, SIM_FENCE_ID_MAX]) */
#define SIM_FENCE_ID_BASE  (1ULL << 32)
#define SIM_FENCE_ID_MAX   INT64_MAX

/**
 * @brief 分配下一个 sim 层 fence_id。
 *
 * 单调递增，原子操作保证未来多线程扩展的安全性。
 * 当前实现基于 std::atomic<uint64_t>，即使单线程调用也保证原子性。
 *
 * @return 新分配的 sim 层 fence_id (>= SIM_FENCE_ID_BASE)
 *         — 当前实现永不失败，预留 < 0 为错误码。
 */
int64_t sim_fence_id_alloc(void);

/**
 * @brief 查询 sim 层 fence_id 是否已触发 (signaled)。
 *
 * @param fence_id  sim 层 fence_id (范围 [SIM_FENCE_ID_BASE, SIM_FENCE_ID_MAX])
 * @param signaled  输出参数，true = 已触发，false = 未触发
 * @return 0 = 查询成功（已写入 *signaled）
 *         -1 = fence_id 越界（< BASE 或 > MAX）
 */
int sim_fence_id_check(uint64_t fence_id, bool *signaled);

/**
 * @brief 触发 sim 层 fence_id（由 submit_batch / submit_memcpy 完成后调用）。
 *
 * 触发后，sim_fence_id_check() 会返回 signaled=true。
 *
 * @param fence_id  sim 层 fence_id
 */
void sim_fence_id_signal(uint64_t fence_id);

/**
 * @brief 重置全局 sim fence 状态（仅用于测试 cleanup）。
 *
 * 释放所有已分配的 fence 状态。下次 sim_fence_id_alloc() 从 SIM_FENCE_ID_BASE
 * 重新开始计数。**注意**：此函数仅用于测试；正常运行时不应调用。
 */
void sim_fence_id_reset_for_test(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* SIM_FENCE_ID_H */
