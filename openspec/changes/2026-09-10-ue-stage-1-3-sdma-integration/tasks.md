# Tasks: ue-stage-1-3-sdma-integration

> **工期**: 0.5-1 周 | TDD 5 步

## §1 阶段 1.3a UE 集成（Ring Buffer）
- [ ] **写失败测试**: `test_dgpu_sdma_ring_buffer_ue.cc::test_ring_buffer_4_sizes`
- [ ] **Verify**: 4 档 cfg.ring_size + BAR1+0x10010000 Doorbell + SG≥8 端到端
- [ ] **Commit**: `test(ue): SDMA Ring Buffer integration (1.3a)`

## §2 阶段 1.3b UE 集成（D2D）
- [ ] **写失败测试**: `test_dgpu_d2d_noc_ue.cc::test_d2d_payload_host_out_zero`
- [ ] **Verify**: ≥100 GB/s + host_out=0
- [ ] **Commit**: `test(ue): D2D NoC integration (1.3b)`

## §3 阶段 1.3c UE 集成（dma_translate，修复 #2）
- [ ] **写失败测试**: `test_dgpu_dma_translate_ue.cc::test_identity_iommu_dual_mode`
- [ ] **Verify**: identity pa=iova + IOMMU 负 errno error_cb
- [ ] **Commit**: `test(ue): dma_translate 修复 #2 integration (1.3c)`

## §4 阶段 1.3d UE 集成（完成通知）
- [ ] **写失败测试**: `test_dgpu_sdma_completion_ue.cc::test_fence_msix_200ms`
- [ ] **Verify**: Fence + MSI-X 接线 + 200ms intr_cb
- [ ] **Commit**: `test(ue): SDMA completion integration (1.3d)`

## §5 双仓 entry sync
- [ ] **UE entry §12** v0.6/v0.7/v0.8/v0.9
- [ ] **CppTLM 18-doc mirror** v0.6-v0.9

## §6 Oracle 复审（1 次轻量）

> **Oracle 复审位置（Metis M6 修订 2026-09-10）**：实施 commit 后、docs mirror commit **前**进行。复审发现问题需追加 commit 而非 amend docs。

- [ ] 跨仓集成测试通过

## §7 总计

- **UE commits**: 8（4 测试 + 4 entry sync）
- **CppTLM commits**: 4（18-doc mirror）
- **双仓合计**: 12
- **工期**: 0.5-1 周
- **Oracle**: 1 次轻量
- **下游**: 5.5.8 阶段 3 gate（§3 ship 后）+ 5.5.7 gate 完整解锁（§1 ship 后）
