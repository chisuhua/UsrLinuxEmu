# 阶段 2: 多设备插件化

> **状态**: ✅ 已达成 (2026-07-05)
> **目标**: 扩展插件化架构支持网络设备 + 存储设备（GPU 之外的设备类型）
> **前置依赖**: 阶段 1（Linux 内核环境模拟基础）✅
> **实施**: 5 commits (net + storage plugins) + 76/76 ctest PASS
> **详情**: [Stage 2 plan](../superpowers/plans/2026-07-05-stage-2-multi-device.md) + [Stage 2 spec](../superpowers/specs/2026-07-05-stage-2-multi-device-design.md) + [spike report](../05-advanced/stage-2-spike-report.md)

---

## 涉及层（按 3 区分）

阶段 2 的工作完全遵循 [ADR-036](../00_adr/adr-036-three-way-separation.md) 确立的 3 区分原则，把"驱动是否仅经 HAL 调硬件"作为可移植性验收基线。

| 层 | 工作 |
|----|------|
| ① Linux 内核环境模拟 | 网络 driver API（netdev, socket）+ 块设备 API（block_device, request_queue）扩展 |
| ② 可移植驱动代码 | `plugins/net_driver/`, `plugins/storage_driver/` |
| ③ 硬件模拟 | NIC emulator + 磁盘 emulator |

阶段 2 的目标是验证 3 区分原则**不仅适用于 GPU 设备**，而是能横向扩展到网络与存储设备。如果阶段 2 暴露了 3 区分模型的盲区，应回写 ADR-036 决策（不破坏该原则），而不是绕开 HAL 桥接机制。

---

## 子任务

### 子任务 2.1 — 网络设备插件

**目标**: 提供可加载的网络设备插件，能响应 socket API 调用

**② 驱动**:

- 创建 `plugins/net_driver/`，参考 `drivers/sample_serial/` 模式
- 实现 `struct net_device_ops` 子集（open, stop, start_xmit, etc.）
- 注册到 VFS（`/dev/net0`）

**① 内核环境模拟**:

- 扩展 `src/kernel/net/`，提供 socket syscall 兼容层
- 实现 `struct sk_buff` 用户态版

**③ 硬件模拟**:

- 创建 `plugins/net_driver/sim/`，模拟 NIC
- 模拟 packet buffer + interrupt on packet arrival

**验收**:

- 加载 `net_driver.so` 后 `/dev/net0` 可见
- 能响应 `sendto` / `recvfrom` syscall

### 子任务 2.2 — 存储设备插件

**目标**: 提供可加载的块设备插件，能响应 read/write syscall

**② 驱动**:

- 创建 `plugins/storage_driver/`
- 实现 `struct block_device_operations` 子集
- 注册到 VFS（`/dev/sda`）

**① 内核环境模拟**:

- 扩展 `src/kernel/block/`，提供 bio/request 兼容层

**③ 硬件模拟**:

- 创建 `plugins/storage_driver/sim/`，模拟磁盘（基于 host 文件）

**验收**:

- 加载 `storage_driver.so` 后 `/dev/sda` 可见
- 能响应 read/write syscall，数据持久化到 host 文件

---

## 涉及 ADR

| ADR | 角色 |
|-----|------|
| [ADR-003](../00_adr/adr-003-plugin-architecture.md) | 插件化架构基础，dlopen/dlsym 的扩展性保证 |
| [ADR-027](../00_adr/adr-027-linux-compat-strategy.md) | Linux 兼容层扩展策略，net_device / block_device API 增量边界 |
| [ADR-036](../00_adr/adr-036-three-way-separation.md) | 3 区分架构原则，阶段 2 工作的统一判定基准 |

---

## 风险

| 风险 | 缓解 |
|------|------|
| Socket syscall 兼容层复杂 | 参考 musl libc 实现，子集优先 |
| 块设备 bio 抽象难 | 直接基于 read/write，不实现 request queue |
| 性能开销 | 用户态模拟有性能上限，文档明示 |
| 3 区分原则在网络/存储场景可能不收敛 | 阶段 2 任一子任务暴露盲区时回写 ADR-036，不绕开 HAL 桥接 |

---

## 下一步

[阶段 3: v1.0 稳定](stage-3-v1.0.md)
