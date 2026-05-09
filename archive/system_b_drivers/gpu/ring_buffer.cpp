#include "ring_buffer.h"
#include <cstring>
#include <iostream>

namespace usr_linux_emu {

RingBuffer::RingBuffer(PcieEmu* pcie_dev, uint64_t base_offset, size_t buffer_size)
    : pcie_dev_(pcie_dev), base_offset_(base_offset), buffer_size_(buffer_size) {
  std::cout << "[RingBuffer] Initialized at: 0x" << std::hex << base_offset << ", size: 0x"
            << buffer_size_ << std::dec << std::endl;
}

bool RingBuffer::submit(const void* raw_packet, size_t packet_size) {
  std::lock_guard<std::mutex> lock(mtx_);

  if (write_ptr_ + packet_size > buffer_size_) {
    if (read_ptr_ > 0 && write_ptr_ + packet_size > buffer_size_) {
      if (read_ptr_ <= packet_size) {
        std::cerr << "[RingBuffer] Not enough space for packet." << std::endl;
        return false;
      }

      write_ptr_ = 0;
    }
  }

  if (write_ptr_ < read_ptr_ && write_ptr_ + packet_size >= read_ptr_) {
    std::cerr << "[RingBuffer] Full, waiting..." << std::endl;
    return false;
  } else if (write_ptr_ + packet_size >= buffer_size_ &&
             (read_ptr_ > 0 && read_ptr_ <= packet_size)) {
    std::cerr << "[RingBuffer] Wrap not enough space, waiting..." << std::endl;
    return false;
  }

  pcie_dev_->write_ram(base_offset_ + write_ptr_, raw_packet, packet_size);
  write_ptr_ += packet_size;

  std::cout << "[RingBuffer] Packet submitted. New write pointer: 0x" << std::hex << write_ptr_
            << std::dec << std::endl;
  return true;
}

}  // namespace usr_linux_emu