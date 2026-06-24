# Design: H-5 TaskRunner Scope Clarification

> **依赖**: proposal ✅, specs ✅
> **状态**: 📋 PROPOSED (2026-06-24)
> **目标**: 详细设计双轨演进的具体实施步骤、技术决策、迁移策略

## Context

### 背景与现状

TaskRunner 子模块（路径 `/workspace/project/UsrLinuxEmu/external/TaskRunner`）当前混合了两类内容：

1. **可执行的 test-fixture 范畴代码**（H-2.5 + H-3 已 shippable）：
   - `IGpuDriver` 抽象（28 方法）
   - `GpuDriverClient`（真 ioctl 客户端）
   - `CudaStub`（in-memory mock）
   - `MockGpuDriver`（headless 测试夹具）
   - `CudaScheduler`（含 `dynamic_cast<CudaStub*>` 抽象泄漏）
   - CLI 工具（6 个子命令）
   - 测试（test_cuda_scheduler 8/8、test_gpu_phase2 12/12）

2. **UMD 范畴的设计愿景**（来自 `docs/plan.md` v0.1 + `tadr-001~003`）：
   - UnifiedScheduler 中央调度
   - CommandTranslator 命令翻译层
   - SyncSource/SyncManager 同步统一抽象
   - 完整 CUDA Runtime API 表面（cudaMalloc/cudaMemcpy/cudaLaunchKernel）
   - Stream/Context 模型
   - CUmodule/CUfunction 加载
   - Vulkan Compute stub

**关键混叠点**：
- `tadr-004`（Stub Tracker，当前规范）vs `tadr-001`（Unified Scheduler，未实施）语义相反，文件名紧邻
- `docs/plan.md` v0.1 提出 libcuda.so 完整愿景，`docs/roadmap/retrospective.md` 已识别 v0.1 vs 实际偏差，但**未**显式化"test-fixture vs UMD"分类
- `tadr-008` H-7 deferred 3 个 issue 是"UMD 范畴上游问题，TaskRunner 不解决"，但与 tadr-101~105 test-fixture 范畴混杂

### 约束

- **AGENTS.md 跨仓协议**：所有 TaskRunner 改动按 ADR-035 §Rule 5.1 4 步同步
- **C++17 标准**：当前代码已用 C++17，重构 MUST 保持兼容
- **doctest 测试**：现有测试基于 doctest 框架，路径变更 MUST 保持测试通过
- **共享 submodule 指针**：UsrLinuxEmu 端 `external/TaskRunner` 是 submodule，commit 后立即更新指针

### 利益相关方

- **TaskRunner 维护者**：负责本次重构 + 后续 test-fixture 演进
- **UsrLinuxEmu 维护者**：负责跨仓 PR review + docs/00_adr/README.md mirror 同步
- **未来 umd-evolution 贡献者**：需要明确的"范畴入口"了解如何贡献
- **集成测试用户**：H-3.5 follow-up 工作必须在 test-fixture 范畴下进行

## Goals / Non-Goals

**Goals:**
- 严格区分 test-fixture vs umd-evolution 范畴，消除实施混淆
- 共享基础设施（IGpuDriver / sync_primitives / error_handling）独立共享区
- TADR 编号按范畴分段（1xx/2xx/3xx），历史链接通过 redirect 保留
- CMake `TASKRUNNER_BUILD_MODE` option 切换范畴，默认 test-fixture
- umd-evolution 提供代码骨架（占位实现），不阻塞主线
- 跨仓同步 + docs/00_adr/README.md mirror 更新

**Non-Goals:**
- 不演化为真实生产用户态驱动（方向 C 不推荐）
- 不实施 CUmodule/CUfunction 加载、不实施完整 CUDA Runtime API
- 不修改 UsrLinuxEmu 主仓 drv/sim/hal 任何代码
- 不修改 H-7 deferred 解决方案（TADR-008）
- 不修改 IGpuDriver 28 方法签名（保持 ABI 稳定）
- 不修改 GPU_IOCTL_* ioctl 编号（保持 ioctl 兼容）

## Decisions

### Decision 1: 单一 git 仓库 + 文档/代码范畴分段（**不**拆 submodule）

**选项**：
- A. 单一 git 仓库，文档/代码按范畴分段
- B. 拆分为两个 git submodule（test-fixture + umd-evolution）
- C. 拆分为两个独立 git 仓 + UsrLinuxEmu 双重 submodule

**选择**: **A**

**理由**：
- B/C 会导致跨仓同步负担 ×3（TaskRunner-test-fixture + TaskRunner-umd + UsrLinuxEmu）
- 共享基础设施（IGpuDriver / sync_primitives / error_handling）需要双向引用，跨 submodule 会增加循环依赖风险
- 当前 UsrLinuxEmu 已是 `external/TaskRunner` submodule，再加 submodule 违反"扁平 submodule"惯例
- ADR-035 §Rule 5.1 4 步同步协议明确假设 TaskRunner 是**单一仓**
- CudaStub 与 CudaScheduler 互相依赖，跨仓拆分需要重新设计边界

**代价**：
- 范畴隔离需通过**目录命名 + CMake option**强制（而非物理仓隔离）
- 需要 reviewer 主动检查"修改是否在正确范畴"

### Decision 2: TADR 编号重映射（1xx/2xx/3xx）+ redirect 文件

**选项**：
- A. 完全重映射（tadr-001~008 → 1xx/2xx）+ redirect 文件
- B. 保留旧编号 + 范畴前缀（`tadr-A-001`、`tadr-B-001`）
- C. 保留旧编号 + 范畴标签（不动编号，文档加 SCOPE 标签）

**选择**: **A**

**理由**：
- 编号段分段（1xx/2xx/3xx）让"这是什么范畴"一眼可见
- redirect 文件保留历史链接兼容（`tadr-004-redirect.md` 指向 `tadr-101`）
- `git mv` 保留 git history，`git log --follow` 可追

**代价**：
- 8 个原 TADR 全部需要重命名 + 内部引用更新
- UsrLinuxEmu 端 `docs/00_adr/README.md` mirror 表需要同步更新

### Decision 3: `TASKRUNNER_BUILD_MODE` 默认 `test-fixture`

**选项**：
- A. 默认 `test-fixture`，UMD 需显式 `-DTASKRUNNER_BUILD_MODE=umd-evolution`
- B. 默认 `umd-evolution`，test-fixture 需显式切换
- C. 不提供 mode 切换，两个范畴都编译（target 命名空间隔离）

**选择**: **A**

**理由**：
- 主线（H-3 已 shippable）是 test-fixture，默认值应该走主线
- umd-evolution 是实验性骨架，**不应**默认构建（避免污染主线 build）
- A 选项保证 CI / 默认 build 不会编译 umd-evolution 范畴代码
- C 选项会导致 umd-evolution 骨架被默认构建，长期腐烂风险高

**代价**：
- 需要在 README 中明确说明"如何切换到 umd-evolution 模式"
- umd-evolution 测试不会在默认 CI 中跑

### Decision 4: shared 区代码 review 严格化（双向 review）

**选项**：
- A. shared 区变更需要 test-fixture + umd-evolution 双向 review
- B. shared 区变更需要任意 1 个 maintainer review
- C. shared 区变更与普通代码无差异

**选择**: **A**

**理由**：
- shared 区代码影响双向（test-fixture 和 umd-evolution 都引用）
- 防止"为了 test-fixture 优化破坏 umd-evolution 接口"或反之
- 即使 umd-evolution 尚未活跃，也需要 maintainer 视角

**代价**：
- PR review 时间增长（需要 2 个 maintainer）
- umd-evolution maintainer 暂缺时需要暂存审查

### Decision 5: umd-evolution 代码骨架仅占位（**不**实现具体逻辑）

**选项**：
- A. 仅占位（头文件 + 空 cpp + `throw not_implemented`）
- B. 实现 doorbell mmap 旁路最小 PoC（200-500 LOC）
- C. 实现完整 cudaMalloc/cudaMemcpy/cudaLaunchKernel（5000+ LOC）

**选择**: **A**

**理由**：
- B/C 远超当前 change 范围（应作为后续独立 change）
- A 选项保留"演进入口"但不引入未完成逻辑
- 占位类声明让未来 PoC 可以基于此骨架增量演进

**代价**：
- umd-evolution target 编译后立即可发现缺哪些方法实现（驱动未来 PoC）

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| **目录移动破坏 git blame** | 使用 `git mv` 而非 `mv` + `git add`；`git log --follow` 可追 |
| **跨仓同步协议未更新** | Phase C 之前先验证 UsrLinuxEmu 端 `adr-035-governance-policy.md` §Rule 5.1 4 步流程 |
| **现有 H-3.5/H-7 工作被阻断** | Phase A/B/C 都不改代码逻辑，只移动文件+改 include 路径，**不**影响功能 |
| **TADR 重映射导致历史链接断裂** | redirect 文件保留旧编号 + `git mv` 保留 git history |
| **umd-evolution 真实代码腐烂** | 默认 `TASKRUNNER_BUILD_MODE=test-fixture`，umd-evolution 需显式开启 |
| **共享区代码变更影响两个范畴** | 共享区 review 严格化（任何 PR 必须双向评估影响） |
| **CMake option 名称冲突** | 用 `TASKRUNNER_BUILD_MODE` 明确命名（不与 BUILD_TESTING 等冲突） |
| **CudaStub 与 CudaScheduler 互相依赖** | Phase B 中分析依赖，必要时把公共部分提取到 shared |
| **docs/00_adr/README.md mirror 表格式未对齐** | Phase C 中先调研现状，再确定 mirror 表格式扩展 |
| **测试路径变更导致 CI 失败** | Phase B 中同步更新 CI 脚本（如有）|

## Migration Plan

### Phase A：文档目录重组 + TADR 重映射（1-2 周）

**入口条件**：H-4 已完成（2026-06-23）

**任务**：
1. 创建 `docs/test-fixture/`、`docs/umd-evolution/`、`docs/shared/` 目录
2. 用 `git mv` 移动现有 `docs/architecture/` → `docs/test-fixture/architecture/`
3. 用 `git mv` 移动现有 `docs/roadmap/` → `docs/test-fixture/roadmap/`
4. 创建 `docs/test-fixture/README.md` + `docs/umd-evolution/README.md` + `docs/shared/README.md`（含 SCOPE 规范）
5. 创建 `docs/umd-evolution/vision.md`（从 `plan.md` v0.1 提取 UMD 完整愿景）
6. 创建 `docs/umd-evolution/gap-analysis.md`（基于 2026-06-24 4 路调研结果）
7. 创建 `docs/umd-evolution/architecture/`（UMD 范畴专属架构）
8. 用 `git mv` 移动 8 个 TADR + 重命名 + 加 SCOPE 元数据
9. 创建 8 个 redirect 文件（tadr-004-redirect.md 等）
10. 创建 4 个 shared 范畴 TADR（tadr-301~304）
11. 创建 3 个范畴规范 TADR（tadr-106 test-fixture scope、tadr-204 UMD scope、tadr-107 shared boundary）

**验证**：
- 所有 redirect 文件可访问（`cat docs/test-fixture/adr/tadr-004-redirect.md`）
- `git log --follow docs/test-fixture/adr/tadr-101-stub-tracker.md` 可追历史
- 文档头部 SCOPE 元数据完整

**回滚**：
- `git revert` 整个 commit
- 或 `git mv` 反向操作（git 会跟踪重命名）

### Phase B：代码目录重组 + CMake 改造（1-2 周）

**入口条件**：Phase A 完成

**任务**：
1. 创建 `include/shared/` 和 `src/shared/` 目录
2. 移动 `include/igpu_driver.hpp` → `include/shared/igpu_driver.hpp`
3. 移动 `include/sync_primitives.hpp` → `include/shared/sync_primitives.hpp`
4. 移动 `include/error_handling.hpp` → `include/shared/error_handling.hpp`（如存在）
5. 创建 `include/test_fixture/` 和 `src/test_fixture/` 目录
6. 移动 test-fixture 范畴代码：
   - `include/cuda_stub.hpp` → `include/test_fixture/`
   - `include/cuda_scheduler.hpp` → `include/test_fixture/`
   - `include/gpu_driver_client.h` → `include/test_fixture/`
   - `src/cuda_stub.cpp` → `src/test_fixture/`
   - `src/cuda_scheduler.cpp` → `src/test_fixture/`
   - `src/gpu_driver_client.cpp` → `src/test_fixture/`
   - `src/cmd_cuda.cpp` → `src/test_fixture/`
7. 创建 `include/umd/` 和 `src/umd/` **骨架**（空文件 + 头文件占位 + 类声明）
8. 创建 `include/umd/cuda_api.hpp`（声明类 `CudaApi`，方法全部 stub 抛 `not_implemented`）
9. 创建 `include/umd/module_loader.hpp`（声明类 `ModuleLoader`，同上）
10. 创建 `include/umd/ring_buffer.hpp`（声明类 `RingBuffer`，同上）
11. 创建 `src/umd/cuda_api.cpp`（占位实现）
12. 创建 `src/umd/module_loader.cpp`（占位实现）
13. 创建 `src/umd/ring_buffer.cpp`（占位实现）
14. 更新所有 include 路径（含 `#include "shared/igpu_driver.hpp"` 形式）
15. 更新 `CMakeLists.txt`（添加 `TASKRUNNER_BUILD_MODE` option）
16. 创建 `cmake/Shared.cmake`、`cmake/TestFixture.cmake`、`cmake/UMDEvolution.cmake`
17. 创建 `tests/shared/` 和 `tests/umd/` 子目录（骨架）
18. 移动 `tests/test_cuda_scheduler.cpp` → `tests/test_fixture/`
19. 移动 `tests/test_gpu_architecture.cpp` → `tests/test_fixture/`
20. 移动 `tests/test_gpu_phase2.cpp` → `tests/test_fixture/`
21. 移动 `tests/mock_gpu_driver.hpp` → `tests/test_fixture/`

**验证**：
- `mkdir -p build && cd build && cmake .. && make -j4` 成功
- `./test_cuda_scheduler` 8/8 通过
- `./test_gpu_phase2` 12/12 通过
- `cmake .. -DTASKRUNNER_BUILD_MODE=umd-evolution && make -j4` 也成功（编译 umd_stub）
- 默认 build 模式不引用 `src/umd/`（`grep -r "src/umd" build/CMakeFiles/` 无匹配）

**回滚**：
- `git revert` Phase B 整个 commit
- 由于 Phase B 是纯文件移动 + include 路径更新，回滚后功能完全一致

### Phase C：跨仓同步（1 周）

**入口条件**：Phase B 完成 + CI 绿

**任务**（按 ADR-035 §Rule 5.1 4 步）：
1. **TaskRunner 端**（submodule 仓库）：
   - `git add` 所有改动（含 `git mv` 的历史）
   - `git commit -m "feat(scope): H-5 TaskRunner scope clarification - test-fixture + umd-evolution + shared"`
   - `git push origin main`
2. **UsrLinuxEmu 端**（主仓）：
   - `git add external/TaskRunner`（更新 submodule 指针）
   - `git commit -m "chore(submodule): bump TaskRunner to H-5 (scope clarification + 16 TADR remap)"`
3. **UsrLinuxEmu 端 docs/00_adr/README.md** mirror 更新：
   - 增加"范畴"列（test-fixture / umd-evolution / shared）
   - 列出 16 个重映射 TADR + 7 个新增 TADR
   - 增加"双轨分类 + 4 步同步流程"说明段
   - `git commit -m "docs(adr): update TaskRunner TADR mirror (H-5 scope clarification)"`
4. **PR**：
   - 跨仓 PR 包含 3 个 commit（submodule pointer + TADR mirror + 共享更新）
   - 至少 1 个 TaskRunner maintainer + 1 个 UsrLinuxEmu maintainer review

**验证**：
- UsrLinuxEmu 端 `git log --oneline -3` 显示 3 个 commit 顺序正确
- `docs/00_adr/README.md` mirror 表内容完整
- 跨仓 PR 通过所有 CI 检查

**回滚**：
- 关闭 PR + revert UsrLinuxEmu 端 submodule pointer commit

### Phase D：umd-evolution PoC 启动（**可选**，1-2 月，独立 change）

**入口条件**：Phase C 完成 + H-3.5 已 shippable

**任务**（作为独立 change，**不**包含在 H-5）：
1. 设计 doorbell mmap 旁路 PoC（参考 ROCm 模式：`amd_aql_queue.cpp:482-493`）
2. 设计最小 CUDA Runtime API 表面 PoC（cudaMalloc/cudaMemcpy/cudaLaunchKernel 3 个 API）
3. 设计 ELF + CUBIN 解析最小 PoC（仅 kernel symbol 提取，不需完整 SASS）
4. 添加 `TASKRUNNER_BUILD_MODE=umd-evolution` build 测试

**注**：Phase D 作为独立 change，**不**阻塞主线 test-fixture 演进。

### Phase E：现有 H-3.5/H-7 工作继续（**不**受重构影响）

**任务**（与 Phase A/B/C **并行**）：
- P0：修复 CudaScheduler 抽象泄漏（`dynamic_cast<CudaStub*>`）
- P0：MockGpuDriver guard 偏差（T6-T9）
- P1：H-7 ADR 跟踪（3 个上游 issue）

**注**：这些工作**仅**修改代码逻辑，include 路径变更可以一次性完成。

## Open Questions

1. **umd-evolution PoC 启动时间**：建议在 H-3.5 完成后启动（不阻塞主线），待 Phase D 评估后决定
2. **共享区 review 流程**：共享区代码变更影响双向，需要更严格的 review 流程（建议建立 `docs/shared/review-process.md`）
3. **TADR 编号空间长期维护**：未来 tadr-3xx 共享区是否进一步分段（如 3xx-shared / 3xx-meta）？待观察
4. **跨仓 mirror 更新**：UsrLinuxEmu 端 `docs/00_adr/README.md` 现状是简短表格，是否需要扩展为按范畴分组的完整列表？需要时再调整
5. **umd-evolution 启动门槛**：H-5 完成 + H-3.5 shippable 后，是否自动开启 umd-evolution PoC？或等待明确需求？建议等待明确需求
6. **错误处理抽象现状**：`include/error_handling.hpp` 当前是否已存在？Phase B Step 4 中需要先调研

## Implementation Notes

### Include 路径约定

```cpp
// shared 范畴（始终构建）
#include "shared/igpu_driver.hpp"
#include "shared/sync_primitives.hpp"
#include "shared/error_handling.hpp"

// test-fixture 范畴（默认）
#include "test_fixture/cuda_stub.hpp"
#include "test_fixture/cuda_scheduler.hpp"
#include "test_fixture/gpu_driver_client.h"

// umd-evolution 范畴（实验性）
#include "umd/cuda_api.hpp"
#include "umd/module_loader.hpp"
#include "umd/ring_buffer.hpp"
```

### 命名空间保留

```cpp
// 保留 namespace async_task::gpu（IGpuDriver 28 方法契约）
namespace async_task::gpu {
    class IGpuDriver { ... };
    class GpuDriverClient : public IGpuDriver { ... };  // test-fixture
    class CudaStub : public IGpuDriver { ... };        // test-fixture
}

// umd-evolution 命名空间独立
namespace async_task::umd {
    class CudaApi { ... };
    class ModuleLoader { ... };
    class RingBuffer { ... };
}
```

### CMake 模块结构

```cmake
# 顶层 CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(TaskRunner CXX)

set(TASKRUNNER_BUILD_MODE "test-fixture" CACHE STRING
    "Build mode: test-fixture (default) | umd-evolution")
set_property(CACHE TASKRUNNER_BUILD_MODE
    PROPERTY STRINGS "test-fixture" "umd-evolution")

# shared 始终构建
add_subdirectory(include/shared)  # INTERFACE library
add_subdirectory(src/shared)

# 条件构建
if(TASKRUNNER_BUILD_MODE STREQUAL "test-fixture")
    message(STATUS "Building TaskRunner in TEST-FIXTURE mode")
    include(${CMAKE_SOURCE_DIR}/cmake/TestFixture.cmake)
    add_subdirectory(src/test_fixture)
    add_subdirectory(tests/test_fixture)
elseif(TASKRUNNER_BUILD_MODE STREQUAL "umd-evolution")
    message(WARNING "Building TaskRunner in UMD-EVOLUTION mode (experimental, not for production)")
    include(${CMAKE_SOURCE_DIR}/cmake/UMDEvolution.cmake)
    add_subdirectory(src/umd)
    add_subdirectory(tests/umd)
endif()
```