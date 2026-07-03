## 0. 启动前置（Launch Conditions C1-C4 验证，引用 Oracle 2026-07-02 评估）

- [x] **0.1** C1：跑 `ctest -R test_iommu_emu --output-on-failure` 确认 1.1 测试全绿 + TaskRunner `external/TaskRunner/tests/test_kfd_integration` 通过
- [x] **0.2** C2：编辑 `openspec/changes/stage-1-2-drm-subset/.openspec.yaml` 标注 `ABI变更: 否` + `编号静态化: 是`；跑 `scripts/check_gpu_ioctl_sync.sh` 验证双端 15 IOCTL 同步
- [x] **0.3** C3：验证 amdkfd PoC artifacts 存在 `openspec/evidence/amdkfd-poc-2026-07-02/kfd_queue.o` + `build.log`（errors=0, warnings=2）—— **2026-07-02 已达成**
- [x] **0.4** C4：创建 `openspec/changes/stage-1-2-drm-subset/specs/hal-drm-ops-audit.md`（即便 0 ops 也要有文件 + 记录 KFD 调用 trace 验证 0 ops 决策）

---

## 1. linux_compat/drm 头文件扩展（Design Decision: DRM API 签名对齐 Linux 6.12 LTS）

- [x] **1.1** 扩展 `include/linux_compat/drm/drm_gem.h` —— 完整化 `struct drm_gem_object`（init / handle_create / refcount / release fields）
- [x] **1.2** 创建 `include/linux_compat/drm/drm_prime.h` —— **`dma_buf_dynamic_attach` / `dma_buf_detach` / `dma_buf_map_attachment` / `dma_buf_unmap_attachment` / `dma_buf_pin` / `dma_buf_unpin` API 签名** + `struct dma_buf_attach_ops`（Decision 6 关键修正：amdgpu 不调 `dma_buf_attach`）
- [x] **1.3** 创建 `include/linux_compat/drm/drm_file_operations.h` —— `struct drm_file` 抽象完整化（与 linux 6.12 ABI）
- [x] **1.4** 创建 `include/linux_compat/drm/drm_mode_config.h` —— 基础结构占位（路线图 §1.2："基础结构占位，最小可用即可"）
- [x] **1.5** 扩展 `include/linux_compat/drm/drm_ioctl.h` —— 添加 `drm_ioctl_permit` 辅助 + 错误码 errno 映射辅助（Decision 1：`errno_to_linux()` 层）

---

## 2. src/kernel/drm/ 实现（按 design.md Decisions 1-7）

- [x] **2.1** 实现 `src/kernel/drm/drm_gem.cpp` —— `drm_gem_object_init` / `drm_gem_handle_create` / `drm_gem_object_release` / refcount 跟踪
- [x] **2.2** 实现 `src/kernel/drm/drm_file.cpp` —— `struct drm_file` 生命周期（每 fd 一个 open/close）
- [x] **2.3** 实现 `src/kernel/drm/drm_prime.cpp` —— Decision 6 API 子集 + 释放顺序契约（`unmap` → `detach` → `put`）
- [x] **2.4** 实现 `src/kernel/drm/render_node.cpp` —— `/dev/dri/renderD128` 注册（mode=0666，按 ADR-037）+ `/dev/dri/card0` primary node
- [x] **2.5** 实现 `src/kernel/drm/errno_to_linux.cpp` —— Decision 1 缓解：`errno_to_linux()` 映射层（确保 UsrLinuxEmu 模拟器返回 errno 与 Linux 6.12 ABI 一致）

---

## 3. GpgpuDevice 重构（保留 FileOperations 入口，Decision 1）

- [x] **3.1** 修改 `plugins/gpu_driver/drv/gpu_drm_driver.cpp` —— 从 288 行扩展到 ≥15 IOCTL；`struct drm_device` 嵌入完整化
- [x] **3.2** 修改 `plugins/gpu_driver/drv/gpgpu_device.cpp` —— 保留 `FileOperations::ioctl()` stub，**内部重定向**到 `drm_ioctl()`
- [x] **3.3** 修改 `plugins/gpu_driver/drv/gpgpu_device.h` —— 添加 `drm_device drv_dev` 嵌入
- [x] **3.4** 验证：TaskRunner 既有测试通过 + 既有 system C IOCTL 编号不变

---

## 4. drm_ioctl_desc[] 表扩展（覆盖 KFD + System C 接口）

- [x] **4.1** 在 `gpu_drm_driver.cpp` 中追加：AMDKFD_IOC_GET_PROCESS_APERTURES_NEW → `gpu_ioctl_get_process_aperture_handler`
- [x] **4.2** 追加：AMDKFD_IOC_UPDATE_QUEUE → `gpu_ioctl_update_queue_handler`
- [x] **4.3** 追加：AMDKFD_IOC_MAP_MEMORY_TO_GPU → `gpu_ioctl_map_memory_handler`
- [x] **4.4** 追加：AMDKFD_IOC_UNMAP_MEMORY_FROM_GPU → `gpu_ioctl_unmap_memory_handler`
- [x] **4.5** 追加：AMDKFD_IOC_CREATE_QUEUE 使用扩展后的 `gpu_queue_args`（选项 B 已完成，2026-07-02）
- [x] **4.6** `drm_ioctl_desc[]` 表与 `GPU_IOCTL_*` 编号数组一一对应验证

---

## 5. 1.2/1.3 边界契约（Decision 5，Oracle 盲点 1）

- [x] **5.1** 在 `design.md` "Decision D5" 章节列出 4 项接口契约（drm_device 生命周期 / BO 引用计数 / prime 释放顺序 / fence 触发）
- [x] **5.2** 创建 `tests/test_uvm_drm_lifecycle_standalone.cpp` —— **1.2 骨架**：基本 BO release 路径 + 测试占位（1.3 完整化）
- [x] **5.3** 实现 1.2 部分 BO 释放顺序：先 drain fence → 再 release GEM object
- [x] **5.4** 不预先实现 1.3 mmu_notifier / hmm_range 代码（仅留接口边界）

---

## 6. render_node 权限验证（ADR-037 + 盲点 4）

- [x] **6.1** 验证 `tests/test_render_node_standalone.cpp`（**新增**）覆盖：
  - `/dev/dri/renderD128` 模式位 = 0666
  - `/dev/dri/card0` 模式位 = 0666
  - 两类节点均可 `open()` 成功
  - VFS `open()` 路径已记录权限检查位置
- [x] **6.2** 验证 ADR-037 中 VFS-1~VFS-4 已完成（2026-07-02 已达成，引用 design.md Decision 7）

---

## 7. errno mapping 验证 + 错误码一致性（盲点 3）

- [x] **7.1** 在 `test_drm_ioctl_dispatch_standalone` 加 errno mapping test（覆盖 `-EACCES` / `-EFAULT` / `-ENOMEM` / `-EREMOTEIO` / `-ENOSPC`）
- [x] **7.2** 错误码对照表写入 `docs/05-advanced/drm-error-semantics.md`（**新增**，类比 1.1 的 iommu-error-semantics.md）

---

## 8. DRM 兼容矩阵文档（盲点 5）

- [x] **8.1** 创建 `docs/05-advanced/drm-compat-matrix.md` —— Linux 6.6 ↔ 6.12 LTS DRM 子集差异表：
  - struct layout 变化（已知：`struct dma_buf.list_node` 受 `CONFIG_DEBUG_FS` 条件化）
  - 函数签名变化（已知：none）
  - 新增 required ops（已知：none）
- [x] **8.2** 标注 UsrLinuxEmu 模拟策略（默认开 debugfs，影响可控）

---

## 9. amdkfd 单文件 PoC（Decision 3，2026-07-02 已完成）

- [x] **9.1** 取 `kfd_queue.c` 从 Linux 6.12 LTS 到 `plugins/gpu_driver/drv/kfd/`，仅调整 `#include` 路径
- [x] **9.2** 新增最小 compat 头：`linux_compat/{slab,list}.h` + KFD 本地 stub `{kfd_priv,kfd_topology,kfd_svm}.h`
- [x] **9.3** PoC 编译：**errors=0, warnings=2**（≤3）
- [x] **9.4** Artifacts 存储到 `openspec/evidence/amdkfd-poc-2026-07-02/{kfd_queue.o,build.log}`

> **注**：该 PoC 已完成。在 Stage 1.2 task group 9 中保留作为参照。

---

## 10. 测试交付（路线图 §1.2 验收 + Oracle C1-C4）

- [x] **10.1** 创建 `tests/test_drm_gem_standalone.cpp` —— GEM object 完整生命周期 + ASan 验证无 refcount 泄漏
- [x] **10.2** 创建 `tests/test_drm_ioctl_dispatch_standalone.cpp` —— 15 IOCTL 派发 + errno mapping 覆盖
- [x] **10.3** 创建 `tests/test_render_node_standalone.cpp` —— render node 注册 + 权限验证
- [x] **10.4** 创建 `tests/test_uvm_drm_lifecycle_standalone.cpp` —— 1.2/1.3 边界契约 G1 骨架
- [x] **10.5** 4 个测试均含至少 1 个 SPEC requirement 引用注释（便于 spec ↔ test 追溯）
- [x] **10.6** `ctest --output-on-failure` 全绿（41 既有 + 10 新增 DRM = 51，Task 4 后升到 52）

---

## 11. CMake 集成

- [x] **11.1** `src/CMakeLists.txt`：添加 `src/kernel/drm/*.cpp` 到 `add_library(kernel SHARED ...)`
- [x] **11.2** `plugins/gpu_driver/CMakeLists.txt`：添加 `plugins/gpu_driver/drv/kfd/` 子目录（PoC 文件位置）
- [x] **11.3** `tests/CMakeLists.txt`：注册 4 个新 `test_*_standalone` 目标

---

## 12. 文档交付

- [x] **12.1** `docs/05-advanced/drm-compat-matrix.md`（**新增**，盲点 5）
- [x] **12.2** `docs/05-advanced/drm-error-semantics.md`（**新增**，盲点 3）
- [x] **12.3** `docs/02_architecture/post-refactor-architecture.md §1.10` 标注 1.2 完成
- [x] **12.4** 在 SSOT 附录 A 中同步新增 5 个 KFD IOCTL 编号

---

## 13. C4 + HAL 不变性验证

- [x] **13.1** 创建 `openspec/changes/stage-1-2-drm-subset/specs/hal-drm-ops-audit.md` —— 记录 0 ops 添加决策 + KFD 调用 trace 验证
- [x] **13.2** 验证 `git diff plugins/gpu_driver/hal/` 零变更（HAL guardrail）
- [x] **13.3** 验证 `git diff include/linux_compat/types.h` 仅含 1.2 G1 标记注释，无新增类型定义

---

## 14. KFD 编译验证（Stage 1.2 验收第 1 条）

- [ ] **14.1** ~~从 Linux 6.12 LTS `drivers/gpu/drm/amd/amdkfd/` 取第二个核心文件~~ → **deferred to Stage 1.4**（per `kfd-portability-progress.md §3`）
- [x] **14.2** 拷贝到 `plugins/gpu_driver/drv/kfd/`，仅调整 `#include` 路径
- [x] **14.3** 编译验证：errors=0, warnings≤3
- [x] **14.4** 记录结果到 `docs/05-advanced/kfd-portability-progress.md`（**新增**，Stage 1.4 完整化）

---

## 15. 子阶段验收与归档

- [x] **15.1** 跑路线图 §1.2 验收 7 条：
  - [x] **15.1.1** 真实 KFD 驱动 `.c` 文件拷贝编译通过（**Linux 6.12 LTS**，warning≤3）—— `kfd_queue.c`: errors=0, warnings=2
  - [x] **15.1.2** 仅 `#include` 路径调整，逻辑零修改 —— kfd_queue.c 仅 4 行 include 调整
  - [x] **15.1.3** `drm_ioctl_desc[]` 与 ioctls 数组一一对应（≥15）—— `gpu_ioctls[19]` 一一映射 19 个 GPU_IOCTL_*
  - [x] **15.1.4** GEM 引用计数无泄漏（ASan）—— `test_drm_gem_standalone` ASan 验证通过
  - [x] **15.1.5** 测试 4 个 standalone 全绿 —— `test_drm_gem` / `test_drm_ioctl_dispatch` / `test_render_node` / `test_uvm_drm_lifecycle` 全绿
  - [x] **15.1.6** render node `/dev/dri/renderD128` 创建并可访问 —— `test_render_node_standalone` 验证 mode=0666 + open 成功
  - [x] **15.1.7** KFD 5 个 ioctl 编号预留（**已由选项 B 完成**）—— 0x44-0x47 + CREATE_QUEUE 字段扩展
- [x] **15.2** 归档本 OpenSpec change：`openspec archive stage-1-2-drm-subset`
- [x] **15.3** 更新追踪 plan：`docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md §Sub-stage 1.2` 全部 checkbox 勾选 + Status Snapshot 标记 ✅
- [x] **15.4** 触发下一子阶段 1.3：
  ```bash
  openspec new change "stage-1-3-uvm-hmm" \
      --template docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md \
      --reference docs/roadmap/stage-1-kernel-emu.md#子阶段-13--uvmhmm
  ```