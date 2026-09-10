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

> **Oracle 复审位置（Metis M6 修订 2026-09-10）**：实施 commit 后、docs mirror commit **前**进行。复审发现问题需追加 commit 而非 amend docs。


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
