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
**最后更新**: 2026-08-17
**下次评审**: ADR-090 升 Accepted 后, 启动 PTX-EMU + TaskRunner + CppTLM 三方 ack 流程