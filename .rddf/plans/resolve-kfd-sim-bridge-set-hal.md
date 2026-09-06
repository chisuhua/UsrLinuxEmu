# resolve-kfd-sim-bridge-set-hal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 删除 `kfd_sim_bridge` 死代码 cluster —— `kfd_sim_bridge_set_hal` / `kfd_sim_bridge_get_hal` / `g_bridge_hal_`（生产代码 0 调用点，唯一调用方是 self-referential `TEST_CASE 1`）。该删除已在 commit `a2701cd` 完成；本计划只剩 OpenSpec archive + ctest blast-radius 验证。

**Architecture:** 删除以下符号：
- `plugins/gpu_driver/drv/kfd_sim_bridge.cpp`：`set_hal()` / `get_hal()` 函数体 + `g_bridge_hal_` 静态变量 + stale 文件头注释
- `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h`：上述符号的声明
- `tests/test_kfd_sim_bridge_audit_standalone.cpp`：`TEST_CASE 1` (set_hal/get_hal self-reference) + `dummy_iommu_map` / `dummy_iommu_unmap` 助手 + `#include "gpu_hal.h"`（仅 helper 用）

修改 OpenSpec artifacts 把 scope 从"删除 setter"扩展到"删除整个 dead cluster"（commit `a2701cd` 已完成）。

**Tech Stack:** C++17 · 现有 kfd_sim_bridge 接口 · Catch2

**Priority:** P3  **Wave:** 1  **Risk:** ZERO（代码已删除；本计划仅含验证 + archive）

**Generated:** 2026-09-06 (post a2701cd, Oracle deep-dive revisions applied)

**Refs:** Oracle deep-dive `ses_f8f0f9467ffe3w1M4uCHZTcFmS` (REJECT — false premise, expanded scope) · commit `a2701cd` (删除已落地) · openspec/changes/resolve-kfd-sim-bridge-set-hal/

---

## File Structure

### Code (deleted in commit `a2701cd` — verify only)

| File | Status | Lines removed |
|------|--------|---------------|
| `plugins/gpu_driver/drv/kfd_sim_bridge.cpp` | M (lines removed) | -14 |
| `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h` | M (lines removed) | -18 |
| `tests/test_kfd_sim_bridge_audit_standalone.cpp` | M (TEST_CASE 1 + helpers + include) | -43 |

### OpenSpec artifacts (already updated in `a2701cd`)

| File | Status |
|------|--------|
| `openspec/changes/resolve-kfd-sim-bridge-set-hal/proposal.md` | M (expanded scope: dead cluster not just setter) |
| `openspec/changes/resolve-kfd-sim-bridge-set-hal/specs/kfd-sim-bridge-api/spec.md` | M (REMOVED Requirements: 4 entries) |
| `openspec/changes/resolve-kfd-sim-bridge-set-hal/tasks.md` | M (7 implement steps) |

---

## Tasks (2) — 全部为 verify + archive

### Task 1: Verify deletion landed correctly + ctest 仍 PASS

- [ ] **Step 1: Verify deletion**
```bash
cd /workspace/project/UsrLinuxEmu
grep -n 'kfd_sim_bridge_set_hal\|kfd_sim_bridge_get_hal\|g_bridge_hal_' \
  plugins/gpu_driver/drv/kfd_sim_bridge.cpp \
  plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h \
  tests/test_kfd_sim_bridge_audit_standalone.cpp 2>&1
# Expected: 空（所有引用都已删除）
git log --oneline -1 plugins/gpu_driver/drv/kfd_sim_bridge.cpp
# Expected: a2701cd refactor(plugin): revise 4 backlog changes per Oracle deep-dive review
```

- [ ] **Step 2: Build verification**
```bash
cd /workspace/project/UsrLinuxEmu/build && cmake --build . -j4 2>&1 | tail -10
# Expected: build clean, 0 errors, 0 warnings (从 gpu_driver_plugin 和 kfd_sim_bridge)
```

- [ ] **Step 3: Blast radius ctest**
```bash
cd /workspace/project/UsrLinuxEmu/build && ctest --output-on-failure 2>&1 | tail -3
# Expected: 157/157 PASS (TEST_CASE 2/3 仍在 test_kfd_sim_bridge_audit_standalone 中)
ctest -R test_kfd_sim_bridge_audit --output-on-failure
# Expected: PASS (TEST_CASE 1 已删除；2/3 仍 work)
```

- [ ] **Step 4: Symbol-table grep 确认 0 调用点**
```bash
cd /workspace/project/UsrLinuxEmu
# 整个仓库扫描，确认 set_hal/get_hal/g_bridge_hal_ 在 production code 中真的 0 调用点
grep -rn 'kfd_sim_bridge_set_hal\|kfd_sim_bridge_get_hal\|g_bridge_hal_' \
  --exclude-dir=build --exclude-dir=.git --exclude-dir=archive 2>&1
# Expected: 空（archive/ 排除；其他路径无残留）
# 若有任何命中：必须调查是否是新引入的死代码
```

- [ ] **Step 5: Phase0 gate verification**
```bash
cd /workspace/project/UsrLinuxEmu && ./tools/check_phase0_gate.sh 2>&1 | tail -10
# Expected: 6/6 PASS
```

### Task 2: Archive OpenSpec change

- [ ] **Step 1: Verify tasks 标完成**
```bash
cd /workspace/project/UsrLinuxEmu && grep -c '^- \[ \]' openspec/changes/resolve-kfd-sim-bridge-set-hal/tasks.md
# Expected: 0（所有 checkbox 已 [x]；本计划无需勾选新 task）
```

- [ ] **Step 2: Validate --strict**
```bash
cd /workspace/project/UsrLinuxEmu
openspec validate resolve-kfd-sim-bridge-set-hal --strict
# Expected: Change 'resolve-kfd-sim-bridge-set-hal' is valid
```

- [ ] **Step 3: Archive**
```bash
cd /workspace/project/UsrLinuxEmu
openspec archive resolve-kfd-sim-bridge-set-hal --yes
# Expected: 归档到 openspec/changes/archive/，specs/kfd-sim-bridge-api/ 已 sync 到 main specs
```

- [ ] **Step 4: Final commit**
```bash
cd /workspace/project/UsrLinuxEmu && git add openspec/ && git status --short
git commit -m "chore(openspec): archive resolve-kfd-sim-bridge-set-hal (dead cluster removed in a2701cd)

All implementation already landed in commit a2701cd. This commit
finalizes the change lifecycle:

  - kfd_sim_bridge_set_hal/get_hal/g_bridge_hal_ removed from .cpp/.h
  - TEST_CASE 1 + dummy_iommu_map/unmap + #include gpu_hal.h removed from test
  - file header stale comment removed
  - openspec proposal/spec/tasks updated with expanded scope

Verified: ctest 157/157 PASS, phase0-gate 6/6 PASS, 0 production
call sites of deleted symbols.

Refs:
  Oracle deep-dive: ses_f8f0f9467ffe3w1M4uCHZTcFmS
  Code deletion:     a2701cd"
git log --oneline -3
# Expected: HEAD 是 archive commit；a2701cd 在其下方；9a60447 更早
```

---

## Out of scope (handled elsewhere)

- Production code 一律不动（a2701cd 已完成删除）
- TEST_CASE 2 / 3 保留（仍测试有效功能）
- gpu_hal.h 中的 `struct gpu_hal_ops` 定义保留（其他生产代码仍引用）