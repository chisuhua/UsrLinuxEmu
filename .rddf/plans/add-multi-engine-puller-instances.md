# Plan: add-multi-engine-puller-instances

**Change**: add-multi-engine-puller-instances (P1, stage-5, core-impl)
**Branch**: openspec/add-multi-engine-puller-instances
**Mode**: 轻量模式 (no worktree isolation)

## Goal

实现多引擎 Puller 实例化（COMPUTE/COPY/GRAPHICS 独立 Puller），解决 `HardwarePullerEmu` 单一通用类问题。

参考 [improvements/add-multi-engine-puller-instances.md](../../improvements/add-multi-engine-puller-instances.md) 中的范围与验收标准。

## Tasks (TDD 5 步结构)

### 1. Write failing test (Red)
- [ ] 在 `tests/test_multi_engine_puller_standalone.cpp` 新建测试 binary
- [ ] 编写 6 个 test case 覆盖：COMPUTE→COPY fence signal/wait、COPY→COMPUTE signal/wait、GRAPHICS 创建路径、engine fence registry 边界
- [ ] 验证测试 FAIL（因为新代码未实现）

### 2. Implement (Green)
- [ ] 在 `plugins/gpu_driver/sim/hardware/` 创建 `ComputePullerEmu` + `CopyPullerEmu` + `GraphicsPullerEmu` 三个类
- [ ] 修改 `GlobalScheduler::selectEngine()` 根据 `entry.type` 返回对应 puller 实例
- [ ] 维护 `engine_fence_registry_`（per-engine fence_id 空间）
- [ ] 修复 `GPU_QUEUE_GRAPHICS` 不再返回 "future" error
- [ ] 验证测试 PASS

### 3. Refactor
- [ ] 保留 `HardwarePullerEmu` 通用类作为 fallback
- [ ] 添加 `Logger::debug` 输出 engine selection
- [ ] 检查 lsp_diagnostics 0 error

### 4. Verify
- [ ] `make -j4` 编译无 warning
- [ ] `ctest --output-on-failure` 全部 PASS（基线 130/130 + 新测试）
- [ ] 提案中「验收标准」100% 完成

### 5. Commit
- [ ] `git add -A && git commit -m "feat(gpu): add multi-engine puller instances"`
- [ ] 验证 commit 成功

## 范围 / Out of Scope

参见 improvements/<name>.md §范围

## 阻塞依赖

无（per deps 静态分析）

## 风险

- **HAL 接口契约**：必须保持 `struct gpu_hal_ops` 签名不变（per ADR-023 §D4）
- **Engine dispatch 路由**：fn-ptr dispatch 需要保留 `HardwarePullerEmu` fallback

