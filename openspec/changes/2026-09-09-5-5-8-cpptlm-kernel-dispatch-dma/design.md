# Design: 5.5.8-cpptlm-kernel-dispatch-dma

> **关联**: [proposal.md](proposal.md) · [tasks.md](tasks.md) · [specs/cpptlm-kernel-dispatch/spec.md](specs/cpptlm-kernel-dispatch/spec.md)
> **前置基线**: 5.5.6 ABI 通道（Oracle 9.5/10）+ 5.5.7.1 P5.NEW-A profile 验证（Oracle 9.4/10 + D.1 Accepted）

---

## §1 设计目标

5.5.8 完成 dGPU E2E 主线第三步：**CP attach + Kernel Dispatch + DMA**。三阶段：

1. **CP attach helper**（D.1 反馈前置）：注册 backdoor_cb + dma_translate_cb 消除 -ETIMEDOUT
2. **`ret == 0` 强约束回归**：5.5.7.1 的 5 个 TEST_CASE 升级 `CHECK` → `REQUIRE`
3. **CommandProcessor + DMA Engine**：PM4 microcode 提交 + ring buffer + transfer 发起/完成

---

## §2 5.5.7.1 实证数据回顾

| 观测 | 数值 | 含义 |
|------|------|------|
| ret=0 | 高频 | status success, 0 bytes transferred |
| ret=4 | 中频（仅 backdoor_read） | 4 bytes transferred (byte-count convention) |
| ret=-110 | 低频 | -ETIMEDOUT, DGpuBoard wait queue timeout |
| ret=-ENOSYS | 0（5.5.6 P4.NEW-B.5 修复后） | ABI 通道未启用（regression） |

**D.1 决策 X Accepted**：唯一稳定断言是 `CHECK(ret != -ENOSYS)`；5.5.8 启动时 attach CP 消除 -ETIMEDOUT 后，可升级为 `REQUIRE(ret == 0)` 强约束。

---

## §3 三阶段架构

### §3.1 阶段 1: CP attach helper（消除 -ETIMEDOUT 根因）

**根因**（推测，需 5.5.8 验证）：`DGpuBoard` 初始化时若未注册 `register_backdoor_cb` / `register_dma_translate_cb`，MMIO read 进入 wait queue 超时。

**实现**：`sim_hardware/src/cpptlm/cp_attach.cpp`

```cpp
namespace usr_linux_emu::sim_hardware {

// Noop callbacks — 5.5.8 范围仅消除 -ETIMEDOUT，不实现真实 backdoor/dma 逻辑
static int noop_backdoor_cb(void* ctx, uint8_t bar, uint64_t offset,
                            void* buf, size_t len) {
  (void)ctx; (void)bar; (void)offset; (void)buf; (void)len;
  return 0;
}

static int noop_dma_translate_cb(void* ctx, uint64_t iova, uint64_t* out_pa) {
  (void)ctx; (void)iova;
  *out_pa = iova;  // identity mapping
  return 0;
}

int cp_attach(cpptlm_emulator_t* emu) {
  if (!emu) return -EINVAL;
  if (bridge_state().syms.register_backdoor_cb) {
    bridge_state().syms.register_backdoor_cb(emu, (void*)noop_backdoor_cb);
  }
  if (bridge_state().syms.register_dma_translate_cb) {
    bridge_state().syms.register_dma_translate_cb(emu, (void*)noop_dma_translate_cb);
  }
  return 0;
}

}  // namespace usr_linux_emu::sim_hardware
```

**集成点**：`bridge.cpp` `init()` 阶段，在 `create(path)` / `create_by_id(1)` 之后调 `cp_attach(emu)`

**5.5.8 验证**：用 5.5.7.1 的 `test_bridge_kcpptlm_profile_real_standalone` 5 个 TEST_CASE，从 `CHECK(ret != -ENOSYS)` 升级为 `REQUIRE(ret == 0)`，3 次稳定 PASS

### §3.2 阶段 2: ret == 0 强约束回归

**修改文件**：`tests/sim_hardware/test_bridge_kcpptlm_profile_real_standalone.cpp`

**按函数定型断言**（基于 5.5.7.1 A.1 实证 byte-count 约定）：

| 函数 | 5.5.7.1（软约束） | 5.5.8（强约束） | 依据 |
|------|------------------|----------------|------|
| `mmio_read` | `CHECK(ret != -ENOSYS)` | `REQUIRE(ret == 0)` | 实证仅 0/-110，attach 后应恒 0 |
| `mmio_write` | `CHECK(ret != -ENOSYS)` | `REQUIRE(ret == 0)` | 同上 |
| `backdoor_read` | `CHECK(ret != -ENOSYS)` | `REQUIRE(ret >= 0)` + `INFO(ret)` | 0 或 4 均合法（byte-count） |
| `backdoor_write` | `CHECK(ret != -ENOSYS)` | `REQUIRE(ret == 0)` | 实证恒 0 |
| gate probe | `CHECK(ret != -ENOSYS)` | `REQUIRE(ret >= 0)` | 含 backdoor_read，按 byte-count 约定 |

**关键不变量**：CP attach 消除 -ETIMEDOUT(-110)，不改变 byte-count 约定。backdoor_read 返 4（4 bytes transferred）= 成功，不应误判为失败。

**5.5.9 真机验证前置**：ret 强约束 baseline（mmio/wbackdoor_write 0；backdoor_read >= 0）

### §3.3 阶段 3: CommandProcessor + DMA Engine

**CommandProcessor**（`sim_hardware/src/cpptlm/command_processor.cpp`）：
- PM4 microcode 提交（`submit_pm4_packet`）
- Ring buffer 消费（`ring_consumer_thread`）
- GPU command dispatch（`dispatch_to_engine`）

**DMA Engine**（`sim_hardware/src/cpptlm/dma_engine.cpp`）：
- Register `cpptlm_emulator_register_dma_translate_cb(emu, dma_translate_cb)`
- Transfer 发起（`dma_submit`）
- Transfer 完成（`dma_complete_cb`）

**D.2 决策 P 落地**（TaskRunner 集成）：
- `external/TaskRunner/integrate_usrlx_emu.cpp`
- 直调 `hal->adapter_get_info(hal, &info)` + `hal->adapter_open(hal, dev_id, &handle)` + `hal->adapter_close(hal, handle)`

---

## §4 测试设计

### §4.1 新增 3 个测试 binary

| 测试 binary | 用例数 | 验证内容 |
|-------------|:------:|----------|
| `test_cp_attach_standalone` | 5 | CP attach helper 行为 + ret==0 强约束回归 |
| `test_command_processor_standalone` | 7 | PM4 提交 + ring buffer + dispatch |
| `test_dma_engine_standalone` | 6 | DMA transfer 发起/完成 + callback 注册 |

### §4.2 复用 5.5.7.1 已有测试

不修改功能，仅升级断言（`CHECK` → `REQUIRE`）：
- `test_bridge_kcpptlm_profile_real_standalone`：5 个 TEST_CASE 全升级

### §4.3 TaskRunner 集成测试

新增 `test_taskrunner_adapter_integration_standalone`（如需要），验证 TaskRunner 直调 3 op 通过

---

## §5 风险评估

| 风险 | 等级 | 缓解 |
|------|------|------|
| CppTLM callback 签名不匹配 | 中 | 阶段 1 实施前先 `dlsym` 验证 `register_backdoor_cb` / `register_dma_translate_cb` 签名 |
| CP attach 后 -ETIMEDOUT 仍偶发 | 中 | 5.5.8 启动条件失败 → 重新决策 D.1（Y 跨仓修复路径） |
| TaskRunner 跨子模块 include gpu_hal.h 编译失败 | 低 | X.5 启动前**必须**执行：① 创建 `external/TaskRunner/UsrLinuxEmu` 符号链接 `ln -s ../../ .` 或 ② 在 TaskRunner CMakeLists 显式加 `include_directories(${UsrLinuxEmu_SOURCE_DIR}/plugins/gpu_driver/hal)`（任选其一；当前仓内该链接缺失） |
| 169 ctest baseline 回归 | 低 | 零 drv/ 改动 + HAL append-only 实证保持 |

---

## §6 跨引用

- [proposal.md](proposal.md) — Why/What/Capabilities/Impact
- [tasks.md](tasks.md) — TDD 5 步结构
- [specs/cpptlm-kernel-dispatch/spec.md](specs/cpptlm-kernel-dispatch/spec.md) — capability 规范
- [前置 5.5.7.1 P5.NEW-A commit `2cf4bc5`](../2026-09-09-5-5-7-cpptlm-cp-real-ification/) — Oracle 9.4/10 PASS
- [5.5.6-archive](../archive/2026-09-08-2026-09-08-5-5-6-cpptlm-ep-binding/) — ABI 通道基线
- [D.1 Accepted](../2026-09-09-5-5-7-cpptlm-cp-real-ification/decisions/D1-etimedout-semantics.md) — CP attach 前置
- [D.2 adapter op 接入点](../2026-09-09-5-5-7-cpptlm-cp-real-ification/decisions/D2-adapter-op-access.md) — TaskRunner 直调
- [D.3 ctest WORKING_DIRECTORY](../2026-09-09-5-5-7-cpptlm-cp-real-ification/decisions/D3-ctest-cwd.md) — opt-in 模式
- [ADR-023](../../00_adr/adr-023-hal-interface.md) — HAL append-only
- [ADR-091](../../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) — 4 象限
- [ADR-092](../../00_adr/adr-092-hal-adapter-and-bypass-binding.md) — HAL adapter binding
