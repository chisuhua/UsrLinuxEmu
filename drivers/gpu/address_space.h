#pragma once

#include <cstdint>
#include <unordered_map>

enum class AddressSpaceType {
    FB_PUBLIC,
    FB_PRIVATE,
    SYSTEM_CACHED,
    SYSTEM_UNCACHED,
    DEVICE_SVM,
    MMIO_REMAP
};

struct SystemMemoryRegion {
    void* cpu_ptr;
    uint64_t gpu_phys;
    size_t size;
    AddressSpaceType type;
};

class GpuSystemMemoryManager {
public:
    static GpuSystemMemoryManager& instance();

    int register_system_memory(void* ptr, size_t size, AddressSpaceType type);
    bool find_by_cpu_ptr(void* ptr, SystemMemoryRegion* out);
    bool find_by_gpu_phys(uint64_t phys, SystemMemoryRegion* out);

private:
    std::unordered_map<void*, SystemMemoryRegion> by_cpu_;
    std::unordered_map<uint64_t, SystemMemoryRegion> by_gpu_phys_;
};
