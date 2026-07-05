# Stage 2 Design Spec (修订 v2 — Oracle 评审后)

> **作者**: Sisyphus
> **日期**: 2026-07-05
> **状态**: 📋 设计阶段 (待用户 review)
> **基础**: Oracle 评审反馈 (6 项关键修订 + scope/structure 调整)
> **前置**: Stage 1.4 Tier-2 完成 (commit `b521f29` on main, ctest 73/73 PASS)

---

## 1. 目标

完成 Stage 2: 在 Stage 1.4 Tier-2 runtime penetration 基础上，横向验证 3 区分架构原则可泛化到 GPU 之外的设备类型（网络 + 存储），并吸收 boundary §5.2 显式延后的 Tier-2 项目。

**交付价值**：
- 第二类设备（网络/存储）证明 ADR-036 三区分原则的可扩展性
- KFD 真实运行时基础（vfio IOMMU + mmu_notifier 真实模型）
- ctest 90+ tests, 0 regression, docs-audit 100% PASS

---

## 2. Oracle 评审修订（vs 原始 v1）

| # | 原始 v1 | 修订后 v2 | 理由 |
|---|--------|-----------|------|
| 1 | 2.0.1 segfault fix 打包进 Stage 2 | **立即热修 main（v1.4.1 hotfix）** | P0 阻塞 bug 不应等待 Stage 2 |
| 2 | 2.0 是 Stage 2 子阶段 | **2.0 拆为 hotfix + Stage 1.5 (v1.5 tag)** | 命名卫生，Stage 2 干净启动 |
| 3 | 2.1.3 ATS PRI/PRG in Stage 2 | **2.1.3 DEFER 到 Stage 3+** | feature 非 foundation；消费级 GPU 不支持 |
| 4 | 2.4 是 sub-stage | **2.4 重命名为 "Stage 2 Release Gate"** | 语义正确：发布门而非构建子阶段 |
| 5 | 2.1 直接 2-3 周实现 | **2.1 前置 2.0.0 spike (3-5 天)** | 验证 vfio 降级 + mm shim 可行性 |
| 6 | ADR-037 与 2.2 并行 | **ADR-037 必须在 2.2.1 之前 Accepted** | 网络栈 sk_buff ② 归属必须先定 |

**附加采纳**（来自 oracle 其他建议）：
- CI 矩阵扩展作为并行任务（不是 2.4 的一部分）
- Stage 2 OpenSpec workflow 设立任务
- 5 决策点 #3 部分修订：ASCII（源文件）+ Mermaid（渲染文档）双轨

---

## 3. 修订后路径（10 子阶段，25 任务）

### Hotfix v1.4.1 (立即启动, 1-2 天)

**H1** `handleMapQueueRing` segfault fix
- 修复 `GpgpuDevice::handleMapQueueRing` Phase 2.5 shared-memory 绑定路径
- 独立 commit `fix(drv): handleMapQueueRing Phase 2.5 segfault fix`
- 不进 Stage 2，立即修
- 验收：ctest 73→74 PASS

### Stage 1.5 收尾 (1 周, tag v1.5)

**1.5.1** docs-audit 二次审计
- 跑 `tools/docs-audit.sh --strict` 在 hotfix 后 main
- 修复发现的任何过期引用
- 验收：43/43 PASS（不变或增加）

**1.5.2** OpenSpec stage-1 changes 归档
- 确认 stage-1.0-pcie / stage-1.1-iommu-ats / stage-1.2-drm-subset / stage-1.3-uvm-hmm / stage-1.4-kfd-portability / stage-1.4-tier2 6 个 changes 全部 archived
- 如有遗漏，`openspec archive`
- tag: `v1.5`
- 验收：`openspec list --json` 显示 0 active stage-1 changes

### ADR-037 (2 周, 与 Stage 1.5 并行 worktree)

**ADR-037** 网络栈 3 区分架构边界
- **核心问题**：② 是否包含 sk_buff 处理？
  - 若 ② 仅 net_device_ops → ① 必须模拟 socket/sock/sk_buff（2.2.1 工作量大）
  - 若 ② 含 sk_buff → 移植性受损
- **建议决议**：① 将 sk_buff 模拟为 opaque token + alloc/free/opaque-access ops；② 含 net_device_ops + 使用 ① 的 sk_buff ops 操作 buff
- ADR-037 必须在 2.2.1 启动前 Accepted
- 验收：ADR-037 文件存在且 Accepted

### Stage 2.0 — Tier-2 延后可行性 Spike (3-5 天)

**2.0.0** spike (无 commit 产物，仅可行性报告)
- (a) **vfio 容器降级路径验证**：在无 root 容器内检测 `/dev/vfio/vfio` + `/sys/kernel/iommu_groups/` 不可用，验证降级到 "vfio-sim" 模式可用
- (b) **mm shim 最小原型**：用 50 行 C++ 写 PID-aware + VMA list 骨架，验证可编译可链接
- 产出：`docs/05-advanced/stage-2-spike-report.md`
- 验收：spike report 明确 GO / NO-GO for 2.1.1 / 2.1.2

### Stage 2.1 — Tier-2 延后吸收 (2-3 周, spike GO 后)

**2.1.1** vfio IOMMU 真实化
- 引入 `/dev/vfio/vfio` 用户态绑定
- `iommu_flush_iotlb` 升级：检测 vfio 可用则调 `ioctl(VFIO_IOMMU_UNMAP_DMA)`；否则降级到现有 page-table walk
- config flag 显式 opt-in 真实模式
- 验收：root 环境下 `iotlb_unmap → flush → /dev/vfio` syscall 可见；非 root 降级不崩

**2.1.2** mmu_notifier 真实进程模型（**scope 严格限定**：PID-aware notifier + VMA list，不做完整 mm_struct 复刻）
- 替换 `SimPageFaultHandler` 匿名 struct 为 userspace mm shim（PID + VMA 跟踪）
- `iommu_invalidate_register_notifier_internal` 关联到真实 PID/VMA
- 验收：`mn->ops->invalidate_range_start` 收到真实 PID + VMA range 参数（不是匿名指针）

~~**2.1.3** ATS PRI/PRG response routing~~ → **DEFER 到 Stage 3+**（与真实 KFD multi-file 一起）

### Stage 2.2 — 网络设备 (3-4 周, ADR-037 Accepted 后)

**2.2.1** `src/kernel/net/` socket + sk_buff 兼容层（按 ADR-037 决议）
- `socket()`, `bind()`, `sendto()`, `recvfrom()` syscall 兼容
- `struct sk_buff` opaque token + alloc/free/opaque-access ops
- 验收：minimal socket round-trip（loopback）通过

**2.2.2** `plugins/net_driver/drv/`
- `struct net_device_ops` 子集（ndo_open, ndo_stop, ndo_start_xmit）
- 注册到 VFS（`/dev/net0`）
- 验收：加载 `net_driver.so` 后 `/dev/net0` 可见

**2.2.3** `plugins/net_driver/sim/` NIC 模拟
- loopback sim（echo sk_buff）
- packet buffer + interrupt on packet arrival
- 验收：发送包 → 接收 echo 包 → 数据一致

**2.2.4** `test_net_driver_standalone`
- happy path: open → sendto → recvfrom (echo) → close
- error path: 重复 open / invalid sk_buff / OOB
- 验收：ctest 90+ PASS

**2.2.5** 3 区分回归
- 把 `net_driver/drv/net_device.c` 拷贝到 `drivers/gpu/xxx/net_compat.c` 编译
- 模拟"在真内核中编译"
- 验收：兼容层编译通过

### Stage 2.3 — 存储设备 (3-4 周, ADR-038 Accepted 后, 与 2.2 并行 worktree)

**2.3.1** `src/kernel/block/` bio 兼容层
- 直接基于 read/write（不实现 request queue）
- 验收：minimal block read/write round-trip 通过

**2.3.2** `plugins/storage_driver/drv/`
- `struct block_device_operations` 子集
- 注册到 VFS（`/dev/sda0`）
- 验收：加载 `storage_driver.so` 后 `/dev/sda0` 可见

**2.3.3** `plugins/storage_driver/sim/` 磁盘模拟
- 基于 host 文件 backing store
- 验收：read/write 持久化到 host 文件，关闭后重开数据保留

**2.3.4** `test_storage_driver_standalone`
- happy path: open → write → close → open → read → verify
- error path: 超大 size / invalid offset
- 验收：ctest 95+ PASS

### Stage 2.4 — Stage 2 Release Gate (1-2 周)

**Stage 2 Release Gate Checklist** (单一发布门任务):
- [ ] **3 区分一致性**：GPU / 网络 / 存储三方分层均符合 ADR-036
- [ ] **ADR 引用**：ADR-036, ADR-037, ADR-038 (如有) 全部 Accepted
- [ ] **TaskRunner 集成**：TaskRunner 验证 net + storage 插件可用
- [ ] **ctest 全量回归**：95+ tests, 0 failed, 0 regression
- [ ] **Boundary SSOT v1.2 更新**：网络/存储的 Tier 划分记录
- [ ] **Roadmap §2 状态**：`📋 规划中` → `✅ 已达成`
- [ ] **docs-audit --strict**：45+/45+ PASS

### CI 矩阵扩展 (持续, 与 Stage 2.0-2.3 并行)

**CI-1** GitHub Actions 工作流更新
- 新增 plugins/net_driver, plugins/storage_driver, src/kernel/net, src/kernel/block 构建目标
- 新增 net + storage standalone tests 到 ctest 列表

**CI-2** docs-audit hook 触发条件更新
- 新增 `plugins/net_driver/**`, `plugins/storage_driver/**`, `src/kernel/net/**`, `src/kernel/block/**` 路径触发

---

## 4. 依赖图（修订后）

```
Hotfix v1.4.1 (1-2 天)        [立即启动]
  │
  ▼
Stage 1.5 (1 周)              ◀── 并行 ──┐
  │                                       │
  ▼                                       ▼
ADR-037 (2 周)            ADR-038 (1.5 周, 类似 037)
  │                                       │
  ▼                                       │
Stage 2.0 spike (3-5 天)    ◀── spike GO ──┘
  │
  ▼
Stage 2.1 (2-3 周)
  │
  ├─────────────────────────────────────┐
  ▼                                     ▼
Stage 2.2 (3-4 周, ADR-037 后)    Stage 2.3 (3-4 周, ADR-038 后, 与 2.2 并行)
  │                                     │
  └─────────────────┬───────────────────┘
                    ▼
              Stage 2.4 Release Gate (1-2 周)

CI-1, CI-2 (持续, 与 2.0-2.3 并行)
```

**总工作量估算**：12-16 周（3-4 个月）

---

## 5. 关键设计决策（已修订）

| # | 决策 | 默认 | 备选 | 来源 |
|---|------|------|------|------|
| 1 | 粒度（8 字段/task） | ✅ Yes | 4 / 12 | 用户原始 |
| 2 | Task ID 命名 `2.X.Y` | ✅ Yes | `stage2-X-Y` | 用户原始 |
| 3 | 依赖图格式 | **ASCII + Mermaid 双轨** | ASCII only / Mermaid only | Oracle 部分同意 |
| 4 | 1 stage/reply 粒度 | ✅ Yes | 全展开 | 用户原始 |
| 5 | 新 ADR-037/038 | ✅ Strong Yes | Skip ADR | 用户原始 + Oracle 加强 |
| 6 | hotfix 立即 vs 排队 | **立即 hotfix v1.4.1** | 排队进 Stage 2 | Oracle |
| 7 | 2.0 拆 vs 保留 | **拆 hotfix + Stage 1.5** | 保留为 Stage 2 子阶段 | Oracle |
| 8 | ATS in Stage 2 vs defer | **DEFER 到 Stage 3+** | in Stage 2 | Oracle |
| 9 | 2.4 sub-stage vs gate | **Release Gate (非 sub-stage)** | sub-stage | Oracle |
| 10 | 2.1 spike 前置 | **spike 2.0.0 前置** | 直接 2.1 实现 | Oracle |

---

## 6. 风险与缓解

| 风险 | 严重度 | 缓解 |
|------|--------|------|
| 网络栈 sk_buff ② 归属未定即开工 | 🔴 HIGH | ADR-037 必须在 2.2.1 前 Accepted |
| vfio 在非 root 容器下无 fallback | 🟡 MED | 运行时检测 + 降级 + config opt-in |
| mmu shim 膨胀到 4-6 周 | 🟡 MED | 严格 scope: PID + VMA list，不复刻 mm_struct |
| 2.2 ∥ 2.3 共享 VFS/CMake/CTest | 🟡 MED | 末尾 2-3 天合并工，2.4 预算内可吸收 |
| ATS PCI 4.0 设备少 | 🟢 LOW | 已 DEFER 到 Stage 3+ |
| 网络协议栈深度（socket/sock） | 🔴 HIGH | ADR-037 决议 opaque sk_buff 模型 |

---

## 7. Spec Self-Review

✅ **Placeholder scan**: 无 TBD/TODO，所有任务有具体描述。
✅ **Internal consistency**: 修订项之间无矛盾（hotfix → Stage 1.5 → spike → 2.1/2.2/2.3 → Gate 逻辑通顺）。
✅ **Scope check**: 25 任务规模合理，与"Stage 2 = 多设备 + Tier-2 吸收"范围一致。
✅ **Ambiguity check**: 每任务有明确目标/验收；ADR-037/038 主题清楚。

---

## 8. 验收标准

Stage 2 完成的最终验证（Release Gate）：

```bash
ctest --test-dir build --output-on-failure
# Expected: 95+/95+ tests passed, 0 failed

tools/docs-audit.sh --strict
# Expected: 45+/45+ PASS, 0 failed, 0 warnings

openspec list --json
# Expected: stage-2.0/2.1/2.2/2.3 changes all archived
```

---

## 9. 下一步（spec approved 后）

按 brainstorming 流程的 terminal state，spec approved 后应调用 **writing-plans skill** 生成 Stage 2 实施计划：

```
writing-plans skill → docs/superpowers/plans/2026-07-05-stage-2-multi-device.md
→ 然后按 plan 启动 Hotfix v1.4.1 (立即)
```

---

**对应 ADR**: ADR-036 (3-way principle) + ADR-037 (网络栈 边界, 待写) + ADR-038 (块栈 边界, 待写)
**对应 SSOT**: post-refactor-architecture.md §1.10 + kfd-portability-boundary.md v1.2 (待升)
**对应 Roadmap**: docs/roadmap/stage-2-multi-device.md (待重写对齐本 spec)