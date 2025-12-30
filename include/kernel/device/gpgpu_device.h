#pragma once

#include "kernel/file_ops.h"
#include "kernel/device/device.h"
#include "gpu/address_space.h"
#include <cstdint>
#include <string>
#include <vector>

// 添加GpuTask结构体定义
struct GpuTask {
    uint64_t task_id;
    uint64_t cmd_buffer_addr;
    size_t cmd_buffer_size;
    void* user_data;
};

struct GpuDeviceInfo {
    std::string name;        // 设备名称
    uint64_t memory_size; // 显存大小
    int max_queues;       // 最大队列数
    int compute_units;   // 计算单元数量
};

struct GpuKernel {
    uint64_t kernel_addr; // 内核函数地址
    uint64_t args_addr;  // 参数地址
    size_t code_size;    // 内核代码大小
};

struct GpuMemoryRequest {
    size_t size;
    AddressSpaceType space_type;
};

struct GpuMemoryHandle {
    uint64_t phys_addr;
    void* user_ptr;
    size_t size;
};

class GpuDevice : public FileOperations {
protected:
    uint64_t bar0_start_;
    uint64_t bar0_size_;
    uint64_t bar1_start_;
    uint64_t bar1_size_;

public:
    GpuDevice();  // 添加构造函数声明
    virtual ~GpuDevice() = default;  // 添加析构函数声明

    virtual long ioctl(int fd, unsigned long request, void* argp) override = 0;
    virtual int allocate_memory(size_t size, GpuMemoryHandle* addr_out) = 0;
    virtual int free_memory(GpuMemoryHandle addr) = 0;
    virtual void submit_task(const GpuTask& task) = 0;

    // 修改函数声明，不再为纯虚函数
    virtual bool is_in_bar0(uint64_t offset) const;
    virtual bool is_in_bar1(uint64_t offset) const;
    virtual ssize_t read(int fd, void* buf, size_t count) override = 0;

    //virtual void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) override;
};