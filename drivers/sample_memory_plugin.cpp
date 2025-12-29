#include "sample_memory.h"
#include <iostream>
#include <memory>
#include "kernel/device.h"
#include "kernel/vfs.h"
#include "kernel/module_loader.h"


//const char* sample_depends[] = { "base_module", NULL };
// sample_driver.cpp

// 模块定义
//extern "C" 
module mod = {
    .name = "sample",
    .depends = nullptr,
    .init = []() -> int {
        // 创建设备实例
        auto mydev = std::make_shared<Device>("sample", 12345,
                                            std::make_shared<SampleMemory>(), nullptr);
        // 注册设备到虚拟文件系统
        VFS::instance().register_device(mydev);
        std::cout << "[SampleMemory] Module initialized." << std::endl;
        return 0;
    },
    .exit = []() {
        std::cout << "[SampleMemory] Module exited." << std::endl;
    }
};
