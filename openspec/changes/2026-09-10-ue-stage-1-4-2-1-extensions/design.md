# Design: ue-stage-1-4-2-1-extensions

## 测试架构

```
UsrLinuxEmu test binary × 2
  ↓ dlopen libcpptlm_emulator.so
CppTLM stage-1-4-2-1 ship
```

## 2 个集成测试

### test_dgpu_power_mgmt_ue.cc
```cpp
TEST_CASE("UE: PM D0/D3 + ASPM integration", "[ue][pm][integration]") {
    ModuleLoader::load_plugins("plugins");
    auto dev = VFS::open("/dev/gpgpu0");
    // set_power_state(D3hot) → PMCSR = D3
    // wake → D0
    // enable_aspm(L1) → 链路进入 L1
}
```

### test_dgpu_p2p_ue.cc
```cpp
TEST_CASE("UE: P2P DMA + ACS + Resizable BAR integration", "[ue][p2p][integration]") {
    // p2p_dma_route(src, dst, addr, len) → 路由成功
    // ACS deny → -EPERM
    // resize_bar(0, 256MB) → BAR0 大小更新
}
```
