# linux-compat-tests: 技术设计

## Context

`include/linux_compat/` 有 9 个 header 文件，0 个测试。linux_compat 层是 UsrLinuxEmu 与 Linux kernel API 兼容的关键桥梁，提供 `u8/u32/GFP_*` 类型、`ERR_PTR/PTR_ERR` 宏、`_IOR/_IOWR` ioctl 编码等内核 API 的用户态实现。这些兼容层定义的正确性是整个 3 区分架构（ADR-036）的基础——如果类型不匹配或宏展开错误，② 可移植驱动代码无法正确表达语义。

## Goals / Non-Goals

**Goals:**
- 为 linux_compat 层 4 个关键模块添加 Catch2 测试：types.h、iommu_domain.h、drm_device.h、pci_device.h
- 验证类型定义、宏展开、API 契约的正确性
- 覆盖 `ERR_PTR` / `PTR_ERR` / `IS_ERR` 宏的正确行为

**Non-Goals:**
- 不修改 linux_compat 头文件（纯测试验证）
- 不涉及真实 IOMMU/DRM/PCI 驱动（仅测试接口契约）
- 不修改 `plugins/` 或 `src/` 中的实现代码

## Decisions

### Decision 1: 编译时 + 运行时双验证

**选择**: 类型测试以编译时 `static_assert` 为主（sizeof/alignment），宏行为以运行时 Catch2 `REQUIRE` 为主。

**理由**:
- 类型定义错误是编译期 bug，`static_assert` 零运行时开销
- 宏行为（如 `ERR_PTR(-EINVAL)` vs `PTR_ERR()`）需要运行时验证
- 与 Linux kernel 习惯一致（kernel 用 `BUILD_BUG_ON`）

**替代方案**:
- 纯运行时验证: 无法捕获 alignment 和 size 问题
- 纯编译时: 无法验证 `IS_ERR` 的条件逻辑

### Decision 2: 独立测试文件，按模块分组

**选择**: 为 linux_compat 创建独立的测试可执行文件（如 `test_linux_compat_standalone`），内部按模块用 `TEST_CASE` 组织。

**理由**:
- linux_compat 层不依赖 kernel SHARED 库（仅头文件），测试更轻量
- 独立可执行文件编译快，CI 并行友好
- 与项目现有 `test_*_standalone` 命名规范一致

### Decision 3: 不 mock 任何 kernel 组件

**选择**: linux_compat 测试纯头文件测试，不链接 kernel 库。

**理由**:
- linux_compat 头文件不依赖 `src/kernel/` 实现
- 减少测试依赖链，构建更快
- 更准确地隔离测试目标

## Risks / Trade-offs

| Risk | Mitigation |
|------|------------|
| IOMMU domain 测试依赖真实 IOMMU 实现 | 只测接口契约（类型正确、函数签名兼容），不测运行时行为 |
| DRM/PCI header 包含大量 inline 函数 | 编译测试即可验证（能 include + 模板实例化 = 通过） |
| 类型测试可能因平台差异失败 | 使用 `sizeof` + `alignof` 断言，非硬编码值 |