# Tasks: kcpptlm-backend-binding-with-handle-and-adapter-info

## 1. 结构与类型定义 (UsrLinuxEmu)

- [x] 1.1 在 `plugins/gpu_driver/shared/gpu_types.h` 中新增 `gpu_adapter_info_t` 结构体定义，对齐 CppTLM 扩展字段：`vendor_id`、`device_id`、`gpu_id`、`gfx_version`、`bdf`、`visible_vram_size`、`invisible_vram_size`、`va_region_size` 及 `bar_sizes[6]`
- [x] 1.2 在 `include/shared/gpu_hal_handles.h` 末尾追加 `gpu_adapter_handle_t` 类型别名 (`typedef uint64_t gpu_adapter_handle_t;`)
- [x] 1.3 在 `sim_hardware/include/cpptlm/backdoor_endpoint.h` 中声明 `ule_dgpu_*` 调试端点 ABI 接口与 `ule_dgpu_space` 枚举

## 2. 调试端点与 Command 模式实现 (sim_hardware)

- [x] 2.1 实现 `sim_hardware/src/cpptlm/backdoor_endpoint.cpp`：提供 `ule_dgpu_acquire`、`ule_dgpu_get_adapter_info`、`ule_dgpu_read/write`、`ule_dgpu_release`，封装 `PendingReq` 注入与 `std::future` 跨线程等待（100ms 超时防假死）
- [x] 2.2 在 `sim_hardware/src/cpptlm/bridge.cpp` 中打通 `kCpptlm` 真实 backend：实现 dlopen `cpptlm_emulator.so` 并绑定 22 个 ABI 函数符号（19 原始 + 3 扩展）
- [x] 2.3 在 `sim_hardware/src/pcie/host_bridge.cpp` 中将 `host_bridge_bypass_read/write` 完善为根据 `bypass_get_mode()` 自动分发至 `mmio_read/write` (Full) 或 `backdoor_read/write` (Bypass)

## 3. HAL 层扩展与适配器发现

- [x] 3.1 在 `plugins/gpu_driver/hal/gpu_hal.h` 末尾追加 3 个新 fn-ptr（`adapter_get_info`、`adapter_open`、`adapter_close`，68→71，符合 ADR-023 D4）及对应 inline 包装函数
- [x] 3.2 在 `plugins/gpu_driver/hal/hal_mock.cpp` 中提供 3 个新 fn-ptr 的默认 mock 实现
- [x] 3.3 在 `plugins/gpu_driver/hal/hal_user.cpp` 中实现 3 个新 fn-ptr：基于 `plugins/pci_driver` 与 Discovery 固件表解析适配器拓扑
- [x] 3.4 新增 `plugins/gpu_driver/hal/hal_cpptlm.cpp`：基于 `sim_hardware/CpptlmBridge` 与 `BackdoorEndpoint` 的真实 CppTLM 适配器驱动

## 4. 测试与验证

- [x] 4.1 编写 `tests/sim_hardware/test_backdoor_endpoint_standalone.cpp`：验证 First-touch Handle 生命周期、空间类型区分读写及并发跨线程安全性
- [x] 4.2 编写 `tests/sim_hardware/test_pcie_bypass_vs_full_standalone.cpp`：验证同一寄存器读写在 Full 模式（TLP）与 Bypass 模式（Backdoor）下行为语义的一致性
- [x] 4.3 编写 `tests/test_hal_adapter_info_standalone.cpp`：验证 HAL 层 3 个新 fn-ptr 在 `hal_user`、`hal_mock` 与 `hal_cpptlm` 上的正确性
- [x] 4.4 在 `tests/CMakeLists.txt` 中注册新增的测试二进制
- [x] 4.5 构建并运行测试，确保全绿且既有 ctest 零回归
