/*
 * hal_mock.h — HAL mock 初始化接口
 *
 * hal_mock_init 初始化 gpu_hal_ops 和 hal_mock_state。
 * 用于单元测试隔离验证 HAL 接口逻辑。
 */
#pragma once

#include "gpu_hal.h"

struct hal_mock_state {
  /* 调用计数 */
  int register_read_count;
  int register_write_count;
  int mem_read_count;
  int mem_write_count;
  int mem_alloc_count;
  int mem_free_count;
  int fence_create_count;
  int fence_read_count;
  int doorbell_ring_count;
  int interrupt_raise_count;
  int time_wait_count;

  /* ── ADR-061/062 扩展（C-12 KFD） ────────────────────── */
  int iommu_map_count;
  int iommu_unmap_count;
  int event_signal_count;

  int iommu_map_result;
  int iommu_unmap_result;
  int event_signal_result;

  /* 控制返回值 */
  int register_read_result;
  int register_write_result;
  int mem_read_result;
  int mem_write_result;
  int mem_alloc_result;
  int mem_free_result;
  int fence_create_result;
  int fence_read_result;

  /* 输出参数注入 */
  uint64_t register_read_out;
  uint64_t mem_alloc_out_addr;
  uint64_t fence_create_out_id;
  uint64_t fence_read_out_val;

  /* 记录最近一次写入的值 */
  uint64_t last_reg_write_val;
  uint64_t last_mem_write_addr;
  uint32_t last_doorbell_queue;
  uint32_t last_interrupt_vector;
  uint64_t last_time_wait_us;
};

void hal_mock_init(struct gpu_hal_ops *hal, struct hal_mock_state *state);
