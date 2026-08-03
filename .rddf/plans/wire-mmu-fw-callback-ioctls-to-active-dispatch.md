# wire-mmu-fw-callback-ioctls-to-active-dispatch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复活跃派发表与 ABI 头的 0x02/0x03 漂移（kNumIoctls 36→38 + handler 转发至 kfd_sim_bridge）

**Architecture:** 在 `GpgpuDevice::getIoctlTablePtr()` 静态表添加 `GPU_IOCTL_REGISTER_MMU_EVENT_CB` / `GPU_IOCTL_REGISTER_FIRMWARE_CB` 两项；私有 handler `handleRegisterMMUCB` / `handleRegisterFirmwareCB` 内部直接调 `kfd_sim_register_mmu_cb` / `kfd_sim_register_firmware_cb`（不走 HAL fn-ptr，与既有 0x40-0x47 KFD handler 同构）。

**Tech Stack:** C++17 · Catch2 (vendored) · GpgpuDevice dispatch table · kfd_sim_bridge · `dispatchCount() == kNumIoctls` 静态不变性

**Priority:** P0  **Wave:** 0

**Generated:** 2026-08-03T02:52:07+00:00 (by guide-ship Phase 1 plan-generation)

---

## File Structure

### Production Code

| File | Responsibility |
|------|----------------|
| `plugins/gpu_driver/drv/gpgpu_device.h` | 派发表计数常量 kNumIoctls (36 → 38) |
| `plugins/gpu_driver/drv/gpgpu_device.cpp` | getIoctlTablePtr() 静态表新增 2 项 + GpgpuDevice 私有 handler 转发函数 |
| `AGENTS.md` | IOCTL 列表确认 0x02/0x03 现为活跃命令 |
| `README.md` | GPU IOCTL 编号表（System C）确认 0x02/0x03 活跃 |

### Tests

| File | Responsibility |
|------|----------------|
| `tests/test_register_cb_ioctl_standalone.cpp` | 新增 Catch2 standalone — plugin 加载 + VFS open + fops->ioctl 验证 0x02/0x03 路径 + sim 侧回调计数 |

---

## Tasks (3)

### Task 1: 扩展派发表 + handler 转发

- Modify: `plugins/gpu_driver/drv/gpgpu_device.h:26`, `plugins/gpu_driver/drv/gpgpu_device.cpp:96-137`
- Test: `(deferred to Task 2)`

- [x] **Step 1: Verify starting state**
  *(production change — verification deferred to Task 2 test or post-commit ctest)*

- [x] **Step 2: Implementation**
  - `plugins/gpu_driver/drv/gpgpu_device.h:26` — `static constexpr size_t kNumIoctls = 38;`
  - `plugins/gpu_driver/drv/gpgpu_device.cpp:96-137` — `getIoctlTablePtr()` 内 36-项表后追加 2 项：
  -   - `{ GPU_IOCTL_REGISTER_MMU_EVENT_CB, &GpgpuDevice::handleRegisterMMUCB }`
  -   - `{ GPU_IOCTL_REGISTER_FIRMWARE_CB, &GpgpuDevice::handleRegisterFirmwareCB }`
  - `plugins/gpu_driver/drv/gpgpu_device.cpp` — 在私有区添加：
  -   - `long GpgpuDevice::handleRegisterMMUCB(void* argp) { if (!argp) return -EINVAL; return kfd_sim_register_mmu_cb(argp); }`
  -   - `long GpgpuDevice::handleRegisterFirmwareCB(void* argp) { if (!argp) return -EINVAL; return kfd_sim_register_firmware_cb(argp); }`
  - 声明在 `GpgpuDevice` 类私有区（与既有 `handleRegisterGPU` 等同位置）。
  - **MUST NOT** 触碰 `gpu_drm_driver.cpp`（DRM 表清理另案 follow-up）；**MUST NOT** 修改 `gpu_ioctl.h`。

- [x] **Step 3: Run tests to verify they pass**
```bash
`cd build && make -j4` 编译通过，无新增 warning；`abi-dump` / `nm` 验证 `handleRegisterMMUCB` 存在
```

- [x] **Step 4: Commit**
```bash
git add plugins/gpu_driver/drv/gpgpu_device.h plugins/gpu_driver/drv/gpgpu_device.cpp
git commit -m "feat(gpu): wire MMU + firmware callback ioctls into active dispatch table (kNumIoctls 36→38)"
```

### Task 2: 新增 E2E 测试 test_register_cb_ioctl_standalone

- Create: `tests/test_register_cb_ioctl_standalone.cpp`
- Modify: `tests/CMakeLists.txt (add_standalone_test)`
- Test: `tests/test_register_cb_ioctl_standalone.cpp`

- [ ] **Step 1: Verify starting state**
```bash
cd build && make test_register_cb_ioctl_standalone -j4
Expected: BUILD/target FAIL or new test binary missing CTest registration
```

- [ ] **Step 2: Implementation**
  - tests/test_register_cb_ioctl_standalone.cpp (Catch2 standalone):
  -   - `TEST_CASE("REGISTER_MMU_EVENT_CB end-to-end via /dev/gpgpu0")`
  -   -   plugin 加载 + `VFS::instance().open("/dev/gpgpu0", 0)`
  -   -   构造合法 `gpu_register_mmu_event_cb_args` 入参 → `fops->ioctl(fd, GPU_IOCTL_REGISTER_MMU_EVENT_CB, &args)` 期望 `ret == 0`
  -   -   验证 sim 侧 `kfd_sim_bridge` MMU notifier 注册表长度从 N → N+1
  -   -   调用 `sim_mmu_invalidate_va(va, len)` → 测试侧回调被触发，计数+1
  -   - `TEST_CASE("REGISTER_FIRMWARE_CB end-to-end")` — 对称，验证 firmware 回调注册表
  -   - `TEST_CASE("nullptr → -EINVAL")` — 两组空指针负面路径
  -   - `TEST_CASE("unhandled request 0xDEADBEEF → -EINVAL")` — fallback 不变性
  -   - `TEST_CASE("dispatchCount() == 38")` — 静态不变性断言
  - Tests/CMakeLists.txt: 添加 `add_standalone_test(test_register_cb_ioctl_standalone)` 行。
  - **MUST NOT** 直接 `#include "sim/"`；通过 `kfd_sim_bridge` 公共 API 访问。

- [ ] **Step 3: Run tests to verify they pass**
```bash
cd build && make test_register_cb_ioctl_standalone -j4 && ./bin/test_register_cb_ioctl_standalone
Expected: 5 TEST_CASE 全 PASS
```

- [ ] **Step 4: Commit**
```bash
git add tests/test_register_cb_ioctl_standalone.cpp tests/CMakeLists.txt
git commit -m "test(gpu): add test_register_cb_ioctl end-to-end coverage for 0x02/0x03"
```

### Task 3: 全量回归验证 + 文档同步

- Modify: `AGENTS.md (IOCTL 列表)`, `README.md (GPU IOCTL 编号表 标注 0x02/0x03 活跃)`

- [ ] **Step 1: Verify starting state**
```bash
N/A (verification only)
```

- [ ] **Step 2: Implementation**
  - AGENTS.md / README.md: 0x02 + 0x03 标记为活跃命令（debounce 之前的「dead code」措辞）
  - `grep -rn '#include.*"sim/' plugins/gpu_driver/drv/` 输出为空 — HAL 边界不变 (ADR-023)

- [ ] **Step 3: Run tests to verify they pass**
```bash
cd build && ctest --output-on-failure
Expected: 既有 PASS + 新增 5 TEST_CASE 全 PASS, 0 regression
git diff plugins/gpu_driver/shared/gpu_ioctl.h  # 必须为空
git diff plugins/gpu_driver/drv/gpu_drm_driver.cpp  # 必须为空
```

- [ ] **Step 4: Commit**
```bash
git add AGENTS.md README.md
git commit -m "docs(gpu): mark GPU_IOCTL_REGISTER_MMU_EVENT_CB / REGISTER_FIRMWARE_CB as active"
```

---

## Self-Review Checklist

- [ ] Each task has explicit `Files:` lines (Create/Modify/Test)
- [ ] Each task has TDD 5-step structure (verify starting state, implement, run tests pass, commit)
- [ ] Each task includes a commit step with conventional message
- [ ] No 'TBD' / 'TODO' placeholders
- [ ] Type/method names consistent across tasks
- [ ] `cd build && ctest --output-on-failure` final regression — 0 failed

