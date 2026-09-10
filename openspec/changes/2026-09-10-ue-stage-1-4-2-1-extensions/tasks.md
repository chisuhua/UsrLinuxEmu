# Tasks: ue-stage-1-4-2-1-extensions

> **工期**: 0.5-1 周 | TDD 5 步

## §1 电源管理集成（0.5 周）
- [ ] **写失败测试**: `test_dgpu_power_mgmt_ue.cc::test_pm_d0_d3_aspm`
- [ ] **Verify**: D0/D3 + ASPM 端到端
- [ ] **Commit**: `test(ue): PM 集成 (1.4)`

## §2 P2P + Resizable BAR 集成（0.5 周）
- [ ] **写失败测试**: `test_dgpu_p2p_ue.cc::test_p2p_acs_resizable_bar`
- [ ] **Verify**: P2P + ACS + BAR resize
- [ ] **Commit**: `test(ue): P2P + Resizable BAR 集成 (2.1)`

## §3 双仓 entry sync
- [ ] **UE entry §12** v0.10.1/v0.11.1
- [ ] **CppTLM 18-doc mirror** 同步

## §4 Oracle 复审（1 次轻量）

## §5 总计

- **UE commits**: 4（2 测试 + 2 entry sync）
- **CppTLM commits**: 2（18-doc mirror）
- **工期**: 0.5-1 周
- **Oracle**: 1 次轻量
- **下游**: 5.5.9 真机双轨验证
