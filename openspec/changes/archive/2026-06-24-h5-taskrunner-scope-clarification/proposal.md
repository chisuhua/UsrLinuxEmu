# H-5: TaskRunner Scope Clarification（双轨演进）

> **状态**: 📋 PROPOSED (2026-06-24)
> **创建**: 2026-06-24
> **目标**: 把 TaskRunner 子模块拆分为两个明确范畴（test-fixture 默认主线 + umd-evolution 演进目标），消除当前文档/代码/TADR 混杂导致的实施混淆
> **前置依赖**:
>   - ✅ H-1 pushbuffer-validation-closeout (已 archived 2026-06-17, PR #6)
>   - ✅ H-2.5 architecture-foundation (已 archived 2026-06-22)
>   - ✅ H-3 phase2-management (已 archived 2026-06-23, `241f3ed..8625b82`)
>   - ✅ H-4 architecture-governance-cleanup (已 archived 2026-06-23)
> **后续约束**: H-3.5 follow-up 工作必须在 test-fixture 范畴下进行；umd-evolution 范畴**不**阻塞主线演进

## Why

TaskRunner 子模块当前文档/代码/TADR 混杂了两种范畴的内容：

1. **方向 A（测试夹具）**：H-2.5 引入的 IGpuDriver 抽象 + H-3 的 Phase 2 实施 + 28 方法 DI + CLI 调试工具 → **已完成**，是当前 main 实际状态
2. **方向 B/C（用户态驱动）**：原 `plan.md` v0.1 提案中的 libcuda.so/libvulkan.so 完整愿景、Stream/Context 模型、CUmodule 加载 → **未实施**，仅作为历史愿景保留

混在一起的副作用：
- 新人按 UMD 愿景理解代码，导致错误决策（参考 `docs/roadmap/retrospective.md` 已识别的 v0.1 vs 实际偏差）
- H-1/H-2.5/H-3 实施时面临"这是 stub 还是 UMD？"的范畴混淆
- `TADR-001~003`（UMD 愿景）vs `TADR-004~008`（test-fixture）的语义混杂在 8 个文件中
- `tadr-004` Stub Tracker 与 `tadr-001` Unified Scheduler 提出的"完整 CUDA Runtime"愿景自相矛盾
- H-7 deferred（TADR-008）的 3 个 issue 是"UMD 范畴"但 TaskRunner 不解决——这种范畴模糊使新人难以理解

**Why Now**:
1. **H-4 governance cleanup 已完成**（2026-06-23），文档治理框架就绪，引入范畴分隔有制度基础
2. **H-7 deferred 范围明确**（TADR-008），3 个上游 issue 不再让 TaskRunner 承担 UMD 责任
3. **H-3.5 follow-up 未启动**（TADR-006 §Follow-up），此时引入范畴分隔不会影响后续 P0 修复工作
4. **H-2.5 + H-3 已 shippable**（plans/sync-plan.md v2.1），主线已经稳定，引入双轨不会破坏现有功能

## What Changes

### 1. 文档双轨制
- 把现有 `docs/architecture/`、`docs/roadmap/`、`docs/adr/` 按范畴拆分为 `docs/test-fixture/` + `docs/umd-evolution/` + `docs/shared/`
- 每个文档头部加 SCOPE/STATUS 元数据（前言块）
- 新建 `docs/umd-evolution/vision.md`（UMD 完整愿景，从 `plan.md` v0.1 提取）+ `gap-analysis.md`（与 AMD ROCm/NVIDIA UMD 差距，2026-06-24 4 路调研结果）

### 2. TADR 编号重映射（8 个 TADR → 16 个范畴分段）
- **UMD 范畴**（vision 文档）：`tadr-001~003` → `tadr-201~203`，移到 `docs/umd-evolution/adr/`
- **test-fixture 范畴**（current main）：`tadr-004~008` → `tadr-101~105`，移到 `docs/test-fixture/adr/`
- **新增 shared 范畴 TADR**（不属 test-fixture 或 UMD）：`tadr-301~304`
- **新增范畴规范 TADR**：`tadr-106`（test-fixture scope）、`tadr-204`（UMD scope）、`tadr-107`（shared boundary）
- 保留 8 个 redirect 文件（旧编号 → 新编号）以兼容历史链接

### 3. 共享基础设施规范
- 把 `include/igpu_driver.hpp` + `include/sync_primitives.hpp` + `include/error_handling.hpp` 移到 `include/shared/`
- 新增 `tadr-301`（Shared Infrastructure Boundary）+ `tadr-302`（IGpuDriver Contract）+ `tadr-303`（Sync Primitives）+ `tadr-304`（Error Handling）

### 4. CMake BUILD_MODE 开关
- 新增 `TASKRUNNER_BUILD_MODE` CMake option：`test-fixture`（默认）/ `umd-evolution`
- 新增 `cmake/Shared.cmake`（始终构建）+ `cmake/TestFixture.cmake`（test-fixture 模式）+ `cmake/UMDEvolution.cmake`（UMD 实验模式）
- target 命名规范：`taskrunner_shared`（STATIC）+ `taskrunner_test_fixture`（STATIC）+ `taskrunner_umd_stub`（SHARED，仅 umd-evolution 模式）

### 5. UMD 演进代码骨架（实验性）
- 创建 `include/umd/` + `src/umd/` + `tests/umd/` 骨架（含占位头文件 + 空 cpp）
- 提供未来 doorbell mmap 旁路 / ring buffer 自管理 / 最小 CUDA Runtime API 表面的入口
- **不**实施具体逻辑（仅占位 + 占位类声明）
- umd-evolution 模式默认关闭（`TASKRUNNER_BUILD_MODE=test-fixture`）

### 6. 跨仓同步
- 按 ADR-035 §Rule 5.1 4 步流程：
  1. TaskRunner 端 commit + push
  2. UsrLinuxEmu 端 `git add external/TaskRunner`（更新 submodule 指针）
  3. UsrLinuxEmu 端 `docs/00_adr/README.md` TaskRunner TADR mirror 段更新（增加"范畴"列）
  4. 跨仓 PR

## Capabilities

### New Capabilities

- **`taskrunner-test-fixture-scope`**: test-fixture 范畴规范 — 文档/代码/TADR 边界、SCOPE 头部、跨范畴引用规范、tadr-101~105 重映射
- **`taskrunner-umd-evolution-scope`**: umd-evolution 范畴规范 — 文档/代码骨架、实验性标注、PoC 路径、tadr-201~203 重映射
- **`taskrunner-shared-infrastructure`**: 共享基础设施规范 — shared 范畴边界、IGpuDriver 契约、sync primitives、error handling 抽象（tadr-301~304）
- **`taskrunner-build-mode`**: CMake BUILD_MODE 开关规范 — `TASKRUNNER_BUILD_MODE` option、`cmake/*.cmake` 模块化、target 命名空间隔离

### Modified Capabilities

（无现有 spec 需修改 — 这是全新范畴分类，无 requirement 变更）

## Impact

### 受影响代码路径
- `external/TaskRunner/docs/` — 完整重组（test-fixture + umd-evolution + shared 三个并列目录）
- `external/TaskRunner/include/` — `igpu_driver.hpp` + `sync_primitives.hpp` + `error_handling.hpp` 移到 shared/，新增 umd/ 骨架
- `external/TaskRunner/src/` — 新增 umd/ 骨架 + shared/ 独立子目录
- `external/TaskRunner/tests/` — 新增 shared/ 和 umd/ 子目录
- `external/TaskRunner/CMakeLists.txt` — 添加 `TASKRUNNER_BUILD_MODE` option
- `external/TaskRunner/cmake/` — 新增 3 个 cmake 模块

### 受影响 TADR（16 个）
- **重映射**：tadr-001~008 → 1xx/2xx（8 个原 TADR + 8 个 redirect）
- **新增 test-fixture 范畴**：tadr-106（scope 规范）
- **新增 umd-evolution 范畴**：tadr-204（scope 规范）+ tadr-205（PoC 路径）
- **新增 shared 范畴**：tadr-301（boundary）+ tadr-302（IGpuDriver 契约）+ tadr-303（sync primitives）+ tadr-304（error handling）

### 受影响 UsrLinuxEmu 端
- `docs/00_adr/README.md` TaskRunner TADR mirror 段（增加"范畴"列）
- `docs/00_adr/README.md` 可能新增一段说明双轨分类与 4 步同步流程
- **不**涉及 drv/ / sim/ / hal/ 等内核侧代码
- **不**影响 H-7 deferred 工作（TADR-008）
- **不**影响 UsrLinuxEmu 端 roadmap

### 受影响外部
- **无** — 纯内部重构 + 文档组织，不改变 TaskRunner 与 UsrLinuxEmu 之间的 ioctl 接口契约
- 不改变 IGpuDriver 28 方法签名
- 不改变 GPU_IOCTL_* ioctl 编号

## Non-Goals（明确不做什么）

- **不**演化为真实生产用户态驱动（方向 C 不推荐）
- **不**实施 CUmodule/CUfunction 加载（libcuda.so 复杂度最大部分）
- **不**实施完整 CUDA Runtime API（cudaMalloc/cudaMemcpy/cudaLaunchKernel 全部）
- **不**实施 Unified Memory + Page Table
- **不**实施 PSX JIT（libnvidia-ptxjitcompiler 等价）
- **不**修改 CudaStub / GpuDriverClient / CudaScheduler 当前行为（仅移动文件位置）
- **不**变更 IGpuDriver 28 方法签名
- **不**变更 GPU_IOCTL_* ioctl 接口
- **不**修改 UsrLinuxEmu 主仓的 drv/ / sim/ / hal/ 任何代码
- **不**修改 H-7 deferred 3 个 issue 的解决方案（TADR-008）
- **不**修改 H-3.5 follow-up 的范围（仅要求在 test-fixture 范畴下进行）

## Open Questions

1. **umd-evolution PoC 启动时间**：建议在 H-3.5 完成后启动（不阻塞主线），待 Phase D 评估后决定
2. **共享区 review 流程**：共享区代码变更影响双向，需要更严格的 review 流程（待讨论）
3. **TADR 编号空间长期维护**：未来 tadr-3xx 共享区是否进一步分段（如 3xx-shared / 3xx-meta）？待观察
4. **跨仓 mirror 更新**：UsrLinuxEmu 端 `docs/00_adr/README.md` 现状是简短表格，是否需要扩展为按范畴分组的完整列表？需要时再调整