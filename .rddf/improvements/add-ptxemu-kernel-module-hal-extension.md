# add-ptxemu-kernel-module-hal-extension

**优先级**: P1 | **来源**: ADR-076 + PTX-EMU ADR-0029 §D8 + arch-side gap analysis (2026-08-13) + Oracle review `ses_008d67937ffeR12KMm5ssrxm8H`
**阶段**: cross-repo（独立于 Stage 5 multi-engine/PM4 per `docs/roadmap/stage-5-multi-engine-pm4.md §scope`） | **分类**: core-impl
**类型**: functional（ADR-076 完整实施；HAL append-only per ADR-023 §D4）

## 架构依据

[ADR-076](docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md) 决定在 UsrLinuxEmu HAL 末尾追加 3 个 fn-ptr（`kernel_module_load` / `kernel_module_execute` / `kernel_module_unload` #66/#67/#68）作为 PTX-EMU `libptxemu_device.so` 的接入点；严格遵循 [ADR-023 §D4](docs/00_adr/adr-023-hal-interface.md) append-only 治理规则。

**姊妹 ADR 模式**：[ADR-061](docs/00_adr/adr-061-hal-iommu-extension.md) / [ADR-062](docs/00_adr/adr-062-hal-event-signal-extension.md)（2026-07-15 Accepted，C-12 已 ship）。**关键区别**：本次 `hal_user.cpp` **真正 dlsym** 调用（非 -ENOSYS 桩 — PTX-EMU 作为 HAL backend 的核心）。

**关联 ADR 集**：

- ADR-023 (HAL append-only) — 总治理规则
- ADR-035 (governance) — 跨仓 commit 顺序
- ADR-036 (3-way separation) — drv/hal/sim 边界
- ADR-039 (MEM_POOL_EXPORT IOCTL pattern) — ioctl 表追加格式
- ADR-061 / ADR-062 (HAL IOMMU/Event Signal extension pattern) — 姊妹 precedent
- ADR-072 (portability validation) — L1 验收
- ADR-076 (canonical source) — 本提案主 ADR

**前置 gate（2026-08-13 arch-side audit 确认）**：

- ✅ PTX-EMU 仓 `libptxemu_device.so` + `cpptlm_module.h` 已 ship + tag `v0.1.0` 已发布
- ✅ PTX-EMU Phase 0+1 全部 gates PASS（5/5 D7 + perf 0.183x + cpptlm_bridge governance diff=0 + test_cpptlm_module coverage）
- ⏳ TaskRunner [tadr-307](external/TaskRunner/docs/shared/adr/tadr-307-igpu-driver-kernel-module-extension.md) PROPOSED（**SOFT gate 不阻塞** per ADR-076 §Acceptance gate 2）

**Oracle 评审纳入**（`ses_008d67937ffeR12KMm5ssrxm8H`，APPROVED-WITH-CONDITIONS — 已全部纳入 proposal）：

- 🔴 C1/C2/C3（CRITICAL）：version floor 改为 `>= 1` + 零 build dep 本地硬编码 prototype + 错误码映射
- 🟡 H1-H4（HIGH）：删除伪 CMake 引用 + `std::call_once` 线程安全 + kernel_name rollback + L2 验收改写
- 🟢 M1-M5（MEDIUM）：含 `${CMAKE_DL_LIBS}`（Oracle M5，glibc < 2.34 兼容 Ubuntu 20.04）
- ⚪ L1-L5（LOW）：ctest 基线 pin + ADR-076 §D1 文案 + 头注释修正

## 范围

### In Scope

**核心代码改动（HAL append-only per ADR-023 §D4）**：

- `plugins/gpu_driver/shared/gpu_ioctl.h` — 追加 3 ioctl (0x27/0x28/0x29) + 3 struct + `#define MAX_KERNEL_IMAGE_SIZE (64ULL * 1024 * 1024)`
- `plugins/gpu_driver/hal/gpu_hal.h` — `struct gpu_hal_ops` 末尾追加 3 fn-ptr (#66/#67/#68) + 3 inline wrapper；修头注释 "10 个" → "65→68"
- `plugins/gpu_driver/drv/gpgpu_device.h` — bump `kNumIoctls` 38→41（编译期防护）
- `plugins/gpu_driver/drv/gpgpu_device.cpp` — `IoctlEntry` 元组表追加 3 行 + 3 个 handler 成员函数
- `plugins/gpu_driver/hal/hal_user.cpp` — `std::call_once` 保护 g_ptxemu_abi 初始化 + 本地硬编码 5 ABI prototypes + version floor = 1 + 三级 fallback + canonical `cudaError_t → errno` 映射表 + kernel_name failure rollback
- `plugins/gpu_driver/hal/hal_mock.cpp` — 追加 3 mock 实现（mock handle + 可注入错误码 map state）
- `plugins/gpu_driver/CMakeLists.txt:33` — 加 `${CMAKE_DL_LIBS}`

**测试改动**：

- `tests/unit/hal/test_hal_kernel_module_standalone.cpp` — 1 binary + 6 SECTIONs（dlsym failure / version mismatch / 并发 ensure_loaded race（TSan）/ in-flight unload / image_size 边界 / kernel_name rollback 无泄漏）
- `tests/unit/test_ioctl_table_coverage_standalone.cpp` — 遍历 41 个 ioctl code 验证表覆盖
- `tests/unit/hal/test_hal_init_completeness_standalone.cpp` — 68 fn-ptr 全非 null

**文档与治理**：

- `docs/00_adr/README.md` — 索引更新（PROPOSED 5→4）
- `docs/00_adr/adr-076-gpgpu-kernel-module-ioctl.md` — amend：C1/C2/C3 + §D1 文案 + §D5 错误码映射表 + D8.4 version 边界
- `openspec/specs/hal-kernel-module-extension/spec.md` — 新 spec

### Out of Scope

- **TaskRunner 仓 `IGpuDriver` 扩展**（独立 tadr-307 review，独立 change，consumer-side 对偶）
- **PTX-EMU 仓 `libptxemu_device.so` 进一步优化**（PTX-EMU owner 责任）
- **异步 launch/fence 机制**（v2 per PTX-EMU ADR-0029 §D6 + TaskRunner `cu_launch.cpp` 当前同步）
- **Multi-kernel image execution**（**刻意 v1 scope cut** — 仅用 `ptxemu_image_kernel_name` = kernels[0]；Oracle C1）
- **L2 kernel-compile 基建**（Oracle H4，列为独立前置 change）
- **重新生成 cpptlm_module.h include 路径**（Oracle C1，零 build dep 原则）
- 修改 PTX-EMU 仓任何代码（已 ship v0.1.0，consumer 端零侵入）
- 修改 TaskRunner 仓任何代码（tadr-307 SOFT gate，独立 track）

## 关键场景

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

## 技术约束

### MUST

- 严格遵守 [ADR-023 §D4](docs/00_adr/adr-023-hal-interface.md) append-only：现有 65 fn-ptrs 签名零修改；新增 3 个追加到 `struct gpu_hal_ops` 末尾（顺序：`kernel_module_load` / `kernel_module_execute` / `kernel_module_unload`）
- 现有 38 ioctl handlers 签名零修改；新增 3 个按 `gpgpu_device.cpp:90` `IoctlEntry{request, name, member-fn}` 元组格式追加
- `gpgpu_device.h:25` `kNumIoctls` constexpr 38→41（编译期防护多余条目）
- `hal_user.cpp` 真正 dlsym（非 -ENOSYS 桩 — 与 ADR-061/062 不同）
- `std::call_once` 保护 `g_ptxemu_abi` 初始化（TaskRunner worker 并发场景）
- Version check 改为 floor `module_version() >= 1`（PTX-EMU shipped v2；v1 符号子集稳定）
- `hal_user.cpp` 本地硬编码 5 个 ABI prototype + version floor = 1，**不 include `<cudart/cpptlm_module.h>`**（零 build dep 原则）
- `ptxemu_module_version` 符号缺失判 -EPROTO（不当作兼容）
- dlsym 三级 fallback：`getenv("PTXEMU_ROOT")` → `/opt/ptxemu/lib/` → `RTLD_DEFAULT`，全部失败返回 -ENOSYS
- image bytes 持有在 HAL 层（PTX-EMU `ptxemu_image_load` 内 deep copy per PTX-EMU ADR-0029 D3）；UsrLinuxEmu ② 层仅持 handle
- D5 错误码降级：load 路径失败统一 -EINVAL（PTX-EMU `image_load` 不可区分 parse/状态满）
- execute/unload 路径通过 canonical `cudaError_t → errno` 映射表（定义在 ADR-076 §D5 修订版；tadr-307 引用），覆盖 `CUDA_SUCCESS / OUT_OF_MEMORY / INVALID_VALUE / INVALID_HANDLE / LAUNCH_FAILED / etc.`
- kernel_name 失败路径 rollback：`image_load` 成功后 `image_kernel_name` 失败 → 先调 `image_unload(handle)` 再返回 -EINVAL（防 PTX-EMU 端 image 泄漏）
- 3 个 ioctl handler 统一约定：返回 HAL rc（负 errno）+ status 字段同时填充作冗余 out-param
- `args_count ≤ 4096`；grid/block 任一为 0 → -EINVAL
- `MAX_KERNEL_IMAGE_SIZE = 64ULL * 1024 * 1024` 定义在 `shared/gpu_ioctl.h`（TaskRunner 经 symlink 同编译）
- `plugins/gpu_driver/CMakeLists.txt:33` 加 `${CMAKE_DL_LIBS}`（glibc < 2.34 兼容 Ubuntu 20.04）

### MUST NOT

- 跨层耦合：`plugins/gpu_driver/drv/` 不直接 #include `<cudart/cpptlm_module.h>` 或任何 PTX-EMU 头文件
- 修改 `hal_mock.cpp` 现有 65 个 mock 实现；新增 3 个 mock 追加到末尾
- 修改 PTX-EMU 仓任何代码（PTX-EMU 已 ship v0.1.0，consumer 端零侵入）
- 修改 TaskRunner 仓任何代码（tadr-307 SOFT gate，独立 track）
- 引入新 `static_assert` 阻断现有 65 fn-ptrs 编译（保持向后兼容）

### SHOULD

- 4 步跨仓 commit 顺序：per ADR-035 §R5.1 + PTX-EMU ADR-0029 §Migration i-iv
- `hal_user.cpp` 注释显式引用 PTX-EMU 治理条款（签名变更必 bump version），让维护者知道本地 prototype 重复的治理边界
- mock 实现支持状态注入（map<handle, error_code>），便于测试路径可重复

## 验收标准

### 代码产出

- [ ] `plugins/gpu_driver/shared/gpu_ioctl.h` 新增 3 个 ioctl (0x27/0x28/0x29) + 3 个结构体 + `MAX_KERNEL_IMAGE_SIZE` 定义
- [ ] `plugins/gpu_driver/hal/gpu_hal.h` `struct gpu_hal_ops` 追加 3 fn-ptr + 3 wrapper + 头注释修正
- [ ] `plugins/gpu_driver/drv/gpgpu_device.h` `kNumIoctls` 38→41
- [ ] `plugins/gpu_driver/drv/gpgpu_device.cpp` 追加 3 个 `IoctlEntry` 元组 + 3 个成员函数 handler（命名 `GpgpuDevice::handle_{load,launch,unload}_kernel_module`）
- [ ] `plugins/gpu_driver/hal/hal_user.cpp` 真正 dlsym + `std::call_once` + version floor check + 三级 fallback + cudaError_t → errno canonical 映射 + kernel_name rollback
- [ ] `plugins/gpu_driver/hal/hal_mock.cpp` mock 实现 + 可注入错误码
- [ ] `plugins/gpu_driver/CMakeLists.txt:33` 加 `${CMAKE_DL_LIBS}`

### 测试产出

- [ ] `tests/unit/hal/test_hal_kernel_module_standalone.cpp` — 1 binary + 6 SECTIONs（dlsym failure / version mismatch / 并发 / in-flight / 边界 / rollback）
- [ ] `tests/unit/test_ioctl_table_coverage_standalone.cpp` — 遍历 41 个 ioctl code 验证表覆盖
- [ ] `tests/unit/hal/test_hal_init_completeness_standalone.cpp` — 68 fn-ptr 全非 null
- [ ] 现有测试基线 `make test` 全部 PASS（145/145 ctest，实施时 pin 当前值）
- [ ] 新增 3 个测试 binary 100% PASS（含 TSan 子集）

### 可移植性验证

- [ ] L1 `tools/check-portability.sh` PASS + drv/ 零新增跨层 include
- [ ] L2 kernel-compile 基建列为独立前置 change（**不在本提案范围**）

### 文档同步

- [ ] ADR-076 amend 完成（C1/C2/C3 + 错误码映射 + 边界处理）
- [ ] `docs/00_adr/README.md` 索引更新（PROPOSED 5→4）
- [ ] `openspec/specs/hal-kernel-module-extension/spec.md` 创建

### 编译 & 静态检查

- [ ] `lsp_diagnostics` 无 error
- [ ] `make -j4` 编译通过，无 warning
- [ ] `ctest --output-on-failure` 全部 PASS

---

**关联审查**: Oracle session `ses_008d67937ffeR12KMm5ssrxm8H`（12min CRITICAL/HIGH/MEDIUM/LOW + KEY RECOMMENDATIONS）
