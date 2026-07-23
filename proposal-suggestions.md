[
  {
    "name": "hal-iommu-full",
    "priority": "P0",
    "source": "ADR-061 §Accepted §B.3.4",
    "status": "已完成",
    "phase": "phase-1",
    "category": "core-impl",
    "effort": "3-5天",
    "description": "## 架构依据\n- ADR-061: HAL IOMMU 扩展，gpu_hal_ops 已扩展到 14 fn-ptrs + hal_user/hal_mock stubs committed\n- 完整实现 tracked in C-12 tasks.md B.3.4\n\n## 范围\n- In Scope: IOMMU map/unmap/invalidate 完整 HAL 实现\n- Out Scope: 不修改现有 hal_ops 接口签名\n\n## 验收标准\n- 所有 IOMMU HAL fn-ptrs 有完整实现\n- Catch2 测试覆盖 map/unmap/invalidate 场景"
  },
  {
    "name": "hal-event-signal",
    "priority": "P0",
    "source": "ADR-062 §Accepted §B.4.4",
    "status": "已完成",
    "phase": "phase-1",
    "category": "core-impl",
    "effort": "3-5天",
    "description": "## 架构依据\n- ADR-062: HAL Event Signal 扩展，gpu_hal_ops 已扩展到 14 fn-ptrs + hal_user/hal_mock stubs committed\n- 完整实现 tracked in C-12 tasks.md B.4.4\n\n## 范围\n- In Scope: event signal/wait/notify 完整 HAL 实现\n- Out Scope: 不修改现有 hal_ops 接口签名\n\n## 验收标准\n- 所有 Event Signal HAL fn-ptrs 有完整实现\n- Catch2 测试覆盖 signal/wait 并发场景"
  },
  {
    "name": "kfd-multi-file-complete",
    "priority": "P0",
    "source": "ADR-059 §Accepted §C-12 81%",
    "status": "已完成",
    "phase": "phase-1",
    "category": "core-impl",
    "effort": "5-7天",
    "description": "## 架构依据\n- ADR-059: KFD 多文件集成，C-12 已完成 81%\n- 剩余 19% 待完成\n\n## 范围\n- In Scope: KFD multi-file integration 剩余模块\n- Out Scope: 不修改已完成 81% 的架构\n\n## 验收标准\n- KFD 多文件集成达到 100%\n- 所有现有测试通过"
  },
  {
    "name": "iommu-mmu-notifier",
    "priority": "P2",
    "source": "iommu_domain.h:96 TODO(stage-1.3)",
    "status": "已完成",
    "phase": "phase-1",
    "category": "core-impl",
    "effort": "2-3天",
    "description": "## 架构依据\n- include/linux_compat/iommu/iommu_domain.h:96: TODO(stage-1.3): full mmu_notifier callback\n\n## 范围\n- In Scope: full mmu_notifier callback 实现\n- Out Scope: 不修改现有 invalidate 逻辑\n\n## 验收标准\n- mmu_notifier 回调完整实现\n- Catch2 测试覆盖页面失效通知场景"
  },
  {
    "name": "linux-compat-tests",
    "priority": "P1",
    "source": "测试缺口扫描: linux_compat 9 headers 0 tests",
    "status": "已完成",
    "phase": "phase-1",
    "category": "core-test",
    "effort": "2-3天",
    "description": "## 架构依据\n- linux_compat 层 9 个 header 文件，0 个测试\n- 测试覆盖缺口影响内核 API 兼容性可靠性\n\n## 范围\n- In Scope: linux_compat 层所有头文件的测试覆盖\n- Out Scope: 不修改 linux_compat 现有实现\n\n## 验收标准\n- linux_compat 所有模块有 Catch2 测试\n- 覆盖关键内核 API 兼容场景"
  },
  {
    "name": "kernel-tests",
    "priority": "P1",
    "source": "测试缺口扫描: kernel 15 headers 0 tests",
    "status": "已完成",
    "phase": "phase-1",
    "category": "core-test",
    "effort": "3-5天",
    "description": "## 架构依据\n- kernel 层 15 个 header 文件，0 个测试\n- 测试覆盖缺口影响核心模拟环境可靠性\n\n## 范围\n- In Scope: kernel 层核心组件的测试覆盖\n- Out Scope: 不修改 kernel 现有实现\n\n## 验收标准\n- kernel 层关键模块有 Catch2 测试\n- 覆盖 VFS/scheduler/IOMMU 核心场景"
  }
]
