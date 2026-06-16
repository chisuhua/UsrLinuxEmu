# ADR-031: TTM 迁移实施优先级 (TTM Migration Priority)

**状态**: 🔄 提议中 (Proposed) — 占位骨架

**日期**: 2026-06-16

**提案人**: UsrLinuxEmu Architecture Team

**评审者**: 待定

**关联 ADR**: ADR-018 (驱动/仿真分离), ADR-019 (DRM/GEM/TTM 对齐), ADR-020 (libgpu_core 提取), ADR-016 (GPU Memory Domain)

**修订记录**:
- 2026-06-16 v0: 占位骨架（来自 ADR 编号治理清理）

## 背景

`docs/00_adr/adr-019-drm-gem-ttm-alignment.md` §6 明确：

> | Phase 2 TTM 迁移路径不清晰 | ADR-031 定义实施优先级，先实现基本 TTM 骨架 |

当前 UsrLinuxEmu 的内存管理路径：

- **驱动层**：`plugins/gpu_driver/drv/gpgpu_device.cpp` 的 `handle_alloc_bo` / `handle_map_bo`
- **HAL 层**：`plugins/gpu_driver/hal/gpu_hal.h` 的 `alloc` / `map` 回调
- **仿真层**：`libgpu_core/gpu_buddy.{h,cpp}`（buddy allocator）+ VA Space 抽象

TTM (Translation Table Manager) 是 Linux 内核的标准 GEM 内存管理框架（DRM 子系统的一部分）。UsrLinuxEmu 当前在 `include/linux_compat/drm_*.h` 中有 stub，但**真正的 TTM 路径未实现**。

## 决策

待定。本 ADR 当前为占位骨架。Phase 3+ 详细设计时需要回答：

1. **TTM 范围**：只做 BO 分配 / placement / fence 同步？还是要做 full TTM（含 swapout）？
2. **与现有 buddy allocator 的关系**：TTM 是替代还是包装？buddy 是 TTM 的底层 page pool 吗？
3. **跨设备 placement 策略**：UsrLinuxEmu 当前只有单设备场景，TTM 的多域（VRAM/GTT/CPU）placement 优先级如何？
4. **fence 同步**：与现有的 `gpu_wait_fence_args` 路径如何整合？
5. **测试覆盖**：能否复用 adr-019 中的 GEM 测试场景？

## 当前状态

| 组件 | 状态 |
|------|------|
| `include/linux_compat/drm_*.h` | ✅ Stubs 已实现 |
| TTM BO allocator | ❌ 未实现 |
| TTM fence 同步 | ❌ 未实现 |
| TTM swapout / page replacement | ❌ 不需要（UsrLinuxEmu 无 swap） |
| 与 buddy allocator 的桥接 | ❌ 未设计 |

## 后续

详细设计待 Phase 3+ 启动后由对应 owner 填充。

## 相关文档

- `docs/00_adr/adr-019-drm-gem-ttm-alignment.md`（TTM 对齐路径）
- `docs/02_architecture/post-refactor-architecture.md` §1.4（数据模型）
- `libgpu_core/include/gpu_buddy.h`（当前内存分配器）
