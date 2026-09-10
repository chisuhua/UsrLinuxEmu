# Tasks: ue-stage-1-2-msix-integration

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **工期**: 0.5 周（依赖 CppTLM stage-1-2-msix ship）
> **TDD 纪律**: 每任务 5 步（write failing test → verify fail → implement → verify pass → commit）

## §1 写跨仓集成测试

### 任务 1.1: 写失败测试
- [ ] **Write test**: `tests/integration/test_dgpu_msix_real_trigger_ue.cc::test_msix_intr_cb_within_200ms`
  - dlopen libcpptlm_emulator.so
  - 注册 mock intr_cb
  - trigger_msix_async(0, 0xDEADBEEF)
  - 200ms 超时窗口验证 cb_called ≥ 1
- [ ] **Verify fail**: 假设 CppTLM stage-1-2 已 ship 但 UE 端未集成（无该集成测试），预期首次失败

### 任务 1.2: CMakeLists.txt 注册
- [ ] **Modify**: `tests/integration/CMakeLists.txt`
  - 添加 `test_dgpu_msix_real_trigger_ue.cc` 到集成测试源列表
  - 注册 ctest target `[ue][msix][integration]` tag

## §2 实施验证

### 任务 2.1: 跨仓构建同步
- [ ] **Verify**: CppTLM `2026-09-10-cpptlm-stage-1-2-msix` change 已 ship to main
- [ ] **Build**: `cd /workspace/project/UsrLinuxEmu && cd build && make -j4`
- [ ] **Verify**: UE 端 dlopen 重新链接 `libcpptlm_emulator.so`

### 任务 2.2: 验证测试通过
- [ ] **Run**: `cd build && ./bin/cpptlm_tests "[ue][msix][integration]"`
- [ ] **Verify pass**: 测试通过（200ms 内 intr_cb ≥1，payload = 0xDEADBEEF）

## §3 Oracle 复审

### 任务 3.1: 1 次轻量复审
- [ ] **Oracle review**: UE 集成测试质量 + 跨仓端到端验证
- [ ] **Verify**: `ctest -R "ue.*msix"` PASS

## §4 双仓 entry sync

### 任务 4.1: UE entry sync
- [ ] **Modify**: `docs/02_architecture/pcie-endpoint-entry.md §12`
- [ ] **Commit**: `docs(pcie-ep): v0.5.1 stage 1.2 UE MSI-X 集成 ship`

### 任务 4.2: CppTLM 18-doc mirror
- [ ] **Modify**: `docs/soc_arch/architecture/18-pcie-endpoint-entry.md §12`
- [ ] **Commit**: `docs(cpptlm): v0.5.5 mirror UE stage 1.2 UE 集成`

## §5 总计

- **UE 仓 commits**: 3（集成测试 + CMake 注册 + UE entry sync）
- **CppTLM 仓 commits**: 1（18-doc mirror）
- **双仓合计 commits**: 4
- **工期**: 0.5 周
- **Oracle 复审**: 1 次轻量
- **下游**: 5.5.7 gate 部分解锁条件之一（仍需 stage-1-3a ship）
