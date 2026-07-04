## 0. 启动前置（Launch Conditions LC1-LC4 验证）

- [ ] **0.1** LC1：1.0/1.1/1.2/1.3 四个子阶段全部完成（SSOT §1.10 标注 `[x]` + ctest 63/63 PASS）—— **2026-07-04 已验证**（`docs/02_architecture/post-refactor-architecture.md §1.10` + `ctest --test-dir build --output-on-failure`）
- [ ] **0.2** LC2：1.2 阶段 KFD 单文件 PoC（`kfd_queue.c`）编译通过 —— **2026-07-02 已达成**（commit `c42e60e`，errors=0, warnings=2, 零逻辑修改）
- [ ] **0.3** LC3：回归测试无 regression（用户决策 3）—— **2026-07-04 验证中**（§10 任务执行）
- [ ] **0.4** LC4：worktree 创建完成（用户决策 1，实施 1.4 代码时创建）—— **待 §1 任务执行**

---

## 1. Worktree 创建（用户决策 1）

- [ ] **1.1** 使用 `superpowers/using-git-worktrees` skill 创建 `stage-1.4-kfd-portability` worktree（基于 main 分支最新 commit）
- [ ] **1.2** 验证 worktree 状态：`git branch --show-current` 显示 `stage-1.4-kfd-portability`，`git status` 干净
- [ ] **1.3** 在 worktree 内验证 baseline 构建：`cmake --build build` + `ctest --test-dir build` 63/63 PASS（避免引入新 regression）

---

## 2. KFD 源码准备（5 个核心 .c 拷贝）

- [ ] **2.1** 确认 Linux 6.6/6.12 LTS KFD 源位置：从上游 `git clone https://github.com/torvalds/linux.git` 取 `v6.12` tag（**不进 git，只读参考**）
- [ ] **2.2** 拷贝 5 个核心 .c 到 `plugins/gpu_driver/drv/kfd/`（决策 4 顺序）：
  - `kfd_module.c`（~500 行，模块入口 / 设备探测）
  - `kfd_queue.c`（~800 行，1.2 阶段已有 PoC 基础）
  - `kfd_device.c`（~1500 行，设备生命周期 / topology）
  - `kfd_doorbell.c`（~500 行，Doorbell MMIO 映射）
  - `kfd_process.c`（~2000 行，进程级 KFD 上下文 / DMA 管理）
- [ ] **2.3** 创建 `plugins/gpu_driver/drv/kfd/CMakeLists.txt`（沿用 1.2 阶段 `gpu_kfd` C 库模式，`add_library(gpu_kfd STATIC ...)`）
- [ ] **2.4** 修改 `plugins/gpu_driver/CMakeLists.txt` 已含 `add_subdirectory(kfd)`（1.2 阶段 commit `ca75fc6` 已连入，验证即可）

---

## 3. #include 路径调整（仅这一项修改，逻辑零修改）

> **决策约束**：根据路线图 §1 显式验收指标 #1，KFD 驱动代码**仅** `#include` 路径需调整，**逻辑零修改**。任何逻辑层面的修改（即使是"修复 bug"）都需要在 `kfd-portability-report.md` 中详细说明理由。

- [ ] **3.1** 编写 sed 脚本批量替换 `<linux/...>` → `linux_compat/...`（按 1.2 阶段 PoC 经验）
- [ ] **3.2** 编写 sed 脚本批量替换 `<drm/...>` → `linux_compat/drm/...`
- [ ] **3.3** 验证替换完整性：`grep -rn "#include <linux/" plugins/gpu_driver/drv/kfd/` 零命中
- [ ] **3.4** 验证替换完整性：`grep -rn "#include <drm/" plugins/gpu_driver/drv/kfd/` 零命中
- [ ] **3.5** 第一次编译：`cmake --build build --target gpu_kfd 2>&1 | tee /tmp/kfd-build-1.log`
  - **Expected**：errors = 0（warnings 数量记录）
  - **失败处理**：errors > 0 → 检查 `kfd-build-1.log` → 修复 `#include` 路径 → 重新编译

---

## 4. 移植 kfd_module.c（决策 4 顺序：最先，最小依赖）

- [ ] **4.1** 单独编译：`cmake --build build --target gpu_kfd` errors = 0（**仅** kfd_module.c 编译通过）
- [ ] **4.2** 验证 `kfd_module_init()` / `kfd_module_exit()` 符号导出（`nm build/libgpu_driver/gpu_kfd.a | grep kfd_module`）
- [ ] **4.3** 在 `plugins/gpu_driver/drv/kfd/Makefile.am` 或 CMakeLists.txt 中添加编译单元（如果存在）

---

## 5. 移植 kfd_queue.c（1.2 PoC 基础扩展）

- [ ] **5.1** 在 1.2 阶段 `kfd_queue.c` PoC 基础上扩展（commit `c42e60e`），替换为完整版
- [ ] **5.2** 单独编译：`cmake --build build --target gpu_kfd` errors = 0
- [ ] **5.3** 验证 queue 创建/销毁符号导出（`nm build/libgpu_driver/gpu_kfd.a | grep -E "kfd_queue|create_queue|destroy_queue"`）
- [ ] **5.4** 跑 `test_drm_kfd_handlers_standalone` 全绿（确保 PoC 升级未引入 regression）

---

## 6. 移植 kfd_device.c（依赖 module + queue）

- [ ] **6.1** 编译：`cmake --build build --target gpu_kfd` errors = 0
- [ ] **6.2** 验证 device 拓扑符号导出（`nm build/libgpu_driver/gpu_kfd.a | grep -E "kfd_device|topology"`）
- [ ] **6.3** 验证 IOMMU group 集成（`grep "iommu_" plugins/gpu_driver/drv/kfd/kfd_device.c` 命中数 ≥ 3）
- [ ] **6.4** 如发现 KFD 调用 `iommu_*` API → **触发决策 2**：创建独立 ADR + 独立 OpenSpec change + 一个独立 commit

---

## 7. 移植 kfd_doorbell.c（依赖 device 拓扑）

- [ ] **7.1** 编译：`cmake --build build --target gpu_kfd` errors = 0
- [ ] **7.2** 验证 doorbell MMIO 映射符号导出（`nm build/libgpu_driver/gpu_kfd.a | grep -E "doorbell"`）
- [ ] **7.3** 验证与 1.2 阶段 doorbell offset 计算一致（`grep "doorbell_offset" plugins/gpu_driver/drv/kfd/kfd_doorbell.c`）

---

## 8. 移植 kfd_process.c（最重，依赖前 4 个 + 1.3 mmu_notifier）

- [ ] **8.1** 编译：`cmake --build build --target gpu_kfd` errors = 0（**重点**：此步骤最易触发新错误）
- [ ] **8.2** 验证 process 生命周期符号导出（`nm build/libgpu_driver/gpu_kfd.a | grep -E "kfd_process|process_create|process_destroy"`）
- [ ] **8.3** 验证与 1.3 `mmu_notifier` 集成（`grep "mmu_notifier\|mmu_interval" plugins/gpu_driver/drv/kfd/kfd_process.c` 命中数 ≥ 1）
- [ ] **8.4** 如发现 KFD 调用 `mmu_interval_*` / `mmu_notifier_*` API → **触发决策 2**：创建独立 ADR + 独立 OpenSpec change + 一个独立 commit

---

## 9. 编译调通（errors = 0 总验证）

- [ ] **9.1** 完整编译：`cmake --build build` errors = 0
- [ ] **9.2** warnings 数量记录：`cmake --build build 2>&1 | grep -c "warning:"` 与 1.2 PoC 时 warnings=2 基线对比
- [ ] **9.3** kernel SHARED 库检查：`file build/libkernel.so` 确认 SHARED（遵守 Issue #11）
- [ ] **9.4** 全量测试：`ctest --test-dir build --output-on-failure` 仍保持 63/63 PASS

---

## 10. 5 个 ioctl 端到端测试（决策 3 回归测试 + 决策 6 验证方法）

- [ ] **10.1** 编写 `tests/test_kfd_portability_standalone.cpp`（Catch2 风格）：
  - 5 个 ioctl 每个跑 happy path（5 个测试 case）
  - 至少 1 个 error path（合计 ≥10 个测试 case）
  - 使用 `GPU_IOCTL_GET_PROCESS_APERTURE` / `GPU_IOCTL_CREATE_QUEUE`（0x40 KFD-compat 扩展） / `GPU_IOCTL_UPDATE_QUEUE` / `GPU_IOCTL_MAP_MEMORY` / `GPU_IOCTL_UNMAP_MEMORY`
- [ ] **10.2** 注册测试到 `tests/CMakeLists.txt`
- [ ] **10.3** 编译 + 跑：`cmake --build build --target test_kfd_portability_standalone` + `./build/bin/test_kfd_portability_standalone`
- [ ] **10.4** 决策 3 回归测试三件套（**LC3 验证**）：
  - `test_drm_kfd_handlers_standalone` 全绿
  - `test_uvm_drm_lifecycle_standalone`（G1-G4 边界契约）全绿
  - `test_kfd_queue_standalone` 或对应 `kfd_queue.c` PoC 全绿
- [ ] **10.5** ctest 全量：`ctest --test-dir build --output-on-failure` 应保持 63+N/63+N PASS（N = 新增测试数）

---

## 11. 文档与 closeout

- [ ] **11.1** 编写 `docs/05-advanced/kfd-portability-report.md`（~150 行）：
  - 移植概览（5 个 .c 文件 + 行数 + 调整的 `#include` 数量）
  - 编译结果（errors = 0，warnings 数量与基线对比）
  - 5 个 ioctl 验证结果（happy path + error path 通过情况）
  - 错误码一致性（参考 iommu-error-semantics.md / drm-error-semantics.md / uvm-error-semantics.md + 新增 `kfd-error-semantics.md`）
  - 决策点（HAL ops 添加情况 + ADR 引用）
  - 后续改进建议（SVM 完整路径作为 follow-up）
- [ ] **11.2** 编写 `docs/05-advanced/kfd-error-semantics.md`（**新增**，KFD 路径错误码对照表）
- [ ] **11.3** 更新 SSOT：`docs/02_architecture/post-refactor-architecture.md §1.10` 标注 Stage 1 完成（1.4 [x]）
- [ ] **11.4** 更新路线图：
  - `docs/roadmap/stage-1-kernel-emu.md` 顶部状态从 🔄 改为 ✅
  - `docs/roadmap/README.md` 阶段 1 行状态从 🔄 计划中 改为 ✅ 已达成
- [ ] **11.5** 更新 README：`README.md` 顶部 badges 同步更新
- [ ] **11.6** 跑 `tools/docs-audit.sh --strict`（pre-commit hook 自动）
- [ ] **11.7** 归档 OpenSpec change：`openspec archive 2026-07-04-stage-1-4-kfd-portability`

---

## 12. Worktree 合并与 Stage 1 closeout

- [ ] **12.1** 在 worktree 内 final commit（conventional commits 格式：`feat(kfd): stage-1.4 KFD 5-file integration`）
- [ ] **12.2** rebase main：`git fetch origin && git rebase origin/main`
- [ ] **12.3** 提 PR：使用项目 PR 模板（如果有），引用 OpenSpec change
- [ ] **12.4** review 后 merge 到 main（squash merge 保留单一 commit 历史）
- [ ] **12.5** 删除 worktree（merge 后）
- [ ] **12.6** Stage 1 全部 checkbox 完成（追踪 plan 文档 §Sub-stage 1.4 全部勾选）