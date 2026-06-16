# ADR-022: GPU 计算单元仿真 (GPU Compute Unit Emulation)

**状态**: 🔄 提议中 (Proposed) — 占位骨架

**日期**: 2026-06-16

**提案人**: UsrLinuxEmu Architecture Team

**评审者**: 待定

**关联 ADR**: ADR-005 (Ring Buffer), ADR-015 (IOCTL Unification), ADR-017 (GPFIFO/Queue), ADR-018 (驱动/仿真分离), ADR-020 (libgpu_core 提取), ADR-021 (Hardware Puller), ADR-023 (HAL 接口契约), ADR-024 (用户态队列提交)

**修订记录**:
- 2026-06-16 v0: 占位骨架（来自 ADR 编号治理清理；详见 `docs/02_architecture/post-refactor-architecture.md` §3.3）

## 背景

UsrLinuxEmu 的 GPU 仿真栈目前已覆盖：

- **驱动层**：`plugins/gpu_driver/drv/gpgpu_device.cpp`（ioctl 派发表）
- **HAL 层**：`plugins/gpu_driver/hal/gpu_hal.h`（11 个 fn-ptr 的 `struct gpu_hal_ops`）
- **调度仿真**：`plugins/gpu_driver/sim/scheduler/GlobalScheduler`
- **拉取仿真**：`plugins/gpu_driver/sim/hardware/HardwarePullerEmu`（FSM: IDLE→FETCH→DECODE→…）
- **内存子分配**：`libgpu_core/gpu_buddy.h`（纯 C buddy allocator）

但 **GPU 计算单元本身**（wave/warp dispatch、register file、SIMD lane、ALU 流水）的仿真**尚未实现**。当前 `HardwarePullerEmu` 只走到「解析」+「转发到 GlobalScheduler」，没有真正执行 shader 指令。

这一缺口在以下场景变得明显：

- TaskRunner 端到端验证需要 kernel 真的「执行」一段 GPGPU shader（哪怕最简单的 add/mul 也要有结果）
- 性能仿真需要参考真实 GPU 的 warp 调度延迟模型
- 调试 / 教学场景需要「单步执行」观察寄存器变化

## 决策

待定。本 ADR 当前为占位骨架，Phase 3+ 详细设计时需要回答：

1. **仿真粒度**：指令级（每条 SASS/GCN ISA 解释执行）vs 算子级（predefined kernel template）vs 块级（性能模型，不做实际计算）
2. **指令集覆盖**：先支持 RISC-V vector extension（最简单）vs 真实 AMD GCN / NVIDIA SASS 子集
3. **寄存器文件大小 / warp 数 / SIMD width** 与目标硬件对齐
4. **性能模型**：是否需要周期精确 (cycle-accurate) 还是统计准确 (statistical)
5. **调试接口**：与 gdb/lldb 的集成方式

## 当前状态

| 组件 | 状态 |
|------|------|
| `plugins/gpu_driver/sim/scheduler/` | ✅ 调度仿真（GlobalScheduler）已实现 |
| `plugins/gpu_driver/sim/hardware/` | ✅ 拉取仿真（HardwarePullerEmu）已实现 |
| `plugins/gpu_driver/sim/compute/` | ❌ 计算单元仿真 — **未启动** |
| Shader 解释器 | ❌ 未实现 |
| 寄存器文件 / Warp 状态 | ❌ 未实现 |
| 性能模型 | ❌ 未实现 |

## 后续

详细设计在 Phase 3+ 阶段展开。本 ADR 文件存在仅为了：

1. 填补 docs-audit §3.1 中"ADR-022 missing (intentional placeholder)"的编号 gap
2. 为未来详细 ADR 提供锚点（owner 在填充时应更新本文档，并将 status 改为 ✅ 已接受）
3. 让 `grep "ADR-022"` 等工具能找到具体位置

## 相关文档

- `docs/00_adr/adr-018-driver-sim-separation.md` §3（明确引用 "见 ADR-022"）
- `docs/00_adr/adr-019-drm-gem-ttm-alignment.md` §6（TTM 迁移优先级）
- `docs/00_adr/adr-021-hardware-puller.md`（compute unit 是 puller 的下游消费者）
- `docs/02_architecture/post-refactor-architecture.md` §1.6（Phase 3+ 规划）
