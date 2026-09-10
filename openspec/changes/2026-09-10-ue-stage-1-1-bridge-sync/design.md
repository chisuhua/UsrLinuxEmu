# Design: ue-stage-1-1-bridge-sync

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **关联**: [proposal.md](../proposal.md) + [tasks.md](../tasks.md) + [specs/ue-stage-1-1-bridge-sync/spec.md](../specs/ue-stage-1-1-bridge-sync/spec.md)

## 设计概述

UsrLinuxEmu 侧桥接层同步 change。设计原则：
1. **TDD 升级**：测试断言从"假通过"升级到数据正确性
2. **跨仓集成**：从 UE 进程 dlopen `libcpptlm_emulator.so` 验证端到端
3. **不修改运行代码**：仅修改测试代码 + 桥接层错误处理细节

## 设计 1: 桥接层断言升级

### 当前断言（5.5.6 测试，假通过）
```cpp
// tests/integration/test_dgpu_bridge_sanity_standalone.cc
TEST_CASE("pcie_config_read returns non-ENOSYS", "[bridge][5-5-6]") {
    auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
    uint32_t val = 0;
    int ret = dev->fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &val);
    REQUIRE(ret != -ENOSYS);  // ← 假通过: data 未必正确
}
```

### 升级后断言
```cpp
TEST_CASE("pcie_config_read returns Vendor ID 0x10DE", "[bridge][stage-1-1]") {
    auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
    gpu_device_info info{};
    int ret = dev->fops->ioctl(fd, GPU_IOCTL_GET_DEVICE_INFO, &info);
    REQUIRE(ret == 0);  // 必须成功 (非 -ENOSYS)
    REQUIRE(info.vendor_id == 0x10DE);  // 必须 Vendor ID 正确
}

TEST_CASE("backdoor_read miss returns -ENOENT", "[bridge][stage-1-1]") {
    auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
    std::vector<uint8_t> buf(64);
    int ret = dev->backdoor_read(0xDEADBEEF, buf.data(), 64);
    REQUIRE(ret == -ENOENT);  // 必须 miss 返 -ENOENT (非 len 伪装)
}

TEST_CASE("mmio_read returns real data not garbage", "[bridge][stage-1-1]") {
    auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
    std::vector<uint8_t> buf(4);
    int ret = dev->mmio_read(0, 0, buf.data(), 4);
    REQUIRE(ret >= 0);  // byte count
    REQUIRE(buf[0] != 0);  // 非 garbage
}

TEST_CASE("mmio_write returns immediately (async)", "[bridge][stage-1-1]") {
    auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
    auto start = std::chrono::high_resolution_clock::now();
    int ret = dev->mmio_write(0, 0, buf, 4);
    auto end = std::chrono::high_resolution_clock::now();
    REQUIRE(ret == 0);
    REQUIRE(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() < 1000);
    // 必须 <1ms 返回 (async)
}
```

## 设计 2: 跨仓集成测试

### 测试架构
```
UsrLinuxEmu test binary
  ↓ dlopen
libcpptlm_emulator.so (CppTLM build)
  ↓ 调用
DGpuBoard shell (CppTLM source)
  ↓ sim_loop
PcieEndpointTLM (CppTLM source)
```

### 测试用例: test_bridge_dgpu_with_real_pcie_ep.cc

```cpp
#include <catch_amalgamated.hpp>
#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "gpu_driver/shared/gpu_ioctl.h"

TEST_CASE("bridge: 4 PCIe EP fixes integrated end-to-end", "[bridge][integration][stage-1-1]") {
    // 1. 加载 CppTLM plugin
    ModuleLoader::load_plugins("plugins");
    
    // 2. 打开 dGPU 设备
    auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
    REQUIRE(dev != nullptr);
    
    // 3. 验证修复 #3: pcie_config_read 返回 Vendor ID
    uint32_t vendor_id = 0;
    int ret = dev->fops->ioctl(dev->fd, GPU_IOCTL_GET_DEVICE_INFO, &vendor_id);
    REQUIRE(ret == 0);
    REQUIRE(vendor_id == 0x10DE);
    
    // 4. 验证修复 #6: backdoor_read miss 返 -ENOENT
    std::vector<uint8_t> buf(64);
    ret = dev->backdoor_read(0xDEADBEEF, buf.data(), 64);
    REQUIRE(ret == -ENOENT);
    
    // 5. 验证修复 #5: mmio_read 真实数据
    ret = dev->mmio_read(0, 0, buf.data(), 4);
    REQUIRE(ret >= 0);
    REQUIRE(buf[0] != 0);
    
    // 6. 验证修复 #7: mmio_write async
    auto start = std::chrono::high_resolution_clock::now();
    ret = dev->mmio_write(0, 0, buf.data(), 4);
    auto end = std::chrono::high_resolution_clock::now();
    REQUIRE(ret == 0);
    REQUIRE(end - start < std::chrono::microseconds(1000));
}
```

## 设计 3: 桥接层错误处理细节

### `bridge.cpp` 当前错误处理
```cpp
// bridge.cpp L65-67: CpptlmBridge::kCpptlm 返 -ENOSYS stub
```

### 升级后（如需要）
- 错误传播：UE 桥接层错误码应与 CppTLM 错误码一致（负 errno）
- 超时处理：mmio_read timeout 应映射到 UE ETIMEDOUT

### 风险
- 桥接层错误码映射可能引入新 ABI 边界（需要 Oracle 复审）

## 实施顺序

```
1. 等待 CppTLM change 完成 → 2. 同步构建 → 3. 断言升级 → 4. 集成测试 → 5. Oracle 复审 → 6. 5.5.7 gate 解锁
```

## 风险评估

| 修改 | 风险 | 缓解 |
|------|------|------|
| 断言升级 | 低（仅测试代码）| TDD 验证 |
| 跨仓集成测试 | 中（双仓构建协同）| Makefile 统一 |
| bridge.cpp 错误处理 | 中（ABI 边界）| Oracle 复审 |

## 不在设计范围

- 5.5.7 / 5.5.8 / 5.5.9 实施
- CommandProcessor 真实化
- Kernel dispatch + DMA
- 真机双轨验证
- CppTLM 端代码修改
