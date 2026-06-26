# H-3 Maintenance Transition: H-3.6/3.7/3.8 收官清扫 + 维护期规划

> **状态**: 📋 PROPOSED (2026-06-26)
> **创建**: 2026-06-26
> **目标**: H-3 系列 3 个 issue 修复完成后，归档遗留 openspec changes + 更新跨仓文档 + 清扫 plan + 全量验证
> **前置依赖**:
>   - ✅ H-3.6 issue-3-coordination (02ae421 合并)
>   - ✅ H-3.7 issue-2-coordination (392a496 合并)
>   - ✅ H-3.8 issue-1-coordination (02ae421 合并 + 跨仓同步完成)
> - **后续约束**: 本 change 完成后，维护期内无阻塞项

## Why

H-3.6/H-3.7/H-3.8 的 3 个 open spec changes 仍处于 `PROPOSED` 状态，停留在 `openspec/changes/` 而非 `archive/`：

```
openspec/changes/
├── 2026-06-26-h3-6-issue-3-coordination/   ← PROPOSED (应 be ARCHIVED)
├── 2026-06-26-h3-7-issue-2-coordination/   ← PROPOSED (应 be ARCHIVED)
└── 2026-06-26-h3-8-issue-1-coordination/   ← PROPOSED (应 be ARCHIVED)
```

同时，以下跨仓文档在 H-3.8 修复后的清扫中需更新：

1. **`cross-repo-h7-template.md`**: 历史 PR 范例表仍有 5 个 TBD（H-3.6/3.7/3.8 的实际 commit hash 未填入）
2. **`sync-plan.md`**: v2.1 版本未提及 H-3.6/3.7/3.8 完成情况
3. **全量测试验证**: 所有 3 个 issue 修复后需跑一次全量 build + test 确认无回归
4. **ADR-034**: 上游 ADR 状态已更新但内容摘要段可补充完成详情

**Why Now**:
1. **清理技术债**：PROPOSED 状态的已完结 openspec changes 应被归档，避免"已完成但未归档"的歧义
2. **维护窗口**：H-3 系列 3 个 issue 全部修复 + 你之前明确"维护优先"决策
3. **跨仓一致性**：跨仓 template 中的 TBD 会让后续使用者（或 AI agent）困惑

## What Changes

### 1. 归档 3 个 openspec changes

```bash
cd /workspace/project/UsrLinuxEmu
openspec archive 2026-06-26-h3-6-issue-3-coordination -y --skip-specs
openspec archive 2026-06-26-h3-7-issue-2-coordination -y --skip-specs
openspec archive 2026-06-26-h3-8-issue-1-coordination -y --skip-specs
```

### 2. 更新 TaskRunner 端跨仓文档

- `docs/07-integration/cross-repo-h7-template.md`: 历史 PR 范例表 5 个 TBD 填入实际数据
- `plans/sync-plan.md`: 更新为 v2.2，新增 H-3.6/3.7/3.8 完成段

### 3. 全量 build + test 验证

```bash
# TaskRunner (test-fixture 模式)
cmake -B build && make -j4 -C build && ctest --test-dir build -V

# TaskRunner (umd-evolution 模式)
cmake -B build -DTASKRUNNER_BUILD_MODE=umd-evolution && make -j4 -C build && ctest --test-dir build -V

# UsrLinuxEmu
cd /workspace/project/UsrLinuxEmu
cmake -B build && make -j4 -C build
ctest --test-dir build -V
```

### 4. 可选清扫项

- tadr-105 确认无遗漏状态问题
- UsrLinuxEmu ADR-034 补充完成详情段（如 owner 有时间）
- 汇总 H-3.5~H-3.8 时间线到 `docs/` 某个位置（可选）

## Non-Goals（明确不做什么）

- **不**启动 Phase D (umd-evolution PoC) 的设计工作
- **不**实施 PR 2 (stream_id_compat 废弃) — 6 月过渡期才开始
- **不**重构或修改任何生产代码 (gpgpu_device.cpp / gpu_ioctl.h / IGpuDriver)
- **不**新建任何 TADR 或 ADR
- **不**修改测试代码（除非回归测试不通过需要修复）

## Capabilities

- **gpu-h3-maintenance-transition**（本 change 新建）：跟踪 H-3 系列扫尾工作

## Impact

### 受影响文件

#### UsrLinuxEmu 端

- `openspec/changes/2026-06-26-h3-6-issue-3-coordination/` → `openspec/changes/archive/`
- `openspec/changes/2026-06-26-h3-7-issue-2-coordination/` → `openspec/changes/archive/`
- `openspec/changes/2026-06-26-h3-8-issue-1-coordination/` → `openspec/changes/archive/`
- `docs/00_adr/adr-034-h7-deferred-registry.md`（可选，补充完成详情段）

#### TaskRunner 端

- `docs/07-integration/cross-repo-h7-template.md`
- `plans/sync-plan.md`

### 受影响外部

- 无（纯文档清扫 + 归档操作）

## Open Questions

1. **是否需要同时提交 UsrLinuxEmu 端 archive + ADR-034 详情段？** 建议：本 change 包含 archive 操作（Step 4 流程），ADR-034 详情段可选
2. **同步计划更新范围**: 仅 @H-3.6/3.7/3.8 completion 段，还是整体重写 v3.0？建议：增量更新 v2.2
3. **全量测试验证在哪个仓做？** 建议：两个仓都做（TaskRunner + UsrLinuxEmu）

---

**变更追踪**: 本文件随 change 推进持续更新
**Owner**: TaskRunner 维护者（清扫）+ UsrLinuxEmu 维护者（跨仓协调）