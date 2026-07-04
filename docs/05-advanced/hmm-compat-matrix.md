# HMM 兼容矩阵：Linux 6.6 LTS ↔ 6.12 LTS

> **目的**：记录 HMM / mmu_notifier 子集在 Linux 6.6 与 6.12 LTS 之间的 API 差异，为 Stage 1.3 锁定 LTS 提供依据。
> **创建日期**：2026-07-04（盲点 5 决策）
> **目标 LTS**：**Linux 6.12 LTS**（Lock，design.md Decision 6）
> **源依据**：librarian 验证 + Linux kernel 头文件 `include/linux/hmm.h` / `include/linux/mmu_notifier.h`

---

## 1. 总体结论

| 维度 | 6.6 LTS | 6.12 LTS | 差异影响 | UsrLinuxEmu 策略 |
|------|---------|----------|---------|-----------------|
| HMM API 集 | 成熟 | 成熟 | **关键修正**：`hmm_mirror` 已移除 | 见 §2 |
| mmu_notifier 框架 | 稳定 | 稳定 | 无核心变更 | 完整实现 |
| sequence 协议 | 一致 | 一致 | 无变更 | `mmu_interval_read_begin/retry/set_seq` |
| HMM PFN flags | 一致 | 一致 | 64-bit 编码无变更 | `HMM_PFN_VALID = 1UL << 63` |

---

## 2. struct layout 差异（**关键**）

### 2.1 `struct hmm_mirror` — 已移除

| 版本 | 状态 |
|------|------|
| Linux 6.6 | ⚠️ **已废弃，仍存在**（过渡期） |
| **Linux 6.12** | ❌ **完全移除** |

**替代**：`struct mmu_interval_notifier` + `mmu_interval_notifier_ops.invalidate` 回调。

**KFD 实际调用**（librarian 2026-07-02 验证）：
- amdkpu / amdgpu 使用 `mmu_interval_notifier_insert()`，**不调用** `hmm_mirror_register()`
- `amdgpu_mn.c` 已迁移到 `mmu_interval_notifier` 模式

**UsrLinuxEmu 策略**：
- `include/linux_compat/hmm.h` 只声明 `struct mmu_interval_notifier`
- **绝不声明** `struct hmm_mirror`（Decision 2，`git grep hmm_mirror` 必须零命中）

### 2.2 `struct hmm_range` 字段

| 字段 | 6.6 | 6.12 | 说明 |
|------|-----|------|------|
| `notifier` | `struct mmu_interval_notifier *` | 同 | 无变更 |
| `notifier_seq` | `unsigned long` | 同 | 无变更 |
| `start` / `end` | `unsigned long` | 同 | 无变更 |
| `hmm_pfns` | `unsigned long *` | 同 | PFN 数组输出 |
| `default_flags` | `unsigned long` | 同 | 64-bit flags |
| `pfn_flags_mask` | `unsigned long` | 同 | 无变更 |

**UsrLinuxEmu 策略**：按 6.12 实现 7 个字段，零条件编译。

### 2.3 `struct mmu_interval_notifier`

| 字段 | 6.6 | 6.12 | 说明 |
|------|-----|------|------|
| `ops` | `const struct mmu_interval_notifier_ops *` | 同 | 无变更 |
| `mm` | `struct mm_struct *` | 同 | 无变更 |
| `start` / `end` | `unsigned long` | 同 | 无变更 |
| `event_seq` | `unsigned long` | 同 | 序列号跟踪 |

**UsrLinuxEmu 策略**：完整实现 6 个字段（含 `priv` driver-private data）。

---

## 3. 函数签名差异

### 3.1 `mmu_interval_notifier_insert`

| 版本 | 签名 |
|------|------|
| 6.6 | `int mmu_interval_notifier_insert(struct mmu_interval_notifier *mni, struct mm_struct *mm, unsigned long start, unsigned long length, const struct mmu_interval_notifier_ops *ops)` |
| 6.12 | `int mmu_interval_notifier_insert(struct mmu_interval_notifier *mni, struct mm_struct *mm, unsigned long start, unsigned long end, const struct mmu_interval_notifier_ops *ops)` |

**差异**：第 4 参数从 `length`（6.6）改为 `end`（6.12）。

> **注**：此变更为语义澄清（`end` 更明确表示 exclusive upper bound），不影响 ABI。

**UsrLinuxEmu 策略**：使用 6.12 签名 `(start, end)`。

### 3.2 其他函数

| 函数 | 6.6 vs 6.12 | UsrLinuxEmu |
|------|------------|-------------|
| `mmu_interval_read_begin()` | **无差异** | 按 6.12 实现 |
| `mmu_interval_read_retry()` | **无差异** | 按 6.12 实现 |
| `mmu_interval_set_seq()` | **无差异** | 按 6.12 实现 |
| `mmu_interval_notifier_remove()` | **无差异** | 按 6.12 实现 |
| `hmm_range_fault()` | **无差异** | 按 6.12 实现 |
| `mmu_notifier_register()` | **无差异** | 按 6.12 实现 |
| `mmu_notifier_unregister()` | **无差异** | 按 6.12 实现 |

---

## 4. 新增必须 ops 差异

### 4.1 `mmu_interval_notifier_ops.invalidate` — 6.12 必须

| 版本 | 状态 |
|------|------|
| 6.6 | 可选（过渡期，可能 fallback 到 `hmm_mirror`） |
| **6.12** | **必须实现**（`hmm_mirror` 已移除） |

**UsrLinuxEmu 策略**：完整实现 `invalidate` 回调（返回 `bool`）。

### 4.2 `mmu_notifier_ops` callback set

| ops | 6.6 | 6.12 | UsrLinuxEmu |
|-----|-----|------|-------------|
| `invalidate_range_start` | 必须 | 必须 | 实现（返回 int） |
| `invalidate_range_end` | 必须 | 必须 | 实现（返回 void） |
| `release` | 必须 | 必须 | 实现（返回 void） |

**无差异**：三个 callback 签名在两版本中完全一致。

---

## 5. HMM PFN flag 编码

| Flag | 值 (6.6) | 值 (6.12) | 说明 |
|------|----------|----------|------|
| `HMM_PFN_VALID` | `(1UL << 63)` | 同 | 无变更 |
| `HMM_PFN_WRITE` | `(1UL << 62)` | 同 | 无变更 |
| `HMM_PFN_ERROR` | `(1UL << 61)` | 同 | 无变更 |
| `HMM_PFN_REQ_FAULT` | `(1UL << 0)` | 同 | request flags in lower bits |
| `HMM_PFN_REQ_WRITE` | `(1UL << 1)` | 同 | 无变更 |

**UsrLinuxEmu 策略**：64-bit 编码完全按 6.12 实现。

---

## 6. 兼容性总结表

| 类别 | 6.6 → 6.12 兼容性 | Stage 1.3 处理 |
|------|----------------|---------------|
| `struct hmm_mirror` | ❌ 已移除 | **不声明**（Decision 2；用 `mmu_interval_notifier`） |
| `struct hmm_range` 字段 | ✅ 无变化 | 7 字段完整实现 |
| `struct mmu_interval_notifier` | ✅ 无变化 | 6 字段完整实现 |
| `mmu_interval_notifier_insert` 签名 | ⚠️ `length→end` | 使用 6.12 签名 |
| Sequence 协议 | ✅ 无变化 | 完整实现三函数 |
| HMM PFN flags | ✅ 无变化 | 64-bit 编码按 6.12 |
| `mmu_notifier_ops` callbacks | ✅ 无变化 | 3 个 callback 完整实现 |
| `mmu_notifier_register/unregister` | ✅ 无变化 | 按 6.12 实现 |

---

## 7. 已知限制

| 项目 | 说明 | 计划 |
|------|------|------|
| 多级页表 | 1.3 仅实现单级 4KB（与 1.1 IOMMU 一致） | Stage 2+ |
| NUMA-aware placement | 1.3 不实现 | Stage 2+ |
| Hardware-accelerated migration | 1.3 仅软件 fallback | Stage 2+ |
| 完整 zone_device | 1.3 仅最简实现（spm vma + 三态机） | Stage 2+ |

---

## 8. 引用

- Linux 6.12 LTS 头文件：`include/linux/hmm.h`、`include/linux/mmu_notifier.h`
- AMD KFD 调用链：`amdgpu_mn.c` → `mmu_interval_notifier_insert()`
- Librarian 验证（2026-07-02）：确认 amdkpu 已迁移到 `mmu_interval_notifier`
- ADR-027 spec-driven 增量原则
- Design.md Decision 2: `mmu_interval_notifier` 替代 `hmm_mirror`
- Design.md Decision 6: 目标 LTS 锁定 Linux 6.12

---

**维护者**：UsrLinuxEmu Architecture Team
**最后更新**：2026-07-04
**对应 SSOT**：`docs/02_architecture/post-refactor-architecture.md §1.10`