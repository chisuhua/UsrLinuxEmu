# Design: ue-stage-1-2-msix-integration

> **关联**: [proposal.md](../proposal.md) + [tasks.md](../tasks.md) + [specs/ue-stage-1-2-msix-integration/spec.md](../specs/ue-stage-1-2-msix-integration/spec.md)

## 设计概述

UE 侧 MSI-X 跨仓集成测试。设计原则：
1. **TDD 5 步**：先写失败测试（假设 CppTLM 已 ship），再验证
2. **dlopen 真实加载**：从 UE 进程 dlopen `libcpptlm_emulator.so`，模拟真实 dGPU E2E
3. **200ms 超时窗口验证**：对齐 entry §5.1 msix intr_cb 触发验证协议

## 测试架构

```
UsrLinuxEmu test binary (test_dgpu_msix_real_trigger_ue)
  ↓ ModuleLoader::load_plugins("plugins")
libcpptlm_emulator.so (CppTLM build, stage-1-2-msix ship)
  ↓ msix_init + register_callbacks + trigger_irq_async
  ↓ inject_q_ + sim_loop drain
intr_cb (UE 端 mock, 200ms 内被调用)
```

## 测试用例: test_dgpu_msix_real_trigger_ue.cc

```cpp
#include <catch_amalgamated.hpp>
#include "kernel/vfs.h"
#include "kernel/module_loader.h"
#include "gpu_driver/shared/gpu_ioctl.h"
#include <atomic>
#include <chrono>

TEST_CASE("UE MSI-X integration: intr_cb triggered within 200ms", "[ue][msix][integration]") {
    // 1. 加载 CppTLM plugin
    ModuleLoader::load_plugins("plugins");
    
    // 2. 打开 dGPU 设备
    auto dev = VFS::instance().open("/dev/gpgpu0", O_RDWR);
    REQUIRE(dev != nullptr);
    
    // 3. 注册 mock intr_cb
    std::atomic<int> cb_called{0};
    uint32_t captured_vector = 999;
    uint64_t captured_payload = 0;
    dev->fops->register_msix_callback([&](uint32_t vector, uint64_t payload) {
        cb_called++;
        captured_vector = vector;
        captured_payload = payload;
    });
    
    // 4. 触发 MSI-X 中断
    int ret = dev->fops->trigger_msix_async(0, 0xDEADBEEF);
    REQUIRE(ret == 0);
    
    // 5. 200ms 超时窗口验证
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (cb_called == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    REQUIRE(cb_called >= 1);
    REQUIRE(captured_vector == 0);
    REQUIRE(captured_payload == 0xDEADBEEF);
}
```

## 风险评估

| 风险 | 缓解 |
|------|------|
| dlopen 链接版本不匹配 | Makefile 同步构建 |
| 测试超时（CI 慢机器）| 200ms 是验证协议上限，不是性能断言 |
| Mock intr_cb 线程安全 | std::atomic 保证 |

## 不在设计范围

- 阶段 1.3a SDMA Ring Buffer 集成（独立 change `ue-stage-1-3-sdma-integration`）
- 阶段 1.4 电源管理集成（独立 change `ue-stage-1-4-2-1-extensions`）
