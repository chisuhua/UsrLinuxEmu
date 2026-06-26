# Design: H-3.6 ADR-034 Issue #3 修复协调

> **依赖**: proposal ✅
> **状态**: 📋 PROPOSED (2026-06-26)
> **目标**: 详细设计 H-3.6 协调工作的技术决策、跨仓工作流、测试覆盖策略

## Context

### 背景与现状

ADR-034 跟踪的 H-7 deferred 3 个 issues 中，Issue #3 (`attached_queues` 弱校验) 是 **风险最低、收益最高**的目标。

**实际代码位置**（UsrLinuxEmu 仓）：

```cpp
// gpgpu_device.h:72-78 (struct VASpace)
struct VASpace {
    uint64_t handle;
    uint32_t page_size;     // 0=4KB, 1=64KB
    uint32_t flags;
    uint64_t created_at;
    std::vector<uint64_t> attached_queues;  // ← 弱校验根源
};

// gpgpu_device.cpp:260-262 (PUSHBUFFER_SUBMIT_BATCH 校验)
{
    std::lock_guard<std::mutex> va_lock(va_space_mutex_);
    const auto& attached = va_spaces_[args->va_space_handle].attached_queues;
    if (std::find(attached.begin(), attached.end(),
                  static_cast<uint64_t>(args->stream_id)) == attached.end()) {
        usr_linux_emu::Logger::warn(
            "[GpgpuDevice] PUSHBUFFER_SUBMIT_BATCH: stream_id " +
            std::to_string(args->stream_id) +
            " not attached to va_space_handle " +
            std::to_string(args->va_space_handle));
        return -EINVAL;
    }
}
```

**问题剖析**：
1. **`std::find` 性能问题**：O(n) 线性查找，n = 关联 queue 数量
2. **弱校验**：仅存在性检查，**无** lifecycle/type/binding 断言
3. **静默错误**：返回 `-EINVAL` 无类型区分，难以诊断 root cause
4. **race condition 风险**：`destroy_va_space` 后仍可能 `submit_batch` 接受已 destroy 的 queue

**调研支撑**（基于 bg_5826c044 AMD ROCm / NVIDIA CUDA 调研）：

| 平台 | 数据结构 | 性能 | 备注 |
|------|---------|------|------|
| AMD ROCm KFD | 链表 (O(n)) | n ≤ 127 | 因 cap 上限小，O(n) 可接受 |
| AMD ROCm UMQ | 红黑树 | O(log n) | 多 GPU 场景 |
| NVIDIA UVM | 红黑树 | O(log n) | N 通常 > 1000 |
| NVIDIA libcuda | 数组 + bitmap | O(1) | 固定 cap |
| **TaskRunner 推荐** | **`unordered_set`** | **O(1) avg** | C++ 标准库最低改动 |

**任务约束**：

- **AGENTS.md 跨仓协议**：按 ADR-035 §Rule 5.1 4 步同步
- **C++17 标准**：当前代码已用 C++17，修改 MUST 保持兼容
- **doctest 测试**：TaskRunner 端测试基于 doctest 框架
- **gpgpu_device.cpp 在 UsrLinuxEmu 仓**：TaskRunner 端**不**直接修改
- **H-3.5 已 shippable**：本 change 不影响 31 方法契约

### 利益相关方

- **TaskRunner 维护者**：负责协调 + 文档 + 测试设计 + 跨仓 PR
- **UsrLinuxEmu 维护者**：负责 gpgpu_device.cpp 实际代码修改 + 跨仓 PR review
- **集成测试用户**：H-3.6 shippable 后可放心使用 strengthened validation

## Goals / Non-Goals

**Goals**:
- TaskRunner 端：跟踪 ADR-034 §Issue #3 修复状态，**不**实施实际代码修改
- TaskRunner 端：设计 race condition 测试用例（mock 端，不依赖 UsrLinuxEmu 修复）
- TaskRunner 端：设计错误码语义化测试用例
- TaskRunner 端：建立跨仓 PR 模板（4 步流程）
- TaskRunner 端：补充 tadr-105 §Trigger Conditions 段
- UsrLinuxEmu 端（owner）：评估 + 实施 `vector` → `unordered_set` 修复
- UsrLinuxEmu 端（owner）：可选 - 改进错误码语义 + lifecycle_state 字段
- 跨仓：按 ADR-035 4 步协议同步

**Non-Goals**:
- 不修改 `gpu_ioctl.h` 任何定义
- 不修改 IGpuDriver 任何方法签名
- 不修改 gpgpu_device.cpp 实际代码（这是 UsrLinuxEmu owner 工作）
- 不同时修复 Issue #1（u32→u64）或 Issue #2（ioctl path 旁路）
- 不演化为真实生产用户态驱动
- 不修改 UsrLinuxEmu drv/sim/hal 其他代码
- 不修改 TaskRunner ↔ UsrLinuxEmu 任何接口契约

## Decisions

### Decision 1: TaskRunner 端**仅协调，不实施**（基于用户决策 1 + 2）

**选项**：
- A. TaskRunner 端**仅协调** + 文档 + 测试设计（**采用**）
- B. TaskRunner 端跨仓修改 gpgpu_device.cpp（**不采用**：违反 submodule 边界）
- C. 在 UsrLinuxEmu 仓开 issue 提议 + 等 owner 实施（**采用作为补充**）

**理由**：
- TaskRunner 是 UsrLinuxEmu 的 git submodule（`external/TaskRunner`）
- gpgpu_device.cpp 在 UsrLinuxEmu 仓**主目录**，不在 submodule 内
- 修改 gpgpu_device.cpp 必须由 UsrLinuxEmu owner 端发起
- TaskRunner 端可推进：文档协调 + 测试设计 + 跨仓 PR 模板

**实施**：
1. TaskRunner 端创建 5 文件 openspec change（tracking）
2. TaskRunner 端创建跨仓 PR 模板（4 步流程）
3. TaskRunner 端在 UsrLinuxEmu 仓开 GitHub issue 提议修复
4. UsrLinuxEmu owner 评估 + 实施
5. 跨仓 submodule bump

### Decision 2: 推荐 UsrLinuxEmu owner 端使用 `std::unordered_set`（基于 bg_5826c044 调研）

**选项**：
- A. `std::unordered_set<uint64_t>` (O(1) avg, 1 行代码改动) — **采用**
- B. `std::set<uint64_t>` (O(log n), 红黑树) — 不采用（N 较小，无需 log n）
- C. `absl::flat_hash_set` (O(1), Google 库) — 不采用（额外依赖）
- D. 保持 `std::vector` + 优化 `std::find` 算法 — 不采用（治标不治本）

**理由**：
- C++ 标准库最低改动方案（仅需 1 行 include + 1 行代码）
- O(1) avg 性能满足 N 通常 < 100 的场景
- 行为兼容（`find` 返回 iterator / end，与现有代码语义一致）
- 不引入新依赖

**实施**（在 gpgpu_device.h:77）：
```diff
- #include <vector>
+ #include <unordered_set>

  struct VASpace {
      ...
-     std::vector<uint64_t> attached_queues;
+     std::unordered_set<uint64_t> attached_queues;
  };
```

**实施**（在 gpgpu_device.cpp:261）：
```diff
  if (attached.find(static_cast<uint64_t>(args->stream_id)) == attached.end()) {
      // 行为与 std::find 完全一致
  }
```

### Decision 3: 错误码语义化**分 2 PR**（基于 Open Question 1）

**选项**：
- A. 2 个独立 PR：先 `unordered_set` 改动，后错误码语义化（**采用**）
- B. 1 个综合 PR：所有改动一起提交 — 不采用（风险聚合）
- C. 仅 `unordered_set` 改动，不改进错误码 — 不采用（错失良机）

**理由**：
- 2 PR 拆分降低单 PR 风险
- 错误码语义化涉及更多测试用例（区分 4 种错误码）
- H-3.6 协调可同时跟踪 2 个 PR 进度

**PR 1 范围（最小化）**：
- `gpgpu_device.h:77` 改 `vector` → `unordered_set`
- `gpgpu_device.cpp:261` 改 `std::find` → `.find()`
- 行为完全兼容，回归风险最小

**PR 2 范围（可选）**：
- 改进错误码：区分 `-EINVAL` (类型不匹配) vs `-ENOENT` (queue 已 destroy) vs `-EBUSY` (queue 仍 attached)
- 添加 `lifecycle_state` 字段
- race condition 测试覆盖

### Decision 4: 测试设计**双轨**（TaskRunner 端 + UsrLinuxEmu 端各做各的）

**TaskRunner 端**（基于 MockGpuDriver）：
- race condition 测试（destroy_va_space vs submit_batch 并发）
- 错误码语义测试（区分 -EINVAL / -ENOENT / -EBUSY）
- 不依赖 UsrLinuxEmu 实际修复（MockGpuDriver 自己实现 mock validation）

**UsrLinuxEmu 端**（基于真实 gpgpu_device.cpp）：
- 真实 race condition 测试
- 性能基准（O(n) vs O(1)）
- 错误码语义测试（与 TaskRunner 端对称）

**理由**：
- TaskRunner 端 mock 验证逻辑与 UsrLinuxEmu 端 real implementation 对称
- 真实修复后，TaskRunner 端测试可作为"reference behavior"对照
- 测试设计文档**不**绑定实际代码修改时序

## Implementation Strategy

### 阶段 A：TaskRunner 端协调（Day 1-3，本 change 主要工作）

```
Day 1: openspec change 创建 (5 文件完整)
Day 2: GitHub issue 草稿 + 跨仓 PR 模板 + 测试设计文档
Day 3: tadr-105 §Trigger Conditions 段补充 + docs-audit 验证
```

### 阶段 B：跨仓 PR 协调（Day 4-7，待 UsrLinuxEmu owner 启动）

```
Day 4: 在 UsrLinuxEmu 仓开 GitHub issue（提议方案）
Day 5: UsrLinuxEmu owner 评估（如拒绝则记录决策）
Day 6: UsrLinuxEmu owner 实施 PR 1（最小 unordered_set 改动）
Day 7: 跨仓 PR review + merge
```

### 阶段 C：状态同步（Day 8-10，PR 1 merged 后）

```
Day 8: TaskRunner 端 bump submodule 指针
Day 9: TaskRunner 端 tadr-105 §Issue #3 状态更新 → Accepted
Day 10: UsrLinuxEmu 仓 archive 本 change + 更新 ADR-034 状态
```

### 阶段 D：可选 PR 2（Day 11+，错误码语义化）

```
Day 11+: UsrLinuxEmu owner 评估 PR 2 范围（错误码 + lifecycle_state）
         TaskRunner 端跟踪 PR 2 进度（如有）
```

## Risks & Mitigations

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| **UsrLinuxEmu owner 不响应 GitHub issue** | 中 | 高 | 升级到 UsrLinuxEmu project lead；或 TaskRunner owner 跨仓提交 patch |
| **`unordered_set` 引入新编译错误** | 低 | 中 | 在 TaskRunner 端先验证 include + 编译可行性 |
| **回归测试不通过** | 低 | 中 | UsrLinuxEmu owner 端先跑全部 test_gpu_ioctl_standalone + test_gpu_plugin |
| **PR 1 + PR 2 互相冲突** | 低 | 中 | PR 1 范围严格最小化，PR 2 严格独立 |
| **跨仓 PR review 周期长** | 中 | 低 | 提前同步 ADR-035 §Rule 5.1 流程，减少 review 阻塞 |
| **错误码语义化影响下游 consumers** | 中 | 中 | PR 2 范围 owner 评估，可拆分为 PR 2a / 2b |

## Cross-Reference

- **上游 UsrLinuxEmu ADR-034** §Issue #3：完整问题描述 + 修复路径
- **TaskRunner tadr-105** §Trigger Conditions：跟踪 H-3.6 启动信号
- **openspec/changes/archive/2026-06-22-h3-phase2-management/design.md** §R4：原始推迟决策来源
- **AMD ROCm / NVIDIA CUDA 调研**：`docs/test-fixture/research/queue-id-patterns-2026-06-26.md`（待创建）
