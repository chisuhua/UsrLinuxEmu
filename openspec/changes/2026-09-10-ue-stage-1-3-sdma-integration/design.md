# Design: ue-stage-1-3-sdma-integration

## 测试架构

```
UsrLinuxEmu test binary × 4
  ↓ ModuleLoader::load_plugins("plugins")
libcpptlm_emulator.so (CppTLM stage-1-3-sdma ship)
  ↓ Ring Buffer / D2D / dma_translate / completion
  ↓ 端到端验证
```

## 4 个集成测试

### test_dgpu_sdma_ring_buffer_ue.cc (1.3a)
```cpp
TEST_CASE("UE: SDMA Ring Buffer cfg.ring_size 4 sizes", "[ue][sdma][ring]") {
    ModuleLoader::load_plugins("plugins");
    auto dev = VFS::open("/dev/gpgpu0");
    // 验证 cfg.ring_size=64KB 写入 WPTR 触发 Doorbell @BAR1+0x10010000
    // SG 链 8 描述符
}
```

### test_dgpu_d2d_noc_ue.cc (1.3b)
```cpp
TEST_CASE("UE: D2D NoC payload ≥100 GB/s + host_out=0", "[ue][sdma][d2d]") {
    // d2d_noc_forward 1MB payload, 验证带宽 ≥100 GB/s
    // 断言 host_out 端口 0 事务
}
```

### test_dgpu_dma_translate_ue.cc (1.3c, 修复 #2)
```cpp
TEST_CASE("UE: dma_translate identity pa=iova + IOMMU 负 errno", "[ue][sdma][translate]") {
    // identity 模式 cb 真实调用，pa==iova
    // IOMMU 模式 cb 失败，error_cb 收到 -ENOSYS/-EIO
}
```

### test_dgpu_sdma_completion_ue.cc (1.3d)
```cpp
TEST_CASE("UE: Fence + MSI-X 接线 200ms intr_cb", "[ue][sdma][completion]") {
    // Fence descriptor → CompletionRing → MSI-X → intr_cb
    // 200ms 超时窗口内 cb_called ≥ 1
}
```

## 风险

| 风险 | 缓解 |
|------|------|
| 测试超时（CI 慢） | 200ms 是验证上限，非性能断言 |
| dlopen 链接版本 | Makefile 同步构建 |

## 不在设计范围

- 阶段 1.4 电源管理集成（独立 change `ue-stage-1-4-2-1-extensions`）
