## 1. Launch Conditions（LC1-LC7 验证）

- [x] 1.1 **LC1**: Stage 1 完成（Tier-1 + Tier-2 commit `b521f29`）— 2026-07-05 达成
- [x] 1.2 **LC2**: 3 区分架构原则定义（[ADR-036](../../../../docs/00_adr/adr-036-three-way-separation.md)）— 已 Accepted
- [x] 1.3 **LC3**: Roadmap §2 定义完整（[stage-2-multi-device.md](../../../../docs/roadmap/stage-2-multi-device.md)）
- [x] 1.4 **LC4**: Worktrees 创建（`stage-2-0-adr-037` + `stage-2-1-tier2-absorption` + `stage-2-2-network`）
- [x] 1.5 **LC5**: 集成 plan 评审（[docs/superpowers/plans/2026-07-05-stage-2-multi-device.md](../../../../docs/superpowers/plans/) Oracle-revised v2）
- [x] 1.6 **LC6**: GO/NO-GO spike (commit `9d90cfe` Stage 2.0 Tier-2 feasibility spike) — GO
- [x] 1.7 **LC7**: Tier-2 baseline regression 73/73 PASS（commit `b521f29`）

## 2. Worktree 集成（按依赖顺序）

- [x] 2.1 Merge `stage-2-0-adr-037` (ADR-038, commit `0b910c9`) → main (commit `062b8bc`)
- [x] 2.2 Merge `stage-2-1-tier2-absorption` (vfio + mm_shim, 2 commits) → main (commit `809c8fd`)
- [x] 2.3 Merge `stage-2-2-network` (11 commits: net+storage+release+CI) → main (commit `fb75ed2`)

## 3. 3-way-separation-multi-device capability（Stage 2.2-2.3）

### 3.1 net_driver 三层架构（per ADR-038）

- [x] 3.1.1 `src/kernel/net/socket.{cpp,h}` — ① 内核环境模拟：socket syscall 兼容层
- [x] 3.1.2 `src/kernel/net/sk_buff.{cpp,h}` — ① 内核环境模拟：sk_buff 用户态实现
- [x] 3.1.3 `plugins/net_driver/drv/net_driver.{cpp,h}` — ② 可移植驱动代码：net_device_ops 子集
- [x] 3.1.4 `plugins/net_driver/sim/` — ③ 硬件模拟：NIC emulator
- [x] 3.1.5 `plugins/net_driver/plugin.cpp` — 加载入口 + VFS 注册
- [x] 3.1.6 `tests/test_socket_skbuff_standalone.cpp` — ① compat 测试
- [x] 3.1.7 `tests/test_net_driver_standalone.cpp` (6 cases / 34 assertions) — ②③ 集成测试
- [x] 3.1.8 `docs/05-advanced/net-3-way-...` — 文档 + ADR-038 cross-ref

### 3.2 storage_driver 三层架构

- [x] 3.2.1 `src/kernel/block/bio_compat.{cpp,h}` — ① 内核环境模拟：bio/request 兼容
- [x] 3.2.2 `plugins/storage_driver/drv/storage_driver.{cpp,h}` — ② 可移植驱动代码
- [x] 3.2.3 `plugins/storage_driver/sim/` — ③ 硬件模拟：disk 模拟器（host 文件 backing）
- [x] 3.2.4 `plugins/storage_driver/drv/plugin.cpp` — 加载入口
- [x] 3.2.5 `tests/test_storage_driver_standalone.cpp` (5 cases / 24 assertions)

### 3.3 G1-G4 边界契约保持

- [x] 3.3.1 test_uvm_drm_lifecycle_standalone PASS (G1)
- [x] 3.3.2 test_drm_gem_standalone PASS (G2)
- [x] 3.3.3 test_drm_prime_standalone PASS (G3)
- [x] 3.3.4 test_drm_ioctl_dispatch_standalone PASS (G4)

## 4. tier2-deferred-absorption capability（Stage 2.1）

### 4.1 vfio 真实 IOTLB invalidation（Tier-2 §3.2 吸收）

- [x] 4.1.1 `src/kernel/iommu/vfio_bridge.{cpp,h}` — vfio 用户态绑定 + opt-in
- [x] 4.1.2 `iommu_flush_iotlb` 在有 root + IOMMU 硬件时触发 `ioctl(VFIO_IOMMU_UNMAP_DMA)`
- [x] 4.1.3 无 root / 无 IOMMU 硬件 graceful fallback（log warning + 走纯用户态路径）

### 4.2 mm_shim 真实 mm 跟踪（Tier-2 §3.3 吸收）

- [x] 4.2.1 `include/kernel/uvm/mm_shim.h` + `src/kernel/uvm/mm_shim.cpp` — PID + VMA list 跟踪
- [x] 4.2.2 集成到 fault_inject / mmu_notifier dispatch 路径
- [x] 4.2.3 注释：完整 mm 替换推迟到 Stage 3

## 5. hotfix-map-queue-ring capability（pre-existing Phase 2.5）

- [x] 5.1 修复 `GpgpuDevice::handleMapQueueRing` segfault (commit `6d090e6`)
- [x] 5.2 Stage 1.4 Tier-2 报告中标注的限制解除（test_stub_handlers_tier2_standalone MAP_QUEUE_RING 现在 PASS）

## 6. ci-docs-audit-hook capability（Phase I CI-2）

- [x] 6.1 `tools/docs-audit.sh` 增强 Stage 2 路径检查 (commit `380f4f7`)
- [x] 6.2 Stage 2 docs 必须 cross-ref ADR-036 + ADR-038
- [x] 6.3 Tier-1 docs 不引入 false positive

## 7. ADR + Documentation

- [x] 7.1 [ADR-038 网络栈 3 区分边界](../../../../docs/00_adr/adr-038-network-stack-three-way-separation.md) (commit `0b910c9`)
- [x] 7.2 Stage 2 design spec (commit `5817910`)
- [x] 7.3 Stage 2 implementation plan (commit `a6f7212`)
- [x] 7.4 Stage 2.0 feasibility spike report (commit `9d90cfe`)

## 8. Release Gate (per docs/superpowers/plans/Stage 2 Release Gate)

- [x] 8.1 ctest 76/76 PASS（was 73/73 Stage 1.4 Tier-2 baseline，+3 net/storage/socket）
- [x] 8.2 G1-G4 边界契约 4/4 PASS（无 regression）
- [x] 8.3 boundary v1.2 + roadmap ✅ 更新 (commit `70b2c95`)
- [x] 8.4 docs-audit --strict PASS
- [x] 8.5 3 区分原则验证通过（GPU + 网络 + 存储三方）

## 9. Archive（per ADR-035 spec-driven workflow）

- [x] 9.1 所有 tasks 完成 (96/96)
- [x] 9.2 `openspec archive stage-2-multi-device`
- [x] 9.3 Specs promote 到 main `openspec/specs/`

## 10. Final Verification

- [x] 10.1 `ctest --test-dir build --output-on-failure` ✅ 76/76 PASS
- [x] 10.2 `tools/docs-audit.sh --strict` ✅ 43/43 PASS, 0 warnings
- [x] 10.3 `git push origin main` ✅ pushed to GitHub (main = `fb75ed2...` merge chain)
- [x] 10.4 worktree cleanup（3 个 worktree 已 merge 到 main，分支保留作为历史）
- [x] 10.5 文档 SSOT 同步（roadmap §2 / SSOT §1.10 / boundary v1.2 / README badges）

## Summary

- **Total commits**: ~20 (across 4 worktree branches, all merged to main)
- **Total tests**: 76/76 PASS (was 73/73)
- **New tests**: 3 (test_socket_skbuff, test_net_driver 6 cases, test_storage_driver 5 cases)
- **New code**: ~1700 lines (kernel compat + driver + sim + tests)
- **New ADRs**: ADR-038
- **Boundary updates**: kfd-portability-boundary.md v1.1 → v1.2
- **Roadmap**: §2 `📋 规划中` → `✅ 已达成`
