# PoC Plan: UVM/HMM userfaultfd Page Fault 链路验证

> **日期**: 2026-07-03
> **状态**: ✅ GREEN（TDD 完成，53/53 全绿）
> **范围**: Stage 1.3 UVM/HMM Change 的 Launch Condition LC2
> **方法**: TDD (RED → GREEN → REFACTOR) ✅
> **参考**: `openspec/changes/stage-1-3-uvm-hmm/design.md` Decision 1 + tasks §1

---

## 1. 目标

验证 `userfaultfd + 专属 handler 线程 → mmu_notifier → SimPageFaultHandler` 链路在 UsrLinuxEmu 的用户态环境中可行。

### 成功标准

1. userfaultfd 成功注册 mmap 共享内存区域
2. 专属 handler 线程接收到 `UFFD_EVENT_PAGEFAULT` 消息
3. handler 线程内调 `mmu_notifier_notify(start, end)` 并计数
4. `SimPageFaultHandler` 记录被通知的地址
5. handler 用 `UFFDIO_COPY` 解决 fault，主线程恢复读取到期望数据
6. Catch2 test 全绿，ctest 53/53（基线 52 + PoC 1）

### 非目标（留给 tasks §2-14）

- 完整 `struct mmu_notifier` + ops 派发
- `hmm_range_fault()` 实现
- `zone_device` / `migrate_to_dev` / `page_state_machine`
- HAL 接口扩展
- CMake 将代码加入 kernel SHARED 库

---

## 2. 技术方案

### userfaultfd + 专属线程架构

```
UsrLinuxEmu Main Thread              UFFD Handler Thread
       │                                  │
       ├─ uffd = syscall(userfaultfd)     │
       ├─ UFFDIO_API → 版本协商           │
       ├─ mmap(ANONYMOUS, 4 pages)        │
       ├─ UFFDIO_REGISTER(MISSING)        │
       ├─ spawn handler thread ──────────►│
       │                                  ├─ poll(uffd, POLLIN)
       │  // trigger fault                │  ← blocks
       │  *((int*)region) = 42;           │
       │  (blocked in kernel)             ├─ read(uffd, &msg)
       │                                  ├─ msg.event = UFFD_EVENT_PAGEFAULT
       │                                  ├─ mmu_notifier_notify(start, end)
       │                                  ├─ sim_handler.record(start)
       │                                  ├─ UFFDIO_COPY(uffd, ...)
       │                                  ├─ ioctl(uffd, UFFDIO_WAKE, ...)
       │  (unblocked, page has data)      │
       │  CHECK(*ptr == 42)               ├─ back to poll(uffd)
```

### 为什么专属线程可行

| 真实内核 | UsrLinuxEmu + 专属线程 |
|---------|----------------------|
| CPU MMU 触发 fault exception | userfaultfd 内核层拦截 fault |
| kernel fault handler (同步上下文) | handler 线程 (异步、独立栈) |
| mmu_notifier 自动调用 | handler 显式调 `mmu_notifier_notify()` |
| `handle_mm_fault()` 解决 fault | `UFFDIO_COPY` + `UFFDIO_WAKE` |

**关键洞察**：UsrLinuxEmu 的 `mmu_notifier` 本身就是模拟层，不存在"内核自动调"的假设。handler 线程显式调 `mmu_notifier_notify()` 等价于模拟层接收到内核的 invalidation 通知——这正是 1.3 需要在 `src/kernel/uvm/mmu_notifier.cpp` 中实现的能力。

### 依赖

| 依赖 | 状态 |
|------|------|
| Kernel ≥ 4.3 | ✅ 5.10.134 |
| `userfaultfd()` syscall | ✅ SYS_userfaultfd=323 |
| `unprivileged_userfaultfd` | ✅ enabled (=1) |
| `UFFD_API` features | ✅ v170, features=0x1ff |
| Catch2 framework | ✅ vendored tests/catch_amalgamated.{hpp,cpp} |
| 基线 52/52 tests | ✅ 全绿 |

---

## 3. TDD 周期计划

### RED Phase 1: 端到端链路测试

**文件**: `tests/poc/test_userfaultfd_poc.cpp`

**测试用例 1**: `TEST_CASE("PoC: userfaultfd handler thread receives page fault")
- GIVEN: userfaultfd 创建 + mmap 4 pages + UFFDIO_REGISTER(MISSING) + handler thread spawned
- WHEN: 主线程触发 page fault (`*(int*)region = 42`)
- THEN: `mmu_notifier_notify_count == 1`
- THEN: `sim_handler.last_page_fault_addr == page_aligned(region)`
- THEN: 主线程恢复后 `*(int*)region == 42`

**预期失败原因**: `mmu_notifier_notify()` 和 `SimPageFaultHandler` 不存在 → compile error

### GREEN Phase 1: 最小实现

添加最小实现到测试文件内部（PoC 阶段）：
- `mmu_notifier_notify()` → atomic counter increment
- `SimPageFaultHandler` → struct with `last_page_fault_addr` + notify method
- `userfaultfd_handler_loop()` → `poll()` + `read()` + `UFFDIO_COPY` + `UFFDIO_WAKE`

### GREEN Phase 2: CMake 集成

- `tests/CMakeLists.txt`: 添加 `test_userfaultfd_poc` target
- 编译通过 + 链接通过

### REFACTOR Phase

- 提取 handler loop 为独立函数（为后续 `src/kernel/uvm/` 框架铺路）
- 确保命名风格：snake_case，struct PascalCase
- 添加注释标注：PoC → 正式实现映射

### VERIFY Phase

- `ctest --output-on-failure`: 53/53 全绿（52 基线 + 1 PoC）
- `lsp_diagnostics` on new files

---

## 4. 文件清单

| 文件 | 操作 | 阶段 |
|------|------|------|
| `tests/poc/test_userfaultfd_poc.cpp` | 新建 | RED → GREEN |
| `tests/CMakeLists.txt` | 修改（+1 target）| GREEN |
| `docs/superpowers/plans/2026-07-03-poc-uvm-userfaultfd.md` | 新建（本文件）| PLAN |

---

## 5. PoC 发现记录

### 接口边界发现

| 发现 | 影响 | 后续行动 |
|------|------|---------|
| userfaultfd 可用（Kernel 5.10.134, `unprivileged_userfaultfd=1`） | ✅ 无需替代方案 | 直接作为 1.3 框架基础 |
| `UFFDIO_COPY` 是 ioctl 命令号，`reg.ioctls` 是 `1 << _UFFDIO_COPY` bitmask | 测试需用 `_UFFDIO_COPY` 位检查 | 已修复，1.3 框架用 `1UL << _UFFDIO_COPY` |
| 专属 handler 线程可安全调 C++（poll/read/ioctl）| ✅ 方案可行 | tasks §3.4 fault_inject.cpp 可参考此模式 |
| `UFFD_EVENT_PAGEFAULT.arg.pagefault.address` 已 page-aligned | 简化地址处理 | SimPageFaultHandler 直接记录 |

### mmu_notifier 行为差异分析

| 项目 | 真实内核 | PoC 实现 | 1.3 正式实现策略 |
|------|---------|---------|----------------|
| fault 触发源 | CPU MMU 硬件 | userfaultfd 内核拦截 | 等价——都是内核层拦截→通知 |
| 通知同步性 | 内核上下文同步 | handler 线程异步 poll | 等价——1.3 模拟层主动调 mmu_notifier |
| 多页 fault | 一次 fault 可覆盖多 page | userfaultfd 逐页消息 | 兼容——hmm_range 自己遍历 |
| mmu_notifier 调用者 | 内核自动 | handler 显式调 `notify_count++` | 1.3 在 `fault_inject.cpp` 包装为 `mmu_notifier_notify()` |

### RED → GREEN → REFACTOR 结果

```
RED:  编译失败 — 'SimPageFaultHandler' + 'userfaultfd_handler_loop' 未声明
GREEN: 2 tests / 14 assertions / 53/53 全绿
REFACTOR: SimPageFaultHandler.notify() 封装 + handle_page_fault() 重命名 + 超时错误路径测试
```

### 环境确认

```
Kernel:    5.10.134-lifsea8.x86_64  ✅ ≥ 4.3
uffd:      syscall(SYS_userfaultfd)  ✅
API:       UFFD_API v170             ✅
features:  0x1ff (MINOR+HUGETLBFS)   ✅
ioctls:    0x1c (COPY+WAKE+ZEROPAGE) ✅
特权:      unprivileged_userfaultfd=1 ✅
基线:      52/52 测试全绿            ✅
PoC:       53/53 测试全绿            ✅
```

---

## 6. 与正式实施的映射

PoC 验证通过后，对应到 `tasks.md` 的实现路径：

| PoC 组件 | 对应正式文件 |
|-----------|-------------|
| `mmu_notifier_notify()` | tasks §3.1 `src/kernel/uvm/mmu_notifier.cpp` |
| `SimPageFaultHandler` | tasks §4.1 `plugins/gpu_driver/sim/page_fault_handler.cpp` |
| `userfaultfd_handler_loop()` | tasks §3.4 `src/kernel/uvm/fault_inject.cpp` (注入路径) |
| UFFDIO_COPY 逻辑 | tasks §5.x `plugins/gpu_driver/uvm/svm_ioctl.cpp` (条件性) |
| mmap shared region | tasks §3.5 `src/kernel/uvm/zone_device.cpp` (spm vma) |

---

**维护者**: UsrLinuxEmu Architecture Team
**创建日期**: 2026-07-03
**对应 Change**: `openspec/changes/stage-1-3-uvm-hmm/`
**对应 Spec**: `openspec/changes/stage-1-3-uvm-hmm/specs/uvm-hmm/spec.md`
**对应 Tasks**: `openspec/changes/stage-1-3-uvm-hmm/tasks.md` §1.1-1.4