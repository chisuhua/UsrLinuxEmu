## 0. 启动前置（Launch Conditions LC1-LC3 验证）

- [x] **0.1** LC1：验证 1.2/1.3 边界契约 G1-G4 已锁 + `tests/test_uvm_drm_lifecycle_standalone` 1.2 骨架存在 —— **2026-07-02 已达成**（在 stage-1-2-drm-subset/tasks.md §5）
- [x] **0.2** LC2：内部 PoC 先完成（userfaultfd + mmap 共享触发场景，验证 ① 内核环境模拟层 + ③ 硬件模拟层 page fault 链路通畅）—— **2026-07-03 已达成**（`tests/poc/test_userfaultfd_poc.cpp`，2 tests / 14 assertions / 53/53 全绿）
- [x] **0.3** LC3：1.2 测试全绿（52/52）—— **2026-07-02 已达成**（`ctest --output-on-failure`）

---

## 1. 内部 PoC（userfaultfd + mmap 共享触发场景，Decision 1）✅ DONE 2026-07-03

- [x] **1.1** 编写 PoC 测试 `tests/poc/test_userfaultfd_poc.cpp` —— userfaultfd 监听 mmap 共享 region + 专属 handler 线程 + 触发 page fault（TDD RED→GREEN→REFACTOR，2 test cases / 14 assertions）
- [x] **1.2** 验证 mmu_notifier notify 回调被调用（`std::atomic<int> notify_count` 从 0→1）
- [x] **1.3** 验证 ③ sim 端 SimPageFaultHandler 接收到通知（`last_page_fault_addr` 记录正确 page-aligned 地址）
- [x] **1.4** PoC 验证通过：不需要调整 Decision 5（zone_device 设计）或 Decision 7（fault 注入路径）—— userfaultfd + 专属线程方案可直接作为 1.3 框架基础

---

## 2. linux_compat/{mmu_notifier,hmm}.h 头文件扩展（Decision 2：mmu_interval_notifier 替代 hmm_mirror）

- [ ] **2.1** 创建 `include/linux_compat/mmu_notifier.h` —— `struct mmu_notifier` + `mmu_notifier_ops`（invalidate_range_start / invalidate_range_end / release）+ `mmu_notifier_register` / `mmu_notifier_unregister` 函数签名
- [ ] **2.2** 创建 `include/linux_compat/hmm.h` —— `struct hmm_range`（7 字段）+ `struct mmu_interval_notifier` + `struct mmu_interval_notifier_ops.invalidate` + `mmu_interval_read_begin/retry/set_seq` + HMM PFN flag 宏（64-bit 编码）+ `hmm_range_fault` 函数签名
- [ ] **2.3** 验证：`git grep "hmm_mirror" include/linux_compat/hmm.h` 零命中（**关键修正**：不声明 `hmm_mirror`）

---

## 3. src/kernel/uvm/ 实现（按 design.md Decisions 1-7）

- [ ] **3.1** 实现 `src/kernel/uvm/mmu_notifier.cpp` —— mmu_notifier register/unregister + invalidate_range_start/end 派发
- [ ] **3.2** 实现 `src/kernel/uvm/hmm_range.cpp` —— `hmm_range_fault()` 实现（range walk + PFN table allocation + sequence number 协议）
- [ ] **3.3** 实现 `src/kernel/uvm/migrate.cpp` —— `migrate_to_ram()` / `migrate_to_dev()` + page state 流转
- [ ] **3.4** 实现 `src/kernel/uvm/fault_inject.cpp` —— user-space mmap → page fault → mmu_notifier 通知 device driver 注入路径
- [ ] **3.5** 实现 `src/kernel/uvm/zone_device.cpp` —— spm vma + page state machine 最简实现
- [ ] **3.6** 实现 `src/kernel/uvm/page_state_machine.cpp` —— `PAGE_STATE_CPU` / `PAGE_STATE_GPU` / `PAGE_STATE_MIGRATING` 三态机

---

## 4. plugins/gpu_driver/sim/ 硬件模拟层（Decision 3 ③ 区域）

- [ ] **4.1** 实现 `plugins/gpu_driver/sim/page_fault_handler.cpp` —— 接收 ① 的 fault 通知 + 触发 page state 流转
- [ ] **4.2** 实现 `plugins/gpu_driver/sim/page_migration.cpp` —— 模拟 device memory ↔ system memory page migration

---

## 5. plugins/gpu_driver/uvm/ 可移植驱动层（**条件性**，按路线图 §1.3 ②）

- [ ] **5.1** **决策点**：1.3 PoC 完成后判断是否需要创建 `plugins/gpu_driver/uvm/svm_ioctl.cpp`
  - 若 1.3 PoC 验证 KFD uvm 子模块需要 driver 侧 svm_ioctl handler → 创建
  - 否则延后到 1.4 集成验证（与 `kfd_svm.c` 一并创建）
- [ ] **5.2** 若创建：`plugins/gpu_driver/uvm/svm_ioctl.cpp` 实现 SVM ioctl 路径
- [ ] **5.3** 若创建：`plugins/gpu_driver/CMakeLists.txt` 添加 `add_subdirectory(uvm)`

---

## 6. 1.2/1.3 边界契约 G1-G4 完整化（Decision 3，承接 stage-1-2-drm-subset）

- [ ] **6.1** 完整化 `tests/test_uvm_drm_lifecycle_standalone`（1.2 骨架 → 1.3 完整）—— 验证 G1 `drm_device` outlive 所有 mmu_interval_notifier
- [ ] **6.2** 验证 G2：`mmu_interval_notifier` + `hmm_range_fault` API 签名与 Linux 6.12 ABI 一致（用 KFD 编译路径走 hmm_queue 触发）
- [ ] **6.3** 验证 G3：design.md "Decision D3: 1.2/1.3 边界契约" 章节明确列出 4 项接口契约（已存在，本任务验证）
- [ ] **6.4** 验证 G4：1.3 不预先实现 1.4 完整 KFD SVM 集成（`git grep "kfd_svm.c"` 在 1.3 commits 中无实现）

---

## 7. HAL 条件性扩展（Decision 4，承接 stage-1-2 Oracle D4）

- [ ] **7.1** 创建 `openspec/changes/stage-1-3-uvm-hmm/specs/hal-uvm-ops-audit.md` —— 记录 0 ops 决策
- [ ] **7.2** 验证 `git diff plugins/gpu_driver/hal/include/hal_ops.h` 零变更（HAL guardrail）
- [ ] **7.3** 若 1.4 PoC 发现 KFD 实际调用 `hal_uvm_*` op → 走 ADR 流程（**本次 0 ops**）

---

## 8. errno 错误码一致性（盲点 3）

- [ ] **8.1** 在 `test_mmu_notifier_standalone` 加 errno mapping test（覆盖 `-ENOMEM` / `-EBUSY` / `-EFAULT` / `-EINVAL` / `-ENOSPC`）
- [ ] **8.2** 错误码对照表写入 `docs/05-advanced/uvm-error-semantics.md`（**新增**，类比 1.1/1.2）

---

## 9. HMM 兼容矩阵文档（盲点 5）

- [ ] **9.1** 创建 `docs/05-advanced/hmm-compat-matrix.md` —— Linux 6.6 ↔ 6.12 LTS HMM/mmu_notifier 差异表：
  - struct layout 变化（已知：`struct hmm_mirror` 在 6.x 已移除）
  - 函数签名变化（已知：`mmu_interval_notifier_insert` 6.6 ↔ 6.12 微调）
  - 新增 required ops（已知：`mmu_interval_notifier_ops.invalidate` 回调）
- [ ] **9.2** 标注 UsrLinuxEmu 模拟策略（目标 LTS = 6.12，6.6 兼容矩阵待 1.4 集成时补）

---

## 10. 测试交付（路线图 §1.3 验收 + Oracle 启动条件）

- [ ] **10.1** 创建 `tests/test_mmu_notifier_standalone.cpp` —— mmu_notifier register/unregister + invalidate_range 派发
- [ ] **10.2** 创建 `tests/test_hmm_range_standalone.cpp` —— hmm_range_fault + sequence number 协议 + PFN table
- [ ] **10.3** 创建 `tests/test_svm_ioctl_standalone.cpp` —— SVM ioctl 路径通过 mmu_notifier + hmm_range
- [ ] **10.4** 完整化 `tests/test_uvm_drm_lifecycle_standalone`（1.2 骨架 → 1.3 完整，G1 全验证）
- [ ] **10.5** 4 个测试均含至少 1 个 SPEC requirement 引用注释（便于 spec ↔ test 追溯）
- [ ] **10.6** `ctest --output-on-failure` 全绿（52 既有 + 3 新增 = 55，**外加 1 个完整化**）

---

## 11. CMake 集成

- [ ] **11.1** `src/CMakeLists.txt`：添加 `src/kernel/uvm/*.cpp` 到 `add_library(kernel SHARED ...)`（保持 kernel SHARED，遵守 Issue #11）
- [ ] **11.2** `plugins/gpu_driver/sim/CMakeLists.txt`：添加 `page_fault_handler.cpp` + `page_migration.cpp`
- [ ] **11.3** `plugins/gpu_driver/CMakeLists.txt`：**仅当 5.1 决策为创建**时，添加 `add_subdirectory(uvm)`
- [ ] **11.4** `tests/CMakeLists.txt`：注册 3 个新 `test_*_standalone` 目标 + 1 个完整化（test_uvm_drm_lifecycle_standalone）

---

## 12. 文档交付

- [ ] **12.1** `docs/05-advanced/uvm-error-semantics.md`（**新增**，盲点 3）
- [ ] **12.2** `docs/05-advanced/hmm-compat-matrix.md`（**新增**，盲点 5）
- [ ] **12.3** `docs/02_architecture/post-refactor-architecture.md §1.10` 标注 1.3 完成（**新增 1.3 条目**，类比 1.2 DRM Subset Layer）
- [ ] **12.4** 在 SSOT 附录 A 同步 `linux_compat/{mmu_notifier,hmm}.h` 头文件清单

---

## 13. C4 + HAL 不变性验证

- [ ] **13.1** 创建 `openspec/changes/stage-1-3-uvm-hmm/specs/hal-uvm-ops-audit.md` —— 记录 0 ops 添加决策
- [ ] **13.2** 验证 `git diff plugins/gpu_driver/hal/` 零变更（HAL guardrail）
- [ ] **13.3** 验证 `git diff include/linux_compat/types.h` 仅含 1.3 标记注释，无新增类型定义

---

## 14. 真实 KFD SVM 单文件 PoC（Decision 1，承接 stage-1-2 kfd_queue.c 模式）

- [ ] **14.1** 从 Linux 6.12 LTS `drivers/gpu/drm/amd/amdkfd/` 取首个 SVM 相关文件（推荐 `kfd_svm.c` 或 `kfd_migrate.c`，覆盖 `hmm_range_fault` 路径）
- [ ] **14.2** 拷贝到 `plugins/gpu_driver/drv/kfd/`，仅调整 `#include` 路径
- [ ] **14.3** 编译验证：errors=0, warnings≤3
- [ ] **14.4** 记录结果到 `docs/05-advanced/kfd-portability-progress.md` §KFD SVM 部分
- [ ] **14.5** Artifacts 存储到 `openspec/evidence/kfd-svm-poc-2026-07-XX/{kfd_svm.o,build.log}`

> **注**：单文件 PoC 验证框架；多文件集成验证（5 个 KFD ioctl + kfd_svm.c 完整路径）留到 Stage 1.4。

---

## 15. 子阶段验收与归档

- [ ] **15.1** 跑路线图 §1.3 验收 5 条：
  - [ ] **15.1.1** 真实 KFD SVM 驱动 `.c` 文件拷贝编译通过（**Linux 6.12 LTS**，warning≤3）
  - [ ] **15.1.2** 仅 `#include` 路径调整，逻辑零修改
  - [ ] **15.1.3** mmap 共享能触发模拟 page fault
  - [ ] **15.1.4** KFD 的 **SVM ioctl** 能跑通
  - [ ] **15.1.5** HMM range fault → migrate → 通知全链路通畅
  - [ ] **15.1.6** mmu_notifier 在用户态 munmap 时正确触发 invalidation
  - [ ] **15.1.7** 测试 3 个 standalone + 1 个完整化全绿
- [ ] **15.2** 归档本 OpenSpec change：`openspec archive stage-1-3-uvm-hmm`
- [ ] **15.3** 更新追踪 plan：`docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md §Sub-stage 1.3` 全部 checkbox 勾选 + Status Snapshot 标记 ✅
- [ ] **15.4** 触发下一子阶段 1.4：
  ```bash
  openspec new change "stage-1-4-kfd-portability" \
      --template docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md \
      --reference docs/roadmap/stage-1-kernel-emu.md#子阶段-14--集成验证
  ```
