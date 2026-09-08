# Design: 5.5.7-cpptlm-cp-real-ification

> **关联**: [proposal.md](proposal.md) · [tasks.md](tasks.md) · [specs/cpptlm-cp-real-ification/spec.md](specs/cpptlm-cp-real-ification/spec.md)
> **前置基线**: 5.5.6 dGPU E2E 主线 P0 ship（commit c63a9f3 + 4bf7508 + 0e300ef）

---

## §1 设计目标

5.5.6 已完成 **ABI 通道打通**（4 数据通路函数真实调用 CppTLM ABI，ret != -ENOSYS），但未验证 **ret == 0** 与 **数据 roundtrip**。5.5.7 是验证阶段，**零代码改动**，仅做：

1. **profile 真实化验证**：`topology_path="configs/dgpu_board_v1.json"` → 期望 4 函数返 0
2. **MMIO/backdoor roundtrip**：写 0xDEADBEEF → 读回相等
3. **3 个关键决策**（D.1 -ETIMEDOUT / D.2 adapter op 接入点 / D.3 ctest cwd）

---

## §2 当前已知基线（5.5.6 ship 后）

| 函数 | 当前位置 | kCpptlm 行为 | 期望 5.5.7 行为 |
|------|----------|--------------|------------------|
| `CpptlmBridge::mmio_read` | `bridge.cpp:283-286` | 调 `syms.mmio_read(emu, ...)` 返 -22/-110 等 | 返 0 |
| `CpptlmBridge::mmio_write` | `bridge.cpp:300-303` | 调 `syms.mmio_write(emu, ...)` 返 -22 | 返 0 |
| `CpptlmBridge::backdoor_read` | `bridge.cpp:317-320` | 调 `syms.backdoor_read(emu, ...)` 返 -22 | 返 0 |
| `CpptlmBridge::backdoor_write` | `bridge.cpp:331-334` | 调 `syms.backdoor_write(emu, ...)` 返 -22 | 返 0 |
| `ule_dgpu_acquire` | `backdoor_endpoint.cpp` | 调 `cpptlm_emulator_create_by_id(1)` 返 emu | 返 0 + valid handle |
| `ule_dgpu_get_adapter_info` | `backdoor_endpoint.cpp` | 调 `cpptlm_emulator_get_adapter_info` | 返 0 + 9 字段填充 |
| `ule_dgpu_read/write` | `backdoor_endpoint.cpp` | 调 `syms.{mmio,backdoor}_*` | 返 0（kBarMmio 期望 0，kBarVram 期望 0） |

5.5.6 状态：从 UsrLinuxEmu cwd 跑 ret 非 0（-22/-110），从 CppTLM cwd 跑 ret 仍非 0（profile 缺 CP attach）。5.5.7 验证目标：从 CppTLM cwd 跑 ret == 0。

---

## §3 验证架构

### §3.1 三档测试结构（沿用 5.5.6）

| 档位 | tag | 含义 | ctest 默认 |
|------|-----|------|------------|
| **Mock** | `[mock]` | 纯 mock backend，无 ABI 调 | ✅ 默认 |
| **Stub** | `[stub]` | -ENOSYS 路径 | ✅ 默认 |
| **Real CppTLM** | `[real][cpptlm]` | 真实 ABI（5.5.6 起） | ❌ 需显式运行 |
| **Profile** | `[profile][.]` | 5.5.7 新增：真实 profile + CP | ❌ 需显式运行 |

### §3.2 测试 cwd 策略（D.3 决策点）

**当前 5.5.6 状态**：测试必须从 CppTLM cwd 跑（因 `configs/dgpu_board_v1.json` 相对路径）。CTest `WORKING_DIRECTORY` 默认为 `${PROJECT_SOURCE_DIR}`（UsrLinuxEmu 根）。

**5.5.7 选项**：

| 选项 | 优点 | 缺点 |
|------|------|------|
| A. ctest WORKING_DIRECTORY 改 CppTLM | 测试自动从正确 cwd 跑 | 影响所有 169 个测试 + 插件相对路径 |
| B. 测试内 `chdir` | 局部影响 | 不可重入，多线程 CI 风险 |
| C. 测试内绝对路径 `topology_path="/workspace/project/CppTLM/configs/dgpu_board_v1.json"` | 零全局影响，跨 cwd 稳定 | 硬编码路径，迁移性差 |
| D. 保持当前模式（需显式从 CppTLM 跑） | 显式 opt-in | 用户需知道运行模式 |

**5.5.7 推荐**：D（保持当前）。理由：5.5.7 是 verify-only change，不引入新全局行为；profile 测试显式 opt-in 避免 CI 噪声。

### §3.3 -ETIMEDOUT 语义（D.1 决策点）

**现象**：从 CppTLM cwd 跑，`mmio_read(emu, 0, 0, &out, 4)` 返 -110（-ETIMEDOUT）。

**根因**（推测，需 CppTLM 仓确认）：`DGpuBoard` 初始化时若未注册某些回调（如 `register_backdoor_cb` / `register_dma_translate_cb`），MMIO read 进入 wait queue 超时。

**5.5.7 决策选项**：

| 选项 | 含义 | 5.5.8 影响 |
|------|------|------------|
| X. 接受 -ETIMEDOUT 为正常返回 | profile 测试跳过 ret == 0 强约束，5.5.8 需先 register CP | 5.5.8 必须 attach CP 才能继续 |
| Y. 修复 CppTLM profile | 在 `dgpu_board_v1.json` 加 CP 默认 attach | 跨仓改动，超出 5.5.7 scope |
| Z. 在 UsrLinuxEmu 端调 `register_backdoor_cb` 修复 | bridge.cpp init 时注册 noop callback | 影响 5.5.6 行为，需 Oracle 复审 |

**5.5.7 推荐**：X（接受 -ETIMEDOUT）。理由：5.5.7 scope 是验证而非修复；5.5.8 启动时自然要 attach CP，-ETIMEDOUT 会在那时消失。

### §3.4 adapter op 接入点（D.2 决策点）

**当前状态**：`adapter_get_info` / `adapter_open` / `adapter_close` 3 个 fn-ptr 由 `hal_cpptlm_init` 注入，**但 drv/ 零调用者**（Oracle P4.NEW-D 建议 3）。

**5.5.7 决策选项**：

| 选项 | 含义 | 5.5.8 影响 |
|------|------|------------|
| P. TaskRunner 直调 `gpu_hal_ops.adapter_*` | HAL 契约层直调，符合 Linux 驱动 idiom | TaskRunner 集成需要符号链接 |
| Q. GpgpuDevice ioctl 派发表分发 | 5.5.6 GPU_IOCTL_GET_DEVICE_INFO 已有，扩到 BO alloc/open/close | drv/ 改动，超出 5.5.7 scope |
| R. 双轨：TaskRunner 默认 P，GpgpuDevice 可选 Q | 灵活 | 复杂度高，5.5.7 内决策 |

**5.5.7 推荐**：P（TaskRunner 直调）。理由：
- TaskRunner 已是 UsrLinuxEmu 外部子模块，通过 `external/TaskRunner/` 链接
- HAL 契约层 `gpu_hal_ops.adapter_*` 已是 Linux 驱动 idiom 的标准抽象
- 避免 drv/ 改动（保持 5.5.6 G1-G4 边界契约）

---

## §4 测试设计

### §4.1 新增 test_bridge_kcpptlm_profile_real_standalone

**位置**：`tests/sim_hardware/test_bridge_kcpptlm_profile_real_standalone.cpp`

**5 个用例**：

| 用例 | tag | 验证内容 | 期望 |
|------|-----|----------|------|
| `bridge: kCpptlm mmio_read ret==0 with dgpu_board_v1.json` | `[profile][mmio]` | ret == 0 | 5.5.7 D.1 决策后定 |
| `bridge: kCpptlm mmio_write ret==0 with dgpu_board_v1.json` | `[profile][mmio]` | ret == 0 | 同上 |
| `bridge: kCpptlm backdoor_read ret==0 with dgpu_board_v1.json` | `[profile][backdoor]` | ret == 0 | 同上 |
| `bridge: kCpptlm backdoor_write ret==0 with dgpu_board_v1.json` | `[profile][backdoor]` | ret == 0 | 同上 |
| `bridge: 5.5.7 profile gate (cpptlm 4 datapath + D.1 semantics)` | `[profile][gate]` | wiring 验证 | 5/5 PASS 或按 D.1 SKIP |

**成功条件**：
- 如果 D.1 决策为 X（接受 -ETIMEDOUT）：测试 SKIP，gate 验证 ABI 通道已通（!= -ENOSYS）
- 如果 D.1 决策为 Y/Z：测试 REQUIRE ret == 0

### §4.2 复用 5.5.6 已有测试

不修改，仅在 5.5.7 文档中确认以下测试在 profile 模式下行为：
- `test_backdoor_endpoint_real_standalone [rw]`：kBarMmio/kBarVram roundtrip
- `test_hal_cpptlm_real_standalone [.]`：3 adapter op 9 字段 mapping
- `test_bridge_kcpptlm_data_path_standalone [datapath]`：4 数据通路 ABI 通道

---

## §5 决策记录

| ID | 主题 | 状态 | 输出 |
|----|------|------|------|
| D.1 | -ETIMEDOUT 语义 | 🔄 Proposed | [decisions/D1-etimedout-semantics.md](decisions/D1-etimedout-semantics.md) |
| D.2 | adapter op 接入点 | 🔄 Proposed | [decisions/D2-adapter-op-access.md](decisions/D2-adapter-op-access.md) |
| D.3 | ctest WORKING_DIRECTORY | 🔄 Proposed | [decisions/D3-ctest-cwd.md](decisions/D3-ctest-cwd.md) |

---

## §6 风险评估

| 风险 | 等级 | 缓解 |
|------|------|------|
| -ETIMEDOUT 是 CppTLM 行为而非实现 bug | 中 | 5.5.7 D.1 决策 X 接受；5.5.8 启动时 attach CP 自然消失 |
| adapter op 接入点影响 drv/ 边界 | 中 | 5.5.7 D.2 决策 P TaskRunner 直调，零 drv/ 改动 |
| ctest cwd 改动影响其他测试 | 高 | 5.5.7 D.3 决策 D 保持当前模式，零全局影响 |
| 真实 ABI 验证引入 flaky（与 backdoor_endpoint 同样） | 中 | 沿用 P4.NEW-A flaky fix 模式：CHECK(rd_ret != -ENOSYS) + INFO 解释 |

---

## §7 跨引用

- [proposal.md](proposal.md) — Why/What/Capabilities/Impact
- [tasks.md](tasks.md) — TDD 5 步结构
- [specs/cpptlm-cp-real-ification/spec.md](specs/cpptlm-cp-real-ification/spec.md) — capability 规范
- [前置 5.5.6](../archive/2026-09-08-2026-09-08-5-5-6-cpptlm-ep-binding/) — ABI 通道基线
- [kcpptlm-archive-audit](../archive/2026-09-08-2026-09-08-kcpptlm-archive-audit/) — 前置审计
- [ADR-091](../../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) — 4 象限
- [ADR-092](../../00_adr/adr-092-hal-adapter-and-bypass-binding.md) — HAL adapter binding
- [ADR-023](../../00_adr/adr-023-hal-interface.md) — HAL append-only
