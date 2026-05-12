#include "gpu_queue_emu.h"

#include <cstring>
#include <iostream>

GpuQueueEmu::GpuQueueEmu(uint32_t queue_id, uint32_t queue_type,
                         uint32_t priority, uint32_t ring_size)
    : queue_id_(queue_id),
      queue_type_(queue_type),
      priority_(priority),
      ring_size_(ring_size > 0 ? ring_size : GPU_MAX_RING_ENTRIES) {
  std::cout << "[GpuQueue] Created queue_id=" << queue_id
            << " type=" << queue_type
            << " ring=" << ring_size_ << "\n";
}

GpuQueueEmu::~GpuQueueEmu() {
  ring_header_ = nullptr;
  std::cout << "[GpuQueue] Destroyed queue_id=" << queue_id_ << "\n";
}

int GpuQueueEmu::attachSharedMemory(void* shm_addr, size_t size) {
  if (!shm_addr) return -1;

  size_t needed = sizeof(gpu_ring_header) + ring_size_ * sizeof(gpu_gpfifo_entry);
  if (size < needed) {
    std::cerr << "[GpuQueue] attachSharedMemory: size " << size
              << " < needed " << needed << "\n";
    return -1;
  }

  ring_header_ = static_cast<struct gpu_ring_header*>(shm_addr);
  ring_header_->write_idx = 0;
  ring_header_->read_idx = 0;
  ring_header_->capacity = ring_size_;
  ring_header_->flags = 0;
  ring_header_->fence_value = 0;

  std::cout << "[GpuQueue] Attached shm queue_id=" << queue_id_
            << " size=" << size << " capacity=" << ring_size_ << "\n";
  return 0;
}

bool GpuQueueEmu::dequeue(gpu_gpfifo_entry* out_entry) {
  if (!ring_header_ || !out_entry) return false;

  std::lock_guard<std::mutex> lock(mutex_);

  uint32_t write = ring_header_->write_idx;
  uint32_t read  = ring_header_->read_idx;

  // 空队列检测
  if (read == write) return false;

  // 计算 Ring Buffer 中的索引
  uint32_t idx = read % ring_size_;

  // 读取 entry（从共享内存）
  gpu_gpfifo_entry* entries = reinterpret_cast<gpu_gpfifo_entry*>(
      reinterpret_cast<uint8_t*>(ring_header_) + sizeof(gpu_ring_header));
  *out_entry = entries[idx];

  // 更新 consumer index
  ring_header_->read_idx = read + 1;

  return true;
}

bool GpuQueueEmu::hasPending() const {
  if (!ring_header_) return false;
  std::lock_guard<std::mutex> lock(mutex_);
  return ring_header_->write_idx != ring_header_->read_idx;
}

uint32_t GpuQueueEmu::pendingCount() const {
  if (!ring_header_) return 0;
  std::lock_guard<std::mutex> lock(mutex_);
  return ring_header_->write_idx - ring_header_->read_idx;
}
