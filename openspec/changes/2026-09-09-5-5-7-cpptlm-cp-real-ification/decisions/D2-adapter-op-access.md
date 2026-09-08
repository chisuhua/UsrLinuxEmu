# D.2: Adapter Op 接入点决策

> **状态**: 🔄 Proposed（5.5.7 P5.NEW-B.1）
> **决策日期**: 2026-09-09
> **影响范围**: 5.5.8 TaskRunner 集成 + drv/ 边界契约
> **关联**: [proposal.md](../proposal.md) §Why / [design.md](../design.md) §3.4

---

## §1 问题陈述

### 现状

5.5.6 P4.NEW-D ship 后，HAL struct `gpu_hal_ops` 有 71 个 fn-ptr，其中：

- **adapter_get_info**（`ule_dgpu_get_adapter_info` 实现）
- **adapter_open**（`ule_dgpu_acquire` 实现）
- **adapter_close**（`ule_dgpu_release` 实现）

3 个 adapter fn-ptr 由 `hal_cpptlm_init` 注入（仅在 `ULE_HAL_BACKEND=cpptlm` 启动时），但 **drv/ 零调用者**（Oracle P4.NEW-D 建议 3 显式登记）。

具体证据：
```bash
$ grep -r "adapter_get_info\|adapter_open\|adapter_close" plugins/gpu_driver/drv/
# 零匹配
```

### 影响

- 3 adapter op 是孤儿 API（接口已契约化、测试已锁定行为，但无生产调用者）
- 5.5.8 kernel dispatch 启动时需决定如何消费 3 op
- TaskRunner 集成时需决定调用路径

---

## §2 决策选项

### P. TaskRunner 直调 `gpu_hal_ops.adapter_*`

**含义**：TaskRunner（`external/TaskRunner/`）通过符号链接访问 UsrLinuxEmu HAL 契约层，直接调 `hal->adapter_get_info(hal, &info)` 等。

**优点**：
- HAL 契约层直调，符合 Linux 驱动 idiom（HAL 是驱动 ↔ 硬件抽象）
- 零 drv/ 改动（保持 5.5.6 G1-G4 边界契约）
- 5.5.7 无 drv/ 改动（5.5.7 是 verify-only）
- TaskRunner 已是 UsrLinuxEmu 外部子模块，符号链接已就位

**缺点**：
- TaskRunner 需要 include `gpu_hal.h` 头文件（跨子模块）
- 3 op 的接口稳定性需 ADR-023 append-only 保护

**5.5.8 影响**：TaskRunner 集成 P5.NEW-X.1 实现 3 op 直调。

### Q. GpgpuDevice ioctl 派发表分发

**含义**：在 `plugins/gpu_driver/drv/gpgpu_device.cpp` 的 ioctl 派发表中加 `GPU_IOCTL_GET_DEVICE_INFO` 等 3 op 的调用，通过 `dev->hal->adapter_get_info()` 转发。

**优点**：
- 保持 drv/ 为单一切入点（用户态应用通过 ioctl 进入）
- 符合 UsrLinuxEmu 现有 ioctl 派发模式

**缺点**：
- drv/ 改动，破坏 5.5.6 G1-G4 边界契约（hal 注入是 drv/ 唯一调用方）
- 5.5.7 是 verify-only，Q 选项需新增 change 立项
- 3 op 语义不匹配 ioctl 语义（adapter_open 是 handle-style，ioctl 是 fd-style）

**5.5.8 影响**：需新增 5.5.7.1 sub-change 立项 drv/ 改动。

### R. 双轨：TaskRunner 默认 P，GpgpuDevice 可选 Q

**含义**：TaskRunner 走 P（HAL 直调），GpgpuDevice 保留 Q（ioctl 派发）作为可选。

**优点**：
- 灵活（TaskRunner 直调高效，GpgpuDevice ioctl 标准）
- 兼容现有 ioctl 用户

**缺点**：
- 复杂度高（同一 op 两条路径）
- 行为对齐风险（两条路径返回值可能不一致）
- 测试需双倍覆盖

**5.5.8 影响**：需 P + Q 双实现，测试覆盖双路径。

---

## §3 推荐决策

**选项 P**（TaskRunner 直调 `gpu_hal_ops.adapter_*`）。

**理由**：
1. 5.5.7 是 verify-only，零 drv/ 改动（Q/R 需新增 change）
2. HAL 契约层直调是 Linux 驱动 idiom 标准（HAL 是驱动 ↔ 硬件抽象边界）
3. TaskRunner 已是 UsrLinuxEmu 外部子模块，符号链接已就位（ADR-091 4 象限）
4. 5.5.6 P4.NEW-D Oracle 9.4/10 PASS 已确立 G1-G4 边界契约，Q 破坏此契约
5. adapter op 语义（handle-style）不匹配 ioctl 语义（fd-style），Q 引入语义错位

**反馈到 5.5.7**：
- 零代码改动（5.5.7 是 verify-only）
- D.2 决策记录在 spec.md `D.2 Adapter Op Access Point Decision` Requirement

**反馈到 5.5.8 启动条件**：
- 5.5.8 P5.NEW-X.1 第一步：TaskRunner 集成 3 op 直调（`hal->adapter_get_info` / `hal->adapter_open` / `hal->adapter_close`）
- 5.5.8 立项 tasks.md 必须明确"TaskRunner 直调是 3 op 唯一生产路径"
- 5.5.8 立项 proposal.md 必须明确"drv/ 零改动，G1-G4 边界契约保持"

---

## §4 决策记录

| 字段 | 值 |
|------|-----|
| **决策 ID** | D.2 |
| **主题** | adapter op 接入点 |
| **推荐选项** | P |
| **影响范围** | 5.5.8 TaskRunner 集成 + drv/ 边界契约 |
| **决策状态** | 🔄 Proposed（待 Oracle 确认） |
| **决策人** | Sisyphus (主对话 agent) |
| **决策日期** | 2026-09-09 |

---

## §5 跨引用

- [proposal.md](../proposal.md) — Why/What/Capabilities/Impact
- [design.md §3.4](../design.md) — adapter op 接入点决策点
- [D.1: -ETIMEDOUT 语义](D1-etimedout-semantics.md)
- [D.3: ctest WORKING_DIRECTORY](D3-ctest-cwd.md)
- [5.5.6 P4.NEW-D commit `a912f4b`](../../../../plugins/gpu_driver/hal/hal_cpptlm.cpp) — adapter op 真化
- [5.5.6 P4.NEW-D followup commit `c63a9f3`](../../../../plugins/gpu_driver/hal/hal_select.cpp) — backend 选择
- [ADR-023](../../../../00_adr/adr-023-hal-interface.md) — HAL append-only
- [ADR-091](../../../../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) — 4 象限
