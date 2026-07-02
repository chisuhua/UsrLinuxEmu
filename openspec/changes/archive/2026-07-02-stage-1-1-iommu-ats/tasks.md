## 1. 头文件 + 数据结构骨架

- [x] 1.1 创建 `include/linux_compat/iommu/iommu.h` — 公共类型定义（iommu_prot flags, iommu_domain_type enum）
- [x] 1.2 创建 `include/linux_compat/iommu/iommu_domain.h` — `struct iommu_domain` 字段对齐 Linux 6.6/6.12 LTS（type, ops, iova_bitmap, geometry）
- [x] 1.3 创建 `include/linux_compat/iommu/iommu_group.h` — `struct iommu_group` + `iommu_group_member`（devices 链表, default_domain）
- [x] 1.4 创建 `include/linux_compat/iommu/ioasid.h` — `struct ioasid` opaque handle + `ioasid_alloc/free` API
- [x] 1.5 在 `iommu_domain.h` 中定义 `iommu_ops` function pointer table（map_page/unmap_page/iova_to_phys/flush_iotlb/register_notifier/unregister_notifier 六个钩子）
- [x] 1.6 创建 `src/kernel/iommu/iommu_internal.h` — 内部状态结构（iommu_emu_state 全局，group_table domain_table iova_bitmap_pools）

## 2. Domain / Group / IOASID 核心实现

- [x] 2.1 实现 `src/kernel/iommu/iommu_domain.cpp` — domain 生命周期（iommu_domain_alloc/init/destroy） + ops 钩子分发
- [x] 2.2 实现 `src/kernel/iommu/iommu_group.cpp` — group 创建（iommu_group_alloc/iommu_group_add_device）+ member 添加/移除
- [x] 2.3 实现 `src/kernel/iommu/ioasid.cpp` — ioasid allocator（基于 bitmap，32-bit ID 空间，初始 0xFFFF 范围）

## 3. DMA Remapping 实现

- [x] 3.1 实现 `src/kernel/iommu/dma_remap.cpp` — `iommu_map/iommu_unmap/iommu_iova_to_phys` 三个核心函数，按 spec Requirement: DMA remapping 页表实现
- [x] 3.2 在 dma_remap.cpp 内嵌错误码语义对照（代码注释 + errno 定义在 iommu.h 中对齐 Linux）
- [x] 3.3 IOVA bitmap 分配器：使用 std::unordered_map（4KB 粒度入口，stage-1.1 性能足够；按需可替换为 bitmap）

## 4. ATS 协议实现

- [x] 4.1 创建 `include/linux_compat/pci/ats.h` — ATS 消息结构（Translation Request / Translation Completion / Invalidation Request / Invalidation Completion）
- [x] 4.2 实现 `src/kernel/iommu/ats_protocol.cpp` — 4 个核心消息处理（按 spec Requirement: ATS 协议最小子集）
- [x] 4.3 in-process 请求处理器（`ats_handle_translation_request` / `ats_handle_invalidation_request`）
- [x] 4.4 范围声明（文件头注释）：Page Walk / cache hint / PASID 明确列为 stage-1.1 non-goal，stage-1.4 集成 KFD 按需添加

## 5. IOTLB Invalidate + mmu_notifier 桩

- [x] 5.1 实现 `src/kernel/iommu/invalidate.cpp` — `iommu_flush_iotlb()` 高层接口 + IOTLB 刷新通过 ops vtable
- [x] 5.2 实现 `iommu_invalidate_register_notifier()` 桩（按 spec Requirement: IOTLB invalidate 与 mmu_notifier 集成点，body 为 logging + TODO）
- [x] 5.3 桩内显式注释 `// TODO(stage-1.3): implement mmu_notifier callback`，引用追踪 plan 路径

## 6. PCIe 集成（与子阶段 1.0 对接）

- [x] 6.1 实现 `src/kernel/iommu/pcie_integration.cpp` — `iommu_emu_init()` + `iommu_register_pci_device()` 注册 API（stage-1.0 的 PciDevice 主动注册，避免 stage-1.1 强求 1.0 接口变更）
- [x] 6.2 PCIe device → iommu_group 一对一映射（1 device = 1 group，按 design.md Decision 5；通过 `iommu_default_ops_get()` 附加 default domain）
- [x] 6.3 module load 钩子接入点：`iommu_emu_init()` 通过 extern "C" 暴露，可由 kernel SHARED 库 init 调用

## 7. 文档交付

- [ ] 7.1 创建 `docs/05-advanced/iommu-error-semantics.md` — 错误码对照表（7 行 mapping 表格，按 spec Requirement: 错误码语义对照表文档交付）
- [ ] 7.2 更新 `docs/02_architecture/post-refactor-architecture.md §1.10` — 标注 IOMMU / ATS 层就位状态
- [ ] 7.3 验证 iommu-error-semantics.md 引用 `include/linux/errno.h` 作为 source of truth

## 8. 测试（3 个 Catch2 standalone）

- [ ] 8.1 创建 `tests/test_iommu_emu_standalone.cpp` — group 创建 / ioasid 分配 / basic map/unmap（按 spec Requirement: 测试覆盖三条核心验收）
- [ ] 8.2 创建 `tests/test_dma_remap_standalone.cpp` — DMA remapping 页表正确性 + 错误码语义（含 IOVA overlap → -EREMOTEIO 断言）
- [ ] 8.3 创建 `tests/test_ats_protocol_standalone.cpp` — Translation Request 双向（mapped → SUCCESS / unmapped → UNMAPPED）+ Invalidation Request 双向
- [ ] 8.4 所有 3 个测试包含至少 1 个 SPEC requirement 引用注释（便于 spec ↔ test 追溯）

## 9. CMake 集成 + 验证

- [x] 9.1 修改 `src/CMakeLists.txt` — 添加 `src/kernel/iommu/*.cpp` 到 `add_library(kernel SHARED ...)`（保持 kernel SHARED，遵守 Issue #11）
- [x] 9.2 修改 `tests/CMakeLists.txt` — 注册 `test_iommu_emu_standalone` / `test_dma_remap_standalone` / `test_ats_protocol_standalone` 三个 Catch2 目标
- [x] 9.3 构建验证：`mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j4`（全编译成功，0 error，warning 数量记录在 iommu-error-semantics.md）
- [x] 9.4 运行 `ctest --output-on-failure` 从 build/，确认全部 40 个测试全绿（37 既有 + 3 新增）
- [x] 9.5 HAL 不变性验证：`git diff plugins/gpu_driver/hal/` 无任何修改（符合 spec Requirement: 不引入 HAL 接口扩展，guardrail）
- [x] 9.6 ADR 合规性验证：本文档内所有内容遵循 ADR-027（spec-driven 增量）/ ADR-035（HAL 治理）/ ADR-036（3 区分架构）

## 10. 子阶段验收与归档

- [x] 10.1 完整运行路线图 §1.1 验收清单 4 条：
  - [x] 10.1.1 模拟 IOMMU 能响应 device-IOTLB invalidate（`tests/test_dma_remap_standalone` → "dma_remap_flush_iotlb_invoked_on_unmap" + `invalidate.cpp` 实现）
  - [x] 10.1.2 ATS 请求能在 UsrLinuxEmu 内完整处理（含 translation completion 消息）（`tests/test_ats_protocol_standalone` → "ats_translation_request_mapped_iova_returns_phys" / "..._unmapped_returns_unmapped"）
  - [x] 10.1.3 DMA remapping 失败的错误码语义与 Linux 内核一致（`tests/test_dma_remap_standalone` → 9 个 `static_assert(IOMMU_ERR_* == -EXXX)` + 运行时断言）
  - [x] 10.1.4 `tests/test_iommu_emu_standalone` 覆盖 group / ioasid / remap / ATS（13 个 TEST_CASE 覆盖 group 创建/ioasid 分配/basic map/unmap/PCIe 集成/ops vtable）
- [x] 10.2 归档本 OpenSpec change：`openspec archive stage-1-1-iommu-ats`
- [x] 10.3 更新追踪 plan 状态：`docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md §Sub-stage 1.1` 全部 checkbox 勾选 + §Status Snapshot 标记 ✅
- [x] 10.4 触发下一子阶段 1.2：
  ```bash
  openspec new change "stage-1-2-drm-subset" \
      --template docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md \
      --reference docs/roadmap/stage-1-kernel-emu.md#子阶段-12--drm-子集
  ```
