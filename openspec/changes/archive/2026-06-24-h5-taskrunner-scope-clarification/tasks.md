# Tasks: H-5 TaskRunner Scope Clarification

> **依赖**: proposal ✅, design ✅, specs ✅
> **预估总工时**: 4-6 周（Phase A + B + C 主体，Phase D/E 可选/并行）
> **前置条件**: H-4 已完成（2026-06-23）
> **后续约束**: H-3.5 follow-up 工作必须在 test-fixture 范畴下进行；umd-evolution 范畴**不**阻塞主线

## Phase A: 文档目录重组 + TADR 重映射（1-2 周）

### A.1 创建新目录结构

- [ ] A.1.1 创建 `docs/test-fixture/`、`docs/umd-evolution/`、`docs/shared/` 三个根目录
- [ ] A.1.2 创建 `docs/test-fixture/architecture/`、`docs/test-fixture/roadmap/`、`docs/test-fixture/adr/` 子目录
- [ ] A.1.3 创建 `docs/umd-evolution/architecture/`、`docs/umd-evolution/roadmap/`、`docs/umd-evolution/adr/` 子目录
- [ ] A.1.4 创建 `docs/shared/` 子目录（IGpuDriver 契约、sync_primitives、error_handling 文档位置）

### A.2 移动现有文档到 test-fixture 范畴

- [ ] A.2.1 用 `git mv` 移动 `docs/architecture/` 全部内容到 `docs/test-fixture/architecture/`
- [ ] A.2.2 用 `git mv` 移动 `docs/roadmap/` 全部内容到 `docs/test-fixture/roadmap/`
- [ ] A.2.3 用 `git mv` 移动 `docs/plan.md`（v0.1 提案）到 `docs/umd-evolution/vision-source.md`（保留作为 vision 来源）
- [ ] A.2.4 在所有移动的文档头部加 SCOPE 元数据（前言块）

### A.3 创建 test-fixture 范畴 README

- [ ] A.3.1 创建 `docs/test-fixture/README.md`，声明 test-fixture 范畴规范（目录、命名、引用规则）
- [ ] A.3.2 在 README 中列出当前 test-fixture 范畴包含的子目录（architecture / roadmap / adr）
- [ ] A.3.3 在 README 中明确"test-fixture 是默认主线，H-3 shippable 状态"

### A.4 创建 umd-evolution 范畴 README + Vision + Gap Analysis

- [ ] A.4.1 创建 `docs/umd-evolution/README.md`，声明 umd-evolution 范畴规范（含 PROPOSED/DRAFT 状态规则）
- [ ] A.4.2 创建 `docs/umd-evolution/vision.md`，从 `vision-source.md` 提取 UMD 完整愿景内容
- [ ] A.4.3 创建 `docs/umd-evolution/gap-analysis.md`，基于 2026-06-24 4 路调研结果：
  - 与 AMD ROCm UMD 的职责对比（rocm-systems 调研）
  - 与 NVIDIA CUDA UMD 的职责对比（open-gpu-kernel-modules 调研）
  - TaskRunner 当前缺失的关键能力清单（CUmodule 加载、Stream/Context 模型、doorbell mmap、PTX JIT 等）
- [ ] A.4.4 在 README + Vision + Gap Analysis 顶部明确标注"未实施，仅文档愿景"

### A.5 创建 shared 范畴 README

- [ ] A.5.1 创建 `docs/shared/README.md`，声明 shared 范畴规范（含双向 review 要求）
- [ ] A.5.2 列出 shared 范畴包含的内容：IGpuDriver 契约、sync_primitives、error_handling
- [ ] A.5.3 在 README 中明确"shared 范畴代码变更需双向 review"
- [ ] A.5.4 **新增（ADR-036 响应）**：在 `docs/shared/README.md` 头部添加段说明 TaskRunner shared 范畴对应 UsrLinuxEmu ADR-036 的 shared ABI 契约层；引用 `[../../../docs/00_adr/adr-036-three-way-separation.md](../../../docs/00_adr/adr-036-three-way-separation.md)` §Decision 共享基础设施层
- [ ] A.5.5 **新增（ADR-036 响应）**：在 `docs/shared/README.md` 添加"跨仓 ABI 契约"段，说明 TaskRunner 的 `include/shared/`（igpu_driver.hpp 等）与 UsrLinuxEmu 的 `plugins/gpu_driver/shared/`（gpu_ioctl.h 等）是同一契约的两侧实现
- [ ] A.5.6 **新增（ADR-036 响应）**：在 `docs/shared/README.md` 添加"修改时双向 ack 要求"段，引用 ADR-036 §Migration "shared 头文件双方不同步"风险与"TaskRunner 与 UsrLinuxEmu 维护人在每次 shared 改动后需双向 ack"缓解措施

### A.6 重映射 8 个原 TADR（保留 git history）

- [ ] A.6.1 用 `git mv` 移动并重命名：
  - `docs/adr/tadr-001-cuda-vulkan-runtime-unified-scheduler.md` → `docs/umd-evolution/adr/tadr-201-unified-scheduler.md`
  - `docs/adr/tadr-002-cuda-vulkan-runtime-layered-design.md` → `docs/umd-evolution/adr/tadr-202-layered-design.md`
  - `docs/adr/tadr-003-cuda-vulkan-runtime-sync-unified-internal.md` → `docs/umd-evolution/adr/tadr-203-sync-unified.md`
  - `docs/adr/tadr-004-cuda-vulkan-runtime-stub-tracker.md` → `docs/test-fixture/adr/tadr-101-stub-tracker.md`
  - `docs/adr/tadr-005-h2-5-igpu-driver-consumer-lens.md` → `docs/test-fixture/adr/tadr-102-igpu-driver.md`
  - `docs/adr/tadr-006-h3-phase2-consumer-lens.md` → `docs/test-fixture/adr/tadr-103-h3-phase2.md`
  - `docs/adr/tadr-007-r2-mapping-contract.md` → `docs/test-fixture/adr/tadr-104-r2-mapping.md`
  - `docs/adr/tadr-008-h7-deferred-mirror.md` → `docs/test-fixture/adr/tadr-105-h7-deferred.md`
- [ ] A.6.2 验证 `git log --follow docs/test-fixture/adr/tadr-101-stub-tracker.md` 可追历史
- [ ] A.6.3 在所有重命名 TADR 头部加 SCOPE 元数据 + REPLACES 字段

### A.7 创建 8 个 redirect 文件（兼容历史链接）

- [ ] A.7.1 创建 `docs/adr/tadr-004-redirect.md`（指向 tadr-101，**保持旧路径** `docs/adr/` 兼容历史链接）
- [ ] A.7.2 创建 `docs/adr/tadr-005-redirect.md`（指向 tadr-102）
- [ ] A.7.3 创建 `docs/adr/tadr-006-redirect.md`（指向 tadr-103）
- [ ] A.7.4 创建 `docs/adr/tadr-007-redirect.md`（指向 tadr-104）
- [ ] A.7.5 创建 `docs/adr/tadr-008-redirect.md`（指向 tadr-105）
- [ ] A.7.6 创建 `docs/adr/tadr-001-redirect.md`（指向 tadr-201）
- [ ] A.7.7 创建 `docs/adr/tadr-002-redirect.md`（指向 tadr-202）
- [ ] A.7.8 创建 `docs/adr/tadr-003-redirect.md`（指向 tadr-203）
- [ ] A.7.9 每个 redirect 文件顶部明确标注 DEPRECATED + REPLACES + REPLACED_DATE
- [ ] A.7.10 **不**在新 scoped 目录（`docs/test-fixture/adr/` / `docs/umd-evolution/adr/`）下创建 redirect 文件（违反"redirect 必须在旧路径"惯例）

### A.8 创建 7 个新增 TADR

- [ ] A.8.1 创建 `docs/test-fixture/adr/tadr-106-test-fixture-scope-clarification.md`（声明 test-fixture 范畴规范）
- [ ] A.8.2 创建 `docs/shared/adr/tadr-107-shared-infrastructure-boundary.md`（声明 shared 范畴边界）
- [ ] A.8.3 创建 `docs/umd-evolution/adr/tadr-204-umd-evolution-scope-clarification.md`（声明 UMD 范畴规范）
- [ ] A.8.4 创建 `docs/umd-evolution/adr/tadr-205-umd-poc-roadmap.md`（UMD PoC 路径，含暂存）
- [ ] A.8.5 创建 `docs/shared/adr/tadr-301-igpu-driver-contract.md`（IGpuDriver 28 方法契约规范）
- [ ] A.8.6 创建 `docs/shared/adr/tadr-302-sync-primitives.md`（同步原语抽象规范）
- [ ] A.8.7 创建 `docs/shared/adr/tadr-303-error-handling.md`（错误处理抽象规范）

### A.9 更新 AGENTS.md / 内部引用

- [ ] A.9.1 在 TaskRunner 仓 `AGENTS.md` 中添加"双轨分类 + SCOPE 元数据规范"段
- [ ] A.9.2 更新所有引用 tadr-XXX 的 markdown 文件（grep 全仓后逐一更新）
- [ ] A.9.3 更新 `docs/test-fixture/roadmap/sync-plan.md` 引用（如有）

### A.10 验证 Phase A

- [ ] A.10.1 验证 8 个 redirect 文件可访问（`cat docs/adr/tadr-004-redirect.md`，确认在旧路径）
- [ ] A.10.2 验证 `git log --follow` 可追所有重命名 TADR 历史
- [ ] A.10.3 验证所有文档头部 SCOPE 元数据完整
- [ ] A.10.4 验证 `docs/adr/` 旧目录**完全清空**（或仅保留 deprecated 索引）

---

## Phase B: 代码目录重组 + CMake 改造（1-2 周）

### B.1 创建 include/shared/ 和 src/shared/

- [ ] B.1.1 创建 `include/shared/` 目录
- [ ] B.1.2 创建 `src/shared/` 目录
- [ ] B.1.3 用 `git mv` 移动 `include/igpu_driver.hpp` → `include/shared/igpu_driver.hpp`
- [ ] B.1.4 用 `git mv` 移动 `include/sync_primitives.hpp` → `include/shared/sync_primitives.hpp`
- [ ] B.1.5 **新建占位** `include/error_handling.hpp`（最小实现：`ErrorCode` enum + `Result<T>` 模板，约 30 行，满足 spec-shared-infrastructure L49-55 REQUIRE 文件必须存在）→ 然后移动到 `include/shared/error_handling.hpp`

### B.2 创建 include/test_fixture/ 和 src/test_fixture/

- [ ] B.2.1 创建 `include/test_fixture/` 和 `src/test_fixture/` 目录
- [ ] B.2.2 用 `git mv` 移动 7 个 test-fixture 范畴核心文件（plan 原清单）：
  - `include/cuda_stub.hpp` → `include/test_fixture/cuda_stub.hpp`
  - `include/cuda_scheduler.hpp` → `include/test_fixture/cuda_scheduler.hpp`
  - `include/gpu_driver_client.h` → `include/test_fixture/gpu_driver_client.h`
  - `src/cuda_stub.cpp` → `src/test_fixture/cuda_stub.cpp`
  - `src/cuda_scheduler.cpp` → `src/test_fixture/cuda_scheduler.cpp`
  - `src/gpu_driver_client.cpp` → `src/test_fixture/gpu_driver_client.cpp`
  - `src/cmd_cuda.cpp` → `src/test_fixture/cmd_cuda.cpp`
- [ ] B.2.3 用 `git mv` 移动遗留源/头文件（**Metis 审查发现 plan 原版遗漏**，6 个文件）：
  - **shared 范畴**（与 test-fixture / umd-evolution 共享）：
    - `include/memory_manager.hpp` → `include/shared/memory_manager.hpp`（被 `cuda_scheduler.hpp:20` `#include`，**必须** 移动）
    - `src/memory_manager.cpp` → `src/shared/memory_manager.cpp`
    - `src/sync_primitives.cpp` → `src/shared/sync_primitives.cpp`
  - **test-fixture 范畴**（核心 + 辅助）：
    - `src/CmdProcessor.cpp` → `src/test_fixture/CmdProcessor.cpp`
    - `src/TaskRunner.cpp` → `src/test_fixture/TaskRunner.cpp`
    - `src/cli_main.cpp` → `src/test_fixture/cli_main.cpp`（CLI 入口，taskrunner 可执行文件）
    - `src/cmd_buffer_v2.cpp` → `src/test_fixture/cmd_buffer_v2.cpp`（CLI 子命令）
- [ ] B.2.4 决策遗留文件（**不**允许静默丢失，必须显式声明）：
  - **`tests/test_taskrunner.cpp`**：3 个 TEST_CASEs（原 plan 未注册到 ctest）；决策：移动到 `tests/test_fixture/test_taskrunner.cpp` + 在新 TestFixture.cmake 中 `add_test` 注册
  - **`sample/main.cpp`**：原 sample 程序；决策：保留并加入 `add_executable(sample ...)`（与 test 模式并列，可通过 `-DBUILD_SAMPLE=ON` 启用，默认 OFF）

### B.3 创建 include/umd/ 和 src/umd/ 骨架（**不**实现具体逻辑）

- [ ] B.3.1 创建 `include/umd/` 目录
- [ ] B.3.2 创建 `src/umd/` 目录
- [ ] B.3.3 创建 `include/umd/cuda_api.hpp`（声明 `namespace async_task::umd::CudaApi` 类，含最小 CUDA Runtime API 方法声明，全部抛 `not_implemented`）
- [ ] B.3.4 创建 `include/umd/module_loader.hpp`（声明 `ModuleLoader` 类，全部抛 `not_implemented`）
- [ ] B.3.5 创建 `include/umd/ring_buffer.hpp`（声明 `RingBuffer` 类，全部抛 `not_implemented`）
- [ ] B.3.6 创建 `src/umd/cuda_api.cpp`（占位实现，每个方法返回 `nullptr`/`-ENOSYS`）
- [ ] B.3.7 创建 `src/umd/module_loader.cpp`（占位实现）
- [ ] B.3.8 创建 `src/umd/ring_buffer.cpp`（占位实现）

### B.4 更新所有 include 路径

- [ ] B.4.1 更新所有 `cuda_stub.cpp`、`cuda_stub.hpp`、`cuda_scheduler.cpp`、`cuda_scheduler.hpp` 的 include 路径（`#include "shared/igpu_driver.hpp"` 形式）
- [ ] B.4.2 更新 `cmd_cuda.cpp`、`gpu_driver_client.h`、`gpu_driver_client.cpp` 的 include 路径
- [ ] B.4.3 更新 `sync_primitives.hpp` 内部 include（如有引用其他 TaskRunner 文件）
- [ ] B.4.4 **新增**：更新 `cuda_scheduler.hpp` 等移动文件的 `memory_manager.hpp` include 路径（`#include "shared/memory_manager.hpp"`）— **Metis 审查发现遗漏**，原 plan 完全未处理
- [ ] B.4.5 验证无残留旧路径（`grep -rn 'include/igpu_driver\|include/sync_primitives\|include/memory_manager\|include/cuda_' src/ include/` 应仅匹配 `shared/` 或 `test_fixture/` 子路径）— grep 模式新增 `include/memory_manager`

### B.5 创建 CMake 模块化拆分

- [ ] B.5.1 创建 `cmake/` 目录
- [ ] B.5.2 创建 `cmake/Shared.cmake`：定义 `taskrunner_shared` INTERFACE library + include 路径
- [ ] B.5.3 创建 `cmake/TestFixture.cmake`：定义 `taskrunner_test_fixture` STATIC lib + `taskrunner` CLI exe
- [ ] B.5.4 创建 `cmake/UMDEvolution.cmake`：定义 `taskrunner_umd_stub` SHARED lib（含 WARNING 消息）
- [ ] B.5.5 更新顶层 `CMakeLists.txt`：
  - 添加 `TASKRUNNER_BUILD_MODE` option + STRINGS 属性
  - 始终 include shared
  - 条件 include TestFixture 或 UMDEvolution
  - 文件总行数 < 50 行

### B.6 创建 tests/shared/ 和 tests/umd/ 子目录

- [ ] B.6.1 创建 `tests/shared/` 目录（用于未来 shared 范畴测试）
- [ ] B.6.2 创建 `tests/umd/` 目录（用于未来 UMD PoC 测试）
- [ ] B.6.3 用 `git mv` 移动现有 test-fixture 范畴测试文件：
  - `tests/test_cuda_scheduler.cpp` → `tests/test_fixture/test_cuda_scheduler.cpp`
  - `tests/test_gpu_architecture.cpp` → `tests/test_fixture/test_gpu_architecture.cpp`
  - `tests/test_gpu_phase2.cpp` → `tests/test_fixture/test_gpu_phase2.cpp`
  - `tests/mock_gpu_driver.hpp` → `tests/test_fixture/mock_gpu_driver.hpp`

### B.7 验证 Phase B

- [ ] B.7.1 验证 `cmake .. && make -j4`（默认 test-fixture 模式）成功
- [ ] B.7.2 验证 `./test_cuda_scheduler` 8/8 通过
- [ ] B.7.3 验证 `./test_gpu_phase2` 12/12 通过
- [ ] B.7.4 验证 `cmake .. -DTASKRUNNER_BUILD_MODE=umd-evolution && make -j4` 成功（编译 `libumd_stub.so`）
- [ ] B.7.5 验证默认 build 模式不引用 `src/umd/`（`grep -r 'src/umd' build/CMakeFiles/` 无匹配）
- [ ] B.7.6 验证 `git log --follow` 仍可追历史（移动文件保留 history）

---

## Phase C: 跨仓同步（1 周）

### C.1 TaskRunner 端 commit + push

- [ ] C.1.1 验证 TaskRunner 端 git 状态：`git status -s`
- [ ] C.1.2 `git add .` （含 git mv 的所有变更）
- [ ] C.1.3 撰写 commit message：`feat(scope): H-5 TaskRunner scope clarification - test-fixture + umd-evolution + shared`
- [ ] C.1.4 commit
- [ ] C.1.5 `git push origin main`

### C.2 UsrLinuxEmu 端 submodule pointer 更新

- [ ] C.2.1 进入 UsrLinuxEmu 仓根目录
- [ ] C.2.2 `cd /workspace/project/UsrLinuxEmu`
- [ ] C.2.3 `git add external/TaskRunner` （更新 submodule 指针）
- [ ] C.2.4 撰写 commit message：`chore(submodule): bump TaskRunner to H-5 (scope clarification + 16 TADR remap)`
- [ ] C.2.5 commit

### C.3 UsrLinuxEmu 端 docs/00_adr/README.md mirror 更新

- [ ] C.3.1 阅读当前 `docs/00_adr/README.md` mirror 段现状
- [ ] C.3.2 更新 TaskRunner TADR mirror 表：
  - 增加"范畴"列（test-fixture / umd-evolution / shared）
  - 列出 5 个 tadr-1xx（test-fixture）
  - 列出 3 个 tadr-2xx（umd-evolution）+ 2 个新增（tadr-204, tadr-205）
  - 列出 4 个 tadr-3xx（shared）+ 1 个 tadr-107（shared boundary）
- [ ] C.3.3 增加"双轨分类 + 4 步同步流程"说明段（指向 ADR-035 + 本 change 的 proposal.md）
- [ ] C.3.4 `git add docs/00_adr/README.md`
- [ ] C.3.5 撰写 commit message：`docs(adr): update TaskRunner TADR mirror (H-5 scope clarification)`
- [ ] C.3.6 commit

### C.4 跨仓 PR

- [ ] C.4.1 推送 UsrLinuxEmu 端：`git push origin main`
- [ ] C.4.2 创建跨仓 PR（如果使用 GitHub Flow）
- [ ] C.4.3 PR 描述包含：H-5 proposal 摘要、5 阶段迁移图、TADR 编号重映射表
- [ ] C.4.4 至少 1 个 TaskRunner maintainer review approval
- [ ] C.4.5 至少 1 个 UsrLinuxEmu maintainer review approval
- [ ] C.4.6 PR 合并 + 关闭

### C.5 验证 Phase C

- [ ] C.5.1 验证 UsrLinuxEmu 端 `git log --oneline -3` 显示 3 个 commit 顺序正确
- [ ] C.5.2 验证 `docs/00_adr/README.md` mirror 表内容完整
- [ ] C.5.3 验证 TaskRunner 端 `git log --oneline -1` 显示 H-5 commit
- [ ] C.5.4 验证 `git submodule status external/TaskRunner` 指向 H-5 commit

---

## Phase D: umd-evolution PoC 启动（**可选，1-2 月，独立 change**）

### D.0 启动条件判断

- [ ] D.0.1 确认 H-3.5 follow-up 已 shippable
- [ ] D.0.2 确认有明确 PoC 需求（否则**不**启动）
- [ ] D.0.3 创建独立 openspec change `2026-MM-DD-h5-1-umd-poc-1`（如启动）

### D.1 调研 + 设计

- [ ] D.1.1 参考 ROCm `amd_aql_queue.cpp:482-493` 设计 doorbell mmap 旁路
- [ ] D.1.2 设计最小 CUDA Runtime API 表面 PoC（cudaMalloc/cudaMemcpy/cudaLaunchKernel 3 个 API）
- [ ] D.1.3 设计 ELF + CUBIN 解析最小 PoC（仅 kernel symbol 提取，不需完整 SASS）

### D.2 PoC 实现（独立 change，**不**阻塞主线）

- [ ] D.2.1 在 `src/umd/cuda_api.cpp` 中实现 cudaMalloc PoC
- [ ] D.2.2 在 `src/umd/cuda_api.cpp` 中实现 cudaMemcpy PoC
- [ ] D.2.3 在 `src/umd/cuda_api.cpp` 中实现 cudaLaunchKernel PoC
- [ ] D.2.4 在 `src/umd/ring_buffer.cpp` 中实现 doorbell mmap 旁路
- [ ] D.2.5 在 `src/umd/module_loader.cpp` 中实现 ELF 解析最小 PoC

### D.3 PoC 测试

- [ ] D.3.1 在 `tests/umd/` 添加 3 个 PoC 测试用例
- [ ] D.3.2 验证 `cmake .. -DTASKRUNNER_BUILD_MODE=umd-evolution && make -j4 && ctest` 全部通过

---

## Phase E: 现有 H-3.5/H-7 工作继续（**与 Phase A/B/C 并行，不受重构影响**）

### E.1 H-3.5 follow-up 工作（在 test-fixture 范畴下）

- [ ] E.1.1 修复 CudaScheduler 抽象泄漏（`cuda_scheduler.cpp:100, 146, 187, 226, 268`）
  - 删除 `dynamic_cast<CudaStub*>`
  - 改用 `IGpuDriver` 抽象的 `alloc_bo/free_bo`/`submit_*` 方法统一调度
- [ ] E.1.2 让 MockGpuDriver 也实现 guard，关闭 T6-T9 mock-behavior deviation（`tests/test_fixture/test_gpu_phase2.cpp:115-195`）

### E.2 H-7 ADR 跟踪（3 个上游 issue）

- [ ] E.2.1 在 UsrLinuxEmu owner 端推动 stream_id u32 → u64 类型匹配（TADR-008 Issue #1）
- [ ] E.2.2 推动 ioctl path vs mmap path 分歧解决（TADR-008 Issue #2）
- [ ] E.2.3 推动 attached_queues 弱校验解决（TADR-008 Issue #3）

### E.3 include 路径同步更新

- [ ] E.3.1 在 H-3.5 / H-7 工作的 PR 中同步更新 include 路径（如 `#include "shared/igpu_driver.hpp"`）
- [ ] E.3.2 验证 E.1 / E.2 完成后 `make -j4` 仍全部通过

---

## 总验证清单（所有 Phase 完成后）

- [ ] V.1 `cmake .. && make -j4`（默认 test-fixture 模式）成功
- [ ] V.2 `./test_cuda_scheduler` 8/8 通过
- [ ] V.3 `./test_gpu_phase2` 12/12 通过
- [ ] V.4 `cmake .. -DTASKRUNNER_BUILD_MODE=umd-evolution && make -j4` 成功
- [ ] V.5 **8 个重映射 TADR + 7 个新增 TADR + 8 个 redirect 文件 = 23 个文档齐全**（V.1 修正：原 "16+8+7=31" 算错，应为 8+8+7=23）
- [ ] V.6 `docs/00_adr/README.md` mirror 表包含全部 23 个 TADR（含范畴列）
- [ ] V.7 跨仓 PR 已合并 + closed
- [ ] V.8 TaskRunner `git submodule status external/TaskRunner` 指向 H-5 commit
- [ ] V.9 `git log --follow` 可追所有重命名 TADR + 移动代码文件的历史
- [ ] V.10 AGENTS.md 包含"双轨分类 + SCOPE 元数据规范"段
- [ ] V.11 共享区代码（如有变更）通过双向 review

---

## 工时估算

| Phase | 主要任务 | 预估工时 |
|-------|---------|---------:|
| Phase A | 文档重组 + TADR 重映射 + redirect + 7 新增 | 1-2 周 |
| Phase B | 代码重组 + include 更新 + CMake 拆分 | 1-2 周 |
| Phase C | 跨仓同步 + PR review | 1 周 |
| Phase D | umd-evolution PoC（可选） | 1-2 月 |
| Phase E | H-3.5/H-7 工作（并行） | 2-4 周 |
| **合计（主线）** | **Phase A + B + C** | **3-5 周** |
| **合计（含 D + E）** | **全部 5 Phase** | **2-3 月** |