# Tasks: ue-stage-1-1-bridge-sync

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **工期**: 0.5-1 周（依赖 CppTLM change 完成）
> **TDD 纪律**: 每任务 5 步（write failing test → verify fail → implement → verify pass → commit）

## §1 前置依赖

### 任务 1.1: 等待 CppTLM change 完成
- [ ] **Wait**: CppTLM `2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes` 完成（4 bug 修复 + Oracle Gate E 复审通过）
- [ ] **Verify**: CppTLM `bab64dd5` 之后有 4-5 个新 commit（每修复一个）

### 任务 1.2: 同步双仓构建
- [ ] **Build CppTLM**: `cd /workspace/project/CppTLM/build && make -j4`
- [ ] **Build UE**: `cd /workspace/project/UsrLinuxEmu/build && make -j4`
- [ ] **Verify**: UE `dlopen` 重新链接 CppTLM `libcpptlm_emulator.so`

## §2 桥接层断言升级（5 测试）

### 任务 2.1: test_dgpu_bridge_sanity 断言升级
- [ ] **Modify**: `tests/integration/test_dgpu_bridge_sanity_standalone.cc`
  - `REQUIRE(ret != -ENOSYS)` → `REQUIRE(ret == 0); REQUIRE(info.vendor_id == 0x10DE)`
- [ ] **Verify pass**: 新断言通过

### 任务 2.2: test_bridge_dgpu_profile_real 断言升级
- [ ] **Modify**: `tests/integration/test_bridge_dgpu_profile_real.cc`
  - 升级 5 个 profile 测试断言
- [ ] **Verify pass**: 新断言通过

### 任务 2.3-2.5: 新建 4 修复专项测试
- [ ] **新建**: `tests/integration/test_dgpu_pcie_config_vendor_id.cc`
  - 验证 Vendor ID 0x10DE
- [ ] **新建**: `tests/integration/test_dgpu_backdoor_miss_enoent.cc`
  - 验证 miss 返 -ENOENT
- [ ] **新建**: `tests/integration/test_dgpu_mmio_real_data.cc`
  - 验证 mmio_read 真实数据
- [ ] **新建**: `tests/integration/test_dgpu_mmio_write_async.cc`
  - 验证 mmio_write <1ms 返回

## §3 跨仓集成测试

### 任务 3.1: test_bridge_dgpu_with_real_pcie_ep
- [ ] **新建**: `tests/integration/test_bridge_dgpu_with_real_pcie_ep.cc`
  - dlopen libcpptlm_emulator.so
  - 调用 4 修复后的 ABI
  - 端到端验证
- [ ] **Verify pass**: 集成测试通过

### 任务 3.2: 添加到 CMake 构建
- [ ] **Modify**: `tests/integration/CMakeLists.txt`
  - 添加 5 个新测试源文件
- [ ] **Verify**: `make -j4` 编译成功

## §4 桥接层错误处理细节（如需要）

> **边界（Oracle R8 修订 2026-09-10）**: 本 §4 仅允许**错误码映射 + timeout 处理区域**修改。**禁止修改 `bridge.cpp` init() / 注册路径**（CP attach helper 注册属于 5.5.8 change 所有）。Verify-only 优先；Modify 仅在 verify 失败时。

### 任务 4.1: bridge.cpp 错误码映射
- [ ] **Verify**: 桥接层错误码与 CppTLM 错误码一致
- [ ] **Modify** (如需要, 仅错误码映射区域): `plugins/gpu_driver/sim_hardware/src/cpptlm/bridge.cpp`
- [ ] **Oracle review**: 错误码映射变更需复审

### 任务 4.2: timeout 处理
- [ ] **Verify**: mmio_read timeout 映射到 UE ETIMEDOUT
- [ ] **Modify** (如需要, 仅 timeout/错误处理区域): bridge.cpp timeout 处理

## §5 Oracle 复审

### 任务 5.1: 1 次轻量复审
- [ ] **Oracle review**: UE 侧桥接同步质量
- [ ] **Verify**: ADR-035 §R2 v1.0 → v1.1 升档流程通过

## §6 5.5.7 启动 gate 解锁

### 任务 6.1: gate 验证
- [ ] **Verify**: 5.5.7 启动条件 = 阶段 1.1 (✅) + 1.2 (待) + 1.3a (待) + UE 侧同步 (本 change ✅)
- [ ] **Document**: 在 `pcie-bus-bridge-roadmap.md` 标记 UE 侧同步完成

### 任务 6.2: 协调 5.5.7 change owner
- [ ] **Notify**: 通知 5.5.7 change owner UE 侧同步完成
- [ ] **Confirm**: 5.5.7 change 可启动实施

### 任务 6.3: Archive 本 change
- [ ] **Run**: `openspec archive 2026-09-10-ue-stage-1-1-bridge-sync`
- [ ] **Verify**: spec 提升到 `openspec/specs/ue-stage-1-1-bridge-sync/spec.md`

## 总计

- **任务数**: 14
- **预期 commit**: 5-6 (断言升级 2 + 新建测试 4-5 + 集成测试 1)
- **工期**: 0.5-1 周（依赖 CppTLM change 完成后启动）
- **前置依赖**: CppTLM `2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes` ship
- **下游解锁**: UsrLinuxEmu `2026-09-09-5-5-7-cpptlm-cp-real-ification` change 部分前置解锁

---

## §7 阶段 1.2: UE 侧 MSI-X 中断集成测试

> **前置**: CppTLM `2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes` §7 stage 1.2 ship

### 任务 7.1: 写集成测试
- [ ] **Write test**: `tests/integration/test_dgpu_msix_real_trigger.cc`
  - 从 UE 进程 dlopen libcpptlm_emulator.so
  - 触发 MSI-X 中断（msix_init + trigger_irq_async + intr_cb 验证）
  - 200ms 超时窗口内 intr_cb ≥1 次触发
- [ ] **Verify pass**: 测试通过

### 任务 7.2: 桥接层验证
- [ ] **Verify**: `plugins/gpu_driver/sim_hardware/src/cpptlm/bridge.cpp` intr_cb 路径正确
- [ ] **Verify**: 测试覆盖 vector 参数化（vector 由驱动/entry 指定）

### 任务 7.3: Commit
- [ ] **Commit (UE)**: `test(ue): MSI-X 集成测试 (intr_cb 200ms 触发验证)`
- [ ] **Commit (entry sync)**: `docs(pcie-ep): v0.5 stage 1.2 UE 集成 ship`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.5.4 mirror UE stage 1.2 UE 集成`

---

## §8 阶段 1.3a: UE 侧 SDMA Ring Buffer 集成测试

> **前置**: CppTLM stage 1.3a ship

### 任务 8.1: 写集成测试
- [ ] **Write test**: `tests/integration/test_dgpu_sdma_ring_buffer_ue.cc`
  - Ring Buffer 4 档 cfg.ring_size 测试
  - BAR1+0x10010000 Doorbell 写入触发 WPTR
  - SG 描述符链（MAX_SG_ENTRIES=8）
- [ ] **Verify pass**: 集成测试通过

### 任务 8.2: 桥接层验证
- [ ] **Verify**: `bridge.cpp` Doorbell 写入路径正确
- [ ] **Verify**: SG 描述符 8 链 UE 端正确处理

### 任务 8.3: Commit
- [ ] **Commit (UE)**: `test(ue): SDMA Ring Buffer + Doorbell + SG 集成测试`
- [ ] **Commit (entry sync)**: `docs(pcie-ep): v0.6 stage 1.3a UE 集成 ship`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.6.0 mirror UE stage 1.3a UE 集成`

---

## §9 阶段 1.3b: UE 侧 D2D NoC 集成测试

> **前置**: CppTLM stage 1.3b ship

### 任务 9.1: 写集成测试
- [ ] **Write test**: `tests/integration/test_dgpu_d2d_noc_ue.cc`
  - NoC payload 转发验证（≥100 GB/s simulated）
  - host_out 零事务断言（VRAM bypass）
- [ ] **Verify pass**: 集成测试通过

### 任务 9.2: Commit
- [ ] **Commit (UE)**: `test(ue): D2D NoC payload + memctl bypass 集成测试`
- [ ] **Commit (entry sync)**: `docs(pcie-ep): v0.7 stage 1.3b UE 集成 ship`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.7.0 mirror UE stage 1.3b UE 集成`

---

## §10 阶段 1.3c: UE 侧 dma_translate_cb + IOMMU 集成测试

> **前置**: CppTLM stage 1.3c ship（修复 #2 真实化）

### 任务 10.1: 写集成测试
- [ ] **Write test**: `tests/integration/test_dgpu_dma_translate_ue.cc`
  - identity 模式：cb 真实调用，pa==iova
  - IOMMU 模式：4 级翻译链验证
  - cb 失败传播负 errno（-ENOSYS/-EIO）
- [ ] **Verify pass**: 集成测试通过

### 任务 10.2: 桥接层验证
- [ ] **Verify**: `bridge.cpp` `register_dma_translate_cb` 路径正确
- [ ] **Verify**: UE 端 IOMMU 模拟路径与 CppTLM 4 级翻译对齐

### 任务 10.3: Commit
- [ ] **Commit (UE)**: `test(ue): dma_translate_cb + IOMMU 4 级翻译集成测试`
- [ ] **Commit (entry sync)**: `docs(pcie-ep): v0.8 stage 1.3c UE 集成 ship`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.8.0 mirror UE stage 1.3c UE 集成`

---

## §11 阶段 1.3d: UE 侧 SDMA 完成通知集成测试

> **前置**: CppTLM stage 1.3d ship

### 任务 11.1: 写集成测试
- [ ] **Write test**: `tests/integration/test_dgpu_sdma_completion_ue.cc`
  - Fence 命令触发完成事件
  - 200ms 超时窗口内 intr_cb ≥1 次触发（验证修复 #4 完整路径）
  - vector 路由正确（由驱动/entry 指定）
- [ ] **Verify pass**: 集成测试通过

### 任务 11.2: Commit
- [ ] **Commit (UE)**: `test(ue): SDMA Fence + MSI-X 接线 + completion ring 集成测试`
- [ ] **Commit (entry sync)**: `docs(pcie-ep): v0.9 stage 1.3d UE 集成 ship`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.9.0 mirror UE stage 1.3d UE 集成`

---

## §12 阶段 1.4: UE 侧电源管理集成测试

> **前置**: CppTLM stage 1.4 ship

### 任务 12.1: 写集成测试
- [ ] **Write test**: `tests/integration/test_dgpu_power_mgmt_ue.cc`
  - PM Capabilities 寄存器读取
  - D0/D3hot/D3cold 状态切换
  - ASPM L0s/L1 自动协商
- [ ] **Verify pass**: 集成测试通过

### 任务 12.2: Commit
- [ ] **Commit (UE)**: `test(ue): PM D0/D3 + ASPM 集成测试`
- [ ] **Commit (entry sync)**: `docs(pcie-ep): v0.10 stage 1.4 UE 集成 ship`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.10.0 mirror UE stage 1.4 UE 集成`

---

## §13 阶段 2.1: UE 侧 P2P + Resizable BAR 集成测试

> **前置**: CppTLM stage 2.1 ship

### 任务 13.1: 写集成测试
- [ ] **Write test**: `tests/integration/test_dgpu_p2p_ue.cc`
  - P2P DMA 跨 root port 路由
  - ACS Capability 验证
  - Resizable BAR 大小动态调整
- [ ] **Verify pass**: 集成测试通过

### 任务 13.2: Commit
- [ ] **Commit (UE)**: `test(ue): P2P + Resizable BAR 集成测试`
- [ ] **Commit (entry sync)**: `docs(pcie-ep): v0.11 stage 2.1 UE 集成 ship`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.11.0 mirror UE stage 2.1 UE 集成`

---

## §14 5.5.7 启动 gate 解锁验证

> **触发条件**: 阶段 1.1 + 1.2 + 1.3a ship + bridge-sync ship（宽松口径 per cb82146c Q1 裁决）

### 任务 14.1: 5.5.7 gate 验证
- [ ] **Verify**: CppTLM 仓 `2026-09-10-cpptlm-stage-1-1-pcie-ep-fixes` §1 §7 §8 全部 ship
- [ ] **Verify**: UE 仓 `2026-09-10-ue-stage-1-1-bridge-sync` §1-§8 全部 ship
- [ ] **Document**: `pcie-bus-bridge-roadmap.md` 标记 UE 侧 + CppTLM 端阶段 1.1+1.2+1.3a ship

### 任务 14.2: 5.5.7 change owner 协调
- [ ] **Notify**: 通知 `2026-09-09-5-5-7-cpptlm-cp-real-ification` change owner gate 解锁
- [ ] **Confirm**: 5.5.7 change 可启动实施（CommandProcessor 真实化）

### 任务 14.3: 5.5.7 启动 gate commit
- [ ] **Commit (UE entry)**: `docs(pcie-ep): v0.12 5.5.7 启动 gate 解锁`
- [ ] **Commit (CppTLM mirror)**: `docs(cpptlm): v0.12.0 mirror UE 5.5.7 gate 解锁`
- [ ] **Commit (CppTLM entry §2.3)**: `docs(cpptlm): 18-doc §2.3 关键路径 gate 解锁标记`

---

## §15 总计

- **UE 仓 commits**: 14（每阶段 1 测试 + 1 entry sync，gate 解锁 2 entry + 1 18-doc）
- **CppTLM 仓 commits**: 14（每阶段 1 18-doc mirror + 1 gate 18-doc）
- **双仓合计 commits**: 28
- **工期**: 5.0-6.0 周（与 CppTLM change 同步推进）
- **下游**: 5.5.7 启动 gate 解锁 → UsrLinuxEmu 5.5.7 change owner 通知

