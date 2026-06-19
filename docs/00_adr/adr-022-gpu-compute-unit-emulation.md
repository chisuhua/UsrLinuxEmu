# ADR-022: GPU 计算单元仿真 (GPU Compute Unit Emulation)

**状态**: ✅ 已接受 (Accepted)

**日期**: 2026-06-16（v0 占位）→ 2026-06-17（v1 决策）

**提案人**: UsrLinuxEmu Architecture Team

**评审者**: 待定

**关联 ADR**: ADR-005 (Ring Buffer), ADR-015 (IOCTL Unification), ADR-017 (GPFIFO/Queue), ADR-018 (驱动/仿真分离), ADR-020 (libgpu_core 提取), ADR-021 (Hardware Puller), ADR-023 (HAL 接口契约), ADR-024 (用户态队列提交)

**修订记录**:
- 2026-06-16 v0: 占位骨架（来自 ADR 编号治理清理；详见 `docs/02_architecture/post-refactor-architecture.md` §3.3）
- 2026-06-17 v1: 填入 operator-level emulation 决策（change cleanup-adr-placeholders）

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

## 决策（v1）

UsrLinuxEmu Compute Unit Emulation 采用 **operator-level emulation**（算子级仿真）：通过 4 个预定义的 kernel template 实现"kernel 真的执行"语义，**不做指令级 ISA 解释**、**不做 cycle-accurate 性能模型**。

### v1 Kernel Templates（4 个，硬编码）

| Template | 语义 | 输入 | 输出 | 用途 |
|----------|------|------|------|------|
| `add_vec4` | float4 加法 | 2 个 `float4` | 1 个 `float4` | 基础算术验证 |
| `mul_vec4` | float4 乘法 | 2 个 `float4` | 1 个 `float4` | 基础算术验证 |
| `memcpy_h2d_via_pull` | host → device memcpy | src ptr + size | dst ptr | 数据通路验证，复用 pushbuffer 路径 |
| `noop` | 无操作 | — | — | 测试占位 / fence-only 提交 |

### 与 `HardwarePullerEmu` FSM 衔接

- **FETCH 阶段**：解析 kernel entry 中的 template name（4 字节 ASCII tag）
- **DECODE 阶段**：根据 template name 路由到对应 C++ 实现
- **DISPATCH 阶段**：调用模板函数，传入 BO 句柄 + 参数
- **COMPLETE 阶段**：通过 `HardwarePullerEmu::completeBatch()` 触发 fence

### 4 个 template 边界

- **v1 硬编码**：template 列表编译期固定，不暴露给用户配置
- **Phase 3+ 再讨论配置化**：若需要第 5 个 template（如 `matmul_4x4`）或 YAML/JSON 配置入口，新建 v2 ADR

### 明确不做什么

- **不做 ISA 解释**：不实现 RISC-V vector 子集、AMD GCN 子集、NVIDIA SASS 子集（工作量 4-8 周；与 `libgpu_core` 路径重叠）
- **不做 cycle-accurate 性能模型**：v1 仿真无延迟、无吞吐统计；只验证"kernel 被调用 + 结果正确"
- **不做 register file / warp 状态建模**：v1 用 BO + 参数传递代替寄存器

### v0 开放问题回答

1. **仿真粒度**：operator-level（v1 决策）。理由：与现有 `HardwarePullerEmu` FSM 衔接最自然；满足 TaskRunner "kernel 真的执行" 需求；工作量 1-2 周内可完成。
2. **指令集覆盖**：N/A for v1。template 是预定义的 C++ 函数，不涉及 ISA。
3. **寄存器文件大小 / warp 数 / SIMD width**：minimal for v1。4 个 template 都不需要 register file；v1 不建模 warp/SIMD 状态。Phase 3+ 引入第 5 个 template 时再讨论。
4. **性能模型**：N/A for v1。template 是函数调用级别，无延迟统计；后续若需 cycle-accurate 仿真，新建 ADR 评估。
5. **调试接口**：stderr per-template。每个 template 在执行时打印 `(template_name, input_size, output_size)` 到 stderr，便于 TaskRunner 验证。若需 gdb/lldb 集成，参考 ADR-030（已 deferred）。

## 当前状态

| 组件 | 状态 |
|------|------|
| `plugins/gpu_driver/sim/scheduler/` | ✅ 调度仿真（GlobalScheduler）已实现 |
| `plugins/gpu_driver/sim/hardware/` | ✅ 拉取仿真（HardwarePullerEmu）已实现 |
| `plugins/gpu_driver/sim/compute/` | ⏳ 计算单元仿真 — **v1 设计完成，Phase 3 实施** |
| Shader 解释器 | ❌ 永久不实现（v1 决策：operator-level 而非 ISA-level）|
| 寄存器文件 / Warp 状态 | ❌ 永久不实现（v1 决策：template 不用 register file）|
| 性能模型 | ❌ 永久不实现（v1 决策：不做 cycle-accurate）|
| 4 个 v1 kernel templates | ⏳ 待 Phase 3 实施 |

## 后续

- **Phase 3 实施**：在 `plugins/gpu_driver/sim/compute/` 目录下实现 4 个 template 的 C++ 代码（约 200-300 行）；扩展 `HardwarePullerEmu` 的 FETCH 阶段识别 template name tag
- **测试覆盖**：`tests/test_gpu_compute_templates_standalone.cpp` 覆盖 4 个 template（每个 ≥3 个 case：正常输入、边界输入、空输入）
- **Phase 3+ 演进**：若需要第 5 个 template 或配置化入口，新建 ADR-022 v2

## 讨论历史 (v0 占位)

> 以下内容来自 2026-06-16 v0 占位骨架，保留作为 ADR 演进的历史记录。

### v0 决策

待定。本 ADR 当前为占位骨架，Phase 3+ 详细设计时需要回答：

1. **仿真粒度**：指令级（每条 SASS/GCN ISA 解释执行）vs 算子级（predefined kernel template）vs 块级（性能模型，不做实际计算）
2. **指令集覆盖**：先支持 RISC-V vector extension（最简单）vs 真实 AMD GCN / NVIDIA SASS 子集
3. **寄存器文件大小 / warp 数 / SIMD width** 与目标硬件对齐
4. **性能模型**：是否需要周期精确 (cycle-accurate) 还是统计准确 (statistical)
5. **调试接口**：与 gdb/lldb 的集成方式

### v0 后续说明

详细设计在 Phase 3+ 阶段展开。本 ADR 文件存在仅为了：

1. 填补 docs-audit §3.1 中"ADR-022 missing (intentional placeholder)"的编号 gap
2. 为未来详细 ADR 提供锚点（owner 在填充时应更新本文档，并将 status 改为 ✅ 已接受）
3. 让 `grep "ADR-022"` 等工具能找到具体位置

> **v1 落地后**：上述 5 个开放问题已在 v1 决策中回答；详见上方"决策（v1）"与"v0 开放问题回答"段。

## 相关文档

- `docs/00_adr/adr-018-driver-sim-separation.md` §3（明确引用 "见 ADR-022"）
- `docs/00_adr/adr-019-drm-gem-ttm-alignment.md` §6（TTM 迁移优先级）
- `docs/00_adr/adr-021-hardware-puller.md`（compute unit 是 puller 的下游消费者）
- `docs/02_architecture/post-refactor-architecture.md` §1.6（Phase 3+ 规划）
- `docs/openspec/changes/cleanup-adr-placeholders/`（本变更的设计与 spec）
