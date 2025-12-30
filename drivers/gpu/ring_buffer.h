#pragma once

#include <cstdint>
#include <mutex>
#include "kernel/pcie/pcie_emu.h"
#include "gpu_command_packet.h"
#include "kernel/wait_queue.h"  // 添加WaitQueue的包含

class RingBuffer {
public:
    explicit RingBuffer(PcieEmu* pcie_dev, uint64_t base_offset, size_t buffer_size);
    ~RingBuffer();

    bool submit(const void* packet, size_t packet_size);
    const GpuCommandPacket* get_next();
    void task_complete(size_t consumed_size);

    size_t read_offset() const { return read_ptr_; }
    size_t write_offset() const { return write_ptr_; }

private:
    PcieEmu* pcie_dev_;
    uint64_t base_offset_;
    size_t buffer_size_;

    size_t read_ptr_ = 0;
    size_t write_ptr_ = 0;

    std::mutex mtx_;
    WaitQueue wait_queue_;
};