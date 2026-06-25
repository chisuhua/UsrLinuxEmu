#pragma once

/**
 * gpu_queue_emu.h - 用户态队列仿真 (ADR-024)
 *
 * 模拟 GPU 硬件队列的行为：
 * - 管理共享内存 Ring Buffer
 * - 响应 Doorbell 触发
 * - 供 Hardware Puller 消费 entry
 *
 * 对标:
 * - AMD KFD Queue + Doorbell
 * - NVIDIA Channel + userd mapping
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

#include "gpu_queue.h"
#include "gpu_types.h"

class HardwarePullerEmu;

class GpuQueueEmu {
 public:
  using DoorbellCallback = std::function<void(uint32_t queue_id)>;

  /**
   * @param queue_id   内部队列 ID
   * @param queue_type GPU_QUEUE_COMPUTE / COPY
   * @param priority   0-100
   * @param ring_size  Ring Buffer 容量（entry 数）
   */
  GpuQueueEmu(uint32_t queue_id, uint32_t queue_type,
              uint32_t priority, uint32_t ring_size);
  ~GpuQueueEmu();

  // 禁止拷贝
  GpuQueueEmu(const GpuQueueEmu&) = delete;
  GpuQueueEmu& operator=(const GpuQueueEmu&) = delete;

  /** 获取内部 ID */
  uint32_t queueId() const { return queue_id_; }

  /** 获取 Queue 类型 */
  uint32_t queueType() const { return queue_type_; }

  /** 获取 Doorbell ID（与 queue_id 相同） */
  uint32_t doorbellId() const { return queue_id_; }

  /** 获取优先 */
  uint32_t priority() const { return priority_; }

  /** 获取 Ring Buffer 容量（entry 数） */
  uint32_t ringSize() const { return ring_size_; }

  /** 注册 Doorbell 触发的 callback */
  void setDoorbellCallback(DoorbellCallback cb) {
    doorbell_cb_ = std::move(cb);
  }

  // ========== 供 Puller 调用（消费者端） ==========

  /**
   * 从 Ring Buffer 取出下一个 entry
   * @param out_entry 输出参数，成功时填充 GPFIFO entry
   * @return true=有 entry 取出，false=空队列
   */
  bool dequeue(gpu_gpfifo_entry* out_entry);

  /** 检查是否有待处理的 entry */
  bool hasPending() const;

  /** 获取当前待处理 entry 数量 */
  uint32_t pendingCount() const;

  // ========== 内存管理 ==========

  /**
   * 绑定共享内存为 Ring Buffer
   * @param shm_addr 共享内存地址
   * @param size     共享内存大小
   * @return 0=成功，-1=失败
   */
  int attachSharedMemory(void* shm_addr, size_t size);

  /** 获取 Ring Buffer 头部指针（共享内存） */
  struct gpu_ring_header* ringHeader() const { return ring_header_; }

  // ========== Doorbell 触发 ==========

  /** 触发 Doorbell（由 mmap 写操作调用） */
  void ringDoorbell() {
    if (doorbell_cb_) {
      doorbell_cb_(queue_id_);
    }
  }

  // ========== 提交操作（ioctl 路径委托） ==========

  /**
   * 提交 GPFIFO 批处理（委托给 HardwarePullerEmu）
   * @param gpfifo_addr GPFIFO GPU 地址
   * @param entry_count entry 数量
   * @return 0=成功，-ENODEV=puller 未绑定
   */
  int submit(uint64_t gpfifo_addr, uint32_t entry_count);

  /** 绑定 HardwarePullerEmu（供 GpgpuDevice 在注册时调用） */
  void setPuller(HardwarePullerEmu* puller) { puller_ = puller; }

 private:
  uint32_t queue_id_;
  uint32_t queue_type_;
  uint32_t priority_;
  uint32_t ring_size_;

  /** Ring Buffer 头部（指向共享内存） */
  struct gpu_ring_header* ring_header_ = nullptr;

  /** Doorbell callback */
  DoorbellCallback doorbell_cb_;

  /** HardwarePullerEmu 引用（可选，用于 ioctl 路径委托） */
  HardwarePullerEmu* puller_ = nullptr;

  /** 线程安全 */
  mutable std::mutex mutex_;
};
