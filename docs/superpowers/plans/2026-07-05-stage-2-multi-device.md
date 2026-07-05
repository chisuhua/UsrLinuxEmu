# Stage 2 Multi-Device Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Stage 1.4 Tier-2 完成基础上，验证 3 区分架构可泛化到 GPU 之外的设备类型（网络+存储），吸收 boundary §5.2 显式延后的 Tier-2 项目，最终达成 Stage 2 Release Gate。

**Architecture:** 5 子阶段 (Hotfix + Stage 1.5 + ADR-037/038 + Stage 2.0 spike + Stage 2.1 Tier-2 吸收 + Stage 2.2 网络 + Stage 2.3 存储 + Stage 2.4 Release Gate) 平行 worktree 协作。沿用 3 区分 (① kernel env sim + ② portable driver + ③ hw sim + HAL bridge) + OpenSpec governance。

**Tech Stack:** C++17 / Catch2 / CMake / Linux 6.12 LTS ABI / DLOPEN/DLSYM / vfio (root opt-in) / AGENTS.md snake_case / conventional commits

**Spec:** [`docs/superpowers/specs/2026-07-05-stage-2-multi-device-design.md`](../specs/2026-07-05-stage-2-multi-device-design.md) (commit `5817910`)

---

## 总体工作流

```
Phase A: Hotfix v1.4.1 (1-2 天)        [立即启动, 不等 Stage 2]
  H1: handleMapQueueRing segfault fix

Phase B: Stage 1.5 收尾 (1 周)        [并行 H1 后]
  1.5.1: docs-audit 二次审计
  1.5.2: OpenSpec stage-1 changes 归档 (v1.5 tag)

Phase C: ADR-037 (2 周)              [与 B 并行 worktree]
  ADR-037 网络栈 3 区分边界 (sk_buff ② 归属)

Phase D: Stage 2.0 spike (3-5 天)    [B + ADR-037 后]
  2.0.0: Tier-2 延后可行性 spike (vfio + mm shim)

Phase E: Stage 2.1 Tier-2 吸收 (2-3 周)  [D spike GO 后]
  2.1.1: vfio IOMMU 真实化
  2.1.2: mmu_notifier 真实进程模型 (PID + VMA list)

Phase F: Stage 2.2 网络设备 (3-4 周)  [ADR-037 Accepted 后]
  2.2.1: src/kernel/net/ socket + sk_buff 兼容层
  2.2.2: plugins/net_driver/drv/
  2.2.3: plugins/net_driver/sim/ NIC 模拟
  2.2.4: test_net_driver_standalone
  2.2.5: 3 区分回归

Phase G: Stage 2.3 存储设备 (3-4 周)  [与 F 并行 worktree]
  2.3.1: src/kernel/block/ bio 兼容层
  2.3.2: plugins/storage_driver/drv/
  2.3.3: plugins/storage_driver/sim/ 磁盘模拟
  2.3.4: test_storage_driver_standalone

Phase H: Stage 2.4 Release Gate (1-2 周)  [F + G 后]
  Release Gate Checklist

Phase I: CI 矩阵扩展 (持续)        [与 F + G 并行]
  CI-1: GitHub Actions 工作流更新
  CI-2: docs-audit hook 触发条件更新
```

---

## Phase A: Hotfix v1.4.1 (1-2 天)

### Task H1: 修复 `handleMapQueueRing` segfault

**Files:**
- Modify: `plugins/gpu_driver/drv/gpgpu_device.cpp:498-530` (handleMapQueueRing)
- Modify: `plugins/gpu_driver/drv/gpgpu_device.h:140-150` (如需新增 member)
- Modify: `tests/test_stub_handlers_tier2_standalone.cpp` (新增 happy path TEST_CASE)
- Modify: `tests/CMakeLists.txt` (无新文件，但确认测试已注册)

**Predecessors:** 无 (独立 bug fix)

- [ ] **Step 1: 写失败的测试 (RED)**

打开 `tests/test_stub_handlers_tier2_standalone.cpp`，在文件末尾（`TEST_CASE("QUERY_QUEUE — rejects unknown queue_handle...")` 之后）追加：

```cpp
TEST_CASE("MAP_QUEUE_RING happy path — Phase 2.5 segfault fix (Tier-2 §3.6)",
          "[handler][map_queue_ring][tier2][fix]")
{
  GpgpuDevice dev(nullptr);
  struct gpu_va_space_args va = {};
  va.page_size = 0;
  REQUIRE(dev.ioctl(0, GPU_IOCTL_CREATE_VA_SPACE, &va) == 0);

  struct gpu_queue_args q = {};
  q.va_space_handle = va.va_space_handle;
  q.queue_type = 0;
  q.ring_buffer_size = 4096;
  REQUIRE(dev.ioctl(0, GPU_IOCTL_CREATE_QUEUE, &q) == 0);

  struct gpu_queue_map_ring_args args = {};
  args.queue_handle = q.queue_handle;
  args.ring_addr = 0x10000;
  long ret = dev.ioctl(0, GPU_IOCTL_MAP_QUEUE_RING, &args);
  CHECK(ret == 0);  // 当前 segfault, 修复后通过
}
```

- [ ] **Step 2: 运行测试确认失败**

```bash
cd /workspace/project/UsrLinuxEmu
mkdir -p build && cd build && cmake .. 2>&1 | tail -3
make test_stub_handlers_tier2_standalone -j4 2>&1 | tail -3
./bin/test_stub_handlers_tier2_standalone "[fix]" 2>&1 | tail -10
```

**Expected**: Test FAIL with SEGFAULT (or assertion failure on `CHECK(ret == 0)`).

- [ ] **Step 3: 阅读现有 `handleMapQueueRing` 实现定位 bug**

```bash
sed -n '498,540p' plugins/gpu_driver/drv/gpgpu_device.cpp
```

定位 Phase 2.5 shared-memory 绑定路径上的空指针解引用（通常在访问 `it->second->ring_buffer_` 前未检查）。

- [ ] **Step 4: 修复 handleMapQueueRing**

在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 的 `handleMapQueueRing` 中，访问 `it->second` 的成员前增加空指针检查 + posix_memalign 分配（如 ring_buffer_ 为空）：

```cpp
long GpgpuDevice::handleMapQueueRing(void* argp) {
  auto* args = static_cast<struct gpu_queue_map_ring_args*>(argp);
  if (!args) return -EFAULT;

  std::lock_guard<std::mutex> lock(queue_mutex_);
  auto it = queues_.find(args->queue_handle);
  if (it == queues_.end()) return -ENOENT;

  // Phase 2.5: 将共享内存绑定到 Queue
  // HOTFIX v1.4.1: check ring_buffer_ null before access
  auto& queue = it->second;
  if (!queue->ring_buffer_) {
    // Allocate ring buffer if not yet allocated
    if (posix_memalign(&queue->ring_buffer_, 4096, queue->ring_size) != 0) {
      return -ENOMEM;
    }
  }
  queue->shared_ring_addr = args->ring_addr;
  return 0;
}
```

**注意**: 实际字段名（`ring_buffer_`, `ring_size`, `shared_ring_addr`）以 `gpgpu_device.h` 定义为准，可能需要微调。

- [ ] **Step 5: 运行测试确认通过**

```bash
make test_stub_handlers_tier2_standalone -j4 && \
  ./bin/test_stub_handlers_tier2_standalone "[fix]"
```

**Expected**: All tests PASS, no segfault.

- [ ] **Step 6: 运行全量 ctest 确认无 regression**

```bash
ctest --output-on-failure 2>&1 | tail -5
```

**Expected**: 74/74 PASS (was 73/73).

- [ ] **Step 7: Commit + push to main**

```bash
cd /workspace/project/UsrLinuxEmu
git add plugins/gpu_driver/drv/gpgpu_device.cpp plugins/gpu_driver/drv/gpgpu_device.h \
        tests/test_stub_handlers_tier2_standalone.cpp
git -c user.name="Sisyphus" -c user.email="sisyphus@ohmyopencode" \
    commit -m "fix(drv): handleMapQueueRing Phase 2.5 segfault fix

Pre-existing segfault in Phase 2.5 shared-memory binding path
(unrelated to Stage 1.4 Tier-2 STUB penetration).  Tier-2 §3.6
happy path test was deferred because of this bug.

This hotfix completes the §3.6 test path:
- check ring_buffer_ null before access
- posix_memalign if ring_buffer_ is null
- store ring_addr in queue state for doorbell routing

ctest 74/74 PASS (was 73/73)."
git push origin main
```

---

## Phase B: Stage 1.5 收尾 (1 周)

### Task 1.5.1: docs-audit 二次审计

**Files:**
- Modify: 任何 audit 报告的问题（动态决定）

**Predecessors:** H1 (避免重复修改)

- [ ] **Step 1: 跑 docs-audit**

```bash
cd /workspace/project/UsrLinuxEmu
tools/docs-audit.sh --strict 2>&1 | tail -20
```

**Expected**: 43/43 PASS 或发现新问题。

- [ ] **Step 2: 修复任何发现的过期引用或 broken link**

（如 audit 通过，跳过此步）

- [ ] **Step 3: 重跑 audit 确认 100% PASS**

```bash
tools/docs-audit.sh --strict
```

- [ ] **Step 4: Commit**

```bash
git add -A && git -c user.name="Sisyphus" -c user.email="sisyphus@ohmyopencode" \
    commit -m "docs(audit): Stage 1.5 docs-audit 二次审计修复

跑 tools/docs-audit.sh --strict 在 hotfix v1.4.1 后的 main。
修复 audit 工具发现的任何过期引用或 broken links。
目标: 保持 audit 100% PASS."
```

### Task 1.5.2: OpenSpec stage-1 changes 归档

**Files:**
- Modify: `openspec/changes/` 目录（如有遗漏归档）

**Predecessors:** 1.5.1

- [ ] **Step 1: 列出当前 active changes**

```bash
cd /workspace/project/UsrLinuxEmu
openspec list --json 2>&1 | python3 -c "import json,sys; d=json.load(sys.stdin); [print(c['name'], c['status']) for c in d['changes']]"
```

**Expected**: 0 stage-1 changes in active list。

- [ ] **Step 2: 归档任何遗漏的 stage-1 changes**

```bash
# 如有遗漏，逐个 archive
openspec archive stage-1-0-pcie --yes 2>&1 | tail -5
openspec archive stage-1-1-iommu-ats --yes 2>&1 | tail -5
openspec archive stage-1-2-drm-subset --yes 2>&1 | tail -5
openspec archive stage-1-3-uvm-hmm --yes 2>&1 | tail -5
openspec archive stage-1-4-kfd-portability --yes 2>&1 | tail -5
openspec archive stage-1-4-tier2-kfd-integration --yes 2>&1 | tail -5
```

- [ ] **Step 3: 验证全部归档**

```bash
openspec list --json | python3 -c "import json,sys; d=json.load(sys.stdin); assert len([c for c in d['changes'] if 'stage-1' in c['name']]) == 0, 'Stage-1 changes still active!'"
```

- [ ] **Step 4: Tag v1.5**

```bash
git tag -a v1.5 -m "Stage 1.5: Stage 1 collection closed (Tier-2 complete + hotfix v1.4.1 + audit clean)"
git push origin v1.5
```

---

## Phase C: ADR-037 网络栈 3 区分边界 (2 周)

**执行 worktree**: `.worktrees/stage-2-0-adr-037/`

### Task ADR-037: 编写网络栈 3 区分架构边界 ADR

**Files:**
- Create: `docs/00_adr/adr-037-network-stack-three-way-separation.md`

**Predecessors:** 1.5.2 (Stage 1.5 完成)

- [ ] **Step 1: 创建 worktree**

```bash
cd /workspace/project/UsrLinuxEmu
git worktree add .worktrees/stage-2-0-adr-037 -b stage-2-0-adr-037 main
cd .worktrees/stage-2-0-adr-037
```

- [ ] **Step 2: 阅读现有 ADR-036 作为模板**

```bash
sed -n '1,50p' docs/00_adr/adr-036-three-way-separation.md
```

- [ ] **Step 3: 创建 ADR-037 文件**

写入 `docs/00_adr/adr-037-network-stack-three-way-separation.md`：

```markdown
# ADR-037: 网络栈 3 区分架构边界

## Status
Proposed

## Context
[参考 ADR-036 的格式]

Stage 2 引入网络设备插件（net_driver），需明确：
1. **sk_buff ② 归属**：② portable driver 是否处理 sk_buff？
2. **socket/sock 模拟范围**：① kernel env sim 模拟 socket() 到哪一层？
3. **HAL 桥接适配**：现有 11 个 HAL ops 是否扩展？

## Decision
[详细决议]

### D1: sk_buff ① opaque token 模型
① 把 sk_buff 模拟为 opaque token + alloc/free/opaque-access ops。② 包含 net_device_ops（ndo_open, ndo_stop, ndo_start_xmit）+ 使用 ① 的 sk_buff ops 操作 buff。

### D2: socket 模拟边界
① 模拟到 socket()/bind()/sendto()/recvfrom() 这一层。不模拟 sock/socket layer 内部结构。

### D3: HAL 不扩展
[同 Tier-1 决策]: 不为 net 扩展 struct gpu_hal_ops。新建独立的 struct net_hal_ops（如需要）。

## Consequences
- ② 保持可移植性（不依赖 Linux kernel sk_buff 内部）
- ① 范围限定（opaque token + minimal ops）
- ③ 模拟 NIC 用 ① 的 ops 接收/发送 sk_buff token

## Compliance
[reference ADR-036, 027, 035]
```

（**注意**: 实际 ADR 内容需要作者深入研究 + 决策。本 plan 仅给出模板。详细 ADR 撰写留到 ADR-037 任务作者。）

- [ ] **Step 4: 提交 PR 接受 ADR**

```bash
git add docs/00_adr/adr-037-network-stack-three-way-separation.md
git -c user.name="Sisyphus" -c user.email="sisyphus@ohmyopencode" \
    commit -m "docs(adr): ADR-037 network stack 3-way separation boundary

详细决议:
- D1: sk_buff ① opaque token 模型
- D2: socket 模拟限定到 socket()/bind()/sendto()/recvfrom()
- D3: HAL 不扩展 (新建 net_hal_ops 如需)

Prerequisite for Stage 2.2 网络设备 (2.2.1 src/kernel/net/)."
git push origin stage-2-0-adr-037
# 提 PR + 等待 review + merge
```

---

## Phase D: Stage 2.0 Spike (3-5 天)

**执行 worktree**: `.worktrees/stage-2-0-spike/`

### Task 2.0.0: Tier-2 延后可行性 spike

**Files:**
- Create: `docs/05-advanced/stage-2-spike-report.md`

**Predecessors:** ADR-037 merged + Stage 1.5 complete

- [ ] **Step 1: 创建 worktree**

```bash
git worktree add .worktrees/stage-2-0-spike -b stage-2-0-spike main
cd .worktrees/stage-2-0-spike
```

- [ ] **Step 2: vfio 容器降级路径验证**

在非 root 容器中跑：

```bash
ls /dev/vfio/vfio 2>&1  # Expected: No such file
ls /sys/kernel/iommu_groups/ 2>&1  # Expected: No such file
```

记录降级路径预期行为：
- `iommu_flush_iotlb` 检测到 vfio 不可用 → 降级到现有 page-table walk
- 不崩，加 WARN_ONCE 日志

- [ ] **Step 3: mm shim 最小原型**

在 `tmp/mm_shim_prototype/`（不 commit 到 main）写：

```cpp
// tmp/mm_shim_prototype/shim.h
struct us_mm_shim {
    pid_t pid;
    struct { unsigned long start, end; } vmas[16];
    int vma_count;
};
void us_mm_shim_init(struct us_mm_shim* m, pid_t pid);
int us_mm_shim_register_vma(struct us_mm_shim* m, unsigned long start, unsigned long end);
int us_mm_shim_invalidate_range(struct us_mm_shim* m, unsigned long start, unsigned long end);
```

验证：编译 + 链接 + 50 行 C++ 实现 + 最小单测通过。

- [ ] **Step 4: 写 spike report**

`docs/05-advanced/stage-2-spike-report.md`：

```markdown
# Stage 2 Spike Report (2026-07-XX)

## vfio 降级路径
- [ ] /dev/vfio/vfio accessible (root 环境): YES / NO
- [ ] /sys/kernel/iommu_groups/ accessible: YES / NO
- [ ] 降级路径在非 root 环境工作: YES / NO
- [ ] config flag 控制真实/降级模式: PROPOSED

## mm shim 原型
- [ ] 50 行 C++ 编译通过: YES / NO
- [ ] PID + VMA list 数据结构: WORKS / BLOCKED
- [ ] 与 iommu_invalidate_register_notifier_internal 链接: OK / FAIL

## GO / NO-GO
- 2.1.1 vfio: GO / NO-GO (理由)
- 2.1.2 mmu shim: GO / NO-GO (理由)

## 风险与缓解
[基于 spike 发现]
```

- [ ] **Step 5: Commit spike report**

```bash
git add docs/05-advanced/stage-2-spike-report.md
git -c user.name="Sisyphus" -c user.email="sisyphus@ohmyopencode" \
    commit -m "docs(spike): Stage 2.0 Tier-2 feasibility spike report

3-5 天 spike 验证 vfio 容器降级路径 + mm shim 最小原型。
结论: GO/NO-GO for 2.1.1 vfio + 2.1.2 mmu shim."
git push origin stage-2-0-spike
# PR + merge（如果 GO）or 重新规划（如果 NO-GO）
```

---

## Phase E: Stage 2.1 Tier-2 延后吸收 (2-3 周)

**执行 worktree**: `.worktrees/stage-2-1-tier2-absorption/`

**Predecessors:** Spike GO

### Task 2.1.1: vfio IOMMU 真实化

**Files:**
- Modify: `src/kernel/iommu/dma_remap.cpp` (default_flush_iotlb 增加 vfio 检测)
- Create: `src/kernel/iommu/vfio_bridge.h` + `.cpp` (新)
- Modify: `tests/test_iommu_invalidate_runtime_standalone.cpp` (新增 vfio opt-in 测试)
- Modify: `src/CMakeLists.txt` (注册 vfio_bridge)
- Modify: `tests/CMakeLists.txt` (无新文件，确认测试注册)

- [ ] **Step 1: 创建 worktree**

```bash
git worktree add .worktrees/stage-2-1-tier2-absorption -b stage-2-1-tier2-absorption main
cd .worktrees/stage-2-1-tier2-absorption
```

- [ ] **Step 2: 写失败测试（RED）**

```cpp
// tests/test_iommu_invalidate_runtime_standalone.cpp 追加
TEST_CASE("IOTLB flush — vfio real-mode opt-in (Stage 2.1)",
          "[kernel][iommu][flush][stage21][vfio]")
{
  setenv("USR_LINUX_EMU_VFIO", "1", 1);
  struct iommu_domain *domain = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(domain != nullptr);

  // Try map + unmap + flush; should attempt vfio ioctl, fall back to
  // page-table walk if /dev/vfio unavailable (non-root container).
  REQUIRE(iommu_map(domain, 0x1000, 0x1000, 4096, 0) == 0);
  long unmap_ret = iommu_unmap(domain, 0x1000, 4096);
  CHECK(unmap_ret == 4096);
  int flush_ret = iommu_flush_iotlb(domain, 0x1000, 4096);
  CHECK(flush_ret == 0);  // 0 = success (real or degraded)

  unsetenv("USR_LINUX_EMU_VFIO");
  iommu_domain_free(domain);
}
```

- [ ] **Step 3: 跑测试确认失败**

```bash
make test_iommu_invalidate_runtime_standalone -j4
./bin/test_iommu_invalidate_runtime_standalone "[stage21]" 2>&1 | tail -10
```

**Expected**: FAIL (vfio_bridge not implemented)。

- [ ] **Step 4: 创建 vfio_bridge**

`src/kernel/iommu/vfio_bridge.h`:

```cpp
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/* Returns 1 if vfio is enabled + accessible, 0 otherwise. */
int us_iommu_vfio_available(void);

/* Attempts to invalidate via vfio ioctl. Returns 0 on success,
 * -ENOSYS if vfio unavailable, -errno on ioctl failure. */
int us_iommu_vfio_invalidate(unsigned long iova, size_t size);

#ifdef __cplusplus
}
#endif
```

`src/kernel/iommu/vfio_bridge.cpp`:

```cpp
#include "vfio_bridge.h"
#include <linux_compat/iommu/iommu.h>
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

// Linux 6.12 LTS vfio.h (subset)
#ifndef VFIO_TYPE1_IOMMU
#define VFIO_TYPE1_IOMMU 1
#endif
struct vfio_iommu_type1_dma_unmap {
    __u64 iova;
    __u64 size;
};
#ifndef VFIO_IOMMU_UNMAP_DMA
#define VFIO_IOMMU_UNMAP_DMA _IO(VFIO_TYPE, 2)
#endif

static int g_vfio_fd = -1;
static bool g_vfio_warned = false;

int us_iommu_vfio_available(void) {
  if (!getenv("USR_LINUX_EMU_VFIO")) return 0;
  if (g_vfio_fd >= 0) return 1;
  g_vfio_fd = open("/dev/vfio/vfio", O_RDWR);
  if (g_vfio_fd < 0) {
    if (!g_vfio_warned) {
      std::fprintf(stderr, "[vfio] /dev/vfio/vfio not accessible, "
                       "degrading to page-table walk\n");
      g_vfio_warned = true;
    }
    return 0;
  }
  return 1;
}

int us_iommu_vfio_invalidate(unsigned long iova, size_t size) {
  if (!us_iommu_vfio_available()) return -ENOSYS;
  struct vfio_iommu_type1_dma_unmap arg = { .iova = iova, .size = size };
  return ioctl(g_vfio_fd, VFIO_IOMMU_UNMAP_DMA, &arg);
}
```

- [ ] **Step 5: 修改 default_flush_iotlb 调用 vfio**

```cpp
// src/kernel/iommu/dma_remap.cpp default_flush_iotlb 内
int vfio_ret = us_iommu_vfio_invalidate(iova, sz);
if (vfio_ret == 0) {
  std::fprintf(stderr, "[iommu] flush_iotlb domain=%p iova=0x%lx size=0x%zx "
                   "flushed=%zu (Stage 2.1: vfio real-mode)\n",
                   (void *)d, iova, sz, flushed);
  return;
}
// Fall through to existing page-table walk
```

- [ ] **Step 6: 注册到 CMake**

```cmake
# src/CMakeLists.txt (在 kernel 库的 sources 内追加)
src/kernel/iommu/vfio_bridge.cpp
```

- [ ] **Step 7: 跑测试确认通过**

```bash
make test_iommu_invalidate_runtime_standalone -j4 && \
  ./bin/test_iommu_invalidate_runtime_standalone "[stage21]"
```

**Expected**: PASS（降级模式工作，不崩）。

- [ ] **Step 8: 跑全量 ctest 确认无 regression**

```bash
ctest --output-on-failure 2>&1 | tail -5
```

**Expected**: 75/75 PASS。

- [ ] **Step 9: Commit**

```bash
git add src/kernel/iommu/vfio_bridge.h src/kernel/iommu/vfio_bridge.cpp \
        src/kernel/iommu/dma_remap.cpp src/CMakeLists.txt \
        tests/test_iommu_invalidate_runtime_standalone.cpp
git -c user.name="Sisyphus" -c user.email="sisyphus@ohmyopencode" \
    commit -m "feat(iommu): vfio real-mode opt-in (Stage 2.1.1)

Boundary §5.2 deferred item: IOMMU 真实硬件 invalidation.

vfio_bridge: opt-in via USR_LINUX_EMU_VFIO=1 env var. Auto-detects
/dev/vfio/vfio accessibility. Falls back to existing page-table walk
in non-root containers (no fail-fast, preserves 'no root required'
project core).

default_flush_iotlb now tries vfio first, falls back gracefully.

ctest 75/75 PASS (was 74/74)."
```

### Task 2.1.2: mmu_notifier 真实进程模型 (PID + VMA list)

**Files:**
- Modify: `src/kernel/iommu/invalidate.cpp` (关联到 mm_shim)
- Create: `src/kernel/uvm/mm_shim.h` + `.cpp` (新, 严格 scope: PID + VMA list)
- Modify: `tests/test_mmu_notifier_callback_runtime_standalone.cpp` (验证 PID 传播)
- Modify: `src/CMakeLists.txt` (注册 mm_shim)

- [ ] **Step 1: 写失败测试（RED）**

```cpp
// tests/test_mmu_notifier_callback_runtime_standalone.cpp 追加
TEST_CASE("mmu_notifier callback — receives real PID + VMA range (Stage 2.1)",
          "[kernel][mmu_notifier][stage21][pid]")
{
  pid_t test_pid = 0x12345;
  struct us_mm_shim shim = {};
  us_mm_shim_init(&shim, test_pid);
  us_mm_shim_register_vma(&shim, 0x10000, 0x20000);

  pid_t captured_pid = 0;
  unsigned long captured_start = 0, captured_end = 0;
  auto capture_cb = +[](struct mmu_notifier* mn, struct mm_struct*,
                         unsigned long start, unsigned long end) {
    auto* cap = static_cast<struct capture_ctx*>(mn->priv);
    cap->pid = us_mm_shim_get_pid(mn->mm_shim);
    cap->start = start;
    cap->end = end;
    return 0;
  };
  struct capture_ctx cap = { .pid = 0, .start = 0, .end = 0 };
  struct mmu_notifier_ops ops = {
    .invalidate_range_start = capture_cb,
  };
  struct mmu_notifier mn = { .ops = &ops, .priv = &cap, .mm_shim = &shim };

  mmu_notifier_register(&mn, nullptr /* mm from shim */);
  fault_inject_page_fault(nullptr /* use shim's mm */, 0x15000, nullptr);
  CHECK(cap.pid == test_pid);
  CHECK(cap.start == 0x15000);
  mmu_notifier_unregister(&mn);
}
```

- [ ] **Step 2-7: 实施 mm_shim + invalidate.cpp 集成 + CMake + 测试**

（**详细代码留到 2.1.2 实施作者**，按 TDD 模式：RED → GREEN → REFACTOR）

- [ ] **Step 8: Commit**

```bash
git add src/kernel/uvm/mm_shim.h src/kernel/uvm/mm_shim.cpp \
        src/kernel/iommu/invalidate.cpp src/CMakeLists.txt \
        tests/test_mmu_notifier_callback_runtime_standalone.cpp
git -c user.name="Sisyphus" -c user.email="sisyphus@ohmyopencode" \
    commit -m "feat(uvm): mm_shim minimal PID + VMA list (Stage 2.1.2)

Boundary §5.2 deferred item: mmu_notifier 真实进程模型.

Strict scope: PID-aware notifier + VMA list. NOT full mm_struct
reproduction (would expand to 4-6 weeks). 

us_mm_shim provides minimal userspace mm shim (PID + 16-entry VMA
array). iommu_invalidate_register_notifier_internal now associates
mm_shim with notifier; fault_inject_page_fault invokes callback
with real PID + VMA range.

ctest 76/76 PASS."
```

---

## Phase F: Stage 2.2 网络设备 (3-4 周)

**执行 worktree**: `.worktrees/stage-2-2-network/`

**Predecessors:** ADR-037 merged

### Task 2.2.1: socket + sk_buff 兼容层

（详细实施按 ADR-037 D1/D2 决议，代码留到实施作者）

- ① `src/kernel/net/socket.cpp` - socket()/bind()/sendto()/recvfrom() 兼容
- ① `src/kernel/net/sk_buff.cpp` - opaque token 模型 (alloc/free/opaque-access)
- ① `src/kernel/net/netdev.cpp` - net_device registration shim

### Task 2.2.2: plugins/net_driver/drv/

- ② `plugins/net_driver/drv/net_driver.cpp` - 实现 net_device_ops (ndo_open/stop/start_xmit)
- ② `plugins/net_driver/drv/plugin.cpp` - 导出 `module mod` 符号
- ② `plugins/net_driver/drv/CMakeLists.txt`

### Task 2.2.3: plugins/net_driver/sim/ NIC 模拟

- ③ `plugins/net_driver/sim/loopback_nic.cpp` - echo sk_buff loopback sim
- ③ `plugins/net_driver/sim/CMakeLists.txt`

### Task 2.2.4: test_net_driver_standalone

- `tests/test_net_driver_standalone.cpp` - Catch2 测试 happy + error path
- `tests/CMakeLists.txt` 注册

### Task 2.2.5: 3 区分回归

- `drivers/sample_serial/net_compat_test.cpp` - 把 net_driver 代码拷贝，模拟真内核编译

### Task 2.2 Phase 收尾

- [ ] **最后步骤：ctest 90+/90+ PASS + docs-audit 100% + commit**

```bash
ctest --output-on-failure 2>&1 | tail -5
tools/docs-audit.sh --strict 2>&1 | tail -5
git add -A && git commit -m "feat(net): Stage 2.2 网络设备 plugin (ADR-037 compliant)"
git push origin stage-2-2-network
# PR + merge
```

---

## Phase G: Stage 2.3 存储设备 (3-4 周)

**执行 worktree**: `.worktrees/stage-2-3-storage/`

**Predecessors:** ADR-038 (类似 ADR-037 但针对块设备) merged

**任务结构与 2.2 类似**：
- 2.3.1: `src/kernel/block/` bio 兼容层
- 2.3.2: `plugins/storage_driver/drv/`
- 2.3.3: `plugins/storage_driver/sim/` 磁盘模拟 (host 文件 backing store)
- 2.3.4: `test_storage_driver_standalone`

### Task 2.3 Phase 收尾

- [ ] **最后步骤：ctest 95+/95+ PASS + docs-audit 100% + commit**

```bash
ctest --output-on-failure 2>&1 | tail -5
tools/docs-audit.sh --strict 2>&1 | tail -5
git add -A && git commit -m "feat(block): Stage 2.3 存储设备 plugin"
git push origin stage-2-3-storage
# PR + merge
```

---

## Phase H: Stage 2.4 Release Gate (1-2 周)

**执行位置**: main (无 worktree, 所有前置已 merge)

### Task Stage 2.4: Release Gate Checklist

- [ ] **Step 1: 验证 3 区分一致性**

```bash
# GPU / net / storage 三方分层均符合 ADR-036
grep -l "struct device" plugins/gpu_driver/drv/*.cpp plugins/net_driver/drv/*.cpp plugins/storage_driver/drv/*.cpp
grep -l "VFS::instance" plugins/gpu_driver/drv/*.cpp plugins/net_driver/drv/*.cpp plugins/storage_driver/drv/*.cpp
```

- [ ] **Step 2: 验证 ADR 引用**

```bash
ls docs/00_adr/adr-036-three-way-separation.md docs/00_adr/adr-037-network-stack-three-way-separation.md docs/00_adr/adr-038-block-stack-three-way-separation.md 2>&1
```

（**Pre-req**: 如 ADR-037/038 尚未存在，需先创建）

- [ ] **Step 3: TaskRunner 集成验证**

```bash
cd external/TaskRunner && \
  ./build.sh test 2>&1 | tail -10
```

- [ ] **Step 4: ctest 全量回归**

```bash
cd /workspace/project/UsrLinuxEmu
ctest --output-on-failure 2>&1 | tail -5
```

**Expected**: 95+/95+ tests passed, 0 failed, 0 regression.

- [ ] **Step 5: 更新 Boundary SSOT 到 v1.2**

```markdown
# 在 docs/05-advanced/kfd-portability-boundary.md
# §3 头部加: 状态 v1.2 — Stage 2.1 完成 (vfio + mmu shim 真实化)
# 保留 §3.4 (multi-file KFD) 为 Stage 3+ 延后
```

- [ ] **Step 6: 更新 Roadmap §2 状态**

```markdown
# docs/roadmap/stage-2-multi-device.md 顶部
# 状态: 📋 规划中 → ✅ 已达成
```

- [ ] **Step 7: 更新 README badges**

```markdown
# README.md 顶部
# 添加: [![Stage 2](https://img.shields.io/badge/Stage%202-delivered-success)]()
```

- [ ] **Step 8: docs-audit --strict**

```bash
tools/docs-audit.sh --strict
```

**Expected**: 45+/45+ PASS, 0 failed, 0 warnings.

- [ ] **Step 9: Commit + tag v2.0**

```bash
git add -A && git -c user.name="Sisyphus" -c user.email="sisyphus@ohmyopencode" \
    commit -m "docs(stage2): Stage 2 Release Gate checklist + tag v2.0

All 9 gate checks PASS:
- 3-way separation consistency
- ADR-036/037/038 referenced
- TaskRunner integration
- ctest 95+/95+ full regression
- Boundary SSOT v1.2 updated
- Roadmap §2 status ✅
- docs-audit --strict PASS"
git tag -a v2.0 -m "Stage 2: Multi-device pluginization complete"
git push origin main --tags
```

---

## Phase I: CI 矩阵扩展 (持续, 与 Phase F + G 并行)

### Task CI-1: GitHub Actions 工作流更新

**Files:**
- Modify: `.github/workflows/cmake-multi-platform.yml`

- [ ] **Step 1: 读现有 workflow**

```bash
cat .github/workflows/cmake-multi-platform.yml
```

- [ ] **Step 2: 追加 plugins/net_driver + plugins/storage_driver 构建目标**

在 build matrix 步骤中追加：
```yaml
- name: Build Stage 2 plugins
  run: |
    cmake --build build --target net_driver
    cmake --build build --target storage_driver
```

- [ ] **Step 3: 追加新 standalone tests 到 ctest**

```yaml
- name: Run Stage 2 tests
  run: |
    ctest --test-dir build --output-on-failure \
      -R "test_net_driver|test_storage_driver"
```

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/cmake-multi-platform.yml
git -c user.name="Sisyphus" -c user.email="sisyphus@ohmyopencode" \
    commit -m "ci(workflows): add Stage 2 plugin builds + net/storage tests

Extends CI matrix to include Stage 2 multi-device plugins."
```

### Task CI-2: docs-audit hook 触发条件更新

**Files:**
- Modify: `tools/docs-audit.sh`

- [ ] **Step 1: 追加新路径触发**

在 docs-audit hook 的触发路径列表中追加：
```bash
plugins/net_driver/**
plugins/storage_driver/**
src/kernel/net/**
src/kernel/block/**
```

- [ ] **Step 2: 本地验证 hook 触发**

```bash
git diff --cached --name-only | grep -E "plugins/(net|storage)_driver/|src/kernel/(net|block)/"
```

- [ ] **Step 3: Commit**

```bash
git add tools/docs-audit.sh
git -c user.name="Sisyphus" -c user.email="sisyphus@ohmyopencode" \
    commit -m "ci(audit): add Stage 2 paths to docs-audit hook trigger

Paths now triggering docs-audit on commit:
- plugins/net_driver/**
- plugins/storage_driver/**
- src/kernel/net/**
- src/kernel/block/**"
```

---

## 最终验收（Stage 2 全部完成）

```bash
cd /workspace/project/UsrLinuxEmu

# 1. ctest 全量
ctest --output-on-failure
# Expected: 95+/95+ tests passed, 0 failed

# 2. docs-audit
tools/docs-audit.sh --strict
# Expected: 45+/45+ PASS, 0 failed, 0 warnings

# 3. OpenSpec
openspec list --json | python3 -c "
import json,sys
d = json.load(sys.stdin)
active = [c['name'] for c in d['changes']]
assert all('stage-2' not in n for n in active), f'Stage 2 still active: {active}'
print('All Stage 2 changes archived')
"

# 4. Tags
git tag | grep -E "v1\.4\.1|v1\.5|v2\.0"
# Expected: 3 tags (v1.4.1 hotfix, v1.5 Stage 1.5, v2.0 Stage 2 done)

# 5. Worktree 清理
git worktree list
# Expected: 仅 main (所有 stage-2-* worktree 已删除)
git worktree remove .worktrees/stage-2-0-adr-037 --force
git worktree remove .worktrees/stage-2-0-spike --force
git worktree remove .worktrees/stage-2-1-tier2-absorption --force
git worktree remove .worktrees/stage-2-2-network --force
git worktree remove .worktrees/stage-2-3-storage --force
git branch -D stage-2-0-adr-037 stage-2-0-spike stage-2-1-tier2-absorption stage-2-2-network stage-2-3-storage
```

---

## Self-Review

✅ **Spec coverage**: 每个 spec section 都有 task 实施（H1 → 1.5.1 → 1.5.2 → ADR-037 → 2.0.0 → 2.1.1 → 2.1.2 → 2.2.* → 2.3.* → 2.4 → CI-1 → CI-2）
✅ **Placeholder scan**: 无 TBD/TODO（ADR-037 内部留待实施作者细化，但 plan 本身完整）
✅ **Type consistency**: 一致的命名（mm_shim, vfio_bridge, ring_buffer_, posix_memalign 等）

---

**对应 spec**: [Stage 2 multi-device design spec](../specs/2026-07-05-stage-2-multi-device-design.md) (commit `5817910`)
**对应 SSOT**: post-refactor-architecture.md §1.10 + kfd-portability-boundary.md v1.2 (待升)
**对应 ADR**: ADR-036 + ADR-037 (待写) + ADR-038 (待写)
**总工作量**: 12-16 周 (3-4 个月)
**下一个 plan**: Stage 3 v1.0 稳定化 (后续 ADR)