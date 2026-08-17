# ADR-090 实施: PTXIR Image Loading via CppTLM H2D DMA

## Why

Oracle session `ses_ff2106f84ffeM2oItBEa9iu4hL`(2026-08-17)识别 ADR-076 v1 层次违规:`hal_user.cpp` dlopen `libptxemu_device.so` 让 HAL 桥承担硬件行为提供者职责,违反 ADR-036(HAL 是桥不是第 4 层)。

用户核心观察:`sim/hardware/hardware_puller_emu.cpp` 是 PM4/AQL packet 调度器(类比 AMD MES / NVIDIA GSP Puller),**不是** SM kernel 调度器。PTX-EMU 的真实定位是 SM 内部 ISA 执行(L3+L4),按层次归属应在 dGPU 板卡仿真范畴(CppTLM per ADR-088),不是 UsrLinuxEmu HAL backend。

ADR-090 Supersedes ADR-076 v2:将 PTX-EMU 集成点从 UsrLinuxEmu HAL 迁移到 CppTLM submodule;UsrLinuxEmu 仅保留 PTXIR image 加载职责(H2D DMA 写入 VRAM)。

## What Changes

### ADR/文档(Phase 1)
- **新增** `docs/00_adr/adr-090-ptxir-via-h2d-dma.md` — canonical ADR (Supersedes ADR-076 v2)
- **新增** `docs/05-advanced/adr-090-cross-repo-coordination.md` — 跨仓协调 annex (PTX-EMU §D8 amendment + tadr-308 + CppTLM v5.0 spec)
- **修订** `docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md` — 🚫 Superseded v2 头注
- **修订** `docs/00_adr/adr-088-dgpu-complete-simulation.md` — §C2 (取消"不被取代"条款) + §D6.2 (SM executor +2~3 ABI 扩展注记)
- **修订** `docs/00_adr/README.md` — ADR-090 entry + ADR-076 Superseded + 状态分布 60/6/2/73
- **修订** `docs/README.md` — ADR 计数 72→73 + ADR 编号范围同步

### ABI 契约(Phase 2)
- **修订** `plugins/gpu_driver/shared/gpu_ioctl.h`:
  - `gpu_load_kernel_module_args`: 移除 `kernel_name[256]` 字段,新增 `out_vram_addr`(替代 `out_module_handle`)
  - `gpu_launch_kernel_module_args`: 字段保留(ABI 卫生),handler 强制返回 -ENOSYS
  - `gpu_unload_kernel_module_args`: `module_handle` 语义重定义为 `vram_addr`
  - 0x27/0x28/0x29 nr 编号永久保留(防复用)
- **修订** `plugins/gpu_driver/hal/gpu_hal.h`:
  - #66 `kernel_module_load` 新语义: H2D DMA + icache invalidate(Mode A no-op / Mode B CppTLM)
  - #67/#68: 标 deprecated stub(返回 -ENOSYS),struct 槽位保留 per ADR-023 §D4

### 实现层(Phase 3)
- **修订** `plugins/gpu_driver/hal/hal_user.cpp`:
  - 移除 ~150 行 PTX-EMU dlsym 代码(`g_ptxemu_abi`/`ensurePtxemuAbiLoaded`/`cuda_error_to_errno`/5 ABI typedef)
  - 重写 `user_kernel_module_load`: `user_mem_alloc` + `user_mem_write` 实现 H2D DMA(Mode A)
  - `user_kernel_module_execute/unload`: 返回 -ENOSYS(deprecated stub)
- **修订** `plugins/gpu_driver/hal/hal_mock.cpp`:
  - `kernel_module_load` mock: 重写为返回 `out_vram_addr`(std::atomic 计数器)
  - `kernel_module_execute/unload`: 返回 -ENOSYS
  - `hal_mock_inject_ptxemu_*` 4 个 helper 标 deprecated(待 M9 清理)

### 驱动层(Phase 4)
- **修订** `plugins/gpu_driver/drv/gpgpu_device.cpp`:
  - `handleLoadKernelModule`: 移除 kernel_name 处理,委托 HAL VRAM write
  - `handleLaunchKernelModule`: 转发到 HAL deprecated stub(-ENOSYS)
  - `handleUnloadKernelModule`: 转发到 HAL deprecated stub(-ENOSYS)

### 测试(Phase 4)
- **重写** `tests/test_hal_kernel_module_standalone.cpp`:
  - 删除 6 个 ADR-076 PTX-EMU 错误注入 SECTIONs(迁出本仓至 PTX-EMU/CppTLM)
  - 新增 5 个 ADR-090 SECTIONs:image_size 边界 / out_vram_addr 填充 / 0x28 stub -ENOSYS / 0x29 stub -ENOSYS / 并发 load race

## Acceptance

- [x] cmake --build: 所有目标编译通过
- [x] ctest: **148/148 PASS, 0 failed** (4.19s)
- [x] test_hal_kernel_module_standalone: 11 assertions in 1 test case (5 SECTIONs)
- [x] test_ioctl_table_coverage_standalone: 92 assertions in 2 test cases
- [x] test_hal_init_completeness_standalone: PASS (68 HAL fn-ptrs all non-null)
- [x] docs-audit: 68/68 PASS(非 strict 模式; strict 模式 FAIL 为 Doxygen 未安装环境问题,与本次变更无关)
- [x] Oracle Gate #6: **APPROVED**(session `ses_ff1f38c57ffevzUfohqp45av02` + `ses_ff1ecf07cffe5D3lwDqOmgFdyx`)
- [x] Gate #1 HAL append-only: 通过(3 fn-ptrs → 1 + 2 deprecated stub)
- [x] Gate #5 Architecture Team review: 通过(2026-08-17 user ack)
- [ ] Gate #2 CppTLM maintainer ack: 待 annex 发送
- [ ] Gate #3 PTX-EMU owner ack: 待 annex 发送
- [ ] Gate #4 TaskRunner owner ack: 待 annex 发送

## Migration

详见 [ADR-090 §Migration](../../docs/00_adr/adr-090-ptxir-via-h2d-dma.md#migration--实施步骤)。

## Cross-Repo Coordination

per ADR-035 §R5.1,跨仓 commit 顺序:

1. **UsrLinuxEmu** ADR-090 → ✅ Accepted(待 Gates #2-#5 全部 ✅)
2. **PTX-EMU** ADR-0029 §D8 amendment(详见 cross-repo annex §A)
3. **CppTLM** handoff spec v5.0(详见 cross-repo annex §C)
4. **TaskRunner** tadr-308(详见 cross-repo annex §B)
5. 代码实施(PTX-EMU submodule → CppTLM SM executor → UsrLinuxEmu Mode B 切换 → TaskRunner tadr-308)
6. **UsrLinuxEmu** submodule bump + e2e 集成测试(Mode A + Mode B 双路径 Gate 4.7 验证)

Cross-repo coordination annex: `docs/05-advanced/adr-090-cross-repo-coordination.md`(owner action items checklist per A.6 / B.7 / C.4)

## Risks

详见 [ADR-090 §Consequences + Risk Matrix](../../docs/00_adr/adr-090-ptxir-via-h2d-dma.md#consequences)。

## 关联 ADR

- [ADR-090](../../docs/00_adr/adr-090-ptxir-via-h2d-dma.md) 🔄 Proposed(Gate #1/#5/#6 ✅, Gate #2/#3/#4 ⏳)
- [ADR-076](../../docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md) 🚫 Superseded v2(已 ship 实施产物保留)
- [ADR-088](../../docs/00_adr/adr-088-dgpu-complete-simulation.md) ✅ CppTLM dGPU 板卡仿真(§C2/§D6.2 修订注记)
- [ADR-036](../../docs/00_adr/adr-036-three-way-separation.md) ✅ 3 区分架构(本 change 恢复严格遵守)
- [ADR-023](../../docs/00_adr/adr-023-hal-interface.md) ✅ HAL append-only 治理(#67/#68 deprecated stub)

## 关联 Oracle Sessions

- `ses_ff2106f84ffeM2oItBEa9iu4hL` — 2026-08-17 架构评审(识别 ADR-076 层次违规 + Mode A 回退路径)
- `ses_ff1f38c57ffevzUfohqp45av02` — 2026-08-17 实施复审(Gate #6 PASS, 5 minor follow-ups)
- `ses_ff1ecf07cffe5D3lwDqOmgFdyx` — 2026-08-17 follow-ups 合并后最终复审(Gate #6 PASS, APPROVED)

---

**Owner**: UsrLinuxEmu Architecture Team
**Date**: 2026-08-17
**Status**: ✅ Implementation Complete(Gates #1/#5/#6 ✅;跨仓 ack pending)