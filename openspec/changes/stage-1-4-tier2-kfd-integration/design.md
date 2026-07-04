## Context

Stage 1.4 Tier-1 KFD portability 已交付（[kfd-portability-boundary.md v1.0](../../docs/05-advanced/kfd-portability-boundary.md) Tier-1 §2，commit `80f6a44`），5 个 KFD ioctl 编号预留、19 个 ioctl 派发表、kfd_queue.c 单文件 PoC 编译通过、3 个设备节点创建、10 个 sim C 接口暴露。但 Tier-1 同时**实证**了 Tier-2 三类遗留（boundary §3）：

1. **9 个 STUB_HANDLER** 在 `plugins/gpu_driver/drv/gpu_drm_driver.cpp:274-282` 仅 `return 0`，无运行时行为：
   - `gpu_ioctl_register_mmu_cb`（关键，mmu_notifier 链路入口）
   - `gpu_ioctl_register_firmware_cb`
   - `gpu_ioctl_create_va_space` / `gpu_ioctl_destroy_va_space`
   - `gpu_ioctl_register_gpu`
   - `gpu_ioctl_create_queue` / `gpu_ioctl_destroy_queue` / `gpu_ioctl_map_queue_ring`
   - `gpu_ioctl_query_queue`
2. **mmu_notifier callback body 是 TODO**：`src/kernel/iommu/invalidate.cpp:27` 标注 `TODO(stage-1.3): implement mmu_notifier callback body`
3. **IOMMU IOTLB flush 是 fprintf stub**：`src/kernel/iommu/dma_remap.cpp:145-151` 标注 logging stub

**关键约束**：
- **承接 G1-G4 边界契约**（1.2/1.3 已锁）：`drm_device` 生命周期 = `GpgpuDevice` 生命周期 / BO 引用计数 / prime 释放顺序 / fence 触发时机
- **承接 Tier-1 决策 2**：HAL ops 严格走 ADR 流程，**不预先**添加
- **承接诚实优先**：handler 注释 "deferred" / "stub" 标记在 Tier-2 完成后**显式替换**为 "Tier-2 penetrated"
- **承接 5 个 KFD IOCTL 编号**（1.2 已预留）：不重新设计 ABI

**利益相关方**：
- KFD 真实驱动开发者（想要运行时真行为，而不仅是 dispatch 正确）
- Stage 2 规划者（KFD Tier-2 runtime 是 Stage 2 已具备能力之一）
- 真机部署团队（Tier-2 runtime 是 HAL 桥真实工作的基础）

## Goals / Non-Goals

**Goals:**

1. **G1**：9 个 STUB_HANDLER 升级为真实运行时行为（基于现有 sim 原语 + GpgpuDevice 既有实现，**不引入新 HAL ops**）
2. **G2**：mmu_notifier callback body 完整化（从 user-space munmap → kernel invalidation → mmu_notifier callback → sim 原语的完整调用图）
3. **G3**：`iommu_flush_iotlb` 升级为真实 page table invalidation（**仅在用户态模拟范围内**，不依赖 host kernel）
4. **G4**：所有 Tier-2 runtime 行为由新增 runtime test 覆盖（happy path + 至少 1 个 error path）
5. **G5**：G1-G4 边界契约在 Tier-2 实施时**不破坏**（`tests/test_uvm_drm_lifecycle_standalone` 全验证）
6. **G6**：诚实记录 Tier-2 完成状态（新增 `tier2-runtime-penetration-report.md`）

**Non-Goals:**

1. **NG1**：多文件 KFD 集成（kfd_module.c / kfd_device.c / kfd_process.c / kfd_doorbell.c）—— 53+ amdgpu headers 阻塞，~50K 行 amdgpu driver 需连同移植（boundary §3.4，Stage 3+ 或独立子项目）
2. **NG2**：完整 kfd_queue.c queue 生命周期（queue_create / destroy / mqd / doorbell）—— 上游原文件后段函数需 amdgpu_* 依赖（boundary §3.5）
3. **NG3**：IOMMU 真实硬件 invalidation（需 host kernel 介入，如 vfio）—— UsrLinuxEmu 用户态无法触发（boundary §3.2）
4. **NG4**：ATS PRI/PRG response routing —— 需 PCIe 4.0+ 设备（boundary §3.2）
5. **NG5**：mmu_notifier 真实进程模型（需真实 mm struct）—— 当前 SimPageFaultHandler 是匿名 namespace struct（boundary §3.3，Stage 2）

## Decisions

### D1：STUB_HANDLER 升级策略——**桥接到既有 GpgpuDevice 实现**（不重写）

**背景**：`plugins/gpu_driver/drv/gpu_drm_driver.cpp` 的 STUB_HANDLER 是薄 wrapper。Tier-1 B.2 阶段已示范成功模式：把 `gpu_ioctl_get_process_aperture` 等 4 个 handler 升级为"参数校验 + 调 gpgpu_device 既有方法"。Tier-2 9 个 STUB 沿用同一模式。

**9 个 STUB 的升级映射**：

| STUB_HANDLER | Tier-2 升级目标 | 桥接路径 |
|--------------|---------------|---------|
| `gpu_ioctl_register_mmu_cb` | 注册 mmu_notifier callback（**Tier-2 G2 关键**） | `mmu_interval_notifier_register` 链路 |
| `gpu_ioctl_register_firmware_cb` | 注册 firmware load callback | `fops->register_fw_cb` 占位实现 |
| `gpu_ioctl_create_va_space` | 调 GpgpuDevice 既有 create_va_space | `gpgpu_device_->create_va_space(args)` |
| `gpu_ioctl_destroy_va_space` | 调 GpgpuDevice 既有 destroy_va_space | `gpgpu_device_->destroy_va_space(args)` |
| `gpu_ioctl_register_gpu` | 注册 GPU device（一次性）| `gpgpu_device_->register_gpu(args)` |
| `gpu_ioctl_create_queue` | 调 GpgpuDevice 既有 create_queue | `gpgpu_device_->create_queue(args)` |
| `gpu_ioctl_destroy_queue` | 调 GpgpuDevice 既有 destroy_queue | `gpgpu_device_->destroy_queue(args)` |
| `gpu_ioctl_map_queue_ring` | 映射 ring buffer 到 user-space | `mmap` / `dma_buf_mmap` |
| `gpu_ioctl_query_queue` | 查询 queue 状态 | `gpgpu_device_->query_queue(args)` |

**为什么不重写**：GpgpuDevice 已有 `create_va_space` / `create_queue` 等成熟实现（Phase 2 + Stage 1.0-1.3 累计），重新实现会引入回归风险。

**拒绝的替代方案**：直接在 gpu_drm_driver.cpp 内实现完整逻辑——会与 GpgpuDevice 重复实现，且破坏 HAL 桥（gpgpu_device.cpp 是 ② 可移植驱动层）。

### D2：mmu_notifier callback body 完整化策略——**最小可行 callback**

**背景**：`mmu_interval_notifier_register` API 已完整（1.3 阶段），但 `invalidate.cpp:27` 标注 TODO。当前 `mmu_interval_read_begin/retry/set_seq` 已可调用，但 callback body 未触发 sim 原语。

**设计**：定义一个 `mmu_invalidate_callback` 函数，注册到 `mmu_interval_notifier`，**不引入新 sim 原语**（沿用 1.3 阶段已暴露的 10 个 sim C 接口）：

```
user munmap()
  → kernel VMA close
  → mmu_interval_notifier_invalidate()  (1.3 API)
  → mmu_invalidate_callback()  (Tier-2 新增)
  → sim_pfh_inject_fault(addr, &pfn)  (1.3 已暴露)
  → sim_pm_migrate_to_system(...)  (1.3 已暴露)
  → user-space 后续访问触发 page fault（已由 1.3 fault_inject 实现）
```

**为什么不引入新 sim 原语**：现有 `sim_pfh_*` / `sim_pm_*` 10 个接口已覆盖 callback 所需全部能力（commit `32e012d` 增强了 cause register，`ff7da37` 增强了 page table simulation），新增接口会破坏 ADR-027 spec-driven 原则。

### D3：IOTLB flush 真实化策略——**用户态 page table 标记 + callback 触发**

**背景**：`iommu_flush_iotlb` 当前 `fprintf(stderr, ...) + return`，无 page table invalidation。

**设计**：
- `iommu_domain->ops->flush_iotlb` 维持函数指针签名（ABI 兼容）
- 实现改为：遍历 `iommu_domain->page_table` → 标记 invalid → 触发 `sim_pfh_inject_fault_with_cause`（**已由 commit `32e012d` 增强**）
- 不依赖 host kernel（**纯用户态模拟**）

**为什么不依赖 host kernel**：UsrLinuxEmu 是用户态环境，无 root 权限；触发 host kernel IOTLB flush 需要 `/dev/iommu` 或 vfio 接口，超出 Stage 1 范围。

**拒绝的替代方案**：用 mprotect + mmap 模拟硬件 IOTLB flush——会破坏 Linux 6.12 ABI 兼容（KFD 真实驱动期望 iommu_domain API，不期望 mmap 副作用）。

### D4：runtime test 设计策略——**每个 STUB 一个 test_*.cpp**

**设计**：
- 9 个 STUB → 9 个 `test_*_runtime_standalone.cpp`
- 每个测试：参数校验路径 + happy path + 至少 1 个 error path
- 不破坏 G1-G4 契约（`tests/test_uvm_drm_lifecycle_standalone` 必须仍全绿）

**为什么每个 STUB 独立测试**：9 个 STUB 独立生命周期（不同 ioctl 编号 + 不同 sim 原语），耦合测试会引入隐式依赖，调试困难。

**拒绝的替代方案**：合并 `test_kfd_runtime_standalone.cpp` 一个测试——与 Tier-1 B.3 的"5 个新 standalone"模式（commit `8a95055`）一致，避免单个测试爆炸增长。

### D5：诚实标记替换策略——**handler 注释 + boundary 文档同步更新**

**设计**：每个 STUB 升级后，handler 注释从 `STUB_HANDLER` 宏改为显式函数体（含 Tier-2 penetrated 注释 + 引用 boundary §3.x）。`kfd-portability-boundary.md` Tier-2 §3.x 状态从 "Stub / Logging / TODO" 改为 "Penetrated" + 完成时间戳。

**为什么不保持 "deferred" 注释**：诚实优先原则要求**显式反映实际状态**。保留 "deferred" 注释是欺骗。

## Risks / Trade-offs

### R1：9 个 STUB 升级可能暴露 Tier-1 隐藏的设计假设

**风险**：Tier-1 阶段 9 个 STUB 仅 return 0，隐藏了"哪些参数需要校验 / 哪些 sim 原语需要先初始化"的假设。升级时可能发现 GpgpuDevice 既有方法对某些参数不支持（如 `gpu_ioctl_register_mmu_cb` 的 mmu_interval_notifier 初始化）。

**缓解**：
- 每个 STUB 升级前先写 runtime test（红）→ 改 handler（绿）→ commit
- 升级失败时回退到 STUB_HANDLER 状态 + 在 boundary §3 标注"穿透受阻：[具体原因]"

### R2：mmu_notifier callback 与 G1-G4 边界契约可能冲突

**风险**：callback body 触发 sim 原语可能破坏 1.2/1.3 已锁的 4 项契约（如 `dma_buf_unmap → dma_buf_detach → dma_buf_put` 顺序假设）。

**缓解**：
- 每个 callback 升级前跑 `tests/test_uvm_drm_lifecycle_standalone` G1-G4 全验证
- 失败时**优先修复 G1-G4 regression**，暂缓 Tier-2 推进（按用户决策 3）

### R3：IOTLB flush 真实实现触发未预期的 IOMMU 域边界

**风险**：遍历 `iommu_domain->page_table` 时遇到多 domain 跨边界场景，导致 segfault 或无限循环。

**缓解**：
- 先在 `tests/test_iommu_invalidate_runtime_standalone` 单测验证
- 单测通过前**不**进入集成测试
- 显式限制递归深度（防御性编程）

### R4：Tier-2 升级导致 commit `80f6a44` 已 merge 的 Tier-1 测试 regression

**风险**：Tier-1 阶段 commit `8a95055 test(runtime): B.3 runtime test matrix` 中的 UPDATE_QUEUE/APERTURE/E2E 测试可能在 Tier-2 升级时被破坏（如 VA Space 销毁顺序变化影响 aperture 查询）。

**缓解**：
- Tier-2 每个 commit 前跑 `ctest --test-dir build --output-on-failure` 全量
- regression 时**立即 revert**该 commit + 在 boundary §3 标注 regression 详情

### R5：9 个 STUB 升级是**重复性工作**，缺乏架构突破

**权衡**：Tier-2 主要是"STUB → 真实"模式应用，**架构创新**有限（对比 Stage 1.0-1.3 的内核环境模拟层新增）。但用户价值高（KFD 真实驱动开发者立刻可用）。

**缓解**：在 `tier2-runtime-penetration-report.md` 中明确记录"本次工作是 Tier-1 → Tier-2 演进，**不引入新架构**"，避免误判 Stage 1 整体价值。

## Open Questions

1. **OQ1**：Tier-2 启动是否需要新建 worktree？—— 用户决策 1 要求"实施 1.4 代码时创建 worktree"，Tier-2 是 1.4 的延伸，**预期需要** `stage-1.4-tier2-kfd-integration` worktree（在 LC4 阶段创建）
2. **OQ2**：`gpu_ioctl_register_mmu_cb` 的 mmu_notifier 注册是否需要限定为**单 callback per drm_file**？—— Tier-2 阶段先实现"单 callback"，多 callback 留待 Stage 2
3. **OQ3**：`gpu_ioctl_register_firmware_cb` 是否真的需要 firmware 加载路径？—— UsrLinuxEmu 当前无 firmware 文件系统支持，**先实现 callback 占位**，firmware 实际加载延后 Stage 2
4. **OQ4**：Tier-2 完成的判定是"9 个 STUB 全部穿透"还是"关键 STUB（mmu_cb / va_space / queue）穿透 + 其余延后"？—— 倾向**全部穿透**（统一交付），但允许 NG1-NG5 的显式延后