# Tasks: ssot-v0-1-7-comprehensive-fix

> **依赖**: proposal ✅ / design ✅ / specs ✅
> **预估总工时**: ~37 min（设计 Migration Plan 已分 Phase）
> **约束**: 单 commit + docs-audit 36/36 PASS + 编译 100%（纯文档，也需 verify per task 6.2）

## 1. 准备 + 验证

- [ ] 1.1 重新读 v0.1.6 审计报告 `docs/02_architecture/audit-reports/v0.1.6-audit.md`（确认 17 项偏差的 SSOT 描述与实际状态对照）
- [ ] 1.2 grep 源代码确认 8 个新补录 struct 的字段（防止"复制"时漏字段）：
  - `grep -A 10 "struct gpu_mmu_event_cb_args" plugins/gpu_driver/shared/gpu_ioctl.h`
  - `grep -A 10 "struct gpu_firmware_cb_args" plugins/gpu_driver/shared/gpu_ioctl.h`
  - `grep -A 10 "struct gpu_map_bo_args" plugins/gpu_driver/shared/gpu_ioctl.h`
  - `grep -A 10 "struct gpu_wait_fence_args" plugins/gpu_driver/shared/gpu_ioctl.h`
  - `grep -A 10 "struct gpu_register_gpu_args" plugins/gpu_driver/shared/gpu_ioctl.h`
  - `grep -A 10 "struct gpu_queue_map_ring_args" plugins/gpu_driver/shared/gpu_queue.h`
  - `grep -A 10 "struct gpu_queue_info_args" plugins/gpu_driver/shared/gpu_queue.h`
  - `grep -A 10 "struct gpu_ring_header" plugins/gpu_driver/shared/gpu_queue.h`

## 2. 附录 A 完整化（A4 #1-#9，共 9 项）

- [ ] 2.1 **A4 #1 (🔴 P0 必修)**: 在 `gpu_pushbuffer_args` 末尾追加 `u64 va_space_handle;` 字段 + H-1 注释（per design D4）
- [ ] 2.2 **A4 #2**: 在附录 A 补录 `struct gpu_mmu_event_cb_args`（IOCTL 0x02 配套）
- [ ] 2.3 **A4 #3**: 在附录 A 补录 `struct gpu_firmware_cb_args`（IOCTL 0x03 配套）
- [ ] 2.4 **A4 #4**: 在附录 A 补录 `struct gpu_map_bo_args`（IOCTL 0x12 配套）
- [ ] 2.5 **A4 #5**: 在附录 A 补录 `struct gpu_wait_fence_args`（IOCTL 0x13 配套）
- [ ] 2.6 **A4 #6**: 在附录 A 补录 `struct gpu_register_gpu_args`（IOCTL 0x32 配套）
- [ ] 2.7 **A4 #7**: 在附录 A 补录 `struct gpu_queue_map_ring_args`（IOCTL 0x42 配套）
- [ ] 2.8 **A4 #8**: 在附录 A 补录 `struct gpu_queue_info_args`（IOCTL 0x43 配套）
- [ ] 2.9 **A4 #9**: 在附录 A 补录 `struct gpu_ring_header`（含 `volatile` 限定符 + `reserved[32]` 注释）

> 8 个新 struct 按 design D3 按 IOCTL 编号顺序排列；位置：`gpu_mmu_event_cb_args` 和 `gpu_firmware_cb_args` 插在 `gpu_pushbuffer_args` 与 `gpu_alloc_bo_args` 之间；`gpu_map_bo_args` 和 `gpu_wait_fence_args` 插在 `gpu_alloc_bo_args` 与 `gpu_va_space_args` 之间；`gpu_register_gpu_args` 插在 `gpu_va_space_args` 与 `gpu_queue_args` 之间；`gpu_queue_map_ring_args` / `gpu_queue_info_args` 紧接 `gpu_queue_args` 之后；`gpu_ring_header` 作为"内部 Ring Buffer 结构"独立段（带注释说明非 IOCTL 直接配套）

## 3. §1.7 表格刷新（A2 #3, #4, #5，共 3 项）

- [ ] 3.1 **A2 #3**: 行 9（ADR-010）"声明: 提议\"迁移到 GTest\"" / "实际: **未实施**" → "声明: **✅ 已接受 Catch2（最终决策）**" / "实际: **Catch2**（ADR-010 自身已对齐实际）"
- [ ] 3.2 **A2 #4**: 表格新增 3 行（per design D5）：
  - `CONTRIBUTING.md` | Catch2 | Catch2
  - `docs/03-development/adding-devices.md` | Catch2 | Catch2
  - `docs/06-reference/glossary.md` | Catch2 | Catch2
- [ ] 3.3 **A2 #5**: 行 3（AGENTS.md）"声明: **—（未表态）**" → "声明: **Catch2（明确反对 GTest）**"
- [ ] 3.4 §1.7 结论行（line 267）"ADR-010 的提议与现实相反；实际项目用 Catch2" → "ADR-010 已正式接受 Catch2（v0.1.7 同步）；实际项目用 Catch2"

## 4. §1.8 自描述更新（A3 #1, #4, #6，共 3 项）

- [ ] 4.1 **A3 #1**: §1.8 line 269-274 末尾新增 `#### §1.8.1 闭环证据（v0.1.6 审计确认）` 小节（per design D6 模板）
- [ ] 4.2 **A3 #4**: SSOT 头部"状态: 🔄 待评审" → "状态: ✅ Approved（v0.1.7）"
- [ ] 4.3 **A3 #4**: SSOT 底部"状态"行同步更新
- [ ] 4.4 **A3 #6**: §1.8 原文"**缺少一份'重构后的架构总览'文档** —— 本文正是为此而写" → "已于 v0.1.5 创建本文闭环此 gap（v0.1.6 审计确认闭环证据见 §1.8.1）"

## 5. 跨文件 + 跨章节更新（A3 #5 + A1 #5，共 2 项）

- [ ] 5.1 **A3 #5**: `docs/README.md` L222 "❌ **未启动**：022（\"GPU 计算单元仿真\"） — 占位编号" → "✅ v1（operator-level emulation，ADR-022）"
- [ ] 5.2 **A3 #5**: `docs/README.md` ADR 列表段（约 L80-100）022 标 ✅ Accepted
- [ ] 5.3 **A1 #5**: SSOT §1.5 line 204 "src/                           (kernel SHARED lib, 14 cpp)" → "(kernel SHARED lib, 12 cpp)"

## 6. 变更记录 + footer

- [ ] 6.1 在 SSOT §变更记录表追加 v0.1.7 条目（per design D7 模板）
- [ ] 6.2 SSOT 底部"最后更新"行：日期更新到 `2026-06-17`，commit hash 留待本 change commit 时填（`git rev-parse HEAD` 取出）

## 7. 验证

- [ ] 7.1 `bash tools/docs-audit.sh --strict` 输出 `✅ Passed: 36 / ❌ Failed: 0`（必须）
- [ ] 7.2 `make -j4 -C build` 输出 `[100%] Built target ...` 所有 target（必须，纯文档也应 verify）
- [ ] 7.3 `git diff --stat` 确认本 change 范围 = `post-refactor-architecture.md` + `docs/README.md`（2 文件）

## 8. 提交 + 归档

- [ ] 8.1 `git add docs/02_architecture/post-refactor-architecture.md docs/README.md`
- [ ] 8.2 单 commit，commit message 模板：
  ```
  docs(ssot): v0.1.7 comprehensive fix — 17 deviations closed

  Implements OpenSpec change `ssot-v0-1-7-comprehensive-fix` to fix
  17 SSOT-side deviations identified in v0.1.6 audit
  (commit 211b48c, change `ssot-deep-audit`):

  - Appendix A struct field completeness (A4 #1 P0 + A4 #2-#9 P2):
    9 struct definitions added/extended, all copied verbatim
    from plugins/gpu_driver/shared/{gpu_ioctl.h,gpu_queue.h}
  - §1.7 test framework table refresh (A2 #3, #4, #5):
    ADR-010 row, 3 new source rows, AGENTS.md row
  - §1.8 self-referential closure (A3 #1, #4, #6):
    §1.8.1 闭环证据 subsection + status upgrade to ✅ Approved
  - Cross-file updates (A3 #5 + A1 #5):
    docs/README.md ADR-022 status + §1.5 src/kernel cpp count 14→12

  Read-only docs change per design D5 (audit-fix separation):
  no code modifications. Remaining 8 deviations
  (A1 #1-#4, A2 #1-#2, A3 #2-#3) covered by independent
  follow-up changes (cleanup-shadow-dead-code,
  fix-sim-hardware-layout, cleanup-gtest-residue,
  fix-agents-md-ssot-link, cleanup-orphan-spec-purpose).

  Validation:
  - docs-audit.sh --strict: 36/36 PASS
  - make -j4 -C build: 100% pass
  ```
- [ ] 8.3 `openspec archive ssot-v0-1-7-comprehensive-fix --yes`（接受 16 个 task incomplete 警告，因为 task list 是手动跟踪的，OpenSpec schema 用的是不同的 5 个 artifacts）
- [ ] 8.4 验证归档：`ls openspec/changes/archive/2026-06-17-ssot-v0-1-7-comprehensive-fix/` 包含 proposal/design/tasks/specs

## 9. 回滚预案（任一步失败可触发）

- **附录 A 字段写错**: 仅 revert §2 单文件 → 重新 grep 源代码 → 重做该 struct
- **§1.7 表格错位**: 撤销 3.1-3.4 → 重新按 9 行 + 3 行格式重做
- **§1.8.1 闭环证据数字过期**: 数字是 v0.1.6 审计快照；若未来 v0.1.7+ 审计再做，按新数字更新
- **docs-audit 失败**: docs-audit 通常检查 markdown 链接/格式；若附录 A 大量新增触发，重排或简化
- **build 失败**: 极不可能（纯文档）→ 检查是否有 tabs 而非 spaces / 行尾 CRLF / 编码异常
- **全部失败**: `git reset --hard HEAD~1` + `openspec archive --help` 查如何撤销归档（可能需手动）

## Out of Scope（其他 changes 修复，本 change 不动）

| 偏差 | 简述 | 目标 change |
|------|------|------------|
| A1 #1 | sim/scheduler/translator 嵌套未在 §1.2 体现 | `cleanup-shadow-dead-code` |
| A1 #2 | sim/hardware/ 布局分裂（.cpp 在 sim/ 根）| `fix-sim-hardware-layout` |
| A1 #3 | sim/doorbell_emu.cpp 空壳（1 行 stub）| `cleanup-shadow-dead-code` |
| A1 #4 | sim/buddy_allocator.cpp + sim/fence_sim.cpp 是 dead code | `cleanup-shadow-dead-code` |
| A2 #1 | .github/copilot-instructions.md 残留 "Google Test" | `cleanup-gtest-residue` |
| A2 #2 | CONTRIBUTING.md 残留 `apt install libgtest-dev` | `cleanup-gtest-residue` |
| A3 #2 | AGENTS.md 0 处反向引用 SSOT | `fix-agents-md-ssot-link` |
| A3 #3 | 3 个 `openspec/specs/*.md` TBD Purpose 占位 | `cleanup-orphan-spec-purpose` |

## 关键风险与监控点

- **R1 (附录 A 字段错)**: 严格按 1.2 的 grep 输出复制，不二次设计
- **R2 (§1.7 表格对齐)**: 用 markdown `|` 表格语法（不依赖空格对齐），GitHub/docs-audit 都接受
- **R3 (SSOT 状态升 ✅ 时机)**: 本 change commit 时升（design D1），v1.0 升版独立决策
- **R4 (闭环证据数字过期)**: 数字加 v0.1.6 审计快照注；不"动态"读取
- **R5 (本 change commit 阻塞其他并行 changes)**: 不阻塞；本 change 不改 `plugins/gpu_driver/shared/*.h` 或 AGENTS.md
