# Tasks: cleanup-adr-placeholders

> **依赖**: proposal ✅ / design ✅ / specs ✅
> **预估总工时**: 1-2 天（无代码，纯治理）

## 1. ADR-022 v1 填决策（commit 1: `docs(adr): adr-022 v1 operator-level emulation`）

- [x] 1.1 打开 `docs/00_adr/adr-022-gpu-compute-unit-emulation.md`
- [x] 1.2 状态行改为 `**状态**: ✅ 已接受 (v1)`
- [x] 1.3 修订记录追加 `2026-06-17 v1: 填入 operator-level emulation 决策（change cleanup-adr-placeholders）`
- [x] 1.4 `## 决策` 段改写为 v1 决策（design D1）：
  - 仿真粒度 = **operator-level**（4 个 v1 template）
  - template 列表：`add_vec4` / `mul_vec4` / `memcpy_h2d_via_pull` / `noop`
  - 与 `HardwarePullerEmu` FSM 衔接：FETCH 阶段解析 template name，DISPATCH 阶段调用对应 C++ 实现
  - 4 个 template 边界：硬编码 v1，Phase 3+ 讨论配置化
  - 明确不做什么：不做 ISA 解释、不做 cycle-accurate 性能模型
- [x] 1.5 5 个 v0 开放问题逐项回答（粒度 / ISA / 寄存器 / 性能 / 调试）
- [x] 1.6 v0 5 个开放问题原文**移动**到 `## 讨论历史 (v0 占位)` 附录（不删除）
- [x] 1.7 `## 当前状态` 表更新：compute emulation 标 "✅ v1 设计完成，Phase 3 实施"

## 2. ADR-031 v1 填决策（commit 2: `docs(adr): adr-031 v1 ttm wrapper architecture`）

- [x] 2.1 打开 `docs/00_adr/adr-031-ttm-migration-priority.md`
- [x] 2.2 状态行改为 `**状态**: ✅ 已接受 (v1)`
- [x] 2.3 修订记录追加 v1 条目
- [x] 2.4 `## 决策` 段改写为 v1 决策（design D2）：
  - TTM = **thin wrapper** over `libgpu_core/gpu_buddy`
  - buddy 是 page pool；TTM 加 BO metadata + placement
  - 新文件位置：`include/linux_compat/drm/ttm.h`（与其他 drm_*.h 并列）
  - 明确不做什么：不做 full TTM（含 swapout）、不替代 buddy
  - v1 仅函数签名稳定；结构体布局不保证
- [x] 2.5 5 个 v0 开放问题逐项回答（范围 / buddy 关系 / 跨设备 placement / fence / 测试）
- [x] 2.6 v0 5 个开放问题原文**移动**到 `## 讨论历史 (v0 占位)` 附录
- [x] 2.7 `## 当前状态` 表更新：TTM BO allocator / fence 同步标 "✅ v1 设计完成，Phase 3 实施"

## 3. ADR-025-030 显式 deferral（commit 3: `docs(adr): explicitly defer 6 placeholders with Phase 3 triggers`）

每份文件 3 步（同模板）：

- [x] 3.1 ADR-025：状态改 `⏸️ 显式 Deferred` + 加触发条件「第一个第三方 .so 插件提交 OR Phase 3 网络设备原型 commit」
- [x] 3.2 ADR-026：状态改 `⏸️ 显式 Deferred` + 加触发条件「第二个 `gpu_driver` 插件原型 OR 第一个设备热插拔测试」
- [x] 3.3 ADR-028：状态改 `⏸️ 显式 Deferred` + 加触发条件「`drivers/network/` 或 `drivers/storage/` 第一个文件 commit」
- [x] 3.4 ADR-029：状态改 `⏸️ 显式 Deferred` + 加触发条件「CI 增加 benchmark 步骤 OR 第一个 tracepoint 用例」
- [x] 3.5 ADR-030：状态改 `⏸️ 显式 Deferred` + 加触发条件「gdb/lldb 集成 issue 创建 OR 第一个 state-serialization 用例」
- [x] 3.6 全部 6 份：把 v0 "候选项 A/B/C/D" 段**移动**到 `## 讨论历史 (v0 占位)` 附录

## 4. 跨文档同步

- [x] 4.1 `docs/02_architecture/post-refactor-architecture.md` §3.3：补"8 份占位清理结果"段（specs/adr-placeholder-cleanup 引用）
- [x] 4.2 `docs/00_adr/README.md` §"编号 gap 治理"：加一句"025-030 已转为 ⏸️ 显式 Deferred"
- [x] 4.3 `docs/PRD.md` 全文 grep `ADR-022` / `ADR-025` / `ADR-026` / `ADR-028` / `ADR-029` / `ADR-030` / `ADR-031` 引用
  - 对每个匹配项：要么删除（如果隐含"决策已存在"），要么改为 `ADR-XXX ✅ 已接受` / `⏸️ Deferred` 标注
- [x] 4.4 `docs/02_architecture/post-refactor-architecture.md` 变更记录表加 v0.1.4 条目："ADR 占位清理（change cleanup-adr-placeholders）"

## 5. 验证

- [x] 5.1 `make -j4` 100% 通过（应无变化）
- [x] 5.2 `ctest` 34/34 通过（应无变化）
- [x] 5.3 `bash tools/docs-audit.sh --strict` 36/36 PASS（应无变化）
- [x] 5.4 `git diff --stat` 确认改动仅限：
  - `docs/00_adr/adr-022/025/026/028/029/030/031-*.md`（8 文件）
  - `docs/02_architecture/post-refactor-architecture.md`
  - `docs/00_adr/README.md`
  - `docs/PRD.md`
  - **无** `src/` `include/` `plugins/` `libgpu_core/` `tests/` `tools/` `CMakeLists.txt` 改动

## 6. 提交与归档

- [x] 6.1 commit 1：`docs(adr): adr-022 v1 operator-level emulation`（含任务 1 全部）
- [x] 6.2 commit 2：`docs(adr): adr-031 v1 ttm wrapper architecture`（含任务 2 全部）
- [x] 6.3 commit 3：`docs(adr): explicitly defer 6 placeholders with Phase 3 triggers`（含任务 3 + 4）
- [x] 6.4 `openspec validate cleanup-adr-placeholders` 通过
- [x] 6.5 `openspec archive cleanup-adr-placeholders` 把 change 归档

## 回滚预案

任一 commit 失败可独立 `git revert`；无 schema migration。deferred ADR 状态回退到 "🔄 提议中 — 占位骨架" 是合法状态（无功能性影响，仅治理状态变化）。
