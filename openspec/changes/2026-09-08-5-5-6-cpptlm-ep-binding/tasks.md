# Tasks: 5.5.6-cpptlm-ep-binding — dGPU E2E 主线 #1 真实 CppTLM EP

> **TDD 纪律**: 每个 task 顺序 = Write test → Verify fail → Implement → Verify pass → Commit
> **状态**: 🔄 Proposed v1.0（2026-09-08）
> **前置基线**: [kcpptlm-archive-audit](../2026-09-08-kcpptlm-archive-audit/)（5 项虚假完成识别）
> **工期**: 4-6 周（4 个 P4.NEW 子任务 + 5 个新测试）
> **关联**: [proposal.md](proposal.md) + [design.md](design.md) + [specs/cpptlm-real-backend/spec.md](specs/cpptlm-real-backend/spec.md)

---

## §1 任务总览（Wave 化 + TDD 5 步）

| Wave | 子任务 | 工期 | 依赖 |
|------|--------|:---:|------|
| **P4.NEW-A** | `backdoor_endpoint.cpp` 真实实现 | 1-2 周 | 无（与 A/B/C/D 可并行）|
| **P4.NEW-B** | `bridge.cpp` kCpptlm dlopen 22 ABI | 1-2 周 | A（依赖 ule_dgpu_* 实现）|
| **P4.NEW-C** | `host_bridge.cpp` bypass/full 分发 | 0.5-1 周 | B（依赖 bridge 新增 backdoor_read/write）|
| **P4.NEW-D** | `hal_cpptlm.cpp` 真化 + `plugin.cpp` backend | 1 周 | A + B + C（全部依赖）|
| **测试** | 5 个新测试 binary | 1 周 | A + B + C + D 全部 |
| **总计** | | **4.5-7 周**（含缓冲） |

---

## §2 P4.NEW-A：backdoor_endpoint.cpp 真实实现（1-2 周）

### 任务 A.1：建立 5 函数骨架（Write test → Verify fail）

- [ ] **Write test**: `tests/sim_hardware/test_backdoor_endpoint_real_standalone.cpp`
  - `TEST_CASE("backdoor_endpoint: acquire returns valid handle on real cpptlm", "[cpptlm][backdoor][.]")`
  - `TEST_CASE("backdoor_endpoint: get_adapter_info populates 9 fields", "[cpptlm][backdoor][.]")`
  - `TEST_CASE("backdoor_endpoint: read/write kBarMmio roundtrip", "[cpptlm][backdoor][.]")`
  - `TEST_CASE("backdoor_endpoint: read/write kBarVram roundtrip", "[cpptlm][backdoor][.]")`
  - `TEST_CASE("backdoor_endpoint: read/write kConfig roundtrip", "[cpptlm][backdoor][.]")`
  - `TEST_CASE("backdoor_endpoint: release invalidates handle", "[cpptlm][backdoor][.]")`
  - `TEST_CASE("backdoor_endpoint: kAxiDirect returns -EOPNOTSUPP", "[cpptlm][backdoor][.]")`
  - `TEST_CASE("backdoor_endpoint: 100ms timeout on acquire", "[cpptlm][backdoor][.]")`（用 sleep 模拟）
- [ ] **Verify fail**: 测试失败（`backdoor_endpoint.cpp` 只有 stub，5 weak symbol 全 `-ENOSYS`）
- [ ] **Implement 骨架**: 创建 `sim_hardware/src/cpptlm/backdoor_endpoint.cpp`，5 函数空实现（返回 `-ENOSYS` 或非法）
- [ ] **Verify pass**: 基础测试通过（mock 路径已存在，仅 stub 行为验证）

### 任务 A.2：`ule_dgpu_acquire` + `ule_dgpu_release` 真实实现

- [ ] **Write test**: `TEST_CASE("backdoor_endpoint: acquire-open-release lifecycle", "[cpptlm][backdoor][.]")`
- [ ] **Verify fail**: 当前 stub 返回 -ENOSYS
- [ ] **Implement**:
  - `ule_dgpu_acquire(dev_id, out_handle)`：
    - 调 `cpptlm_emulator_create()` 获取 `cpptlm_emulator_t*`
    - 调 `cpptlm_emulator_open(dev_id, &handle)` 获取 handle
    - 写 `*out_handle`，返回 0
  - `ule_dgpu_release(handle)`：
    - 调 `cpptlm_emulator_close(handle)` 释放
    - 调 `cpptlm_emulator_destroy(emu)` 销毁仿真器
- [ ] **Verify pass**: 测试通过

### 任务 A.3：`ule_dgpu_get_adapter_info` 字段映射

- [ ] **Write test**: `TEST_CASE("backdoor_endpoint: get_adapter_info field mapping", "[cpptlm][backdoor][.]")`
- [ ] **Verify fail**: stub 返回 -ENOSYS
- [ ] **Implement**:
  - 调 `cpptlm_emulator_get_adapter_info(handle, cpptlm_info)`
  - 字段映射：`cpptlm_device_info` → `ule_dgpu_adapter_info`（9 字段）
- [ ] **Verify pass**: 9 字段全部正确填充

### 任务 A.4：`ule_dgpu_read/write` 空间分发（kConfig/kBarMmio/kBarVram）

- [ ] **Write test**: `TEST_CASE("backdoor_endpoint: read/write space dispatch", "[cpptlm][backdoor][.]")`
- [ ] **Verify fail**: stub 返回 -ENOSYS
- [ ] **Implement**:
  - `kConfig (0)` → `cpptlm_emulator_pcie_config_read/write`
  - `kBarMmio (1)` → `cpptlm_emulator_mmio_read/write`
  - `kBarVram (2)` → `cpptlm_emulator_backdoor_read/write`
  - `kAxiDirect (3)` → `-EOPNOTSUPP`
- [ ] **Verify pass**: 4 个空间类型路由正确

### 任务 A.5：100ms 超时与 PendingReq/future 跨线程

- [ ] **Write test**: `TEST_CASE("backdoor_endpoint: 100ms timeout on slow acquire", "[cpptlm][backdoor][.]")`
- [ ] **Verify fail**: 当前同步实现无超时
- [ ] **Implement**:
  - PendingReq 结构：`std::promise<int> + std::future<int> + deadline`
  - 异步提交到 CppTLM sim thread queue
  - `std::future::wait_for(100ms)` 超时返回 -ETIMEDOUT
  - 强制 cleanup：`cpptlm_emulator_close()` + 标记 handle invalid
- [ ] **Verify pass**: 测试用 `sleep_for(50ms)` 模拟成功路径 + `sleep_for(150ms)` 模拟超时路径

### 任务 A.6：删除 stub 文件

- [ ] **删除** `sim_hardware/src/cpptlm/backdoor_endpoint_stub.cpp`（被 backdoor_endpoint.cpp 替代）
- [ ] **更新** `sim_hardware/CMakeLists.txt`：移除 stub 引用
- [ ] **更新** `tests/CMakeLists.txt`：移除 4.1 测试 `test_backdoor_endpoint_standalone.cpp`（已 ship 但测 stub）→ 保留并改为 mock fallback 测试（`ULE_CPPTLM_SKIP` env）

**任务 A 验收标准**：所有 P4.NEW-A 测试 PASS + 164/164 既有 ctest 零回归

---

## §3 P4.NEW-B：bridge.cpp kCpptlm dlopen 22 ABI（1-2 周）

> **前置**: P4.NEW-A 完成（依赖 ule_dgpu_* 接口定义）

### 任务 B.1：dlopen libcpptlm_emulator.so 真实加载

- [ ] **Write test**: `tests/sim_hardware/test_bridge_kcpptlm_dlopen_standalone.cpp`
  - `TEST_CASE("bridge: dlopen libcpptlm_emulator.so succeeds when sibling build present", "[cpptlm][bridge][.]")`
  - `TEST_CASE("bridge: dlopen fails gracefully + fallback to mock", "[cpptlm][bridge][.]")`
  - `TEST_CASE("bridge: 22 ABI symbols resolved via dlsym", "[cpptlm][bridge][.]")`
- [ ] **Verify fail**: `bridge.cpp:65-67` 当前 `return -ENOSYS`
- [ ] **Implement**:
  - `bridge.cpp:65` 替换为：
    - `void* handle = dlopen("libcpptlm_emulator.so", RTLD_NOW | RTLD_GLOBAL)`
    - 失败 → 试 `../CppTLM/build/lib/libcpptlm_emulator.so`（开发期回退）
    - 失败 → WARN 日志 + `backend = kMock` + 返回 `-ENOSYS`
    - 成功 → 保存到 `CpptlmBridge::Impl::cpptlm_handle_`
- [ ] **Verify pass**: 测试通过

### 任务 B.2：dlsym 22 ABI 绑定表

- [ ] **Write test**: `TEST_CASE("bridge: dlsym all 22 ABI + verify signatures", "[cpptlm][bridge][.]")`
- [ ] **Verify fail**: 当前 dlsym 未实施
- [ ] **Implement**:
  - 定义 `struct CpptlmSymbols`（22 字段，per design §2.2）
  - 循环 dlsym，每个 ABI 失败 → WARN 日志 + 整个 binding 失败 + 回退 mock
  - 全部成功 → 写入 `CpptlmBridge::Impl::cpptlm_syms`
- [ ] **Verify pass**: 22 ABI 全部解析成功

### 任务 B.3：bridge.cpp 新增 backdoor_read/write 方法

- [ ] **Write test**: `TEST_CASE("bridge: backdoor_read/write delegates to cpptlm ABI", "[cpptlm][bridge][.]")`
- [ ] **Verify fail**: 当前 bridge 类无 backdoor_read/write 方法
- [ ] **Implement**:
  - `CpptlmBridge::backdoor_read(bar, offset, dst, len)`：调 `cpptlm_syms.backdoor_read`
  - `CpptlmBridge::backdoor_write(bar, offset, src, len)`：调 `cpptlm_syms.backdoor_write`
  - 更新 `bridge.h` 公开接口
- [ ] **Verify pass**: 测试通过

### 任务 B.4：bridge.cpp:65-67 kCpptlm 分支真实调用

- [ ] **Write test**: `TEST_CASE("bridge: kCpptlm backend creates real emulator", "[cpptlm][bridge][.]")`
- [ ] **Verify fail**: 当前 `return -ENOSYS`
- [ ] **Implement**:
  - `bridge.cpp:65-67` 替换：
    - `cpptlm_syms.create()` 创建 emulator 实例
    - 保存到 `Impl::cpptlm_emulator_`
    - 设 `backend = kCpptlm` + `initialized = true`
- [ ] **Verify pass**: bridge init with kCpptlm 成功

**任务 B 验收标准**：22 ABI 全部 dlopen + dlsym 成功 + bridge 切到 kCpptlm backend + 4 个新测试 PASS

---

## §4 P4.NEW-C：host_bridge.cpp bypass/full 自动分发（0.5-1 周）

> **前置**: P4.NEW-B 完成（依赖 bridge 新增的 backdoor_read/write）

### 任务 C.1：bypass_get_mode() 调用集成

- [ ] **Write test**: `tests/sim_hardware/test_host_bridge_bypass_full_dispatch_standalone.cpp`
  - `TEST_CASE("host_bridge: bypass_read kFull mode dispatches to mmio_read", "[host_bridge][.]")`
  - `TEST_CASE("host_bridge: bypass_read kBypass mode dispatches to backdoor_read", "[host_bridge][.]")`
  - `TEST_CASE("host_bridge: bypass_read kPartial mode uses mmio_read (default)", "[host_bridge][.]")`
  - `TEST_CASE("host_bridge: bypass_write consistency across modes", "[host_bridge][.]")`
- [ ] **Verify fail**: `host_bridge.cpp:88-94` 当前直接调 `bridge->mmio_*`，无模式分发
- [ ] **Implement**:
  - `host_bridge_bypass_read/write`：调 `bypass_get_mode()` + 分发至 `mmio_*` (kFull) 或 `backdoor_*` (kBypass)
  - `kPartial` 保持 `mmio_*`（默认行为）
- [ ] **Verify pass**: 4 个模式测试通过

**任务 C 验收标准**：4 个模式分发测试 PASS

---

## §5 P4.NEW-D：hal_cpptlm.cpp 真实 backend + plugin.cpp backend 选择（1 周）

> **前置**: P4.NEW-A + B + C 全部完成

### 任务 D.1：hal_cpptlm.cpp 3 op 真实实现

- [ ] **Write test**: `tests/test_hal_cpptlm_real_standalone.cpp`
  - `TEST_CASE("hal_cpptlm: adapter_get_info via cpptlm backend succeeds", "[hal_cpptlm][.]")`
  - `TEST_CASE("hal_cpptlm: adapter_open/close lifecycle", "[hal_cpptlm][.]")`
  - `TEST_CASE("hal_cpptlm: 68 non-adapter fn-ptrs delegated to hal_user", "[hal_cpptlm][.]")`
- [ ] **Verify fail**: `hal_cpptlm.cpp:44-65` 当前 3 op 全 -ENOSYS
- [ ] **Implement**:
  - `cpptlm_adapter_get_info(ctx, out_info)`：调 `ule_dgpu_acquire` + `ule_dgpu_get_adapter_info` + 字段填充 + `ule_dgpu_release`
  - `cpptlm_adapter_open(ctx, out_handle)`：调 `ule_dgpu_acquire`
  - `cpptlm_adapter_close(ctx, handle)`：调 `ule_dgpu_release`
- [ ] **Verify pass**: 3 个测试通过

### 任务 D.2：组合策略 — 68 fn-ptr 委托 hal_user

- [ ] **Write test**: `TEST_CASE("hal_cpptlm: non-adapter fn-ptrs work via hal_user delegation", "[hal_cpptlm][.]")`
- [ ] **Verify fail**: 当前 `hal_cpptlm_init` 只填 3 op，其他 fn-ptr 是 NULL
- [ ] **Implement**:
  - `hal_cpptlm_init(hal, ctx)`：
    - 调 `hal_user_init(hal, ctx)` 填 68 fn-ptr
    - 覆盖 3 个 adapter fn-ptr 为 cpptlm 真实版本
- [ ] **Verify pass**: 测试通过

### 任务 D.3：plugin.cpp:119 backend 选择点

- [ ] **Write test**: `tests/test_backend_selection_standalone.cpp`
  - `TEST_CASE("plugin: env ULE_HAL_BACKEND=cpptlm uses hal_cpptlm", "[plugin][.]")`
  - `TEST_CASE("plugin: env ULE_HAL_BACKEND=user uses hal_user", "[plugin][.]")`
  - `TEST_CASE("plugin: default (no env) uses hal_user for ctest compat", "[plugin][.]")`
  - `TEST_CASE("plugin: env ULE_HAL_BACKEND=invalid falls back to hal_user", "[plugin][.]")`
- [ ] **Verify fail**: `plugin.cpp:119` 硬编码 `hal_user_init`
- [ ] **Implement**:
  - 读 `getenv("ULE_HAL_BACKEND")` + 分支选择
  - 默认 fallback 到 `hal_user_init`（保留 164/164 ctest）
- [ ] **Verify pass**: 4 个测试通过

**任务 D 验收标准**：hal_cpptlm 真化 + backend 选择 + 7 个新测试 PASS

---

## §6 测试集成（1 周）

### 任务 T.1：CMakeLists 注册 5 个新测试

- [ ] **修改** `tests/CMakeLists.txt`：注册 5 个新测试 binary
- [ ] **修改** `sim_hardware/CMakeLists.txt`：移除 stub 引用 + 添加 backdoor_endpoint.cpp
- [ ] **修改** `plugins/gpu_driver/CMakeLists.txt`：添加 cpptlm 链接

### 任务 T.2：164/164 ctest 零回归验证

- [ ] **运行** `ctest --output-on-failure` — 必须保持 164/164 PASS
- [ ] **运行** 新增 5 个测试 — 必须全部 PASS

### 任务 T.3：docs-audit 验证

- [ ] **运行** `./tools/docs-audit.sh` — 67/67 PASS

### 任务 T.4：CppTLM sibling 仓集成验证

- [ ] **运行** `ULE_HAL_BACKEND=cpptlm ./bin/test_hal_cpptlm_real_standalone` — 真实 CppTLM binding 跑通
- [ ] **运行** `nm -D ../CppTLM/build/lib/libcpptlm_emulator.so | grep cpptlm_ | wc -l` — 24 ABI 可解析

---

## §7 Gate & Acceptance（最终验收）

### Gate 5.5.6-A：4 子任务全部完成

- [ ] P4.NEW-A: `backdoor_endpoint.cpp` 真实实现（5 函数 + 100ms 超时）
- [ ] P4.NEW-B: `bridge.cpp` kCpptlm dlopen 22 ABI + backdoor_read/write 方法
- [ ] P4.NEW-C: `host_bridge.cpp` bypass/full 自动分发
- [ ] P4.NEW-D: `hal_cpptlm.cpp` 真化 + `plugin.cpp` backend 选择

### Gate 5.5.6-B：测试全部通过

- [ ] 5 个新测试 binary PASS（test_backdoor_endpoint_real + test_bridge_kcpptlm_dlopen + test_host_bridge_bypass_full + test_hal_cpptlm_real + test_backend_selection）
- [ ] 164/164 既有 ctest 零回归
- [ ] 67/67 docs-audit PASS

### Gate 5.5.6-C：架构约束保持

- [ ] `drv/` 零修改
- [ ] HAL struct 形状不变（71 fn-ptr）
- [ ] ADR-023 append-only 保持
- [ ] 既已 ship 的 backdoor_endpoint.h header 不变（兼容已 ship 的 kcpptlm-backend-binding）

### Gate 5.5.6-D：5.5.6 主线解锁 5.5.7

- [ ] 文档更新：`stage-5-5-2-driver-stack-flow.md §-1.2.1` 8 模块矩阵更新（PCIe ✅ / 固件 ⚠️ / 中断 ⚠️ / 显存 ⚠️ / 命令 ⚠️ / 同步 ⚠️ / 错误 ⛔ / 电源 ❌ → PCIe ✅ / 固件 ⚠️ / 中断 ⚠️ / 显存 ⚠️ / 命令 ⚠️ / 同步 ⚠️ / 错误 ⛔ / 电源 ❌，但 5.5.6 已完成 EP binding）
- [ ] 路线图：pcie-bus-bridge-roadmap.md §修订记录 v0.2.3 加入"5.5.6 ✅ 已归档"

---

## §8 实施顺序与依赖图

```
[A.1 骨架 + 测试] → [A.2 acquire/release] → [A.3 get_info] → [A.4 read/write] → [A.5 timeout]
                                                                       ↓
                                                              [A.6 删除 stub]
                                                                       ↓
                                                              [B.1 dlopen] → [B.2 dlsym 22 ABI] → [B.3 backdoor_read/write] → [B.4 kCpptlm 分支]
                                                                                                                              ↓
                                                                                                                    [C.1 bypass/full 分发]
                                                                                                                              ↓
                                                                                                                    [D.1 3 op 真化] → [D.2 组合] → [D.3 backend 选择]
                                                                                                                              ↓
                                                                                                                    [T.1 CMake] → [T.2 ctest 164] → [T.3 docs-audit] → [T.4 集成验证]
```

**关键路径**: A.1 → A.2 → A.3 → A.4 → B.1 → B.2 → B.3 → C.1 → D.1 → D.2 → D.3 → T.4
**总工时**: 1-2 + 1-2 + 0.5-1 + 1 + 1 = **4.5-7 周**（含 1 周缓冲）

---

## §9 风险与回退

### 风险 1：CppTLM sibling 仓 ABI 不兼容（中等）

**症状**：dlopen 失败 / dlsym 符号缺失 / dlsym 签名不匹配
**回退**：WARN 日志 + 回退 mock + 不影响 164/164 ctest

### 风险 2：5 个新测试偶发超时（中等）

**症状**：backdoor_endpoint 100ms 超时分支触发导致测试失败
**回退**：测试 fixture 用 `sleep_for(50ms)` 模拟负载；超时参数可调

### 风险 3：组合策略导致 hal_user/hal_cpptlm 行为不一致（低）

**症状**：同一 IOCTL 在 user vs cpptlm backend 下结果不同
**回退**：cpptlm backend 测试明确标 backend；行为对齐契约（per design §1.2）

---

## §10 跨引用

- [proposal.md](proposal.md) — Why/What/Capabilities/Impact
- [design.md](design.md) — 技术设计
- [specs/cpptlm-real-backend/spec.md](specs/cpptlm-real-backend/spec.md) — capability 规范
- [kcpptlm-archive-audit](../2026-09-08-kcpptlm-archive-audit/) — 前置基线（5 项虚假完成识别）
- [ADR-091](../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ — 4 象限
- [ADR-092](../00_adr/adr-092-hal-adapter-and-bypass-binding.md) 🔄 — HAL adapter binding
- [ADR-023](../00_adr/adr-023-hal-interface.md) ✅ — HAL append-only
- [docs/roadmap/driver-stack-flow-roadmap.md §0.4/P4](../../docs/roadmap/driver-stack-flow-roadmap.md) — 实施清单

---

**任务作者**: UsrLinuxEmu Architecture Team
**创建日期**: 2026-09-08
**预期完成**: 2026-10-20（4-6 周后）
