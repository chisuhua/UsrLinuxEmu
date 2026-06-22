# ADR-032: H-2.5 IGpuDriver 抽象层 (IGpuDriver Abstraction Layer)

**状态**: ✅ 已接受 (Accepted)
**日期**: 2026-06-23
**提案人**: TaskRunner owner 协同 (Sisyphus session)
**评审者**: UsrLinuxEmu Architecture Team + TaskRunner owner
**关联 ADR**: ADR-015 (IOCTL Unification), ADR-024 (User Mode Queue), ADR-017 (GPFIFO/Queue Abstraction)
**关联 Change**: `openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/` (D6-D11 决策来源)
**关联 Source**: `openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/design.md` §D6-D11

---

## Context

H-2.5 是 H-3 (Phase 2 VA Space/Queue lifecycle) 的前置依赖。H-2 实施时，`GpuDriverClient` 类是 dead code（无 IGpuDriver 抽象，所有调用方直接 new `CudaStub`），导致：

1. **测试不可注入 mock**：无法在单测中替换 `CudaStub` 为 `MockGpuDriver`，测试与真驱动行为耦合
2. **接口契约不清晰**：`GpuDriverClient` 是"调用 GPU 的具体类"而非"GPU 驱动契约"，跨仓治理难以演进
3. **CLI dead code**：`cmd_cuda.cpp` 调用 `g_gpu_client->...`，但 `g_gpu_client` 在某些路径下是 null，导致 "dead call" 路径

Path D (重构优先) 决策触发 H-2.5：用 IGpuDriver 抽象接口把"GPU 驱动契约"显式化。

---

## Decision

引入 `IGpuDriver` 抽象接口作为 GPU 驱动契约的 SSOT（Single Source of Truth）。

### 核心架构

```
┌────────────────────────────────────────────────────────────────┐
│ IGpuDriver (28 pure virtual methods, 311 lines)                  │
│                                                                  │
│ 核心 (3)             │ open() / close() / is_open()              │
│ FD 访问 (1)          │ fd()                                     │
│ 设备信息 (8)         │ get_device_info / get_warp_size / ...    │
│ Buffer Object (4)    │ alloc_bo / alloc_bo_vram / free_bo / map_bo│
│ 提交 (3)             │ submit_batch / submit_memcpy / submit_launch│
│ Fence (2)            │ wait_fence (2 overloads)                  │
│ VA Space 透传 (2)    │ set_current_va_space / get_current_va_space│
│ Phase 2 占位 (5)     │ create_va_space / destroy_va_space / ...    │
└────────────────────────────────────────────────────────────────┘
            ▲                        ▲                     ▲
            │ DI 注入                │ DI 注入             │ 测试夹具
┌───────────┴──────────┐ ┌──────────┴──────────┐ ┌────────┴────────┐
│ GpuDriverClient      │ │ CudaStub           │ │ MockGpuDriver  │
│ (真实 ioctl 实现)     │ │ (in-memory mock)   │ │ (headless 测试) │
│ 通过 /dev/gpgpu0       │ │ monotonic handle    │ │ history()       │
│ 5 Phase 2 ioctl      │ │ atomic + map 跟踪   │ │ inject_error()  │
└──────────────────────┘ └─────────────────────┘ └─────────────────┘
```

### 6 项关键决策（D6-D11）

#### D6 — 接口签名对齐（Method Signature Alignment）

**决策**：`IGpuDriver` 28 个方法签名与 `GpuDriverClient` 既有公开 API 对齐。

**理由**：
- 避免 ABI 破坏（TaskRunner 侧调用方零修改）
- 允许 `GpuDriverClient` 直接 `override` IGpuDriver 方法
- spec.md R1-R8 测试场景可同时验证两种实现

**后果**：
- ✅ `GpuDriverClient` 28 个方法逐一 `override` IGpuDriver 虚方法
- ✅ `CudaStub` 28 个方法逐一 `override`（其中 H-3 5 个方法为 in-memory mock）
- ✅ `MockGpuDriver` 28 个方法逐一 `override`（全部为 headless recording）

#### D7 — `GpuDriverClient` 真实实现

**决策**：`GpuDriverClient` 真实实现 28 个方法，通过 `/dev/gpgpu0` ioctl 与 UsrLinuxEmu GPU 插件通信。

**理由**：
- 提供生产路径（CLI / 真 GPU 联调）
- 保持向后兼容（TaskRunner 既有用法不变）
- 5 Phase 2 ioctl wrapper (H-3) 在此基础上实现

**实现位置**: `include/gpu_driver_client.h` (327 lines) + `src/gpu_driver_client.cpp`

#### D8 — `CudaStub` Mock 实现 + 命名空间迁移

**决策**：`CudaStub` 实现 28 个方法（mock 语义），从 `taskrunner::CudaStub` 命名空间迁移到 `async_task::gpu::CudaStub`。

**理由**：
- 与 IGpuDriver 在同一命名空间（`async_task::gpu::`），调用方代码更统一
- 旧 `taskrunner::` 通过 `using alias` 兼容 1 release（详见 ADR-032 §Migration）

**实现位置**: `include/cuda_stub.hpp` (238 lines) + `src/cuda_stub.cpp` (414 lines)

#### D9 — 命名空间迁移（D9）

**决策**：整个 GPU 驱动抽象层统一在 `async_task::gpu::` 命名空间。

**理由**：
- 跨仓一致性：UsrLinuxEmu 侧使用 `async_task::gpu::IGpuDriver` 与 TaskRunner 侧无歧义
- 避免 C++ namespace pollution

**Migration**:
- 旧 `taskrunner::CudaStub` → `async_task::gpu::CudaStub`
- 旧 `taskrunner::CudaResult` → `async_task::gpu::CudaResult`
- 旧 `taskrunner::LaunchParams` → `async_task::gpu::LaunchParams`
- 兼容策略：`namespace taskrunner { using async_task::gpu::*; }` 1 release 过渡

#### D10 — 依赖注入（Dependency Injection）

**决策**：`CudaScheduler` 构造函数接受 `async_task::gpu::IGpuDriver*`，默认 nullptr 时自动 new `CudaStub()`。

**理由**：
- 测试可注入 `MockGpuDriver`
- 生产可注入 `GpuDriverClient`（真驱动）或 `CudaStub`（mock）
- `nullptr` 默认行为保持向后兼容

**API**:
```cpp
class CudaScheduler {
public:
    explicit CudaScheduler(async_task::gpu::IGpuDriver* driver = nullptr);
    // ...
    async_task::gpu::IGpuDriver* driver() const { return driver_; }
private:
    async_task::gpu::IGpuDriver* driver_;  // may own CudaStub if nullptr
};
```

#### D11 — CLI 死调用修复（Dead Call Fix）

**决策**：`cli_main.cpp` 必须显式调用 `init_gpu_client()` 初始化 `g_gpu_client`，否则 CLI subcommand 走 stub mode fallback。

**理由**：
- `cmd_cuda.cpp` 检查 `g_gpu_client == nullptr || !g_gpu_client->is_open()` 时走 stub mode
- 若 `init_gpu_client()` 未被调用，所有 CLI 命令均 silent fallback，无报错
- 修复：在 `cli_main.cpp` 入口处显式调用 `init_gpu_client()`

**实施**：H-2.5 commit `1684fa1` (D6-D11 整体)

---

## Consequences

### Positive

- ✅ **测试隔离**：单测可注入 `MockGpuDriver`，测试与真驱动解耦
- ✅ **生产路径**：CLI 可注入 `GpuDriverClient` 走真 ioctl
- ✅ **跨仓治理**：IGpuDriver 接口是 TaskRunner ↔ UsrLinuxEmu 的契约 SSOT
- ✅ **H-3 基础**：Phase 2 ioctl wrapper (create_va_space 等) 可在统一接口下实现

### Negative

- ⚠️ **学习曲线**：新贡献者需理解 3 个实现（GpuDriverClient / CudaStub / MockGpuDriver）的语义差异
- ⚠️ **CudaStub 命名空间迁移**：需 1 release 兼容期，旧 `taskrunner::CudaStub` 调用方需逐步迁移
- ⚠️ **抽象泄漏**：IGpuDriver 28 个方法中部分（如 `fd()`）仅 `GpuDriverClient` 有意义，其他实现返回 stub 值

### Mitigation

- 📚 docs-audit pre-commit hook 36/36 包含 IGpuDriver 抽象检查
- 📚 `post-refactor-architecture.md` §1.3 文档化 3 个实现的关系（待 H-4 Phase 4 更新）
- 📚 H-2.5 README 提供 3 实现的快速对照表

---

## Migration

### 已完成（H-2.5 archived）

- ✅ `IGpuDriver` 接口定义（`include/igpu_driver.hpp`）
- ✅ `GpuDriverClient` 28 方法 `override`（commit `4834d5a`）
- ✅ `CudaStub` 28 方法 `override` + 命名空间迁移（commit `1684fa1`）
- ✅ `MockGpuDriver` 28 方法 `override`（`tests/mock_gpu_driver.hpp`）
- ✅ `CudaScheduler` 构造函数接受 IGpuDriver*（DI）
- ✅ CLI `init_gpu_client()` 显式调用（修复 dead call）

### 兼容性

- 旧 `taskrunner::CudaStub` 通过 `using alias` 兼容 1 release
- IGpuDriver 接口向前兼容（H-3 5 个方法已在接口定义中预留）

### 跨引用

- H-3 (`h3-phase2-management`) 依赖本 ADR — 在 H-3 的 5 Phase 2 ioctl wrapper 实现中使用 IGpuDriver 接口
- ADR-033（H-3 Phase 2 Lifecycle）记录 H-3 的 D1-D5 决策
- ADR-035（Governance Policy）记录 ADR 编号规则

---

## 验证

- ✅ `tests/test_gpu_architecture.cpp` 11 cases 验证 3 实现的 DI 路径
- ✅ docs-audit 36/36 PASS（IGpuDriver 抽象结构 + 命名空间一致性）
- ✅ 跨仓 submodule 同步 OK（TaskRunner HEAD 包含 H-2.5 commits）

---

**最后更新**: 2026-06-23（H-4 governance cleanup 阶段从 `openspec/changes/archive/2026-06-19-h2-5-architecture-foundation/design.md` 提炼为 ADR）