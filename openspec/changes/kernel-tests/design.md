# kernel-tests: 技术设计

## Context

`include/kernel/` 有 15 个 header 文件，0 个测试。kernel 层是 UsrLinuxEmu 的核心模拟环境——VFS 单例（Meyers singleton）、ModuleLoader、scheduler、device model 等关键组件均在其中。VFS singleton 的隔离性（Issue #11）和 device 注册的正确性是当前架构中最脆弱的点，缺少测试验证。

## Goals / Non-Goals

**Goals:**
- 为 kernel 层 4 个核心组件添加 Catch2 单元测试：VFS、Device model、Scheduler、Module Loader
- 覆盖正常路径（open/read/write/register/create/destroy）和关键错误路径（NULL 文件、不存在设备）
- 测试所有从项目根目录运行（`./build/bin/test_kernel_standalone`）

**Non-Goals:**
- 不修改 kernel 层实现代码（纯测试增量）
- 不涉及性能/压力测试（仅功能正确性）
- 不修改 `src/kernel/` 或 `include/kernel/` 中的任何文件

## Decisions

### Decision 1: 使用 Catch2 SECTION 隔离每个组件

**选择**: 单个 `test_kernel.cpp` 文件（或以 `test_kernel_vfs.cpp` 等独立文件），每个组件用 `TEST_CASE` + `SECTION` 划分场景。

**理由**:
- Catch2 SECTION 机制天然支持组件内场景隔离
- 与项目现有 30+ Catch2 测试风格保持一致
- 单个可执行文件编译快，CI 并行度可通过 ctest 设置

**替代方案**:
- 每组件独立可执行文件: CMake target 过多，管理成本高
- GTest 框架: 项目已迁移到 Catch2（ADR-010），不使用 GTest

### Decision 2: VFS 测试使用真实 VFS 单例

**选择**: VFS 测试直接使用 `VFS::instance()`（Meyers singleton），通过 `ModuleLoader::load_plugins("plugins")` 加载设备后操作。

**理由**:
- 测试 kernel SHARED 库的单例隔离（Issue #11 的回归保护）
- 不需要 mock VFS（mock 本身可能引入假阳性）
- 需要从项目根目录运行（插件相对路径）

### Decision 3: 测试从项目根目录运行

**选择**: 测试二进制编译到 `build/bin/`，但运行时 cwd 必须在项目根目录。

**理由**:
- `ModuleLoader::load_plugins("plugins")` 使用相对路径
- 所有现有 30+ 测试都遵循此约定
- 避免硬编码绝对路径（可移植性）

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| VFS singleton 状态污染导致测试间耦合 | Catch2 SECTION 隔离 + 测试后重置 VFS 状态 |
| Module loading 性能影响 CI 速度 | module_loader 测试只加载一次，复用单例 |
| Scheduler 依赖时间变更导致 flaky test | 不测时间相关行为，仅测调度正确性 |