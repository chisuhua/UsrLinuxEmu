#include "gpu_driver.h"
#include <iostream>
#include <cstring>
#include <unistd.h>

GpuDriver::GpuDriver()
    : bar0_mapped_base_(nullptr), bar0_mapped_size_(0),
      gpu_phys_base_(0), gpu_phys_size_(0) {
    PciDevice* pci_dev = dynamic_cast<PciDevice*>(this);
    if (!pci_dev) {
        std::cerr << "[GpuDriver] Device does not support PCI interface." << std::endl;
        return;
    }

    // 从 MMIO 寄存器中读取 GPU 显存信息
    uint64_t ram_base = 0, ram_size = 0;

    pci_dev->read_mmio(GPU_REGISTER((uint64_t)GpuRegisterOffsets::GPU_RAM_BASE_ADDR), &ram_base, sizeof(ram_base));
    pci_dev->read_mmio(GPU_REGISTER((uint64_t)GpuRegisterOffsets::GPU_RAM_SIZE), &ram_size, sizeof(ram_size));

    if (ram_base == 0 || ram_size == 0) {
        std::cerr << "[GpuDriver] GPU RAM base or size is zero!" << std::endl;
        return;
    }

    gpu_phys_base_ = ram_base;
    gpu_phys_size_ = ram_size;

    // RingBuffer 分配在显存末尾
    ring_buffer_phys_base_ = ram_base + ram_size - RING_BUFFER_SIZE;
    ring_buffer_size_ = RING_BUFFER_SIZE;

    // 初始化内存池
    fb_public_pool_.reset(new BuddyAllocator(ram_base, ram_size * 3 / 4));
    system_uncached_pool_.reset(new BuddyAllocator(ram_base + ram_size * 3 / 4, ram_size / 4));

    // 初始化 RingBuffer
    command_queue_.reset(new RingBuffer(pci_dev, ring_buffer_phys_base_, ring_buffer_size_));
}

GpuDriver::~GpuDriver() {
    //gpu_sim_->stop();
}

std::unique_ptr<BuddyAllocator> GpuDriver::get_allocator(AddressSpaceType type) {
    switch (type) {
        case AddressSpaceType::FB_PUBLIC:
            return fb_public_pool_;
        case AddressSpaceType::SYSTEM_UNCACHED:
            return system_uncached_pool_;
        default:
            std::cerr << "[GpuDriver] Unsupported memory space: " << static_cast<int>(type) << std::endl;
            return nullptr;
    }
}

long GpuDriver::ioctl(int fd, unsigned long request, void* argp) {
    switch (request) {
        case GPGPU_ALLOC_MEM: {
            auto req = static_cast<GpuMemoryRequest*>(argp);
            allocate_memory(req->size, req->space_type, &req->phys_addr, &req->user_ptr);
            break;
        }
        case GPGPU_FREE_MEM: {
            uint64_t addr = *static_cast<uint64_t*>(argp);
            free_memory(addr);
            break;
        }
        case GPGPU_SUBMIT_PACKET: {
            auto req = static_cast<const GpuCommandRequest*>(argp);
            if (!command_queue_->submit(req->packet_ptr, req->packet_size)) {
                return -1;
            }
            break;
        }
        default:
            std::cerr << "[GpuDriver] Unknown ioctl command" << std::hex << request << std::dec << std::endl;
            return -1;
    }

    return 0;
}
long GpuDriver::ioctl(int fd, unsigned long request, void* argp) {
    switch (request) {
        case GPGPU_GET_DEVICE_INFO: {
            auto info = static_cast<struct GpuDeviceInfo*>(argp);
            fill_info(info);
            break;
        }
        case GPGPU_REGISTER_SYS_MEM: {
            auto reg = static_cast<SystemMemoryRegion*>(argp);
            GpuSystemMemoryManager::instance().register_system_memory(reg->cpu_ptr, reg->size, reg->type);
            break;
        }
        case GPGPU_ALLOC_MEM: {
            GpuMemoryHandle handle{};
            allocate_memory(argp, &handle);
            break;
        }
        case GPGPU_FREE_MEM: {
            free_memory(argp);
            break;
        }
        case GPGPU_SUBMIT_PACKET: {
            auto req = static_cast<const GpuCommandRequest*>(argp);
            submit_packet(argp);
            break;
        }
        default:
            std::cerr << "[Gpu] Unknown ioctl command" << std::endl;
            return -1;
    }
    return 0;
}

void GpuDriver::submit_packet(size_t argp) {
    auto req = static_cast<const GpuCommandRequest*>(argp);
    if (!command_queue_->submit(req->packet_ptr, req->packet_size)) {
        return -1;
    }
}

int GpuDriver::allocate_memory(size_t argp, GpuMemoryHandle* out) {
    auto req = reinterpret_cast<const GpuMemoryRequest*>(argp);
    AddressSpaceType type = req->space_type;

    BuddyAllocator* pool = get_allocator(type).get();

    if (!pool) return -1;


    uint64_t phys_addr = 0;
    if (pool->allocate(req->size, &phys_addr) != 0) {
        return -1;
    }

    out->phys_addr = phys_addr;
    out->user_ptr = reinterpret_cast<void*>(out->phys_addr); // 用户态访问地址
    out->size = size;
    return 0;
}

int GpuDriver::free_memory(GpuMemoryHandle handle) {
    BuddyAllocator* pool = get_allocator(handle.space_type).get();
    if (!pool) return -1;

    return pool->free(handle.phys_addr);
}

int GpuDriver::submit_kernel(const GpuTask& task) {
    GpuCommandPacket packet{};
    packet.type = CommandType::KERNEL;
    packet.size = sizeof(packet.kernel);

    memcpy(&packet.kernel, &task, sizeof(task));
    packet.kernel.callback = [grid = task.grid, block = task.block]() {
        std::cout << "[GpuDriver] Simulating kernel execution..." << std::endl;
        usleep(500000); // 模拟执行时间
    };

    if (!command_queue_->submit(&packet, packet.size)) {
        return -1;
    }

    return 0;
}

int GpuDriver::submit_dma(const GpuDmaTask& dma_task) {
    GpuCommandPacket packet{};
    packet.type = CommandType::DMA_COPY;
    packet.size = sizeof(packet.dma);
    memcpy(&packet.dma, &dma_task, sizeof(dma_task));

    packet.dma.callback = [src = dma_task.src_phys,
                           dst = dma_task.dst_phys,
                           size = dma_task.size]() {
        std::cout << "[GpuDriver] Simulating DMA copy from 0x" << std::hex << src
                  << " to 0x" << dst << " size: 0x" << size << std::dec << std::endl;

        char* src_ptr = reinterpret_cast<char*>(src);
        char* dst_ptr = reinterpret_cast<char*>(dst);
        memcpy(dst_ptr, src_ptr, size);
    };

    if (!command_queue_->submit(&packet, packet.size)) {
        return -1;
    }

    return 0;
}
