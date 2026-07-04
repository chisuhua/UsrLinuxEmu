## 1. Launch Conditions（LC1-LC4 验证）

- [x] 1.1 **LC1**：1.4 Tier-1 KFD portability 已 merge（commit `80f6a44`）—— **2026-07-04 已达成**
- [x] 1.2 **LC2**：`docs/05-advanced/kfd-portability-boundary.md` Tier-1/Tier-2 划分 SSOT 已发布（v1.0）—— **2026-07-04 已达成**
- [x] 1.3 **LC3**：Tier-1 回归测试无 regression —— **2026-07-04 已验证**（commit `2322429` baseline fix 后 68/68 PASS，纠正了 LC3 仅 8/8 的不完整判定）
- [x] 1.4 **LC4**：worktree 创建完成 —— **2026-07-05 已达成**（`stage-1.4-tier2-kfd-integration` 基于 main@`b65acad`）

## 2. Worktree 创建（决策 1）

- [x] 2.1 使用 `superpowers/using-git-worktrees` skill 创建 `stage-1.4-tier2-kfd-integration` worktree（基于 main 分支最新 commit `b65acad`）
- [x] 2.2 验证 worktree 状态：`git branch --show-current` 显示 `stage-1.4-tier2-kfd-integration`，`git status` 干净（建时干净；baseline fix 后改动 → 已 commit `2322429`）
- [x] 2.3 在 worktree 内验证 baseline 构建：`cmake --build build` + `ctest --test-dir build` 68/68 PASS（commit `2322429` baseline fix 后达成，原始 main 存在 pre-existing build break）

## 3. 9 个 STUB_HANDLER 升级（D1 决策：桥接到既有 GpgpuDevice 实现）

### 3.1 `gpu_ioctl_register_mmu_cb`（Tier-2 G2 关键）

- [x] 3.1.1 写失败的测试（红）：`tests/test_register_mmu_cb_runtime_standalone.cpp` 覆盖 happy path（注册成功）+ error path（重复注册返回 `-EALREADY`）
- [x] 3.1.2 跑测试确认红
- [x] 3.1.3 升级 handler：调 `kfd_sim_register_mmu_cb(args)`（桥接到 sim 单 callback 注册表，而非 mmu_interval_notifier_register — 见 boundary §3.3 callback body 延后至 Stage 3+）
- [x] 3.1.4 跑测试确认绿
- [x] 3.1.5 替换 `STUB_HANDLER` 宏为显式函数体 + `// Tier-2 penetrated: [date] - references boundary §3.3` 注释
- [x] 3.1.6 Commit: `feat(handler): gpu_ioctl_register_mmu_cb penetrate to mmu event registry`

### 3.2 `gpu_ioctl_register_firmware_cb`

- [x] 3.2.1 写失败的测试：`tests/test_register_firmware_cb_runtime_standalone.cpp`（5 cases / 14 assertions）
- [x] 3.2.2 跑测试确认红（bridge 未声明 → 编译失败 = RED）
- [x] 3.2.3 升级 handler：调 `kfd_sim_register_firmware_cb(args)` 占位实现（**不实际加载 firmware**，boundary §5.2 显式排除）
- [x] 3.2.4 跑测试确认绿（70/70 PASS，+1 测试目标）
- [x] 3.2.5 替换 STUB_HANDLER 宏 + Tier-2 penetrated 注释
- [x] 3.2.6 Commit: `feat(handler): gpu_ioctl_register_firmware_cb tier-2 penetrated`

### 3.3 `gpu_ioctl_create_va_space` / `gpu_ioctl_destroy_va_space`

- [x] 3.3.1 写失败的测试：`tests/test_create_destroy_va_space_runtime_standalone.cpp`
- [x] 3.3.2 跑测试确认红
- [x] 3.3.3 升级 create_va_space handler：调 `gpgpu_device_->create_va_space(args)` 既有实现
- [x] 3.3.4 升级 destroy_va_space handler：调 `gpgpu_device_->destroy_va_space(args)` 既有实现
- [x] 3.3.5 跑测试确认绿
- [x] 3.3.6 跑 `tests/test_uvm_drm_lifecycle_standalone` 确认 G1 边界契约仍保持
- [x] 3.3.7 替换 STUB_HANDLER 宏 + Tier-2 penetrated 注释
- [x] 3.3.8 Commit: `feat(handler): gpu_ioctl_create/destroy_va_space penetrate to GpgpuDevice`

### 3.4 `gpu_ioctl_register_gpu`

- [x] 3.4.1 写失败的测试：`tests/test_register_gpu_runtime_standalone.cpp`
- [x] 3.4.2 跑测试确认红
- [x] 3.4.3 升级 handler：调 `gpgpu_device_->register_gpu(args)` 一次性实现
- [x] 3.4.4 跑测试确认绿（含 error path：重复注册返回 `-EALREADY`）
- [x] 3.4.5 替换 STUB_HANDLER 宏 + Tier-2 penetrated 注释
- [x] 3.4.6 Commit: `feat(handler): gpu_ioctl_register_gpu tier-2 penetrated`

### 3.5 `gpu_ioctl_create_queue` / `gpu_ioctl_destroy_queue`

- [x] 3.5.1 写失败的测试：`tests/test_create_destroy_queue_runtime_standalone.cpp`
- [x] 3.5.2 跑测试确认红
- [x] 3.5.3 升级 create_queue handler：调 `gpgpu_device_->create_queue(args)` 既有实现（0x40 KFD-compat 扩展）
- [x] 3.5.4 升级 destroy_queue handler：调 `gpgpu_device_->destroy_queue(args)` 既有实现
- [x] 3.5.5 跑测试确认绿
- [x] 3.5.6 替换 STUB_HANDLER 宏 + Tier-2 penetrated 注释
- [x] 3.5.7 Commit: `feat(handler): gpu_ioctl_create/destroy_queue penetrate to GpgpuDevice`

### 3.6 `gpu_ioctl_map_queue_ring`

- [x] 3.6.1 写失败的测试：`tests/test_map_queue_ring_runtime_standalone.cpp`
- [x] 3.6.2 跑测试确认红
- [x] 3.6.3 升级 handler：完成 `mmap` 或 `dma_buf_mmap` 映射
- [x] 3.6.4 验证 args->ring_addr / args->doorbell_offset 填入有效地址
- [x] 3.6.5 跑测试确认绿
- [x] 3.6.6 替换 STUB_HANDLER 宏 + Tier-2 penetrated 注释
- [x] 3.6.7 Commit: `feat(handler): gpu_ioctl_map_queue_ring tier-2 penetrated (mmap)`

### 3.7 `gpu_ioctl_query_queue`

- [x] 3.7.1 写失败的测试：`tests/test_query_queue_runtime_standalone.cpp`
- [x] 3.7.2 跑测试确认红
- [x] 3.7.3 升级 handler：调 `gpgpu_device_->query_queue(args)` 既有实现
- [x] 3.7.4 验证 args 填入 queue 状态（pending / running / completed）
- [x] 3.7.5 跑测试确认绿
- [x] 3.7.6 替换 STUB_HANDLER 宏 + Tier-2 penetrated 注释
- [x] 3.7.7 Commit: `feat(handler): gpu_ioctl_query_queue tier-2 penetrated`

## 4. mmu_notifier callback body 完整化（D2 决策：最小可行 callback）

- [x] 4.1 写失败的测试：`tests/test_mmu_notifier_callback_runtime_standalone.cpp`（覆盖 happy path：munmap → callback → sim 原语触发 + error path：未注册 callback 时不触发）
- [x] 4.2 跑测试确认红
- [x] 4.3 实现 `mmu_invalidate_callback` 函数：在 `src/kernel/iommu/invalidate.cpp` 替换 TODO 为真实 callback
- [x] 4.4 callback 实现调 `sim_pfh_inject_fault(addr, &pfn)` 触发 sim page fault handler
- [x] 4.5 callback 实现调 `sim_pm_migrate_to_system(addr, dst, size)` 触发 sim page migration
- [x] 4.6 跑测试确认绿
- [x] 4.7 跑 `tests/test_uvm_drm_lifecycle_standalone` 确认 G1-G4 边界契约仍保持
- [x] 4.8 替换 `TODO(stage-1.3)` 注释为 `// Tier-2 penetrated: [date] - references boundary §3.3`
- [x] 4.9 跑全量 ctest 确认无 regression
- [x] 4.10 Commit: `feat(uvm): mmu_notifier callback body tier-2 penetrated (sim_pfh + sim_pm)`

## 5. IOTLB flush 真实化（D3 决策：用户态 page table invalidation）

- [x] 5.1 写失败的测试：`tests/test_iommu_invalidate_runtime_standalone.cpp`（覆盖 happy path：unmap → flush → page table invalid + error path：domain 不存在返回 `-EINVAL`）
- [x] 5.2 跑测试确认红
- [x] 5.3 在 `src/kernel/iommu/dma_remap.cpp` 替换 `iommu_flush_iotlb` fprintf stub 为真实实现
- [x] 5.4 实现遍历 `iommu_domain->page_table` 标记对应 iova 范围为 invalid
- [x] 5.5 实现触发 `sim_pfh_inject_fault_with_cause(addr, cause=IOTLB_FLUSH)`（commit `32e012d` 增强的 cause register）
- [x] 5.6 实现多 domain 跨边界处理（限制递归深度，防御性编程）
- [x] 5.7 跑测试确认绿
- [x] 5.8 验证不依赖 host kernel（在用户态环境运行通过，不需 root 权限）
- [x] 5.9 替换 `Stage-1.1 IOTLB flush is a logged stub` 注释为 `// Tier-2 penetrated: [date] - references boundary §3.2`
- [x] 5.10 跑全量 ctest 确认无 regression
- [x] 5.11 Commit: `feat(iommu): iommu_flush_iotlb tier-2 penetrated (user-space page table)`

## 6. 边界契约验证（G1-G4 不破坏）

- [x] 6.1 跑全量 `ctest --test-dir build --output-on-failure` 确认无 regression
- [x] 6.2 重点验证 G1：`tests/test_uvm_drm_lifecycle_standalone` `drm_device` 生命周期测试全绿
- [x] 6.3 重点验证 G2：`tests/test_drm_gem_standalone` BO 引用计数测试全绿
- [x] 6.4 重点验证 G3：`tests/test_drm_prime_standalone` prime 释放顺序测试全绿
- [x] 6.5 重点验证 G4：`tests/test_drm_ioctl_dispatch_standalone` fence 触发时机测试全绿
- [x] 6.6 如果任一 G 契约失败，**优先修复 regression**，暂缓 Tier-2 推进
- [x] 6.7 Commit（如有 regression 修复）: `fix(test): G1-G4 boundary contract regression`

## 7. 文档与 closeout

- [x] 7.1 编写 `docs/05-advanced/tier2-runtime-penetration-report.md`：
  - 9 个 STUB 升级演进记录（每个含 Tier-1 → Tier-2 行为对比 + commit 引用）
  - mmu_notifier callback body 完整化记录
  - IOTLB flush 真实化记录
  - Tier-1 回归测试结果（全绿）
  - G1-G4 边界契约验证结果
  - **诚实记录**任何 Tier-2 范围内的妥协（如 `gpu_ioctl_register_firmware_cb` 仅 callback 占位）
- [x] 7.2 更新 `docs/05-advanced/kfd-portability-boundary.md`：
  - Tier-2 §3.1 状态从 "日志桩" 改为 "Penetrated" + 完成时间戳
  - Tier-2 §3.2 状态从 "logging stub" 改为 "Real Implementation (user-space)"
  - Tier-2 §3.3 状态从 "TODO(stage-1.3)" 改为 "Implemented"
  - 保留 Tier-2 §3.4 多文件 KFD 集成为 "Stage 3+ 延后"
  - 保留 Tier-2 §3.5 完整 kfd_queue.c 为 "随多文件集成延后"
- [x] 7.3 更新 `docs/02_architecture/post-refactor-architecture.md §1.10`：标注 Stage 1 Tier-2 完成（Tier-1 仍 `[x]` + Tier-2 新增 `[x]`）
- [x] 7.4 保留 `docs/roadmap/stage-1-kernel-emu.md` 顶部状态 `🔄 计划中`（Stage 1 整体未达 `✅`，因 Tier-2-D 多文件 KFD 仍延后 Stage 3+）
- [x] 7.5 更新 `README.md` 顶部 badges：新增 `Tier-2` 状态标识
- [x] 7.6 跑 `tools/docs-audit.sh --strict`（pre-commit hook 自动）通过
- [x] 7.7 归档 OpenSpec change：`openspec archive stage-1-4-tier2-kfd-integration`

## 8. Worktree 合并与 Tier-2 closeout

- [x] 8.1 在 worktree 内 final commit（conventional commits 格式：`feat(kfd): stage-1.4 Tier-2 runtime penetration`）
- [x] 8.2 rebase main：`git fetch origin && git rebase origin/main`
- [x] 8.3 提 PR：使用项目 PR 模板（如果有），引用 OpenSpec change + boundary §3
- [x] 8.4 review 后 merge 到 main（squash merge 保留单一 commit 历史）
- [x] 8.5 删除 worktree（merge 后）
- [x] 8.6 Tier-2 全部 checkbox 完成（追踪 plan 文档 §Sub-stage 1.4 Tier-2 全部勾选）
- [x] 8.7 同步 `docs/superpowers/plans/2026-07-04-stage-1.4-tier1-delivery.md` 添加"Tier-2 follow-up"章节引用本 change