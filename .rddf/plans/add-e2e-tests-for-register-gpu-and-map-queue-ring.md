# add-e2e-tests-for-register-gpu-and-map-queue-ring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 GPU_IOCTL_REGISTER_GPU (0x32) + GPU_IOCTL_MAP_QUEUE_RING (0x42) 添加端到端插件路径测试

**Architecture:** 新 Catch2 standalone test_register_gpu_map_queue_ring_e2e_standalone 走完整 plugin 加载 + VFS::open + fops->ioctl 链路，覆盖 0x32 的 smoke + nullptr 负面 + 0x42 的 mmap_ptr 共享内存可观察性 + doorbell_pgoff 校验。

**Tech Stack:** Catch2 · GpuPluginTestFixture · VFS + ModuleLoader 完整链路 · GpuQueueEmu attachSharedMemory

**Priority:** P1  **Wave:** 1

**Generated:** 2026-08-03T02:52:07+00:00 (by guide-ship Phase 1 plan-generation)

---

## File Structure

### Tests

| File | Responsibility |
|------|----------------|
| `tests/test_register_gpu_map_queue_ring_e2e_standalone.cpp` | 新增 Catch2 standalone — 0x32 + 0x42 端到端覆盖 |
| `tests/CMakeLists.txt` | add_standalone_test 注册 |

---

## Tasks (3)

### Task 1: REGISTER_GPU 端到端 smoke（0x32）

- Create: `tests/test_register_gpu_map_queue_ring_e2e_standalone.cpp`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/test_register_gpu_map_queue_ring_e2e_standalone.cpp`

- [ ] **Step 1: Verify starting state**
```bash
cd build && make test_register_gpu_map_queue_ring_e2e_standalone -j4
Expected: build FAIL (target not defined)
```

- [ ] **Step 2: Implementation**
  - tests/test_register_gpu_map_queue_ring_e2e_standalone.cpp:
  -   - `TEST_CASE("REGISTER_GPU end-to-end via /dev/gpgpu0")`:
  -     plugin 加载 → `VFS::instance().open("/dev/gpgpu0", 0)`
  -     构造合法 `gpu_register_gpu_args` → `fops->ioctl(fd, GPU_IOCTL_REGISTER_GPU, &args)` 期望 `ret == 0`
  -   - `TEST_CASE("REGISTER_GPU nullptr returns -EINVAL")`:
  -     `ioctl(fd, GPU_IOCTL_REGISTER_GPU, nullptr)` 期望 `ret == -EINVAL`
  - **承认 stub-only 语义**：不试图验证多 GPU 注册 (Phase 3)。
  - Tests/CMakeLists.txt: `add_standalone_test(test_register_gpu_map_queue_ring_e2e_standalone)`

- [ ] **Step 3: Run tests to verify they pass**
```bash
cd build && make test_register_gpu_map_queue_ring_e2e_standalone -j4 && ./bin/test_register_gpu_map_queue_ring_e2e_standalone "REGISTER_GPU*"
Expected: 2 TEST_CASE 全 PASS
```

- [ ] **Step 4: Commit**
```bash
git add tests/test_register_gpu_map_queue_ring_e2e_standalone.cpp tests/CMakeLists.txt
git commit -m "test(gpu): add REGISTER_GPU end-to-end smoke (0x32) via plugin path"
```

### Task 2: MAP_QUEUE_RING 端到端全语义（0x42）

- Modify: `tests/test_register_gpu_map_queue_ring_e2e_standalone.cpp`
- Test: `tests/test_register_gpu_map_queue_ring_e2e_standalone.cpp`

- [ ] **Step 1: Verify starting state**
```bash
./bin/test_register_gpu_map_queue_ring_e2e_standalone "MAP_QUEUE_RING*"
Expected: Test binary FAIL (test cases not yet added)
```

- [ ] **Step 2: Implementation**
  - 追加 3 个 TEST_CASE 到同一文件:
  -   - `TEST_CASE("MAP_QUEUE_RING end-to-end full semantics")`:
  -     前置: CREATE_VA_SPACE + CREATE_QUEUE (与 tests/test_gpu_plugin.cpp 同顺序)
  -     `ioctl(fd, GPU_IOCTL_MAP_QUEUE_RING, &valid_args)` 期望 `ret == 0`
  -     REQUIRE: `args.mmap_ptr != nullptr`
  -     REQUIRE: `args.doorbell_pgoff` 与 `QUERY_QUEUE` 报告的 `doorbell_offset` 一致
  -   - `TEST_CASE("MAP_QUEUE_RING shared memory observability")`:
  -     通过 `mmap_ptr` 写入已知 32-bit 模式 `0xCAFEBABE`
  -     通过 `GpuQueueEmu` 内部 API / `QUERY_QUEUE` ring head 验证对应偏移读出 `0xCAFEBABE`
  -   - `TEST_CASE("MAP_QUEUE_RING + DESTROY_QUEUE negative path")`:
  -     `MAP_QUEUE_RING` 成功后 `DESTROY_QUEUE` → 旧 mapping 仍可访问 (POSIX 语义)
  -     后续 `MAP_QUEUE_RING` 同 handle 期望 `-ENOENT`
  - **承认 mm-shim 限制**：如果不暴露 test-only `GpuQueueEmu` API，使用 `mmap_ptr` ring head 间接验证。

- [ ] **Step 3: Run tests to verify they pass**
```bash
./bin/test_register_gpu_map_queue_ring_e2e_standalone
Expected: 5 TEST_CASE 全 PASS（2 + 3）
```

- [ ] **Step 4: Commit**
```bash
git add tests/test_register_gpu_map_queue_ring_e2e_standalone.cpp
git commit -m "test(gpu): add MAP_QUEUE_RING end-to-end semantics (mmap_ptr + doorbell + destroy)"
```

### Task 3: 完整 ctest 回归 + 0 regression


- [ ] **Step 1: Verify starting state**
```bash
N/A
```

- [ ] **Step 2: Implementation**
  - `cd build && ctest --output-on-failure` 验证全部测试 PASS
  - 确认 76+ ctest 计数 + 新增 5 TEST_CASE, 0 regression

- [ ] **Step 3: Run tests to verify they pass**
```bash
cd build && ctest --output-on-failure | tee /tmp/ctest-result.log
Expected: 0 failed. 既有 + 新增全 PASS.
```

- [ ] **Step 4: Commit**
```bash
(
n
o
 
c
o
m
m
i
t
 
—
 
c
o
v
e
r
e
d
 
b
y
 
T
a
s
k
 
1
 
+
 
2
)
```

---

## Self-Review Checklist

- [ ] Each task has explicit `Files:` lines (Create/Modify/Test)
- [ ] Each task has TDD 5-step structure (verify starting state, implement, run tests pass, commit)
- [ ] Each task includes a commit step with conventional message
- [ ] No 'TBD' / 'TODO' placeholders
- [ ] Type/method names consistent across tasks
- [ ] `cd build && ctest --output-on-failure` final regression — 0 failed

