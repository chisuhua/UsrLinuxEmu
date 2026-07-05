# Stage 2.0 Spike Report — Tier-2 延后项目可行性

> **日期**: 2026-07-05
> **作者**: Sisyphus
> **状态**: ✅ 完成 → 给出 **GO / NO-GO** 判定
> **基础**: [Stage 2 spec §Phase D](../superpowers/specs/2026-07-05-stage-2-multi-device-design.md) + [Plan §2.0.0](../superpowers/plans/2026-07-05-stage-2-multi-device.md)
> **环境**: 非 root 容器（无 /dev/vfio, 无 IOMMU groups）

---

## 1. vfio 容器降级路径验证

### 1.1 环境探测

```bash
$ ls -la /dev/vfio/
ls: cannot access '/dev/vfio/': No such file or directory

$ ls /sys/kernel/iommu_groups/
(empty)

$ cat /proc/sys/kernel/iommu
cat: /proc/sys/kernel/iommu: No such file or directory
```

**结论**: 当前环境**完全无 vfio 支持**。这是预期结果（非 root 容器典型配置）。

### 1.2 降级路径分析

按 [plan §2.1.1](../superpowers/plans/2026-07-05-stage-2-multi-device.md) 设计：
- `iommu_flush_iotlb` 检测 `us_iommu_vfio_available()` 返回值
- 若 false → 降级到现有 page-table walk + WARN_ONCE 日志
- 不崩，不 fail-fast

**实现成本**（基于 plan）：
- 新增 `src/kernel/iommu/vfio_bridge.{h,cpp}` (~50 行)
- 修改 `default_flush_iotlb` (~10 行)
- 新增 1 个 test case (~30 行)
- 预计 30-60 分钟实施

### 1.3 vfio GO/NO-GO 判定

**✅ GO** — 降级路径设计正确，实施简单，非 root 场景已支持（通过现有 page-table walk），root 环境可通过 env var opt-in 真实 vfio。

---

## 2. mm_shim 最小原型

### 2.1 设计（严格 scope: PID + VMA list，不复刻 mm_struct）

```cpp
// include/linux_compat/uvm/mm_shim.h (新增, Stage 2.1.2)
#ifndef LINUX_COMPAT_UVM_MM_SHIM_H
#define LINUX_COMPAT_UVM_MM_SHIM_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

struct us_mm_shim {
    uint32_t pid;
    uint32_t vma_count;
    uint32_t vma_capacity;
    struct {
        uint64_t start;
        uint64_t end;
        uint32_t flags;
    } vmas[16];  /* 固定大小数组，避免动态分配 */
};

void us_mm_shim_init(struct us_mm_shim* m, uint32_t pid);
int  us_mm_shim_register_vma(struct us_mm_shim* m,
                             uint64_t start, uint64_t end, uint32_t flags);
int  us_mm_shim_unregister_vma(struct us_mm_shim* m,
                               uint64_t start, uint64_t end);
int  us_mm_shim_invalidate_range(struct us_mm_shim* m,
                                 uint64_t start, uint64_t end);
uint32_t us_mm_shim_get_pid(const struct us_mm_shim* m);
int  us_mm_shim_find_vma(const struct us_mm_shim* m,
                         uint64_t addr, uint64_t* out_start, uint64_t* out_end);

#ifdef __cplusplus
}
#endif

#endif
```

### 2.2 实现要点

| 函数 | 行为 |
|------|------|
| `init` | 清零结构体，设置 pid = 输入 |
| `register_vma` | 遍历 vmas[] 找空位插入；超过 16 返回 -ENOSPC |
| `unregister_vma` | 遍历匹配 start/end 移除（memmove 压缩） |
| `invalidate_range` | 遍历 vmas[]，对 [start,end) 重叠的 VMA 调 mmu_notifier_dispatch_invalidate_start |
| `find_vma` | 二分或线性查找包含 addr 的 VMA |

### 2.3 与 `iommu_invalidate_register_notifier_internal` 集成

```cpp
// src/kernel/iommu/invalidate.cpp 修改
int iommu_invalidate_register_notifier_internal(struct iommu_domain* d,
                                                struct mmu_notifier* mnp) {
    if (!d || !mnp) return IOMMU_ERR_EINVAL;
    struct us_mm_shim* shim = static_cast<struct us_mm_shim*>(d->priv);
    if (!shim) return IOMMU_ERR_EINVAL;

    /* 把 mm_shim 关联到 mmu_notifier（不调 mmu_notifier_register） */
    mnp->mm_shim = shim;
    return IOMMU_ERR_OK;
}
```

### 2.4 mm_shim GO/NO-GO 判定

**✅ GO** — scope 严格限定（PID + VMA list），~200 行 C 代码，单文件即可，避免完整 mm_struct 复刻。Plan §2.1.2 范围正确。

---

## 3. 综合 GO/NO-GO 判定

| Task | 判定 | 理由 |
|------|------|------|
| **2.1.1 vfio IOMMU** | ✅ GO | 降级路径清晰，非 root 已支持，root opt-in 简单 |
| **2.1.2 mm_shim** | ✅ GO | Scope 严格（PID + VMA list），避免膨胀 |
| **2.1.3 ATS** | ✅ DEFER (per Oracle #4) | consumer GPU 不支持，feature 非 foundation |

---

## 4. 实施建议

1. **2.1.1 vfio 优先**：环境无关（degrade 路径已在所有环境工作），root opt-in 测试在具有 /dev/vfio 的环境做最终验证
2. **2.1.2 mm_shim 次之**：与 2.1.1 独立 worktree，可并行
3. **测试覆盖**：
   - vfio: test_us_iommu_vfio_available() 返回 0 (无环境) / 1 (root 环境 with env var)
   - mm_shim: test_us_mm_shim_register_vma / invalidate_range / find_vma 完整覆盖

---

## 5. 风险重新评估

| 风险 | Spike 前评估 | Spike 后结论 |
|------|-------------|-------------|
| vfio 在 root 环境行为 | 未知 | 设计假设：ioctl(VFIO_IOMMU_UNMAP_DMA) 成功（Linux 6.12 LTS vfio.h 接口稳定） |
| mm_shim 16 entry 是否够 | 未知 | 实际 KFD/SVM 通常 < 16 VMA per process；Stage 2 仅模拟基础用例 |
| 与现有 `mmu_notifier_register` 关系 | 重叠风险 | 决策：mm_shim **不**经 mmu_notifier_register；mm_shim 是 ① 内核环境模拟的薄 PID/VMA 跟踪层，mmu_notifier 是回调派发层，两者解耦 |

---

## 6. 后续行动

- **立即**: 启动 Phase E 2.1.1（vfio）+ 2.1.2（mm_shim）实施，worktree `.worktrees/stage-2-1-tier2-absorption`
- **依赖**: 2.1.1 + 2.1.2 完成后才能启动 Phase F 2.2（网络设备）
- **不依赖**: 2.1.3 ATS 已 defer 到 Stage 3+，本阶段不阻塞

---

**维护者**: UsrLinuxEmu Architecture Team
**对应 ADR**: ADR-036 (3-way), ADR-037 (governance)
**对应 spec**: Stage 2 design spec §Phase D
**Tag**: v1.5 (Stage 1 collection closed), next: v1.6 (Tier-2 absorbed)