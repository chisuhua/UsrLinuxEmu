# ADR-090 Cross-Repo Coordination Annex

> **目的**: 本文档作为 [ADR-090](adr-090-ptxir-via-h2d-dma.md) 跨仓契约修订的实施指南,提供给 PTX-EMU 仓 owner 与 TaskRunner 仓 owner 的 commit-ready spec 草案。
>
> **正式生效条件**: ADR-090 升 ✅ Accepted 后,本 annex 作为 PTX-EMU ADR-0029 §D8 amendment 与 TaskRunner tadr-308 的 draft template。
>
> **最后更新**: 2026-08-17

---

## A. PTX-EMU 仓 ADR-0029 §D8 Amendment 草案

### A.1 修订范围

[PTX-EMU ADR-0029 §D8](../external/PTX-EMU/docs/adr/ADR-0029-ptxemu-image-executor.md#d8-cp-端集成约定--hal-扩展方案usrlinuxemu--ptx-emu-跨仓契约) 当前 spec: 集成点 = UsrLinuxEmu HAL 扩展方案 (per UsrLinuxEmu ADR-076 v1)。

**修订后**: 集成点 = **CppTLM submodule / Mode A interim (sim/ translateLaunch)**, UsrLinuxEmu HAL 不再 dlopen `libptxemu_device.so`。

### A.2 保留内容 (5 个 PTX-EMU ABI 函数原样保留)

```cpp
// libptxemu_device.so 对外暴露的 5 个 ABI 函数 (per cpptlm_module.h)
int         ptxemu_module_version(void);
unsigned long ptxemu_image_load(const void* image_bytes, unsigned long size,
                                unsigned long* out_handle);
int         ptxemu_image_kernel_name(unsigned long handle, char* buf, size_t buf_size);
int         ptxemu_image_execute(unsigned long handle,
                                 const uint32_t grid[3], const uint32_t block[3],
                                 const void* args, unsigned int args_count,
                                 unsigned int shared_mem);
int         ptxemu_image_unload(unsigned long handle);
```

**所有 5 个 ABI 函数签名零修改**——调用方从 UsrLinuxEmu `hal_user.cpp` 改为 CppTLM 内部代码 (或 sim/ `translateLaunch` 回调 Mode A interim)。

### A.3 新承诺 (PTX-EMU 仓)

1. **构建系统支持作为 submodule 链接**: `libptxemu_device.so` 或源代码需支持作为 CppTLM 子模块被构建/链接/装载
2. **符号可见性**: 5 个 ABI 函数符号需 `__attribute__((visibility("default")))` 或 dlopen 时可解析
3. **无全局状态冲突**: PTX-EMU 进程内全局变量 (如有) 不得与 CppTLM 共享 namespace (per ADR-088 §C2 验证)
4. **版本字符串**: `ptxemu_module_version()` 返回 >= 1 (per ADR-076 §D5 Version floor)

### A.4 CppTLM 端集成接口 (草案)

```cpp
// 在 CppTLM header (cpptlm_executor.h 或类似) 中:
#ifdef __cplusplus
extern "C" {
#endif
// CppTLM 调 PTX-EMU 的封装 (Owner: CppTLM maintainer)
int cpptlm_sm_install_ptxir(uint64_t vram_addr, size_t image_size,
                            uint64_t* out_ptx_handle);
int cpptlm_sm_dispatch_kernel(uint64_t ptx_handle,
                              const uint32_t grid[3], const uint32_t block[3],
                              const void* kernargs, uint32_t args_count,
                              uint32_t shared_mem);
int cpptlm_sm_uninstall_ptxir(uint64_t ptx_handle);
#ifdef __cplusplus
}
#endif
```

**接口契约 (与 ADR-090 对齐)**:
- `cpptlm_sm_install_ptxir(vram_addr, image_size)` → 接收 UsrLinuxEmu 写入 VRAM 的 PTXIR bytes (via H2D DMA),返回 PTX-EMU handle
- `cpptlm_sm_dispatch_kernel(handle, grid, block, args...)` → DISPATCH_KERNEL packet 触发 (vs ADR-076 v1 的 `ioctl 0x28`)
- `cpptlm_sm_uninstall_ptxir(handle)` → Code BO 释放时调 (vs ADR-076 v1 的 `ioctl 0x29`)

**注**: 此 C ABI 接口形态需 CppTLM maintainer 在 handoff spec v5.0 中最终定稿 (per Oracle 评审开放问题 3)。

### A.5 跨仓 Commit 顺序

per ADR-035 §R5.1 canonical 流程:

1. **UsrLinuxEmu** ADR-090 ✅ Accepted (canonical source)
2. **PTX-EMU** ADR-0029 §D8 amendment (本文档 A.1-A.4 内容)
3. **CppTLM** handoff spec v5.0 (SM executor 章节 + A.4 接口定稿)
4. **TaskRunner** tadr-308 Accepted (详见 B 节)
5. 代码实施 (PTX-EMU submodule 链接 → CppTLM executor → UsrLinuxEmu HAL cleanup → TaskRunner tadr-308 集成)
6. **UsrLinuxEmu** submodule bump + e2e 集成测试

### A.6 PTX-EMU 仓 owner Action Items

- [ ] 评审 ADR-090 内容 (Context C1-C5 + Decision D1-D5)
- [ ] 评审本文档 A.1-A.4 spec 草案
- [ ] 在 ADR-0029 §D8 创建 amendment 章节 (canonical mirror)
- [ ] 评估 PTX-EMU 构建系统支持 submodule 链接 (Makefile / Bazel / CMake)
- [ ] 评估符号可见性 + 全局状态冲突 (per ADR-088 §C2 4 维验证)
- [ ] ack or 提供 feedback 给 UsrLinuxEmu Architecture Team

---

## B. TaskRunner 仓 tadr-308 草案

### B.1 修订范围

[TaskRunner tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) 当前 spec: 3 个 IGpuDriver 纯虚方法 (#48-50): `load_kernel_module / launch_kernel_module / unload_kernel_module` (mirror UsrLinuxEmu ADR-076 v1 HAL fn-ptrs)。

**修订后 (tadr-308)**: 1 个 IGpuDriver 纯虚方法: `load_kernel_module(image, size) -> vram_addr` (mirror UsrLinuxEmu ADR-090 HAL #66)。

`tadr-307 Superseded by tadr-308` — 维持 mirror 关系, 但内容反映 ADR-090 简化契约。

### B.2 IGpuDriver 接口修订

**保留 #47** (`memPoolExportShareable`, per tadr-305):

```cpp
// 沿用 tadr-301 IGpuDriver 47 方法契约
class IGpuDriver {
 public:
  virtual ~IGpuDriver() = default;
  // ... 现有 47 个方法 (per tadr-301/305) ...

  /** tadr-308 #48: Load PTXIR image bytes to GPU VRAM.
   *  Mirrors UsrLinuxEmu GPU_IOCTL_LOAD_KERNEL_MODULE (ADR-090).
   *  @param image     PTXIR bytes (PTXIR or PTXIR-Embedded CUBIN)
   *  @param size      byte count (must be <= MAX_KERNEL_IMAGE_SIZE)
   *  @param out_vram_addr [OUT] code BO GPU VA; non-zero on success
   *  @return 0 on success, -EINVAL/-ENOMEM/-EFAULT/-ENOSYS per ADR-090 §D1
   */
  virtual int load_kernel_module(const void* image, size_t size,
                                  uint64_t* out_vram_addr) = 0;
  // #49/#50 删除 (ADR-090 §D1: kernel exec via DISPATCH_KERNEL pushbuffer)
};
```

### B.3 移除的方法 (tadr-307 → tadr-308)

```cpp
// 删除 (per ADR-090 §D1):
// virtual int launch_kernel_module(uint64_t handle, ...) = 0;  // #49
// virtual int unload_kernel_module(uint64_t handle) = 0;       // #50
```

**理由**: kernel launch + unload 走 pushbuffer `GPU_OP_DISPATCH_KERNEL` packet + `GPU_IOCTL_FREE_BO`, 不需要独立的 IGpuDriver 方法。

### B.4 cu_module.cpp + cu_launch.cpp 修订

```cpp
// src/umd/libcuda_shim/cu_module.cpp (tadr-308)
CUresult cuModuleLoadData(CUmodule* module, const void* image) {
  // 1. UMD 侧解析 PTXIR header (新增能力, 或直链 libptxemu 作 UMD 辅助)
  //    提取 kernel entry offsets, __constant__, etc.
  // 2. IGpuDriver::load_kernel_module(image, size, &vram_addr)
  //    → 返回 code BO 的 GPU VA
  // 3. 缓存 vram_addr → kernel entry offset 映射 (UMD 内部状态)
  // 4. *module = reinterpret_cast<CUmodule>(vram_addr)  // 伪 handle
  return CUDA_SUCCESS;
}

// src/umd/libcuda_shim/cu_launch.cpp (tadr-308)
CUresult cuLaunchKernel(CUfunction f, ...) {
  // 1. f 是 cuModuleGetFunction 返回的 function handle (含 entry offset)
  // 2. 构造 DISPATCH_KERNEL pushbuffer entry (payload: vram_addr + entry_offset + grid + block + args)
  // 3. GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH (existing path, no change)
  return CUDA_SUCCESS;
}
```

### B.5 UMD 侧 PTXIR 解析能力 (新增)

**open question**: UMD 侧 PTXIR 解析如何实现?

| 方案 | 实现 | 优点 | 缺点 |
|------|------|------|------|
| **A. TaskRunner 自实现 parser** | 解析 PTX/CUBIN header (PTXIR v1 single-kernel 限制) | 无依赖 | parser 代码量 (~500 行 C++ per PTXIR spec) |
| **B. 直链 libptxemu 作 UMD helper** | UMD 调 `ptxemu_image_kernel_name(handle, buf)` 等辅助 API | 复用 PTX-EMU 已 ship 解析逻辑 | 构建依赖 PTX-EMU |
| **C. 预留 query ioctl** | GPU_IOCTL_QUERY_KERNEL_INFO (新增) → driver 侧解析 PTXIR header | driver 侧维护 | driver 侧增加 PTX 解析依赖,违反 ADR-090 §C2 |

**推荐**: **B** (直链 libptxemu 作 UMD helper)。理由:
- PTX-EMU 仓已 ship PTXIR 解析能力 (per cpptlm_module.h)
- 避免 TaskRunner 自实现 parser (~500 行代码)
- 维护责任清晰 (PTX-EMU 仓负责 PTXIR spec 演进)

### B.6 跨仓 Commit 顺序

per ADR-035 §R5.1 (续 A.5):
4. **TaskRunner** tadr-308 Accepted
5. 代码实施 (TaskRunner 端 cu_module.cpp + cu_launch.cpp + IGpuDriver 改造)
6. **UsrLinuxEmu** submodule bump + e2e 集成测试

### B.7 TaskRunner owner Action Items

- [ ] 评审 ADR-090 内容
- [ ] 评审本文档 B.1-B.5 spec 草案
- [ ] 评估 UMD 侧 PTXIR 解析能力 (方案 A/B/C 选择, 推荐 B)
- [ ] 评估 cuModuleLoadData + cuLaunchKernel 改造范围
- [ ] 评估 IGpuDriver vtable 变更影响 (整体编译可控)
- [ ] ack or 提供 feedback 给 UsrLinuxEmu Architecture Team

---

## C. CppTLM 仓 handoff spec v5.0 草案 (Owner: CppTLM maintainer)

### C.1 范围扩展 (per ADR-088 §D6.2 BREAKING 流程)

[Cpptlm handoff spec v4.0](../05-advanced/cpptlm-v4-implementation-handoff.md) 当前覆盖 23 ABI (dGPU 板卡)。**v5.0** 新增 SM executor 子系统章节,扩展 **+2~3 ABI**:

| ABI (NEW in v5.0) | 描述 | 对应 PTX-EMU API |
|-------------------|------|------------------|
| `cpptlm_sm_install_ptxir` | 安装 PTXIR image 到 CppTLM SM | `ptxemu_image_load` + `ptxemu_image_kernel_name` |
| `cpptlm_sm_dispatch_kernel` | 触发 DISPATCH_KERNEL packet | `ptxemu_image_execute` |
| `cpptlm_sm_uninstall_ptxir` | 卸载 PTXIR | `ptxemu_image_unload` |

### C.2 版本字符串更新

```cpp
const char* cpptlm_emulator_get_version(void) {
  return "v1.1-dgpu-v0";  // per ADR-088 §D6.2 BREAKING flow
  // (was: "v1.0-dgpu-v0" in v4.0)
}
```

### C.3 `tools/docs-audit.sh` BREAKING 检测

扩展检测规则:
- 23 ABI 头文件 (`include/cpptlm_emulator.h`) 变更自动检测
- 追加 new ABI struct → 标记 BREAKING
- 同步升级 minor version (v1.0 → v1.1)

### C.4 CppTLM maintainer Action Items

- [ ] 评审 ADR-090 + 本文档 C.1-C.3 spec 草案
- [ ] 评估 SM executor 子系统设计与 handoff spec v4.0 兼容性
- [ ] 主笔 handoff spec v5.0 (owner: CppTLM maintainer)
- [ ] 评估 PTX-EMU submodule 链接方式 (CMake ExternalProject / git submodule / 系统库)
- [ ] 评估与 Mode A (sim/ translateLaunch) 兼容性
- [ ] ack or 提供 feedback 给 UsrLinuxEmu Architecture Team

---

## D. 综合跨仓时间线

```
Week 0 ────────── Week 2 ────────── Week 4 ────────── Week 6 ────────── Week 12+
[ADR-090 Accepted] [PTX-EMU §D8 amend] [CppTLM v5.0] [TaskRunner tadr-308] [Mode B 切换]
                                          ↓
                              [代码实施跨仓 commit per ADR-035 §R5.1]
                                          ↓
                          [Submodule bump + e2e 集成测试 (Mode A + Mode B 双路径)]
```

**Net 新增工作量 (vs ADR-076 v1)**:
- UsrLinuxEmu: ADR-090 mode A 实施 ~2-3 周
- PTX-EMU: §D8 amendment 文档 0.5 周 (代码零修改,仅 5 ABI 函数签名不变)
- CppTLM: handoff spec v5.0 + SM executor submodule 集成 ~3-4 周 (可与 ADR-088 phase 4 并行)
- TaskRunner: tadr-308 + cu_module.cpp/cu_launch.cpp 改造 ~2 周

**Total**: ~7-10 周, 嵌入 ADR-088 24-32 周主线内净增量 ≈ 0 (Mode A 解耦节省 4-6 周串行时间)

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-08-17 (新增 §E 跟踪表 + §E.0 投递计划 + 状态符号约定)
**下次评审**: ADR-090 升 Accepted 后, 启动 PTX-EMU + TaskRunner + CppTLM 三方 ack 流程
**当前阶段**: 三方 RFC 投递准备就绪 (Plan A, critical path = CppTLM)

---

## E. 跨仓 Ack 跟踪表

由 UsrLinuxEmu Architecture Team 维护。每收到一方 ack 后更新对应行。

**投递方案**: 方案 A — 三仓并行开 RFC issue (GitHub Issue), CppTLM 优先 push (critical path, v5.0 BREAKING 流程主开关)。详见顶部决策摘要。

| Gate | 端 | Draft Anchor | Issue / PR | Owner | Status | Opened Date | Closed/Patched | Notes |
|------|-----|--------------|------------|-------|--------|-------------|----------------|-------|
| #2 | CppTLM | annex §C | [#18](https://github.com/chisuhua/CppTLM/issues/18) | _CppTLM maintainer_ | 🟡 v2 Patch Posted | 2026-08-17 | 2026-08-17 | handoff spec v5.0 + SM executor submodule 集成 — **critical path**, 仍 OPEN 等 owner ack |
| #3 | PTX-EMU | annex §A | [#12](https://github.com/chisuhua/PTX-EMU/issues/12) | _PTX-EMU owner_ | 🚫 Closed (superseded by v2) | 2026-08-17 | 2026-08-17 16:58:50 UTC | ADR-0029 §D8 amendment — **CLOSED**, v1 RFC 4 项硬 BLOCKERS 无法 patch, 关闭保留历史 |
| #4 | TaskRunner | annex §B | [#10](https://github.com/chisuhua/TaskRunner/issues/10) | _TaskRunner owner_ | 🚫 Closed (superseded by v2) | 2026-08-17 | 2026-08-17 16:59:09 UTC | tadr-308 + cu_module.cpp / cu_launch.cpp 改造 — **CLOSED**, v1 RFC 5 项 BLOCKERS 无法 patch, 关闭保留历史 |

**状态符号**:
- `🟡 v2 Patch Posted` — v2 patch 已就地推送到 issue, 等待 owner ack
- `🚫 Closed (superseded by v2)` — issue 已关闭, v1 RFC 已被 v2 取代, 历史保留
- `🟢 Approved` — 对方 issue 评论 / label 显示 ack
- `✅ Acked` — 对方 PR 已 merge 或显式 ack + Synced Date 已填

**Ack 收齐判定**: #2 row ✅ Acked (含 Ack Date) → 由 UsrLinuxEmu Architecture Team 触发 ADR-090 状态升级 (见 §F)。#3 / #4 已关闭但仍作为 v2 修订依据被引用, 不阻塞 ADR-090 升级。

### E.0 投递计划 (Plan A)

- **Day 0 (2026-08-17)**: 三仓开 RFC issue, 投递 cover letter (见 §E.2 / §E.3 / §E.4) ✅ 已完成
- **Day 0 (2026-08-17 16:58-17:00 UTC)**: v1 RFC 关闭 + v2 patch 推送 ✅ 已完成
  - PTX-EMU #12 → closed with v2 close comment (留 v2 链接)
  - TaskRunner #10 → closed with v2 close comment (留 v2 链接)
  - CppTLM #18 → v2 patch posted (保持 OPEN, 等 owner ack)
- **Day 1-3**: 跟进 CppTLM owner 对 v2 patch 的 ack / clarification
- **Day 4-7**: 重点 push CppTLM (v5.0 BREAKING 关键)
- **Day 7-14**: CppTLM owner ack Gate #2 → ADR-090 v2 升 ✅ Accepted
- **Day 14-28**: HSK-6 联发协议 (PTX-EMU 发起) + G-D4 迁移 (CppTLM P0-1) + ANTLR4 spike (P1)
- **Day N > 28**: 若 CppTLM 未 ack, 升级抄送 CppTLM architecture lead

### E.1 评审 cover letter 模板

投递时建议搭配 PR/issue 描述,模板见 §E.2 / §E.3 / §E.4 (按端复用)。

### E.1a v2 重启投递记录 (2026-08-17 16:58-17:00 UTC)

- ✅ PTX-EMU #12 关闭 + v2 close comment: https://github.com/chisuhua/PTX-EMU/issues/12#issuecomment-5317969588
  - 关闭理由: v1 RFC 含 4 项硬 BLOCKERS(F1 5 函数假设错 / F2 §D8 scope 错 / F3 ADR-0029 status 错 / F4 D1 已 ship 冲突),patch 不可挽回可信度
  - 关闭 comment 链接 ADR-090 v2 commit `0e67d41` + Oracle session `ses_fef78854dffeLfDJh7p8ELuMLy` + 等待 HSK-6 公告 ack
- ✅ TaskRunner #10 关闭 + v2 close comment: https://github.com/chisuhua/TaskRunner/issues/10#issuecomment-5317969901
  - 关闭理由: v1 RFC 含 5 项 BLOCKERS(F3 路径错 / F4 删除清单虚构 / F6 接口 diff 错 / F7 PTXIR 解析错 / F11/F12 文件缺失)
  - 关闭 comment 提供 tadr-308 待创建文档路径 + `IGpuDriver` 接口修订草案 diff
- ✅ CppTLM #18 v2 patch 就地更新 (issue 仍 OPEN): https://github.com/chisuhua/CppTLM/issues/18#issuecomment-5317970852
  - patch 内容: v2 ↔ #18 反馈 15 维度映射表(角色反转 / 桥接代码删除两阶段 / ptx_emu_driver_shim.cc / 4 测试文件 / G-D4 / HSK 协议 / namespace tlm / v2.1→v3.0 / SmExecutor / git submodule / CompletionRing / dGPU 最小集 / ANTLR4 / P0-P4 / layered fallback 废除)
  - 3 项关键反转: HSK-6 归属修正(PTX-EMU 发起) / ANTLR4 风险修正(vendored in-tree) / patch 就地而非新 RFC
  - 等 CppTLM owner ack → Gate #2 ✅ → ADR-090 v2 升 ✅ Accepted

### E.1a 投递记录 (2026-08-17)

- ✅ PTX-EMU Issue #12 投递: https://github.com/chisuhua/PTX-EMU/issues/12
- ✅ TaskRunner Issue #10 投递: https://github.com/chisuhua/TaskRunner/issues/10
- ✅ CppTLM Issue #18 投递: https://github.com/chisuhua/CppTLM/issues/18
- ✅ PTX-EMU 仓 issues 启用 (admin 操作, 此前 disabled)
- ⚠️ Label `rfc` 在 TaskRunner/CppTLM 仓不存在, 暂不挂 label (后续 owner ack 后可加)

### E.2 Cover Letter — PTX-EMU Owner

```
Subject: [RFC] ADR-0029 §D8 Amendment — PTX-EMU 集成点迁移到 CppTLM

Hi [PTX-EMU Owner],

UsrLinuxEmu 仓刚 Accepted ADR-090, 决定把 PTX-EMU 集成点从 UsrLinuxEmu HAL
移到 CppTLM submodule。 ADR-090 ⇄ ADR-0029 §D8 amendment 草案见:

  PR/issue: <PR URL 在 PTX-EMU 仓里>
  草案     : docs/05-advanced/adr-090-cross-repo-coordination.md §A
  源头     : UsrLinuxEmu/docs/00_adr/adr-090-ptxir-via-h2d-dma.md
  commit   : c07d245 (feat/hal) + b26412a (docs/adr-090)

涉及你方的变化 (代码零修改, 文档修订):
  - ADR-0029 §D8 新增 "PTX-EMU as CppTLM submodule" 段
  - 5 ABI 函数签名不变, 仅 ownership 注释从 UsrLinuxEmu HAL → CppTLM
  - CppTLM 接口草案 (§A.4) 请同步 review

评审重点 (Gate #3):
  [ ] 5 ABI 函数签名是否被破坏 (C ABI 头文件 cptxemu.h)
  [ ] §D8 文案是否清晰反映"PTX-EMU 不再 dlopen 在 UsrLinuxEmu 进程"
  [ ] 跨仓 commit 顺序 (§A.5) 是否符合你方发布节奏

请 ack 或在该 PR/issue 下回复反馈。预计评审周期 5 个工作日。

Re: docs/05-advanced/adr-090-cross-repo-coordination.md §A
```

### E.3 Cover Letter — TaskRunner Owner

```
Subject: [RFC] tadr-308 — CUDA Launch Path Refactor (UMD 侧 PTXIR 解析)

Hi [TaskRunner Owner],

ADR-090 实施后, TaskRunner UMD 侧的 cu_module.cpp / cu_launch.cpp 需要
适配: 移除 cuda_* ABI 调用, 改为 GPU_IOCTL_LOAD_KERNEL_MODULE +
GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH (DISPATCH_KERNEL packet)。

  PR/issue: <PR URL 在 TaskRunner 仓里>
  草案     : docs/05-advanced/adr-090-cross-repo-coordination.md §B
  源头     : UsrLinuxEmu/docs/00_adr/adr-090-ptxir-via-h2d-dma.md
  变更 commit: c07d245 (UsrLinuxEmu)

涉及你方的变化:
  - IGpuDriver 接口修订 (§B.2): 移除 module_load_execute, 改为
    load_kernel_module(vram_addr) + submit_batch(DISPATCH_KERNEL)
  - cu_module.cpp 重构: PTXIR header 解析迁移到 UMD 侧
    (UMD 职责, 非 driver 职责, per Linux amdgpu 真实架构)
  - cu_launch.cpp 重构: 用 cuLaunchKernel 替代 pushbuffer inject
  - 移除方法清查 (§B.3): tadr-307 已 undefine 的 4 个 cuda_* 函数

评审重点 (Gate #4):
  [ ] IGpuDriver 接口变更是否在 E2E 测试中覆盖
  [ ] UMD 侧 PTXIR 解析是否复用现有 cuobjdump 路径
  [ ] cu_module.cpp → cuobjdump 依赖是否需要版本锁

请 ack。预计 ~2 周工作量, 期望与 UsrLinuxEmu Mode A 切 Mode B 同步。

Re: docs/05-advanced/adr-090-cross-repo-coordination.md §B
```

### E.4 Cover Letter — CppTLM Maintainer

```
Subject: [RFC] handoff spec v5.0 — SM Executor Submodule Integration

Hi [CppTLM Maintainer],

ADR-088 阶段 4 (Week 12-21) 的 SM executor 子系统集成, 现在有了具体
源头 — UsrLinuxEmu ADR-090 决定 PTX-EMU 通过 CppTLM submodule 集成。

  PR/issue: <PR URL 在 CppTLM 仓里>
  草案     : docs/05-advanced/adr-090-cross-repo-coordination.md §C
  源头     : UsrLinuxEmu/docs/00_adr/adr-088-dgpu-complete-simulation.md
                  §D6.2 BREAKING 流程注记 (+2~3 ABI)
  UsrLinuxEmu commit: c07d245 (feat/hal) + b26412a (docs/adr-090)

涉及你方的变化:
  - handoff spec v5.0 (§C.1): 新增 "SM Executor Submodule" 章节
  - 版本字符串 v4.x → v5.0 (BREAKING, per ADR-088 §D6.2 五步流程)
  - +2~3 ABI 扩展 (image install / dispatch / completion callback)
  - tools/docs-audit.sh 增加 BREAKING 检测 (§C.3)

评审重点 (Gate #2):
  [ ] SM executor submodule 集成方式 (CMake ExternalProject vs git
    submodule vs 系统库) — 评估三种方案的优劣
  [ ] 23 ABI 冻结 × +2~3 BREAKING 流程是否被正确触发
  [ ] Mode A (sim/ translateLaunch) 与 Mode B (submodule) 是否能双轨
    并行而不冲突 (Gate 4.7 双路径测试)

请 ack。这一块工作量 ~3-4 周, 与 ADR-088 阶段 4 主线并行。

Re: docs/05-advanced/adr-090-cross-repo-coordination.md §C
```

---

## F. ADR-090 状态升级触发条件

升级路径: `🔄 Proposed` → `✅ Accepted` 当且仅当 6 个 Gate 全部 ✅。

| Gate | Owner | 当前状态 | 升 Accepted 前置 |
|------|-------|----------|------------------|
| #1 HAL append-only | UsrLinuxEmu | ✅ | (已验证) |
| #2 CppTLM maintainer ack | CppTLM | ⏳ | annex §C 投递 + 对方 PR merged |
| #3 PTX-EMU owner ack | PTX-EMU | ⏳ | annex §A 投递 + 对方 PR merged |
| #4 TaskRunner owner ack | TaskRunner | ⏳ | annex §B 投递 + 对方 PR merged |
| #5 Architecture Team | UsrLinuxEmu | ✅ | (已 user ack on 2026-08-17) |
| #6 Oracle | UsrLinuxEmu | ✅ | (sessions ses_ff1f38c57ffe... / ses_ff1ecf07cffe... APPROVED) |

### F.1 升级流程 Checklist (UsrLinuxEmu Architecture Team 执行)

1. [ ] 收集 Gates #2 / #3 / #4 的对方 ack(PR merged 或 issue 显式 approved)
2. [ ] 同步 §E 跟踪表(E 列填入 ack 日期)
3. [ ] 更新 `docs/00_adr/adr-090-ptxir-via-h2d-dma.md` 头部:
   - `Status: 🔄 Proposed` → `Status: ✅ Accepted`
   - `Last updated: 2026-08-17` → `Last updated: <ack 完成日期>`
   - `Acceptance Gates` 表格 6 行全部 ✅
4. [ ] 更新 `docs/00_adr/README.md` 状态分布 (Proposed-1 +0 / Accepted +1)
5. [ ] 更新 `docs/README.md` ADR 计数 + 状态分布行
6. [ ] 触发 openspec change 归档:
   - `openspec/changes/2026-08-17-adr-090-ptxir-via-h2d-dma/` → `archive/`
   - `openspec/changes/INDEX.md` 加一行 `✅ 2026-08-XX 归档`
7. [ ] 提交 follow-up commit: `docs(adr-090): accepted (cross-repo ack 收齐)`
8. [ ] 在 PTX-EMU / TaskRunner / CppTLM 仓的对应 PR/issue 下发"released"回复

### F.2 降级风险 (若某方长期不 ack)

- **3 个月内未 ack**: ADR-090 状态降为 `🟡 Stalled`, 通知 UsrLinuxEmu && 对方仓 Owner 升级优先级
- **6 个月内未 ack**: 进入 ADR-038 风格的"factual orphan" — 保留 ADR 但文档显式标注"未达成共识, 实施暂停"
- **PTX-EMU 仓永久不维护**: 退化为 Mode A 永久 — `sim/translateLaunch.cpp` 维护到下一个硬件模拟器或淘汰

---

**本节(E + F)新增**: 2026-08-17, by UsrLinuxEmu Architecture Team
**下次更新**: 三方 ack 状态变化时同步 §E 表
