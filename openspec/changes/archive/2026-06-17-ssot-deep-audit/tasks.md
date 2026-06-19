# Tasks: ssot-deep-audit

> **依赖**: proposal ✅ / design ✅ / specs ✅
> **预估总工时**: 1-2 小时（agent 并行 + 报告聚合 + SSOT 勘误）

## 1. 审计准备

- [ ] 1.1 创建 `docs/02_architecture/audit-reports/` 目录
- [ ] 1.2 读 v0.1.2 勘误报告（commit `9e5d5ea` 的 diff）作为 prompt 模板
- [ ] 1.3 准备 4 个 agent 的 prompt（每个含 CONTEXT/GOAL/DOWNSTREAM/REQUEST/SCOPE/SKIP 6 段）

## 2. 启动 4 个并行 explore agent（核心步骤）

- [ ] 2.1 启动 **A1: §1.2 架构图硬件仿真层** agent（`run_in_background=true`）
  - 关键文件：`plugins/gpu_driver/sim/` 子树、`libgpu_core/*.h`、`libgpu_core/*.c`、SSOT §1.2 全文
  - 必答：libgpu_core 命名、`sim/scheduler/` + `sim/hardware/` 存在性、`gpu_queue_emu.{h,cpp}` 位置、`sim/buddy_allocator.cpp` 与 `sim/fence_sim.cpp` 是否仍存在
- [ ] 2.2 启动 **A2: §1.7 测试框架声明** agent（`run_in_background=true`，与 A1 并行）
  - 关键文件：11 个声明源（见 spec Requirement 2）+ `tests/catch_amalgamated.{hpp,cpp}` 实证
  - 必答：每个声明源当前是 Catch2 / GTest / 未表态；ADR-010 实际状态
- [ ] 2.3 启动 **A3: §1.8 权威空白 + orphan spec Purpose** agent（`run_in_background=true`，与 A1/A2 并行）
  - 关键文件：`post-refactor-architecture.md`、AGENTS.md、`openspec/specs/` 全树
  - 必答：§1.8 gap 是否仍存在；AGENTS.md 漂移；orphan spec 是否有 TBD Purpose
- [ ] 2.4 启动 **A4: 附录 A struct 字段** agent（`run_in_background=true`，与 A1/A2/A3 并行）
  - 关键文件：`plugins/gpu_driver/shared/gpu_ioctl.h`、`plugins/gpu_driver/shared/gpu_queue.h`、SSOT §1.3 + 附录 A
  - 必答：13 个 struct 的字段名/类型/顺序是否与 SSOT 一致；`gpu_pushbuffer_args.va_space_handle` 存在性；`gpu_queue_args.doorbell_pgoff` 存在性

## 3. 收集与聚合

- [ ] 3.1 等 4 个 `<system-reminder>` 通知（wall time 约 2-4 分钟）
- [ ] 3.2 用 `background_output(task_id=...)` 拉取每个 agent 结果
- [ ] 3.3 去重跨 agent 发现的同一偏差（保留严重度最高者）
- [ ] 3.4 汇总偏差数（按严重度四档）

## 4. 写 v0.1.4 审计报告

- [ ] 4.1 写 `docs/02_architecture/audit-reports/v0.1.4-audit.md`：
  - 顶部：执行摘要（偏差数表）
  - 4 个章节：A1 §1.2 / A2 §1.7 / A3 §1.8 / A4 附录 A struct
  - 每节：统一 6 列偏差表
- [ ] 4.2 在 SSOT `post-refactor-architecture.md` 顶部加引用行：
  ```
  > 历史审计报告：docs/02_architecture/audit-reports/
  ```

## 5. SSOT v0.1.4 勘误（仅追加变更记录）

- [ ] 5.1 在 SSOT §变更记录表追加一行 v0.1.4：
  ```
  | 2026-06-17 | 0.1.4 审计 | <author> | **SSOT 全章节深度审计（change ssot-deep-audit）**：4 个并行 explore agent 覆盖 §1.2/1.7/1.8 + 附录 A；产出 audit-reports/v0.1.4-audit.md；发现 N 个偏差（M 个 🔴/X 个 🟠/...）|
  ```
- [ ] 5.2 **不修改** SSOT 现有章节内容（仅追加）
- [ ] 5.3 在 SSOT 底部"最后更新"行更新到 `2026-06-17`，commit hash 留待本 change commit 时填

## 6. 验证与提交

- [ ] 6.1 `bash tools/docs-audit.sh --strict` 仍 36/36 PASS
- [ ] 6.2 `make -j4 -C build` 100% 编译通过（审计不应改代码，但保险起见跑一次）
- [ ] 6.3 单个 commit：
  ```
  docs(audit): v0.1.4 SSOT deep audit — 4 regions covered
  ```
  - 文件：`docs/02_architecture/audit-reports/v0.1.4-audit.md` (新增)
  - 文件：`docs/02_architecture/post-refactor-architecture.md` (变更记录 +1 行 + 顶部 +1 行)
- [ ] 6.4 `openspec archive ssot-deep-audit` 把 change 归档
- [ ] 6.5 用户的 follow-up：根据 v0.1.4 报告决定开 N 个修复 change

## 回滚预案

| 失败 | 回滚 |
|------|------|
| agent prompt 写错产出无意义 | 重启 agent，不影响其他工作 |
| 报告路径冲突 | 改名 v0.1.4 → v0.1.4-{retry} |
| SSOT 变更记录追加错 | 单独 revert SSOT 那 1 行 |
| 全部失败 | 删除 `audit-reports/v0.1.4-audit.md` + revert SSOT 顶部那行 + revert 变更记录 |

## 关键风险与监控点

- **R1 (agent 格式不统一)**: 4 个 agent 的 prompt 必须都用 6 段结构 + 6 列偏差表（design D2/D3 强制）
- **R2 (跨 agent 重复偏差)**: 聚合阶段去重，保留严重度最高者
- **R3 (漏掉隐藏偏差)**: 接受——本次只覆盖 4 个已识别盲区，v0.1.5+ 可深化
- **R4 (太多偏差导致 follow-up 排队)**: 决策权在用户，change 本身已闭环
