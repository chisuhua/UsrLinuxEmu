# Tasks: add-abi-dispatch-consistency-test

## 1. Implementation

- [ ] 1.1 新增 `tests/test_ioctl_abi_dispatch_consistency_standalone.cpp`（Catch2）
- [ ] 1.2 测试内硬编码 `constexpr std::array<uint32_t, 38> ABI_IOCTL_REQUESTS`，包含全部 38 个 `GPU_IOCTL_*` 命令的 request 值（按数值排序）
- [ ] 1.3 数值取自 `plugins/gpu_driver/shared/gpu_ioctl.h`（`#define` 后的 `_IOWR(...)` 编码结果）
- [ ] 1.4 注释行引用每个值对应的 ABI 名（如 `{0x01, "PUSHBUFFER_SUBMIT_BATCH"}`）
- [ ] 1.5 测试逻辑：
- [ ] 1.6 CMake 注册：`add_standalone_test` + `add_test`，WORKING_DIRECTORY = PROJECT_SOURCE_DIR
- [ ] 1.7 测试需链接 `gpu_drv`（仅访问 `GpgpuDevice::getIoctlTablePtr()`，不需 sim/hal 链接）
- [ ] 1.8 测试成功 FAIL 时输出：缺失 request 列表 + 期望但未派发的 ABI 名 + 派发表多余项
- [ ] 1.9 AGENTS.md "关键架构决策"中增加一条"ABI—派发表漂移检测"说明

## 2. Verification

- [ ] 2.1 `tests/test_ioctl_abi_dispatch_consistency_standalone.cpp` CTest-registered 并 PASS（前提：P0 已合，活跃派发表 38 项）
- [ ] 2.2 `ABI_IOCTL_REQUESTS` 数组含 38 个值，按数值升序，每个值后有 ABI 名注释
- [ ] 2.3 `REQUIRE(std::is_sorted(ABI_IOCTL_REQUESTS.begin(), ABI_IOCTL_REQUESTS.end()))` PASS
- [ ] 2.4 `REQUIRE(GpgpuDevice::dispatchCount() == 38)` PASS
- [ ] 2.5 派发表 request 值集合 == `ABI_IOCTL_REQUESTS`（`REQUIRE` 逐项等值 + 总大小）
- [ ] 2.6 派发表无重复 request（`REQUIRE(std::adjacent_find(...) == end)`）
- [ ] 2.7 派发表无 nullptr handler（防御性 SECTION 可选）
- [ ] 2.8 测试运行时间 ≤ 100ms（不加载 plugin/HAL/sim）
- [ ] 2.9 模拟"新增 ABI 但漏接派发表"：临时注释掉派发表某项 → 测试 FAIL，CI 拦截
- [ ] 2.10 模拟"误删 ABI 但保留派发表"：临时在数组中删除某值 → 测试 FAIL，CI 拦截
- [ ] 2.11 既有 ctest 全 PASS，0 regression
- [ ] 2.12 `plugins/gpu_driver/shared/gpu_ioctl.h` ABI 头未变
- [ ] 2.13 `plugins/gpu_driver/drv/gpgpu_device.cpp` 派发表未变（除非 P0 同步合入）
- [ ] 2.14 `docs/00_adr/` 无新增 ADR（不需新架构决策）
- [ ] 2.15 AGENTS.md 增加 1 行说明"ABI—派发表漂移检测（test_ioctl_abi_dispatch_consistency）"
- [ ] 2.16 commit message 引用 openspec 事故 + 说明 P0 协同合入

## 3. Process / Documentation

- [ ] 3.1 AGENTS.md / README 中 IOCTL 列表同步（如适用）
- [ ] 3.2 提交信息包含 openspec change 名（`add-abi-dispatch-consistency-test`）+ 引用改进提案路径
- [ ] 3.3 CTest 全 PASS（`cd build && ctest --output-on-failure`），0 regression
