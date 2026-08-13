# ADR-076 PTX-EMU HAL Backend — 架构差距分析

> **创建时间**: 2026-08-13
> **来源**: guide-arch Phase 3 — 架构差距分析（ADR-076 实施前审计）
> **关联 ADR**: [ADR-076](../00_adr/adr-076-gpgpu-kernel-module-ioctl.md) 🔄 Proposed
> **关联路线图**: [roadmap.md §跨仓评审中 ADRs](../../roadmap.md) + [stage-5-multi-engine-pm4.md §scope 说明](../../roadmap/stage-5-multi-engine-pm4.md)
> **关联 TADR**: [tadr-307](../../../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 🔄 Proposed
> **关联外部 ADR**: [PTX-EMU ADR-0029 §D8](../../../external/PTX-EMU/docs/adr/ADR-0029-ptxemu-image-executor.md#d8-cp-端集成约定--hal-扩展方案usrlinuxemu--ptx-emu-跨仓契约) ✅ Accepted
> **执行审计人**: Sisyphus（基于跨仓 symbol resolution + grep audit）

---

## 1. 摘要（TL;DR）

| 项目 | 状态 | 备注 |
|------|------|------|
| **PTX-EMU HARD gate** | ✅ CLEARED | `libptxemu_device.so` 已 build + `cpptlm_module.h` 已 ship + tag `v0.1.0` 已发布 |
| **TaskRunner tadr-307 SOFT gate** | ⏳ PROPOSED | consumer-side 文档已起草；review pending（建议不影响 UsrLinuxEmu HAL extension 启动）|
| **UsrLinuxEmu HAL extension 代码实施** | 📋 0% 启动 | 未发现 `kernel_module_load/execute/unload` 任何符号、`GPU_IOCTL_LOAD/LAUNCH/UNLOAD_KERNEL_MODULE` (0x27/0x28/0x29) ioctl 编号、对应 handler |
| **HAL append-only (ADR-023 §D4)** | ✅ VALID | 当前 65 fn-ptrs（66 含 1 helper）；追加 3 个仍 ≤68 append-only |
| **ioctl 编号兼容性** | ✅ VALID | 0x27/0x28/0x29 在当前已用 0x01~0x26 与 0x30 之间的 gap，无冲突 |
| **3 区分架构（ADR-036）** | ✅ VALID | HAL 仍是 ②③ 之间唯一桥；PTX-EMU 作为 HAL backend 不破坏分层 |

**核心结论**：**UsrLinuxEmu 仓 HAL extension 实施准备就绪**（arch-side ready）；可进入 guide-design 阶段创建 `openspec/changes/2026-08-15-stage5-ptxemu-kernel-module-hal-extension/`。

---

## 2. 目标状态 vs 当前状态

### 2.1 目标状态（ADR-076 §D1-D6）

| 子项 | 目标 | 文件位置（ADR-076 spec） |
|------|------|--------------------------|
| 3 个新 System C ioctl | `GPU_IOCTL_LOAD_KERNEL_MODULE` (0x27) / `LAUNCH` (0x28) / `UNLOAD` (0x29) | `plugins/gpu_driver/shared/gpu_ioctl.h` |
| 3 个新结构体 | `gpu_load_kernel_module_args` / `gpu_launch_kernel_module_args` / `gpu_unload_kernel_module_args` | 同上 |
| 3 个新 HAL fn-ptrs (#66/#67/#68) | `kernel_module_load` / `kernel_module_execute` / `kernel_module_unload` | `plugins/gpu_driver/hal/gpu_hal.h` |
| 3 个 inline wrapper | `hal_kernel_module_load/execute/unload` | 同上 |
| 3 个 ioctl handler | `handle_load/launch/unload_kernel_module` | `plugins/gpu_driver/drv/gpgpu_device.cpp` |
| 3 个 ioctl 派发表 entries | `[GPU_IOCTL_*]` slots | 同上（38 → 41 行） |
| `hal_user.cpp` 实现 | dlsym `libptxemu_device.so` + 5 ABI 函数调用 + version 检查 + 三级 fallback | `plugins/gpu_driver/hal/hal_user.cpp` |
| `hal_mock.cpp` 实现 | mock handle + 可注入错误码 for unit test | `plugins/gpu_driver/hal/hal_mock.cpp` |
| 3+ 个新测试 binary | `tests/unit/hal/test_hal_kernel_module_*.cpp`（mock / dlsym failure / version mismatch） | `tests/unit/hal/` |
| 跨仓 commit 顺序协议 | §Migration Step 1-4 严格 4 步 | `openspec/changes/<name>/tasks.md` + `README.md` 引用 |

### 2.2 当前状态（2026-08-13 audit）

#### 2.2.1 UsrLinuxEmu 仓实施现状

```
$ grep -rn "kernel_module_load\|kernel_module_execute\|kernel_module_unload" plugins/ src/ include/
0 matches

$ grep -rn "GPU_IOCTL_LOAD_KERNEL_MODULE\|GPU_IOCTL_LAUNCH_KERNEL_MODULE\|GPU_IOCTL_UNLOAD_KERNEL_MODULE" plugins/
0 matches

$ grep -cE "^\s+(int|void|size_t|ssize_t|uint64_t|uint32_t) \(\*\w+\)" plugins/gpu_driver/hal/gpu_hal.h
66  (含 1 helper; 纯 fn-ptrs = 65 per ADR-075)
```

| 子项 | 当前状态 | 缺口 |
|------|---------|------|
| 3 个新 ioctl 定义 | ❌ 未定义 | **需新增** |
| 3 个新结构体 | ❌ 未定义 | **需新增** |
| 3 个新 HAL fn-ptrs | ❌ 未追加（fn-ptrs 当前 = 65） | **需 append-only 追加 3 个** |
| 3 个 inline wrapper | ❌ 未实现 | **需新增** |
| 3 个 ioctl handler | ❌ 未实现 | **需新增** |
| `hal_user.cpp` dlsym | ❌ 未实现 | **需新增（不是 -ENOSYS 桩，是真正调用）** |
| `hal_mock.cpp` mock | ❌ 未实现 | **需新增** |
| openspec/changes/<name>/ | ❌ 目录不存在 | **需 guide-design 创建** |
| 测试 binary | ❌ 未创建 | **需新增 ≥3 个** |

**UsrLinuxEmu 仓整体进度**：**0/9 完成**（架构定义已就绪，代码实施未启动）。

#### 2.2.2 PTX-EMU 仓 ship 现状（HARD gate）

| 验收项 | 状态 | 证据路径 |
|--------|------|---------|
| `libptxemu_device.so` 存在 | ✅ | `/workspace/project/PTX-EMU/build/lib/libptxemu_device.so` + `/workspace/project/PTX-EMU/lib/libptxemu_device.so` |
| `cpptlm_module.h` 存在 | ✅ | `/workspace/project/PTX-EMU/include/cudart/cpptlm_module.h` |
| PTX-EMU ADR-0029 状态 | ✅ Accepted | `docs/adr/ADR-0029-ptxemu-image-executor.md` 第一行 |
| Phase 0 Step 0（amend ADR-0021 D-PTX-1） | ✅ 已 merged | ADR-0029 §Phase 0 5 gates 全部 PASS |
| Phase 0 Step 1（4 个全局符号搬迁） | ✅ 已完成 | ADR-0029 §Phase 0 Step 1 标 ✅ |
| Phase 1 (perf gate 6) | ✅ cute_rmsnorm D3 0.183x (margin 81%) | ADR-0029 §Phase 1 perf |
| Phase 1 (cpptlm_bridge.h governance) | ✅ `git diff` 为空 | ADR-0029 §Phase 1 governance |
| Phase 1 (test_cpptlm_module.cpp) | ✅ 覆盖 5 ABI + invalid handle + concurrent serialization | ADR-0029 §Phase 1 tests |
| Tag `v0.1.0` 发布 | ✅ | `git tag v0.1.0` 在 PTX-EMU 仓 |

**PTX-EMU 仓 HARD gate**：**✅ CLEARED**（per ADR-076 §Acceptance gate 1："PTX-EMU 仓 ship 确认 gate"）

#### 2.2.3 TaskRunner 仓评审现状（SOFT gate）

| 验收项 | 状态 | 证据 |
|--------|------|------|
| tadr-307 文档存在 | ✅ | `/workspace/project/UsrLinuxEmu/external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md` |
| tadr-307 状态 | 🔄 PROPOSED (2026-08-09) | frontmatter `STATUS: PROPOSED` |
| TaskRunner owner 评审 | ⏳ PENDING | （尚未通过 CI gate） |
| 待澄清问题 | ⚠️ 2 项 | (a) `image_size` 参数缺失推理策略；(b) cu_launch.cpp 默认走新路径 vs legacy name-based 路径 |

**TaskRunner 仓 SOFT gate**：**⏳ PROPOSED** — per ADR-076 §Acceptance gate 2：未通过 → ADR 仍可 Accepted，集成延后。**不阻塞** UsrLinuxEmu HAL extension 启动。

---

## 3. 架构预条件验证

### 3.1 HAL append-only 治理（ADR-023 §D4）

| 检查项 | 当前 | 目标 | 验证 |
|--------|------|------|------|
| 当前 HAL fn-ptr 数量 | 65 + 1 helper = 66 | 65 + 3 + 1 helper = 69 | ✅ append-only，零修改现有 |
| 现有 65 个 fn-ptr 签名 | 不变 | 不变 | ✅ 严格保留（per ADR-023 §D4）|
| `hal_user.cpp` / `hal_mock.cpp` 调用方 | 现有调用零修改 | 仅 `.h` 文件需要更新 + `.cpp` 追加 3 个实现 | ✅ 兼容 |

**结论**：HAL append-only 治理完全 valid。HAL 65 → 68 fn-ptrs（追加 3 个），符合 ADR-023 §D4 + ADR-061/062 模式。

### 3.2 ioctl 编号兼容性

当前 System C ioctl 占用分布（`plugins/gpu_driver/shared/gpu_ioctl.h`）：

| 范围 | 用途 |
|------|------|
| 0x01-0x03 | pushbuffer / MMU event / firmware |
| 0x10-0x13 | BO ops（alloc/free/map/wait_fence）|
| 0x20 | GET_DEVICE_INFO |
| 0x30-0x32 | VA Space（create/destroy/register_gpu）|
| 0x40-0x47 | Queue（create/destroy/map_ring/query + get_process_aperture/update_queue/map_memory/unmap_memory）|
| 0x50-0x59 | Graph（create/destroy/add_kernel_node/add_memcpy_node/instantiate/launch/destroy_exec）|
| 0x60-0x68 | MEM_POOL 系列（create/destroy/alloc/alloc_async/free_async/set_attr/get_attr/trim/export）|
| **0x27/0x28/0x29** | **FREE** — 在 GET_DEVICE_INFO (0x20) 与 CREATE_VA_SPACE (0x30) 之间的 gap |

**结论**：0x27/0x28/0x29 是 ADR-076 §D1 的合理选择（与 0x01~0x26 范围连续 + 不冲突 MEM_POOL 0x60+）。

> **注**：ADR-076 §D1 文本说"与现有 0x01~0x26 连续"，但实际当前占用延伸至 0x68。建议实施时更新 ADR-076 §D1 末尾注释，明确"0x27/0x28/0x29 位于 0x20 GET_DEVICE_INFO 与 0x30 CREATE_VA_SPACE 之间的 gap，与现有所有 ioctl 无冲突"。

### 3.3 3 区分架构（ADR-036）

| 层次 | 当前 | ADR-076 增量 |
|------|------|--------------|
| ① Linux 内核环境模拟 | 完整（`src/kernel/` + `include/kernel/`）| 零修改 |
| ② 可移植的驱动代码 | 完整（`plugins/gpu_driver/drv/`）| 追加 3 个 ioctl handler（38 → 41 行）|
| HAL（②③ 之间唯一桥） | 65 fn-ptrs | 追加 3 个 fn-ptrs（65 → 68），`hal_user.cpp` 真正 dlsym 调用 PTX-EMU |
| ③ 硬件模拟 | 完整（`plugins/gpu_driver/sim/`）| 零修改 |

**结论**：3 区分架构完全 valid。PTX-EMU 作为 HAL backend（implantation detail）不暴露到 ② 层；TaskRunner 端零 PTX-EMU 链接依赖。

### 3.4 跨仓 commit 顺序协议（ADR-035 §R5.1）

PTX-EMU ADR-0029 §Migration i-iv：

| Step | 内容 | 当前状态 | 备注 |
|------|------|---------|------|
| i | PTX-EMU ship `libptxemu_device.so` + `cpptlm_module.h` + tag v0.1.0+ | ✅ DONE | tag `v0.1.0` 已发布 |
| ii | UsrLinuxEmu ship HAL extension + bump submodule pointer | 📋 PENDING | 待 guide-design 启动 |
| iii | TaskRunner ship IGpuDriver extension | 📋 PENDING | tadr-307 review 后可启动 |
| iv | UsrLinuxEmu final integration + ADR-076 → Accepted | 📋 PENDING | depends on ii+iii |

**结论**：Step i 已完成；Step ii 可启动（独立于 Step iii per ADR-076 §Acceptance gate 2 soft gate 性质）。

---

## 4. 风险评估与缓解（per ADR-076 §风险表）

| 风险 | 等级 | 缓解措施 | 验证手段 |
|------|------|---------|---------|
| `libptxemu_device.so` 加载失败 | 🟡 中 | 三级 fallback（env → /opt → RTLD_DEFAULT）+ `-ENOSYS` 返回 | unit test: 模拟 dlsym failure |
| ABI version mismatch | 🟡 中 | 启动时 `module_version()` + `static_assert` + `-EPROTO` | unit test: 模拟 version != CPPTLM_MODULE_VERSION |
| ioctl handler 表 size 不匹配 | 🟢 低 | CMake 检查：handler 表 size == 已定义 ioctl 数量（per ADR-039 既有约束）| build-time 静态检查 |
| MockGpuDriver 字段更新遗漏 | 🟢 低 | ADR-032 既有约束：MockGpuDriver 与 IGpuDriver 字段数必须一致 | taskrunner 端 tadr-307 D4 |
| image bytes deep copy vs mmap 冲突 | 🟡 中 | HAL 层负责 image bytes 持有（PTX-EMU `ptxemu_image_load` 内 deep copy per PTX-EMU ADR-0029 D3）| unit test: validate image 持有 |
| 跨仓 commit 顺序错位 | 🟡 中 | ADR-035 §R5.1 4 步顺序 + CI 校验 | `openspec validate` gate |
| v1 单 kernel 限制拖累集成 | 🟢 低 | `kernel_name[256]` 预留；multi-kernel 解锁 per PTX-EMU ADR-0028 | spec conformance test |

---

## 5. 实施建议（待 guide-design 消费）

### 5.1 建议 OpenSpec change 命名

`openspec/changes/2026-08-15-stage5-ptxemu-kernel-module-hal-extension/`（per ADR-076 §Migration Step 2 推荐）

> **scope 说明**：与 `docs/roadmap/stage-5-multi-engine-pm4.md` 命名碰撞。Stage 5 严格限定为 ADR-049 Phase 6+ / ADR-052 Phase 6.5 触发的 multi-engine Puller + PM4 microcode 工作；cross-repo 集成（如 adr-076）**不属本 stage**，临时挂在 roadmap "跨仓评审中 ADRs" 段单独跟踪。建议 change 命名保留 adr-076 推荐命名但通过 parent_feature 区分（"stage5-ptxemu-kernel-module-hal-extension" 不归入 stage-5 父 feature；建议作为独立 cross-repo feature）。

### 5.2 建议 implementation waves（per ADR-076 §Migration Step 2）

| Wave | 内容 | 验证 |
|------|------|------|
| **W1: spec + header** | `gpu_ioctl.h` 追加 3 ioctl + 3 struct；`gpu_hal.h` 追加 3 fn-ptr + 3 wrapper | header self-contained；现有 65 fn-ptr + 现有 38 ioctl 零修改 |
| **W2: ioctl handler + mock** | `gpgpu_device.cpp` 追加 3 handler + 派发表 3 行；`hal_mock.cpp` 追加 3 mock 实现 | unit test: mock round-trip + 错误码注入 |
| **W3: hal_user dlsym** | `hal_user.cpp` 追加 dlsym + 5 ABI 调用 + version 检查 + 三级 fallback | unit test: dlsym failure → -ENOSYS；version mismatch → -EPROTO |
| **W4: 集成 + e2e** | mock `libptxemu_device.so` + IGpuDriver load → launch → unload | e2e test: 完整 launch round-trip + in-flight unload busy |

### 5.3 TaskRunner 协调建议

**不阻塞**：per ADR-076 §Acceptance gate 2 soft gate 性质，UsrLinuxEmu HAL extension 可独立 ship，TaskRunner 集成延后。

**协调机制**：
- TaskRunner owner 完成 tadr-307 review 后，单列独立 change `igpu-driver-kernel-module-extension` 实施
- UsrLinuxEmu 仓 ship 后，TaskRunner submodule pointer bump（per ADR-035 §R5.1）
- UsrLinuxEmu 仓 final integration（per ADR-076 §Migration Step 4）

---

## 6. 结论与下一步

| 维度 | 状态 |
|------|------|
| ADR-076 arch-side 审计 | ✅ COMPLETE |
| PTX-EMU HARD gate | ✅ CLEARED (2026-08-13) |
| TaskRunner SOFT gate | ⏳ PROPOSED（不阻塞）|
| UsrLinuxEmu HAL extension 实施 | 📋 待 guide-design 启动 |
| arch-done gate（ADR ≥ 1 + roadmap.md）| ✅ PASS（71 ADRs + roadmap.md 存在）|

**下一步**：进入 guide-design 阶段创建 `openspec/changes/2026-08-15-stage5-ptxemu-kernel-module-hal-extension/` proposal artifacts（proposal.md + design.md + tasks.md + specs/*.md）。

---

**最后审计**: 2026-08-13
**审计证据来源**:
- `plugins/gpu_driver/hal/gpu_hal.h`（66 fn-ptr/helper entries；grep regex `^\s+(int|void|size_t|ssize_t|uint64_t|uint32_t) \(\*\w+\)`）
- `plugins/gpu_driver/shared/gpu_ioctl.h`（ioctl 占用至 0x68）
- `/workspace/project/PTX-EMU/build/lib/libptxemu_device.so`（存在）
- `/workspace/project/PTX-EMU/include/cudart/cpptlm_module.h`（存在）
- `/workspace/project/PTX-EMU/git tag v0.1.0`（存在）
- `/workspace/project/PTX-EMU/docs/adr/ADR-0029-ptxemu-image-executor.md`（Accepted + Phase 0+1 gates PASS）
- `/workspace/project/UsrLinuxEmu/external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md`（PROPOSED）
- `docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md`（Proposed 2026-08-09）
- `roadmap.md`（"跨仓评审中 ADRs" 段 + "派生建议" 表）

**维护者**: UsrLinuxEmu Architecture Team
