#pragma once

#include <cstdint>
#include <functional>

enum class CommandType : uint32_t {
    KERNEL,
    DMA_COPY
};

struct KernelCommand {
    uint64_t kernel_addr;
    uint64_t args_addr;
    size_t shared_mem;
    unsigned int grid[3];
    unsigned int block[3];

    std::function<void()> callback; // 模拟执行回调
};

struct DmaDirection {
    enum Type {
        H2D, // Host to Device
        D2H, // Device to Host
        D2D  // Device to Device
    };
};

struct DmaCommand {
    uint64_t src_phys;
    uint64_t dst_phys;
    size_t size;
    DmaDirection::Type direction;

    std::function<void()> callback; // 模拟 memcpy 行为
};

struct GpuCommandPacket {
    CommandType type;
    uint32_t size; // packet 总大小
    union {
        KernelCommand kernel;
        DmaCommand dma;
    };
};
