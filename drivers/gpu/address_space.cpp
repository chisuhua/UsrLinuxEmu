#include "address_space.h"
#include <iostream>

GpuSystemMemoryManager& GpuSystemMemoryManager::instance() {
    static GpuSystemMemoryManager manager;
    return manager;
}

int GpuSystemMemoryManager::register_system_memory(void* ptr, size_t size, AddressSpaceType type) {
    uint64_t gpu_phys = reinterpret_cast<uint64_t>(ptr);
    by_cpu_[ptr] = {ptr, gpu_phys, size, type};
    by_gpu_phys_[gpu_phys] = {ptr, gpu_phys, size, type};

    std::cout << "[GpuSystemMemoryManager] Registered system memory at: 0x" << std::hex << gpu_phys
              << " (" << std::dec << size << " bytes)" << std::endl;

    return 0;
}

bool GpuSystemMemoryManager::find_by_cpu_ptr(void* ptr, SystemMemoryRegion* out) {
    auto it = by_cpu_.find(ptr);
    if (it == by_cpu_.end()) return false;
    *out = it->second;
    return true;
}

bool GpuSystemMemoryManager::find_by_gpu_phys(uint64_t phys, SystemMemoryRegion* out) {
    auto it = by_gpu_phys_.find(phys);
    if (it == by_gpu_phys_.end()) return false;
    *out = it->second;
    return true;
}
