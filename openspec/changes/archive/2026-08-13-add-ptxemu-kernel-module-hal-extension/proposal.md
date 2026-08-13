# add-ptxemu-kernel-module-hal-extension

> **提案名**: add-ptxemu-kernel-module-hal-extension
> **优先级**: P1
> **来源**: ADR-076 + PTX-EMU ADR-0029 §D8 + arch-side gap analysis (2026-08-13) + Oracle review `ses_008d67937ffeR12KMm5ssrxm8H`
> **阶段**: cross-repo（独立于 Stage 5 multi-engine/PM4 per `docs/roadmap/stage-5-multi-engine-pm4.md §scope`）
> **分类**: core-impl
> **类型**: functional（ADR-076 完整实施；HAL append-only per ADR-023 §D4）
> **关联 ADR**: ADR-023 (HAL append-only), ADR-035 (governance), ADR-036 (3-way separation), ADR-039 (MEM_POOL_EXPORT IOCTL pattern), ADR-061 (HAL IOMMU extension pattern), ADR-062 (HAL Event Signal pattern), ADR-072 (portability validation), ADR-076 (canonical source)
> **关联 TADR**: [tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) (TaskRunner consumer-side mirror)
> **关联外部 ADR**: [PTX-EMU ADR-0029 §D8](../external/PTX-EMU/docs/adr/ADR-0029-ptxemu-image-executor.md#d8-cp-端集成约定--hal-扩展方案usrlinuxemu--ptx-emu-跨仓契约)
> **关联 Gap Analysis**: [docs/architecture/adr-076-ptxemu-hal-backend-gap-analysis.md](../architecture/adr-076-ptxemu-hal-backend-gap-analysis.md)
> **关联 Improvement**: [improvements/add-ptxemu-kernel-module-hal-extension.md](../improvements/add-ptxemu-kernel-module-hal-extension.md)

---

## Why

[ADR-076](../docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md) 决定在 UsrLinuxEmu HAL 末尾追加 3 个 fn-ptr（`kernel_module_load` / `kernel_module_execute` / `kernel_module_unload` #66/#67/#68）作为 PTX-EMU `libptxemu_device.so` 的接入点；严格遵循 [ADR-023 §D4](../docs/00_adr/adr-023-hal-interface.md) append-only 治理规则。

**姊妹 ADR 模式**：[ADR-061](../docs/00_adr/adr-061-hal-iommu-extension.md) / [ADR-062](../docs/00_adr/adr-062-hal-event-signal-extension.md)（2026-07-15 Accepted，C-12 已 ship）。**关键区别**：本次 `hal_user.cpp` **真正 dlsym** 调用（非 -ENOSYS 桩 — PTX-EMU 作为 HAL backend 的核心）。

**前置 gate（2026-08-13 arch-side audit 确认）**：
- ✅ PTX-EMU 仓 `libptxemu_device.so` + `cpptlm_module.h` 已 ship + tag `v0.1.0` 已发布
- ✅ PTX-EMU Phase 0+1 全部 gates PASS（5/5 D7 + perf 0.183x + cpptlm_bridge governance diff=0 + test_cpptlm_module coverage）
- ⏳ TaskRunner [tadr-307](../external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) PROPOSED（**SOFT gate 不阻塞** per ADR-076 §Acceptance gate 2）

**Oracle 评审修订**（`ses_008d67937ffeR12KMm5ssrxm8H`，**APPROVED-WITH-CONDITIONS** — 已全部纳入）：

| 等级 | 问题 | 修订 |
|------|------|------|
| 🔴 **C1** | PTX-EMU ABI 已升 v2（multi-kernel Phase C4），`== CPPTLM_MODULE_VERSION` 检查 100% 失败 | Version 检查改为 floor `module_version() >= 1`；hal_user.cpp 本地硬编码 5 个 ABI prototypes + version floor = 1，**不 include `<cudart/cpptlm_module.h>`**（零 build dep）；"multi-kernel" 重写为**刻意 v1 scope cut** |
| 🔴 **C2** | D5 错误码对 load 路径不可实现；execute 路径无 cudaError_t → errno 映射表 | D5 降级：load 失败统一 -EINVAL；execute/unload 通过 canonical `cudaError_t → errno` 映射表 |
| 🔴 **C3** | ADR-076 §D3 伪代码与实际派发结构不匹配 | 按 `gpgpu_device.cpp:90` `IoctlEntry` 元组追加 3 行；bump `gpgpu_device.h:25` `kNumIoctls` 38→41 |
| 🟡 **H1** | "CMake 检查 handler 表 size per ADR-039" **不存在** | 删引用；新增 init 完备性测试 + ioctl 表覆盖测试 |
| 🟡 **H2** | `ensure_ptxemu_abi_loaded` 线程不安全 | `std::call_once` 保护 |
| 🟡 **H3** | kernel_name 失败路径泄漏 PTX-EMU 端 image | hal_user 内 rollback `image_unload` 后返回 -EINVAL |
| 🟡 **H4** | L2 验收标准无法验证 | 验收改写为 "L1 `tools/check-portability.sh` PASS + drv/ 零新增跨层 include"；L2 基建列独立前置 |
| 🟢 **M1-M5** | ioctl 返回约定 / args_count 上限 / 测试重组 / MAX_KERNEL_IMAGE_SIZE / -ldl | 全部纳入 |
| ⚪ **L1-L5** | ctest 基线 145 / ADR-076 §D1 文案 / gpu_hal.h 头注释 / version 符号缺失边界 / 治理注释 | 全部顺手修 |

---

## What Changes

### In Scope

**核心代码改动（HAL append-only per ADR-023 §D4）**：

- **`plugins/gpu_driver/shared/gpu_ioctl.h`** — 追加 3 ioctl (0x27/0x28/0x29) + 3 struct + `#define MAX_KERNEL_IMAGE_SIZE (64ULL * 1024 * 1024)`（Oracle M4）
- **`plugins/gpu_driver/hal/gpu_hal.h`** — `struct gpu_hal_ops` 末尾追加 3 fn-ptr (#66/#67/#68) + 3 inline wrapper；修头注释 "10 个" → "65→68"（Oracle L3）
- **`plugins/gpu_driver/drv/gpgpu_device.h`** — bump `kNumIoctls` 38→41（Oracle C3，constexpr 数组上界编译期防护）
- **`plugins/gpu_driver/drv/gpgpu_device.cpp`** — `IoctlEntry` 元组表追加 3 行 + 3 个成员函数 handler：`GpgpuDevice::handle_load_kernel_module(void*)` / `handle_launch_kernel_module(void*)` / `handle_unload_kernel_module(void*)`（Oracle C3）
- **`plugins/gpu_driver/hal/hal_user.cpp`** — `std::call_once` 保护 g_ptxemu_abi 初始化（Oracle H2）；本地硬编码 5 个 ABI prototypes + version floor = 1（Oracle C1 零 build dep）；三级 fallback + version check `>= 1` + kernel_name failure rollback `image_unload`（Oracle H3）+ canonical `cudaError_t → errno` 映射表（Oracle C2）
- **`plugins/gpu_driver/hal/hal_mock.cpp`** — 追加 3 mock 实现（mock handle + 可注入错误码 map<handle, error_code> 状态）
- **`plugins/gpu_driver/CMakeLists.txt:33`** — 加 `${CMAKE_DL_LIBS}`（Oracle M5，glibc < 2.34 兼容 Ubuntu 20.04）

**测试改动**：

- **`tests/unit/hal/test_hal_kernel_module_standalone.cpp`** — **1 binary + 6 SECTIONs**（Oracle M3）：
  1. dlsym failure → -ENOSYS
  2. version mismatch → -EPROTO
  3. 并发 ensure_loaded race（TSan）
  4. in-flight unload → -EBUSY（并发 launch+unload）
  5. image_size 边界 0 / MAX / 超限
  6. kernel_name rollback 无泄漏（mock image_kernel_name 失败验证 image_unload 被调）
- **`tests/unit/test_ioctl_table_coverage_standalone.cpp`** — 遍历 41 个 ioctl code 验证表覆盖（Oracle H1）
- **`tests/unit/hal/test_hal_init_completeness_standalone.cpp`** — hal_user_init/hal_mock_init 后 68 fn-ptr 全非 null（Oracle H1）

**文档与治理**：

- **`docs/00_adr/README.md`** — 索引更新（PROPOSED 5→4）
- **`docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md`** — amend：C1/C2/C3 三处 + §D1 文案修正（"0x27-0x29 位于 0x20 与 0x30 之间 gap"）+ §D5 加 canonical cudaError_t → errno 映射子表 + D8.4 version 符号缺失边界 -EPROTO 处理
- **`openspec/specs/hal-kernel-module-extension/spec.md`** — 新 spec（HAL kernel_module 3 fn-ptrs + ioctl 0x27/0x28/0x29 + 错误码语义）

### Out of Scope

- **TaskRunner 仓 `IGpuDriver` 扩展**（独立 tadr-307 review，独立 change，consumer-side 对偶）
- **PTX-EMU 仓 `libptxemu_device.so` 进一步优化**（PTX-EMU owner 责任）
- **异步 launch/fence 机制**（v2 per PTX-EMU ADR-0029 §D6 + TaskRunner `cu_launch.cpp` 当前同步）
- **Multi-kernel image execution**（**刻意 v1 scope cut** — 仅用 `ptxemu_image_kernel_name` = kernels[0]，非 PTX-EMU 限制；Oracle C1）
- **L2 kernel-compile 基建**（Oracle H4，列为独立前置 change）
- **重新生成 cpptlm_module.h include 路径**（Oracle C1，零 build dep 原则）
- **修改 PTX-EMU 仓任何代码**（已 ship v0.1.0，consumer 端零侵入）
- **修改 TaskRunner 仓任何代码**（tadr-307 SOFT gate，独立 track）

---

## Key Scenarios

```gherkin
GIVEN 用户调用 ioctl(fd, GPU_IOCTL_LOAD_KERNEL_MODULE, &args) 传入 PTXIR bytes
  AND args.image_size ∈ [1, MAX_KERNEL_IMAGE_SIZE]
  AND args.image_ptr 用户态可读
  AND hal_user_init 已完成（68 fn-ptr 全非 null）
  WHEN hal_user_kernel_module_load() 经 std::call_once 首次 dlsym 加载 libptxemu_device.so
    AND image_load(image, size) → handle != 0
    AND image_kernel_name(handle, buf, 256) 成功
  THEN args.out_module_handle = handle
  AND args.kernel_name[256] 填充
  AND ioctl handler 返回 0
  AND 错误场景:
    - image_size == 0 || > MAX → -EINVAL
    - args.image_ptr 不可读 → -EFAULT (真机 hedge；emulator 直接 reinterpret_cast)
    - lib 加载失败 (三级 fallback 全失败) → -ENOSYS
    - lib 加载成功但 ptxemu_module_version 符号缺失 → -EPROTO
    - lib 加载成功但 module_version() < 1 → -EPROTO
    - image_load 返回 0（PTX-EMU 端 parse/deserialize/状态满 全部压成 0）→ -EINVAL
    - image_load 成功但 image_kernel_name 失败 → rollback image_unload 后返回 -EINVAL

GIVEN 已加载 handle，用户调用 ioctl(fd, GPU_IOCTL_LAUNCH_KERNEL_MODULE, &args)
  AND args.module_handle != 0
  AND args.args_count ∈ [0, 4096]
  AND args.grid_{x,y,z} != 0 && args.block_{x,y,z} != 0
  WHEN hal_user_kernel_module_execute() 调用 image_execute(handle, grid, block, args, args_count, shared_mem)
  THEN 同步等待 kernel 完成
  AND args.launch_status 填充 HAL rc 转换后的 cudaError_t 反向映射值
  AND ioctl handler 返回 HAL rc（负 errno，3 个 handler 统一约定）
  AND 错误场景:
    - module_handle == 0 → -EINVAL
    - args_count > 4096 → -EINVAL
    - grid/block 任一维度为 0 → -EINVAL
    - execute 返回非 0 cudaError_t → 按 canonical 表映射为负 errno

GIVEN 用户调用 ioctl(fd, GPU_IOCTL_UNLOAD_KERNEL_MODULE, &args)
  AND args.module_handle != 0
  WHEN hal_user_kernel_module_unload() 调用 image_unload(handle)
  THEN ioctl handler 返回 HAL rc（负 errno）
  AND args.unload_status 同步填充作冗余 out-param
  AND 错误场景:
    - module_handle == 0 → -EINVAL
    - 仍有 in-flight kernel → -EBUSY
    - image_unload 失败 → 按 cudaError_t 表映射
```

---

## Capabilities

### MUST（强制约束）

- **MUST** 严格遵守 [ADR-023 §D4](../docs/00_adr/adr-023-hal-interface.md) append-only：现有 65 fn-ptrs 签名零修改；新增 3 个追加到 `struct gpu_hal_ops` 末尾（顺序：`kernel_module_load` / `kernel_module_execute` / `kernel_module_unload`）
- **MUST** 现有 38 ioctl handlers 签名零修改；新增 3 个按 `gpgpu_device.cpp:90` `IoctlEntry{request, name, member-fn}` 元组格式追加
- **MUST** `gpgpu_device.h:25` `kNumIoctls` constexpr 38→41（编译期防护多余条目）
- **MUST** `hal_user.cpp` 真正 dlsym（非 -ENOSYS 桩 — 与 ADR-061/062 不同）
- **MUST** `std::call_once` 保护 `g_ptxemu_abi` 初始化（TaskRunner worker 并发场景）
- **MUST** Version check 改为 floor `module_version() >= 1`（PTX-EMU shipped v2；v1 符号子集稳定）
- **MUST** `hal_user.cpp` 本地硬编码 5 个 ABI prototype + version floor = 1，**不 include `<cudart/cpptlm_module.h>`**（零 build dep 原则）
- **MUST** `ptxemu_module_version` 符号缺失判 -EPROTO（不当作兼容）
- **MUST** dlsym 三级 fallback：`getenv("PTXEMU_ROOT")` → `/opt/ptxemu/lib/` → `RTLD_DEFAULT`，全部失败返回 -ENOSYS
- **MUST** image bytes 持有在 HAL 层（PTX-EMU `ptxemu_image_load` 内 deep copy per PTX-EMU ADR-0029 D3）；UsrLinuxEmu ② 层仅持 handle
- **MUST** D5 错误码降级：load 路径失败统一 -EINVAL（PTX-EMU `image_load` 不可区分 parse/状态满）
- **MUST** execute/unload 路径通过 canonical `cudaError_t → errno` 映射表（定义在 ADR-076 §D5 修订版；tadr-307 引用），覆盖 `CUDA_SUCCESS / OUT_OF_MEMORY / INVALID_VALUE / INVALID_HANDLE / LAUNCH_FAILED / etc.`
- **MUST** kernel_name 失败路径 rollback：`image_load` 成功后 `image_kernel_name` 失败 → 先调 `image_unload(handle)` 再返回 -EINVAL（防 PTX-EMU 端 image 泄漏）
- **MUST** 3 个 ioctl handler 统一约定：返回 HAL rc（负 errno）+ status 字段同时填充作冗余 out-param
- **MUST** 既有 direct-cast 模式（参考 `gpgpu_device.cpp:334-335` pushbuffer entries_addr）；`args_count ≤ 4096`；grid/block 任一为 0 → -EINVAL
- **MUST** `MAX_KERNEL_IMAGE_SIZE = 64ULL * 1024 * 1024` 定义在 `shared/gpu_ioctl.h`（TaskRunner 经 symlink 同编译）
- **MUST** `plugins/gpu_driver/CMakeLists.txt:33` 加 `${CMAKE_DL_LIBS}`（glibc < 2.34 兼容 Ubuntu 20.04）

### MUST NOT（禁止事项）

- **MUST NOT** 跨层耦合：`plugins/gpu_driver/drv/` 不直接 #include `<cudart/cpptlm_module.h>` 或任何 PTX-EMU 头文件
- **MUST NOT** 修改 `hal_mock.cpp` 现有 65 个 mock 实现；新增 3 个 mock 追加到末尾
- **MUST NOT** 修改 PTX-EMU 仓任何代码（PTX-EMU 已 ship v0.1.0，consumer 端零侵入）
- **MUST NOT** 修改 TaskRunner 仓任何代码（tadr-307 SOFT gate，独立 track）
- **MUST NOT** 引入新 `static_assert` 阻断现有 65 fn-ptrs 编译（保持向后兼容）

### SHOULD（建议）

- **SHOULD** 4 步跨仓 commit 顺序：per ADR-035 §R5.1 + PTX-EMU ADR-0029 §Migration i-iv（无 CI 强制，纯流程约束）
- **SHOULD** `hal_user.cpp` 注释显式引用 PTX-EMU 治理条款（签名变更必 bump version），让维护者知道本地 prototype 重复的治理边界
- **SHOULD** mock 实现支持状态注入（map<handle, error_code>），便于测试路径可重复

---

## Impact

**影响面（affected areas）**：

- **HAL 接口契约层**（plugins/gpu_driver/hal/）：65 → 68 fn-ptrs append-only；`hal_user.cpp` 真正 dlsym；`hal_mock.cpp` 追加 3 个 mock
- **ioctl 契约层**（plugins/gpu_driver/shared/gpu_ioctl.h）：新增 3 ioctl (0x27/0x28/0x29) + 3 struct + `MAX_KERNEL_IMAGE_SIZE`
- **ioctl 派发层**（plugins/gpu_driver/drv/）：3 个成员函数 handler 追加到 `IoctlEntry` 元组表 + `kNumIoctls` 38→41
- **构建系统**（plugins/gpu_driver/CMakeLists.txt:33）：加 `${CMAKE_DL_LIBS}`
- **测试覆盖**：新增 3 个测试 binary（dlsym failure / version mismatch / 并发 / in-flight / 边界 / rollback / ioctl 表覆盖 / init 完备性）
- **ADR 文档**：ADR-076 amend（C1/C2/C3 三处 + 错误码映射表 + 边界处理）
- **spec 文档**：新增 `openspec/specs/hal-kernel-module-extension/spec.md`

**影响边界（boundary）**：

- ✅ **不动 PTX-EMU 仓**：consumer 端零侵入，PTX-EMU 已 ship v0.1.0
- ✅ **不动 TaskRunner 仓**：tadr-307 SOFT gate 独立 track
- ⏸️ **L2 kernel-compile 基建未做**：列为独立前置 change（Oracle H4）
- ✅ **零破坏现有 65 fn-ptrs / 38 ioctl handlers**（HAL append-only 治理严格保留）

**跨仓 commit 顺序**（per ADR-035 §R5.1 + PTX-EMU ADR-0029 §Migration i-iv）：

| Step | 内容 | 状态 |
|------|------|------|
| i | PTX-EMU ship `libptxemu_device.so` + `cpptlm_module.h` + tag v0.1.0+ | ✅ 已完成 |
| ii | UsrLinuxEmu ship HAL extension + bump submodule pointer | 📋 **本提案产出** |
| iii | TaskRunner ship IGpuDriver extension（独立 change per tadr-307 review） | 📋 PENDING |
| iv | UsrLinuxEmu final integration + ADR-076 → Accepted（depends on ii+iii）| 📋 PENDING |

---

## Acceptance

### 代码产出

- [ ] `plugins/gpu_driver/shared/gpu_ioctl.h` 新增 3 个 ioctl (0x27/0x28/0x29) + 3 个结构体 + `MAX_KERNEL_IMAGE_SIZE` 定义
- [ ] `plugins/gpu_driver/hal/gpu_hal.h` `struct gpu_hal_ops` 追加 3 fn-ptr + 3 wrapper + 头注释修正
- [ ] `plugins/gpu_driver/drv/gpgpu_device.h` `kNumIoctls` 38→41
- [ ] `plugins/gpu_driver/drv/gpgpu_device.cpp` 追加 3 个 `IoctlEntry` 元组 + 3 个成员函数 handler（命名 `GpgpuDevice::handle_{load,launch,unload}_kernel_module`）
- [ ] `plugins/gpu_driver/hal/hal_user.cpp` 真正 dlsym + `std::call_once` + version floor check + 三级 fallback + cudaError_t → errno canonical 映射 + kernel_name rollback
- [ ] `plugins/gpu_driver/hal/hal_mock.cpp` mock 实现 + 可注入错误码
- [ ] `plugins/gpu_driver/CMakeLists.txt:33` 加 `${CMAKE_DL_LIBS}`

### 测试产出

- [ ] `tests/unit/hal/test_hal_kernel_module_standalone.cpp` — 1 binary + 6 SECTIONs：
  1. [ ] dlsym failure → -ENOSYS
  2. [ ] version mismatch → -EPROTO
  3. [ ] 并发 ensure_loaded race（TSan）
  4. [ ] in-flight unload → -EBUSY（并发 launch+unload）
  5. [ ] image_size 边界 0 / MAX / 超限
  6. [ ] kernel_name rollback 无泄漏（mock image_kernel_name 失败验证 image_unload 被调）
- [ ] `tests/unit/test_ioctl_table_coverage_standalone.cpp` — 遍历 41 个 ioctl code 验证表覆盖
- [ ] `tests/unit/hal/test_hal_init_completeness_standalone.cpp` — hal_user_init/hal_mock_init 后 68 fn-ptr 全非 null
- [ ] 现有测试基线 `make test` 全部 PASS（**145/145 ctest**，实施时 pin 当前值）
- [ ] 新增 3 个测试 binary 100% PASS（含 TSan 子集）

### 可移植性验证

- [ ] L1 `tools/check-portability.sh` PASS + drv/ 零新增跨层 include（per Oracle H4）
- [ ] L2 kernel-compile 基建列为独立前置 change（**不在本提案范围**）

### 文档同步

- [ ] ADR-076 amend 完成：
  - §D1 文案修正（"0x27-0x29 位于 0x20 GET_DEVICE_INFO 与 0x30 CREATE_VA_SPACE 之间 gap"）
  - §D4 version check 改为 `>= 1` floor
  - §D5 加 canonical `cudaError_t → errno` 映射子表 + load 失败降级 -EINVAL 说明
  - §D5 加 kernel_name 失败 rollback 路径
  - §D5 加 `ptxemu_module_version` 符号缺失 -EPROTO 处理
  - §D8.4 同步上述变更
- [ ] `docs/00_adr/README.md` 索引更新（PROPOSED 5→4）
- [ ] `openspec/specs/hal-kernel-module-extension/spec.md` 创建

### 编译 & 静态检查

- [ ] `lsp_diagnostics` 无 error
- [ ] `make -j4` 编译通过，无 warning
- [ ] `ctest --output-on-failure` 全部 PASS

---

**最后更新**: 2026-08-13（Oracle review `ses_008d67937ffeR12KMm5ssrxm8H` APPROVED-WITH-CONDITIONS；3 CRITICAL + 5 HIGH + 6 MEDIUM + 5 LOW 全部纳入；HARD-GATE 批准后落盘；proposal.md 重写以符合 OpenSpec 规范）
**关联审查**: Oracle session `ses_008d67937ffeR12KMm5ssrxm8H`（12min CRITICAL/HIGH/MEDIUM/LOW + KEY RECOMMENDATIONS）
