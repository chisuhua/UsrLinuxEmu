#include "ring_buffer.h"
#include <cstring>
#include <iostream>

RingBuffer::RingBuffer(PcieEmu* pcie_dev, uint64_t base_offset, size_t buffer_size)
    : pcie_dev_(pcie_dev), base_offset_(base_offset), buffer_size_(buffer_size) {
    std::cout << "[RingBuffer] Initialized at: 0x" << std::hex << base_offset
              << ", size: 0x" << buffer_size_ << std::dec << std::endl;
}

bool RingBuffer::submit(const void* raw_packet, size_t packet_size) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (write_ptr_ + packet_size > buffer_size_) {
        // 环绕支持
        if (read_ptr_ > 0 && write_ptr_ + packet_size > buffer_size_) {
            // 如果空间不足，等待
            if (read_ptr_ <= packet_size) {
                std::cerr << "[RingBuffer] Not enough space for packet." << std::endl;
                // wait_queue_.wait(); // 阻塞等待空闲空间 - 暂时注释掉避免死锁
                return false;
            }

            // 绕回头部
            write_ptr_ = 0;
        }
    }

    // 检查是否有足够的空间
    if (write_ptr_ < read_ptr_ &&
        write_ptr_ + packet_size >= read_ptr_) {
        std::cerr << "[RingBuffer] Full, waiting..." << std::endl;
        // wait_queue_.wait(); // 等待 RingBuffer 释放空间 - 暂时注释掉避免死锁
        return false;
    } else if (write_ptr_ + packet_size >= buffer_size_ &&
               (read_ptr_ > 0 && read_ptr_ <= packet_size)) {
        std::cerr << "[RingBuffer] Wrap not enough space, waiting..." << std::endl;
        // wait_queue_.wait(); // 暂时注释掉避免死锁
        return false;
    }

    // 写入 packet 到 RingBuffer
    pcie_dev_->write_ram(base_offset_ + write_ptr_, raw_packet, packet_size);
    write_ptr_ += packet_size;

    // 暂时注释掉GPU寄存器相关的代码，因为可能不存在
    // 更新写指针寄存器
    // uint32_t wrptr = static_cast<uint32_t>(write_ptr_);
    // pcie_dev_->write_mmio(GPU_REGISTER(GPU_RB_WRPTR), &wrptr, sizeof(wrptr));

    std::cout << "[RingBuffer] Packet submitted. New write pointer: 0x" << std::hex << write_ptr_ << std::dec << std::endl;
    return true;
}

/*
const GpuCommandPacket* RingBuffer::get_next() {
    std::lock_guard<std::mutex> lock(mtx_);

    if (read_ptr_ >= write_ptr_) {
        return nullptr; // 没有新任务
    }

    auto* buffer = reinterpret_cast<const GpuCommandPacket*>(base_phys_ + read_ptr_);
    const GpuCommandPacket* packet = buffer;

    read_ptr_ += packet->size;

    std::cout << "[RingBuffer] Got next command. New read pointer: 0x" << std::hex << read_ptr_ << std::dec << std::endl;
    return packet;
}

void RingBuffer::task_complete(size_t consumed_size) {
    read_ptr_ += consumed_size;
    wait_queue_.wake_up();
}
*/