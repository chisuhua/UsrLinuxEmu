
void main() {
    ModuleLoader::load_plugins("plugins");

    auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
    if (!dev || !dev->fops) {
        std::cerr << "[TestGPU] Failed to open GPU device." << std::endl;
        return;
    }

    int fd = 0;

    // 写入 WR pointer 到 GPU_RB_WRPTR 寄存器
    off_t wrptr_offset = GPU_REGISTER(GPU_RB_WRPTR);
    lseek(fd, wrptr_offset, SEEK_SET);
    uint32_t wrptr = 0x1000;
    dev->fops->write(fd, &wrptr, sizeof(wrptr));

    // 读取 RD pointer
    off_t rdptr_offset = GPU_REGISTER(GPU_RB_RDPTR);
    lseek(fd, rdptr_offset, SEEK_SET);
    uint32_t rdptr = 0;
    dev->fops->read(fd, &rdptr, sizeof(rdptr));
    std::cout << "[TestGPU] Read RDPTR from register: 0x" << std::hex << rdptr << std::dec << std::endl;

    ModuleLoader::unload_plugins();
}
