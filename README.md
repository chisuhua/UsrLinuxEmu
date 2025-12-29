
## files
UserLnxEmu/
├── CMakeLists.txt
├── README.md
├── include/
│   └── kernel/
│       ├── types.h
│       ├── file_ops.h          <-- file_operations抽象接口
│       ├── wait_queue.h
│       ├── device.h            <-- 设备抽象类 
│       ├── vfs.h               <-- 组册查找设备
│       ├── plugin_manager.h
│       ├── config_manager.h
│       ├── module_loader.h     <-- 加载器：不涉及设备类型
│       ├── service_registry.h
│       ├── logger.h
│       ├── poll_watcher.h
│       └── device/
│           ├── serial_device.h
│           └── memory_device.h
├── src/
│   └── kernel/
│       ├── types.cpp
│       ├── module.cpp
│       ├── file_ops.cpp
│       ├── wait_queue.cpp
│       ├── plugin_manager.cpp
│       ├── config_manager.cpp
│       ├── service_registry.cpp
│       ├── logger.cpp
│       ├── poll_watcher.cpp
│       ├── serial_device.cpp
│       └── memory_device.cpp

UserLnxEmu/
├── include/kernel/
│   ├── device/
│   │   └── gpgpu_device.h
│   └── pcie/
│       └── pcie_emu.h
│
├── include/
│   └── kernel/
│       ├── wait_queue.h
|
├── src/kernel/device/
│   └── gpgpu_device.cpp
|
├── src/
│   └── kernel/
│       ├── wait_queue.cpp
│       ├── poll_watcher.cpp
|
│
├── simulator/gpu/
│   ├── basic_gpu_simulator.h/cpp
│   ├── command_parser.h/cpp
│
├── drivers/gpu/
│   ├── ring_buffer.h/cpp
│   ├── buddy_allocator.h/cpp
│   ├── address_space.h/cpp
│   ├── gpu_command_packet.h/cpp
│   ├── gpu_driver.h/cpp
│   ├── ioctl-gpgpu.h
│   └── plugin_gpu.cpp
│
└── test/
    └── test_gpu_submit.cpp


[用户程序]
   ↓
cudaMalloc(&d_data, size)
   ↓
ioctl(fd, GPGPU_ALLOC_MEM, &handle)
   ↓
BuddyAllocator.allocate(size, &phys_addr)
   ↓
返回 phys_addr = GPU 设备本地物理地址
   ↓
用户拿到 user_ptr = phys_addr （由 mmap 提供）
   ↓
submit_kernel(..., phys_addr)
   ↓
GpuDriver.write(NV_GPU_COMMAND_QUEUE, ...) → 触发 GPU 执行
   ↓
GPU 模拟器 copy_from_device(...) → 访问 SYSTEM_UNCACHED 内存
