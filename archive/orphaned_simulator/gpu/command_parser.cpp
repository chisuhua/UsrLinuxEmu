#include "command_parser.h"
#include <iostream>

CommandParser::CommandParser(BasicGpuSimulator* sim, PcieEmu* pcie_dev, uint64_t rb_base, size_t rb_size)
    : sim_(sim), pcie_dev_(pcie_dev), rb_base_(rb_base), rb_size_(rb_size) {
    start();
}

void CommandParser::start() {
    running_ = true;
    parser_thread_ = std::thread(&CommandParser::run, this);
}

void CommandParser::run() {
    while (running_) {
        uint32_t wrptr;
        pcie_dev_->read_mmio(GPU_REGISTER(GPU_RB_WRPTR), &wrptr, sizeof(wrptr));

        if (wrptr != last_wrptr_) {
            last_wrptr_ = wrptr;

            // 获取当前 packet
            GpuCommandPacket packet{};
            pcie_dev_->read_ram(rb_base_ + wrptr - packet.size, &packet, packet.size);

            switch (packet.type) {
                case CommandType::KERNEL:
                    sim_->process_kernel(packet.kernel);
                    break;
                case CommandType::DMA_COPY:
                    sim_->process_dma(packet.dma);
                    break;
            }

            // 更新读指针
            last_rdptr_ = wrptr;
            pcie_dev_->write_mmio(GPU_REGISTER(GPU_RB_RDPTR), &last_rdptr_, sizeof(last_rdptr_));
        }

        usleep(10000); // 轮询间隔
    }
}
