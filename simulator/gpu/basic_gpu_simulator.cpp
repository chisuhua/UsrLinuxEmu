#include "basic_gpu_simulator.h"
#include <iostream>
#include <sys/mman.h>

BasicGpuSimulator::BasicGpuSimulator()
    : ram_phys_base_(reinterpret_cast<uint64_t>(
          mmap(nullptr, ram_phys_size_, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_ANONYMOUS, -1, 0))),
      mmio_phys_base_(0xbeefcafe00000000ULL),
      mmio_phys_size_(0x1000) {
    if (ram_phys_base_ == reinterpret_cast<uint64_t>(MAP_FAILED)) {
        std::cerr << "[BasicGpuSimulator] Failed to allocate GPU RAM!" << std::endl;
        ram_phys_base_ = 0;
        ram_phys_size_ = 0x80000000;
    }

    reg_space_.resize(0x1000 / 4, 0); // 初始化寄存器空间

    // 设置显存基地址寄存器
    reg_space_[static_cast<int>(GpuRegisterOffsets::GPU_RAM_BASE_ADDR) / 4] = ram_phys_base_;
    reg_space_[static_cast<int>(GpuRegisterOffsets::GPU_RAM_SIZE) / 4] = ram_phys_size_;

    // BAR 分配
    assign_bar(0, ram_phys_base_, ram_phys_size_, false);
    assign_bar(1, mmio_phys_base_, mmio_phys_size_, true);

    parser_.reset(new CommandParser(this, this, ram_phys_base_, ram_phys_size_));
}
