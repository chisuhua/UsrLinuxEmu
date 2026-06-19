# ADR-031: TTM 迁移实施优先级 (TTM Migration Priority)

**状态**: ✅ 已接受 (Accepted)

**日期**: 2026-06-16（v0 占位）→ 2026-06-17（v1 决策）

**提案人**: UsrLinuxEmu Architecture Team

**评审者**: 待定

**关联 ADR**: ADR-018 (驱动/仿真分离), ADR-019 (DRM/GEM/TTM 对齐), ADR-020 (libgpu_core 提取), ADR-016 (GPU Memory Domain)

**修订记录**:
- 2026-06-16 v0: 占位骨架（来自 ADR 编号治理清理）
- 2026-06-17 v1: 填入 TTM wrapper 决策（change cleanup-adr-placeholders）

## 背景

`docs/00_adr/adr-019-drm-gem-ttm-alignment.md` §6 明确：

> | Phase 2 TTM 迁移路径不清晰 | ADR-031 定义实施优先级，先实现基本 TTM 骨架 |

当前 UsrLinuxEmu 的内存管理路径：

- **驱动层**：`plugins/gpu_driver/drv/gpgpu_device.cpp` 的 `handle_alloc_bo` / `handle_map_bo`
- **HAL 层**：`plugins/gpu_driver/hal/gpu_hal.h` 的 `alloc` / `map` 回调
- **仿真层**：`libgpu_core/include/gpu_buddy.h` + `libgpu_core/src/gpu_buddy.c`（buddy allocator）+ VA Space 抽象

TTM (Translation Table Manager) 是 Linux 内核的标准 GEM 内存管理框架（DRM 子系统的一部分）。UsrLinuxEmu 当前在 `include/linux_compat/drm/drm_*.h` 中有 stub，但**真正的 TTM 路径未实现**。

## 决策（v1）

TTM 在 UsrLinuxEmu 中作为 **thin wrapper layer**（包装层）实现，**不替代** `libgpu_core/gpu_buddy`，**不实现 full TTM**。

### 架构

```
┌──────────────────────────────────────┐
│  handle_alloc_bo / handle_map_bo     │  ← plugins/gpu_driver/drv/gpgpu_device.cpp
└──────────────────┬───────────────────┘
                   │
                   ↓
┌──────────────────────────────────────┐
│  TTM 包装层 (include/linux_compat/   │  ← 新增：drm/ttm.h
│  drm/ttm.h) — BO metadata + placement│
└──────────────────┬───────────────────┘
                   │ (调用 page pool 接口)
                   ↓
┌──────────────────────────────────────┐
│  libgpu_core/gpu_buddy              │  ← 已有：纯 C buddy allocator
│  (page pool — 实际分配器)            │
└──────────────────────────────────────┘
```

### 关键点

- **buddy 是 page pool**：实际物理页分配走 `libgpu_core/include/gpu_buddy.h` 的 `gpu_buddy_alloc()` / `gpu_buddy_free()`（不改名、不重构）
- **TTM 加 BO metadata**：`ttm_buffer_object` 结构包含 `size`, `domain` (VRAM/GTT/CPU), `flags`, `gpu_va`, `handle`, `fence_id`
- **TTM 加 placement 策略**：`ttm_place` 函数根据 `domain + flags` 选择 `gpu_buddy_alloc(domain, size)` 参数
- **不替代 buddy**：v1 决策保留 ADR-020 提取的纯 C buddy allocator，不把 buddy 代码搬入 TTM 内部

### 新文件位置

- `include/linux_compat/drm/ttm.h`（与其他 drm_*.h 并列）

### v1 函数签名（仅声明，结构体布局不保证）

```c
/* 仅函数签名稳定；struct 内部字段为 v1 实现细节 */
int  ttm_bo_create(u64 size, u32 domain, u32 flags, u32 *out_handle);
int  ttm_bo_map(u32 handle, u64 *out_gpu_va);
int  ttm_bo_destroy(u32 handle);
int  ttm_bo_wait_fence(u32 handle, u64 fence_id);
int  ttm_bo_place(u32 handle, u32 new_domain);  /* placement 调整 */
```

> ⚠️ v1 仅函数签名稳定；`ttm_buffer_object` 结构体内部布局**不保证 ABI 兼容**。后续 v2 实施时需引入版本号或重新评估。

### 明确不做什么

- **不做 full TTM**（含 swapout）：UsrLinuxEmu 无 swap 设备，swapout 留 Phase 3+ 后续 ADR
- **不替代 buddy**：保留 ADR-020 治理意图（libgpu_core 提取，buddy 作为 page pool）
- **不保证结构体布局兼容**：仅函数签名稳定；struct 内部字段为 v1 实现细节
- **不实现用户态 page fault handler**：BO 分配时直接 mmap，不模拟 page fault

### v0 开放问题回答

1. **TTM 范围**：仅 BO 分配 / placement / fence 同步（v1 决策）。不做 swapout、不做 page replacement、不做 userptr。
2. **与现有 buddy allocator 的关系**：包装（wrapper）关系（v1 决策）。buddy 是底层 page pool；TTM 加 metadata + placement 策略。**不**替代。
3. **跨设备 placement 策略**：v1 仅支持单设备场景（UsrLinuxEmu 现状）。placement 优先级：VRAM > GTT > CPU（与真内核 TTM 一致）；多设备 P2P 留 Phase 3+ 后续 ADR。
4. **fence 同步**：复用现有 `gpu_wait_fence_args` 路径（`GPU_IOCTL_WAIT_FENCE` ioctl）。TTM BO 的 fence 字段映射到全局 `fence_id`（由 `GpgpuDevice::handle_create_fence` 分配）。
5. **测试覆盖**：复用 adr-019 中的 GEM 测试场景（`tests/test_gpu_alloc_bo_standalone.cpp` 已有 8 个 case）。新增 `tests/test_ttm_wrapper_standalone.cpp`（v1 实施时新增，≥6 个 case：alloc/map/place/destroy/fence/domain-优先级）。

## 当前状态

| 组件 | 状态 |
|------|------|
| `include/linux_compat/drm_*.h` | ✅ Stubs 已实现（drm_ioctl / drm_gem / drm_driver）|
| `libgpu_core/include/gpu_buddy.h` | ✅ 纯 C buddy allocator（ADR-020 产物）|
| TTM 包装层（`include/linux_compat/drm/ttm.h`）| ⏳ v1 设计完成，Phase 3 实施 |
| TTM BO allocator | ⏳ v1 设计完成，Phase 3 实施 |
| TTM placement 策略 | ⏳ v1 设计完成，Phase 3 实施 |
| TTM fence 同步 | ⏳ v1 设计完成，Phase 3 实施（复用 `gpu_wait_fence_args`）|
| TTM swapout / page replacement | ❌ 永久不实现（v1 决策：UsrLinuxEmu 无 swap）|
| Full TTM（含 swapout）| ❌ 永久不实现（v1 决策）|

## 后续

- **Phase 3 实施**：
  - 新增 `include/linux_compat/drm/ttm.h`（v1 函数签名）
  - 新增 `plugins/gpu_driver/sim/ttm_wrapper.cpp`（包装层实现）
  - 扩展 `handle_alloc_bo` 调用 `ttm_bo_create()` → `gpu_buddy_alloc()`
  - 新增 `tests/test_ttm_wrapper_standalone.cpp`（≥6 个 case）
- **代码引用**：
  - Page pool：`libgpu_core/include/gpu_buddy.h`（不修改）
  - TTM header：`include/linux_compat/drm/ttm.h`（新增）
  - TTM impl：`plugins/gpu_driver/sim/ttm_wrapper.cpp`（新增）
- **Phase 3+ 演进**：若需要 full TTM（含 swapout）或 P2P 多设备 placement，新建 ADR-031 v2

## 讨论历史 (v0 占位)

> 以下内容来自 2026-06-16 v0 占位骨架，保留作为 ADR 演进的历史记录。

### v0 决策

待定。本 ADR 当前为占位骨架。Phase 3+ 详细设计时需要回答：

1. **TTM 范围**：只做 BO 分配 / placement / fence 同步？还是要做 full TTM（含 swapout）？
2. **与现有 buddy allocator 的关系**：TTM 是替代还是包装？buddy 是 TTM 的底层 page pool 吗？
3. **跨设备 placement 策略**：UsrLinuxEmu 当前只有单设备场景，TTM 的多域（VRAM/GTT/CPU）placement 优先级如何？
4. **fence 同步**：与现有的 `gpu_wait_fence_args` 路径如何整合？
5. **测试覆盖**：能否复用 adr-019 中的 GEM 测试场景？

### v0 后续说明

详细设计待 Phase 3+ 启动后由对应 owner 填充。

> **v1 落地后**：上述 5 个开放问题已在 v1 决策中回答；详见上方"决策（v1）"与"v0 开放问题回答"段。

## 相关文档

- `docs/00_adr/adr-019-drm-gem-ttm-alignment.md`（TTM 对齐路径）
- `docs/02_architecture/post-refactor-architecture.md` §1.4（数据模型）
- `libgpu_core/include/gpu_buddy.h`（page pool，v1 决策保留）
- `include/linux_compat/drm/drm_*.h`（TTM header 位置参考）
- `docs/openspec/changes/cleanup-adr-placeholders/`（本变更的设计与 spec）
