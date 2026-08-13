# complete-mmu-notifier-callback

**优先级**: P2 | **来源**: ADR-027 §spec-driven + code TODO `include/linux_compat/iommu/iommu_domain.h:96`
**阶段**: stage-1.3 | **分类**: core-impl
**类型**: functional (callback completeness)

## 架构依据

[ADR-027](docs/00_adr/adr-027-linux-compat-strategy.md) §spec-driven 策略：Linux 兼容层按 spec 扩展，仅在 KFD driver code demonstrably requires 时添加新 ops。

[ADR-061](docs/00_adr/adr-061-hal-iommu-extension.md) 扩展 HAL IOMMU ops，关联 C-12 KFD multi-file integration tasks.md B.3.4。

`include/linux_compat/iommu/iommu_domain.h:96` 当前声明：

```c
int (*register_notifier)(struct iommu_domain *domain,
                         struct mmu_notifier *mnp);
/* returns 0 on success; TODO(stage-1.3): full mmu_notifier callback */
```

**当前后果**：
- `register_notifier` 函数指针存在但无实现（仅有 stub 返回 0）
- 真实 GPU 需要 mmu_notifier 回调来响应用户进程 munmap（page migration 触发）
- KFD page migration 路径依赖 `mmu_notifier_invalidate_range_start/end` 5 个 callback
- 当前阶段 1.3 显式延后

## 范围

- **In Scope**:
  - 实现 5 个 `mmu_notifier_ops` 回调：
    - `invalidate_range_start`
    - `invalidate_range_end`
    - `release`
    - `clear_flush_young` (optional)
    - `clear_young` (optional)
  - `register_notifier` 完整实现（注册到 domain 的 notifier 链表）
  - 在 `iommu_unmap` / `iommu_map` 路径中触发 `invalidate_range_start/end`
  - 配套测试 `test_iommu_notifier_standalone.cpp`
- **Out Scope**:
  - KFD page migration 完整实现（独立 task）
  - IOMMU hardware TLB invalidation 模拟（属 ADR-073）
  - Per-process address space tracking（属 ADR-064）

## 关键场景

- GIVEN `register_notifier(domain, mnp)` 被调用
  - WHEN 注册
  - THEN `mnp` 加入 `domain->notifier_list`，返回 0
- GIVEN 用户进程 munmap 一段 GPU VA
  - WHEN `iommu_unmap` 触发
  - THEN `mmu_notifier_invalidate_range_start(mnp, mm, start, end)` 被调用
  - 随后 `mmu_notifier_invalidate_range_end(...)` 被调用
- GIVEN KFD page migration 触发
  - WHEN 执行
  - THEN mmu_notifier 链路完整传递到 sim 层
- GIVEN 测试套件执行 WHEN 实现完成 THEN ctest 全部 PASS

## 技术约束

- MUST 保持 `struct iommu_domain_ops` 签名不变
- MUST 遵循 Linux kernel `mmu_notifier_ops` ABI（5 个 callback 签名）
- MUST NOT 修改 `unregister_notifier`（已 stub 即可）
- SHOULD 复用 `kernel_workqueue`（per ADR-060）做 async invalidate
- SHOULD 对每个 notifier 调用顺序：start → end → release

## 验收标准

- `register_notifier` 完整实现（添加 mnp 到 list）
- 5 个 callback 函数实现：`invalidate_range_start/end` + `release` + 2 个可选
- `iommu_unmap` 路径触发 callback
- 新增 `test_iommu_notifier_standalone.cpp`，至少 4 个 test case 覆盖：
  - register + 触发 invalidate_range
  - 多个 notifier 注册
  - release 触发清理
  - unregister 正确移除
- `make -j4` 编译通过，无 warning
- `ctest --output-on-failure` 全部 PASS
- 修改的代码行通过 `lsp_diagnostics` 检查
