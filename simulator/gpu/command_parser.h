#pragma once

#include <thread>
#include <mutex>
#include "wait_queue.h"
#include "basic_gpu_simulator.h"
#include "kernel/device/gpu_command_packet.h"

class CommandParser {
public:
    explicit CommandParser(BasicGpuSimulator* sim, PcieEmu* pcie_dev, uint64_t rb_base, size_t rb_size);
    ~CommandParser();

    void start();
    void stop();

private:
    void run();

    BasicGpuSimulator* sim_;
    PcieEmu* pcie_dev_;
    uint64_t rb_base_;
    size_t rb_size_;

    uint32_t last_wrptr_ = 0;
    uint32_t last_rdptr_ = 0;
    std::thread parser_thread_;
    bool running_ = false;
};
