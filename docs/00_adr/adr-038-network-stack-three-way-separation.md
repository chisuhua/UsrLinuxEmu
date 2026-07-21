# ADR-038: 网络栈 3 区分架构边界

**状态**: ✅ 已接受 (Stage 2 已交付，前置条件全部满足)
**日期**: 2026-07-05
**提案人**: UsrLinuxEmu Architecture Team
**关联 ADR**: ADR-036 (3-way separation), ADR-018 (driver-sim separation), ADR-023 (HAL interface), ADR-027 (linux compat strategy), ADR-035 (governance)
**关联 Spec**: [Stage 2 design spec](../specs/2026-07-05-stage-2-multi-device-design.md) §3 (修订项 #3)
**关联 Plan**: [Stage 2 implementation plan](../plans/2026-07-05-stage-2-multi-device.md) Phase C

---

## Context

Stage 2 引入网络设备插件（net_driver）。GPU IOCTL 路径下 ADR-036 三区分原则工作良好，但网络栈深度远超 GPU：

```
GPU IOCTL:   user → ioctl → drv → hw              （薄层）
网络栈:      user → socket → sock → sk_buff → net_device → driver → NIC
```

三区分在网络场景下需要回答三个架构问题：

1. **`struct sk_buff` ② 归属**：② portable driver 是否包含 sk_buff 操作？
   - 若 ② **仅** 含 `net_device_ops` → ① 必须模拟 socket/sock/sk_buff（大量代码）
   - 若 ② **含** sk_buff 操作 → ② 与真实 Linux kernel sk_buff 强耦合，移植性受损

2. **socket() 模拟范围**：① kernel env sim 模拟到 socket 哪一层？
   - 模拟整个 BSD socket API（包括 TCP/IP）→ 范围爆炸
   - 仅模拟 syscall 兼容层 → ② 需要自带协议栈

3. **HAL 桥接适配**：现有 `struct gpu_hal_ops`（11 个函数指针）是否扩展？
   - 扩展 gpu_hal_ops → 混合 GPU + 网络抽象，违反单一职责
   - 新建 `struct net_hal_ops` → 增加 HAL 层数，但语义清晰

---

## Decision

### D1: sk_buff ① opaque token 模型

① 把 `struct sk_buff` 模拟为 **opaque token**（不透明句柄），暴露以下 ops：

```cpp
// include/linux_compat/net/sk_buff.h (新增, Stage 2.2.1)
struct sk_buff;  // opaque

struct sk_buff_ops {
  struct sk_buff* (*alloc)(size_t data_len);
  void (*free)(struct sk_buff*);
  void* (*data)(const struct sk_buff*);       // 返回内部 data 指针
  size_t (*len)(const struct sk_buff*);
  void (*put)(struct sk_buff*);                // 引用计数减
  struct sk_buff* (*get)(struct sk_buff*);     // 引用计数增
};

const struct sk_buff_ops* sk_buff_ops_get(void);  // ① 提供的单例
```

② net_driver 通过 `sk_buff_ops_get()` 获取 ops 指针，调用 `alloc/free/data/len/put/get` 操作 buffer。

**结果**：② portable driver 调用 `ops->alloc(...)` 而非直接 `kmalloc(sizeof(struct sk_buff))`，保留可移植性。

### D2: socket 模拟边界（限定到 syscall 兼容层）

① 模拟范围：
- ✅ `socket(domain, type, protocol)` → 返回 fd
- ✅ `bind(fd, addr, addrlen)` → VFS 注册
- ✅ `sendto(fd, buf, len, flags, addr, addrlen)` → 调用 ② net_device_ops->ndo_start_xmit
- ✅ `recvfrom(fd, buf, len, flags, addr, addrlen)` → 等待 ③ NIC sim 数据
- ✅ `close(fd)` → 清理

① **不** 模拟：
- ❌ TCP/IP 协议栈（`tcp_sendmsg`, `ip_route_output`, ...）
- ❌ socket filter（`BPF`, `SO_ATTACH_FILTER`）
- ❌ 复杂 socket options（`SO_TIMESTAMP`, `SO_LINGER`, ...）

**结果**：网络栈是 "L2 直通"（user → L2 frame → NIC sim），不含 L3/L4 协议处理。② net_driver 需自带 TCP/IP（如需）；这与项目 "驱动开发框架" 定位一致（驱动只需关心 L2 与硬件交互）。

### D3: HAL 不扩展（新建 `net_hal_ops` 如需）

不扩展现有 `struct gpu_hal_ops`（11 个函数指针），原因：

1. **单一职责**：GPU HAL 与网络 HAL 语义不同（ioctl dispatch vs sk_buff ops），合并会污染抽象
2. **隔离变更**：Stage 2 网络栈演进不应影响 GPU driver code path
3. **可替换性**：GPU HAL 与网络 HAL 可独立替换（如 GPU 真机 mode 与 net loopback sim mode）

如 Stage 2.2 需要 HAL-like 抽象，新建独立 `struct net_hal_ops`：

```c
// plugins/net_driver/hal/net_hal.h (Stage 2.2.2)
struct net_hal_ops {
  int  (*xmit)(struct net_device* dev, struct sk_buff* skb);
  int  (*set_mac)(struct net_device* dev, const u8* mac);
  // ... 扩展点
};
```

但 Stage 2 范围内**不预先添加** net_hal_ops；仅当 2.2.4 测试表明需要时才引入（避免 HAL 膨胀）。

---

## Consequences

### 正面

- ② 保持与 Linux kernel idioms 兼容（`net_device_ops->ndo_start_xmit(skb)` 签名不变）
- ① 范围限定（opaque sk_buff + 6 ops + 4 syscalls）→ 实现可控
- HAL 不污染 → GPU 路径零影响
- ② 可独立编译进 drivers/net/xxx/（验证 3 区分原则可移植性）

### 负面 / 风险

- **sk_buff ops 通过函数指针调用** → 性能开销（vs 内联）
  - 缓解：ops_get 返回 `const static`，编译器可能内联
- **socket() 仅 L2 直通** → ② 如需 TCP/IP 必须自带协议栈（不与项目目标冲突）
- **net_hal_ops 暂不引入** → 2.2.4 测试若发现需要重新设计 → 延迟发布
- **① sk_buff 状态 vs 真实 kernel sk_buff 不完全兼容** → ② 移植时需小幅调整

### 兼容性

- ② net_driver/drv 代码可直接拷贝到 `drivers/net/xxx/net_compat.c`，编译时仅替换 `sk_buff_ops_get()` 为 `<linux/skbuff.h>` 原生 API（proof-of-concept 在 Stage 2.2.5）
- ADR-036 的依赖规则保持不变（① 仍为驱动宿主；HAL 仍为桥；HAL 不混合 GPU+net）

---

## Compliance

本 ADR 是 Stage 2.2 网络设备插件（`plugins/net_driver/`）的 **强制前置**：
- 2.2.1 (`src/kernel/net/ socket + sk_buff 兼容层`) 必须按 D1/D2 实现
- 2.2.2 (`plugins/net_driver/drv/`) 必须按 D1 调用 `sk_buff_ops_get()` 而非直接构造 sk_buff
- 2.2.5 (3 区分回归) 验证 ② 可拷贝到 `drivers/net/xxx/` 编译

违反本 ADR 的 PR 必须附带 ADR 修订或被驳回。

---

## 替代方案与拒绝理由

| 方案 | 拒绝理由 |
|------|---------|
| ② 直接包含 `struct sk_buff` 定义 | 与 Linux kernel sk_buff 强耦合，② 不可移植 |
| ① 模拟完整 socket subsystem（含协议栈）| 实现复杂度爆炸（数万行） |
| 扩展 `struct gpu_hal_ops` 加 net 字段 | 违反单一职责，污染 GPU 抽象 |
| ② 实现 TCP/IP 协议栈 | 范围爆炸，非 Stage 2 目标 |

---

## 变更记录

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-07-05 | v1 | 初始版本：🔄 Proposed |

---

**维护者**: UsrLinuxEmu Architecture Team
**评审要求**: Stage 2 启动前必须 Review + 状态升 ✅ Accepted
**对应 SSOT**: docs/02_architecture/post-refactor-architecture.md §1.10 (待追加 §1.10.6 网络栈 3 区分)