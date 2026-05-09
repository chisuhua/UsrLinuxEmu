#pragma once

#include <cstdint>

// GPU 寄存器宏定义
#define GPU_REGISTER(x) (x)

// 示例寄存器偏移（NVIDIA Fermi/Turing 类比）
enum class GpuRegisterOffsets {
    // RAM 基地址与大小
    GPU_RAM_BASE_ADDR = 0x00100C88,
    GPU_RAM_SIZE      = 0x00100C8C,

    // RingBuffer 控制寄存器
    GPU_RB_RDPTR      = 0x00100D04,
    GPU_RB_WRPTR      = 0x00100D08,
    GPU_COMMAND_TRIGGER = 0x00100D0C,
};
