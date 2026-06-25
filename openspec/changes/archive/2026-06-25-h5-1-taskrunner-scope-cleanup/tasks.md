# Tasks: H-5.1 TaskRunner Scope Cleanup

> **状态**: ✅ ALL TASKS COMPLETED (2026-06-25)
> **依赖**: [H-5 taskrunner-scope-clarification](../2026-06-24-h5-taskrunner-scope-clarification/) ✅ Archived
> **关联 commits**: TaskRunner `afae340..57b471a` (6 commits) + UsrLinuxEmu `89113ec + 69c9c0f`

## Phase 1.1: 核心 P0 修复（week 1）

### 1.1.1 TADR 补齐 + 文档 frontmatter 批处理

- [x] **创建 tadr-108** (build mode selection) — `docs/shared/adr/tadr-108-build-mode-selection.md`
  - [x] 引用 AGENTS.md §Scope Classification lines 161-197
  - [x] 引用 §Build Mode Selection lines 189-197 精确行号
  - [x] Content: Context / Decision / Consequences / Alternatives / References
- [x] **创建 tadr-304** (Error Handling strategy layer) — `docs/shared/adr/tadr-304-error-handling-strategy.md`
  - [x] 明确"扩展自 tadr-303"的关系
  - [x] Content: 13 种 Linux errno 语义表 + 传播规则 + 日志宏 + 致命 vs 可恢复分类
- [x] **19 个文档补 frontmatter**（YAML `SCOPE + STATUS`）
  - [x] docs/shared/adr/README.md, tadr-000-template.md (2)
  - [x] docs/test-fixture/architecture/* (5)
  - [x] docs/test-fixture/archive/* (4) — STATUS: DEPRECATED
  - [x] docs/test-fixture/phase1-week1-plan.md — STATUS: DEPRECATED
  - [x] docs/test-fixture/roadmap/* (6)
  - [x] docs/umd-evolution/vision-source.md — STATUS: DEPRECATED

### 1.1.2 CMake 模块化重构

- [x] **创建 `cmake/Shared.cmake`** — 定义 `taskrunner_shared` STATIC lib（src/shared/* 2 sources）
- [x] **创建 `cmake/TestFixture.cmake`** — 定义 `taskrunner_test_fixture` STATIC lib + 3 test executables + 1 CLI executable
- [x] **创建 `cmake/UMDEvolution.cmake`** — 定义 `taskrunner_umd_stub` SHARED lib + 1 test executable
- [x] **改写根 CMakeLists.txt** — 139 行 → 43 行（include() 形式）
- [x] **创建 `tests/shared/`** — README 跨 scope 测试说明
- [x] **链式 doctest discovery** — `find_package(doctest) → add_subdirectory(doctest) → FetchContent(v2.4.12)` 兜底

## Phase 1.2: P1 修复（week 2）

### 1.2.1 SCOPE 注释修正

- [x] `src/shared/sync_primitives.cpp:1` — `TEST-FIXTURE` → `SHARED`
- [x] `src/shared/memory_manager.cpp:1` — `TEST-FIXTURE` → `SHARED`
- [x] `sample/main.cpp` — 加 `// SCOPE: TEST-FIXTURE` 注释
- [x] 完整性验证: **38/38 源文件 0 MISSING**

### 1.2.2 docs-audit + README/AGENTS/死链/tadr-301

- [x] **重写 `tools/docs-audit.sh`** — 从 H-4.5 单层 适配 H-5 3-scope（8 段验证）
- [x] **重写 `docs/shared/adr/README.md`** — 从 TADR-001~008 索引 → 3 scope 表格（1xx/2xx/3xx）
- [x] **更新 `AGENTS.md` 头部架构段** — 3-scope 路径 + 跨文档引用表
- [x] **修复 ~25 处死链** — `tadr-001` → `tadr-201`, `tadr-005` → `tadr-102`, 等（8 条重映射规则应用至 12 个文件）
- [x] **更新 `tadr-301` 描述** — 28 → 31 方法（via tadr-109），加方法演进表
- [x] docs-audit 验证: **PASS 51 / FAIL 0 / WARN 3** ✓

## Phase 1.3: 工作树清理

- [x] **Commit 4 research 文档**:
  - [x] `docs/shared/research/usrlinuxemu-gpu-driver-design-2026-06-24.md` (41 KB)
  - [x] `docs/test-fixture/research/taskrunner-positioning-2026-06-24.md` (17 KB)
  - [x] `docs/umd-evolution/research/external-amd-rocm-umd-2026-06-24.md` (22 KB)
  - [x] `docs/umd-evolution/research/external-nvidia-cuda-umd-2026-06-24.md` (61 KB)
- [x] **更新 `.gitignore`** — `build/` → `build/` + `build_*/`（match `build_stage3/`, `build_test_check/`, `build_umd_check/`）

## Phase 1.4: Link-time 必要项补齐（spec 漏掉的）

- [x] **UsrLinuxEmu 符号链接 include path** — 3 个 .cmake 文件加 `${CMAKE_SOURCE_DIR}` 到 PUBLIC include dirs
- [x] **doctest include path** — 3 个 test executable 加 `${CMAKE_SOURCE_DIR}/doctest/doctest` 到 PRIVATE include dirs
- [x] **CLI main 补齐** — `taskrunner` executable 加 `cli_main.cpp` + `cmd_buffer_v2.cpp`
- [x] **Test executable 拆分** — 3 个 test 拆为 `test_cuda_scheduler` + `test_gpu_architecture` + `test_gpu_phase2`（避免 `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` 冲突）

## Phase 1.5: 跨仓 4 步同步

- [x] **Step 1**: TaskRunner 端 6 commits 本地落库（`afae340..57b471a`）
  - [ ] ⏸️ **push 等待用户授权**（per H-3.5 follow-up 协议）
- [x] **Step 2**: UsrLinuxEmu 端 `git add external/TaskRunner` 更新 submodule pointer 到 `57b471a` (commit `89113ec`)
- [x] **Step 3**: UsrLinuxEmu 端 mirror 更新 (commit `89113ec`):
  - [x] 加 tadr-108 (build mode selection) 到 shared scope 表
  - [x] 加 tadr-304 (Error Handling strategy) 到 shared scope 表
  - [x] 更新 tadr-301 描述（28→31 方法 via tadr-109）
  - [x] 更新"最后更新"日期到 2026-06-25
- [x] **Step 4**: UsrLinuxEmu 端 plan archive commit (commit `69c9c0f`)
  - [x] `docs/superpowers/plans/2026-06-24-h5-taskrunner-scope-clarification.md`
- [x] **Step 5**: H-5.1 openspec archive 创建（本文件）
- [ ] ⏸️ **push 等待用户授权**（TaskRunner + UsrLinuxEmu）

## 验证结果（2026-06-25）

| 验证项 | 结果 |
|--------|------|
| TaskRunner `docs-audit.sh` | PASS 51 / FAIL 0 / WARN 3 |
| TaskRunner test-fixture 模式 build | 6/6 targets OK |
| TaskRunner test-fixture 模式 test | 31/31 cases, 119/119 assertions, 0 failed |
| TaskRunner umd-evolution 模式 build | 3/3 targets OK |
| TaskRunner umd-evolution 模式 test | 3/3 cases, 8/8 assertions, 0 failed |
| UsrLinuxEmu pre-commit docs-audit | 43/43 PASS / 0 FAIL / 0 WARN |

## 后续（已 deferred，按用户决策"维护优先"）

- ❌ 不启动 H-3.5 后续工作（R2 mapping LOW32、ioctl path、attached_queues）
- ❌ 不启动 Phase D umd-evolution PoC
- ❌ 不启动 Stage 1.4 集成验证
- ❌ 不启动 Multi-GPU (cudaSetDevice)
- ❌ 不启动 Vulkan Compute stub
- ⏸️ 维护期内观察，等待 H-7 上游 issue 修复或 UsrLinuxEmu 端新 H-stage
