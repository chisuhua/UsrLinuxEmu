# strengthen-semantic-assertions-for-destroy-va-space-and-query-queue Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 强化 DESTROY_VA_SPACE (0x31) 与 QUERY_QUEUE (0x43) 的语义断言，从 ret==0 提升为字段值 REQUIRE

**Architecture:** 在 tests/test_va_space.cpp 追加 DESTROY_VA_SPACE 的下游失效断言（destroy 二次调用 / destroy 后 create 返回 < 0）；在 tests/test_gpu_plugin.cpp 追加 QUERY_QUEUE 端到端语义 TEST_CASE，验证 queue_type / queue_id / doorbell_offset 字段值。

**Tech Stack:** Catch2 · gpu_va_space_handle · gpu_queue_handle · REQUIRE/CHECK 混合断言

**Priority:** P1  **Wave:** 1

**Generated:** 2026-08-03T02:52:07+00:00 (by guide-ship Phase 1 plan-generation)

---

## File Structure

### Tests

| File | Responsibility |
|------|----------------|
| `tests/test_va_space.cpp` | DESTROY_VA_SPACE 语义 — destroy 二次调用 / destroy 后 create 期望 < 0 |
| `tests/test_gpu_plugin.cpp` | QUERY_QUEUE E2E semantic TEST_CASE 验证字段值 |

---

## Tasks (3)

### Task 1: DESTROY_VA_SPACE 失效语义断言

- Modify: `tests/test_va_space.cpp`
- Test: `tests/test_va_space.cpp`

- [ ] **Step 1: Verify starting state**
```bash
cd build && make test_va_space -j4 && ./bin/test_va_space
Expected: 既有 TEST_CASE PASS, 增强后的新断言因不存在而 PASS (no-op)
```

- [ ] **Step 2: Implementation**
  - tests/test_va_space.cpp: 找到既有 `TEST_CASE("...DESTROY_VA_SPACE...")` 在其后追加新 TEST_CASE:
  -   - `TEST_CASE("DESTROY_VA_SPACE downstream invalidation")`:
  -     `CREATE_VA_SPACE` 拿 handle
  -     `DESTROY_VA_SPACE` (handle) 期望 0
  -     `DESTROY_VA_SPACE` (handle 再次) 期望 `< 0` — 二次 destroy 应返错
  -     `CREATE_QUEUE` (use destroyed va_space_handle) 期望 `< 0`
  -     重复多次 destroy 不引发崩溃 (REQUIRE_NOTHROW 等)
  - **不破坏既有 CREATE_VA_SPACE / CREATE_QUEUE / DESTROY_QUEUE 测试用例。**

- [ ] **Step 3: Run tests to verify they pass**
```bash
cd build && make test_va_space -j4 && ./bin/test_va_space "DESTROY_VA_SPACE*"
Expected: 新 + 既有 TEST_CASE 全 PASS
```

- [ ] **Step 4: Commit**
```bash
git add tests/test_va_space.cpp
git commit -m "test(gpu): strengthen DESTROY_VA_SPACE semantic assertions (destroy invalidation)"
```

### Task 2: QUERY_QUEUE 字段值 E2E 断言

- Modify: `tests/test_gpu_plugin.cpp`
- Test: `tests/test_gpu_plugin.cpp`

- [ ] **Step 1: Verify starting state**
```bash
./bin/test_gpu_plugin "QUERY_QUEUE*"
Expected: 找不到 TEST_CASE → 测试通过但缺失覆盖
```

- [ ] **Step 2: Implementation**
  - tests/test_gpu_plugin.cpp: 追加新 TEST_CASE:
  -   - `TEST_CASE("GPU_IOCTL_QUERY_QUEUE E2E semantic")`:
  -     前置: CREATE_VA_SPACE + CREATE_QUEUE (特定 queue_type=COMPUTE=0 + ring_buffer_size)
  -     QUERY_QUEUE 期望 `ret == 0`
  -     REQUIRE `args.queue_type` 与创建时设定值一致
  -     REQUIRE `args.queue_id != 0`
  -     REQUIRE `args.doorbell_offset ∈ [DOORBELL_ALLOC_BASE, ...)` 合理范围
  - **走完整 plugin 路径（不是 GpgpuDevice(nullptr) 直接调用）** — 区别于 test_stub_handlers_tier2。

- [ ] **Step 3: Run tests to verify they pass**
```bash
cd build && make test_gpu_plugin -j4 && ./bin/test_gpu_plugin "QUERY_QUEUE*"
Expected: 1+ TEST_CASE 全 PASS
```

- [ ] **Step 4: Commit**
```bash
git add tests/test_gpu_plugin.cpp
git commit -m "test(gpu): add QUERY_QUEUE E2E semantic assertions (queue_type + queue_id + doorbell)"
```

### Task 3: 全量 ctest 回归验证


- [ ] **Step 1: Verify starting state**
```bash
N/A
```

- [ ] **Step 2: Implementation**
  - `cd build && ctest --output-on-failure`
  - 确认既有 76+ ctest 全 PASS + 新增强化 TEST_CASE PASS, 0 regression

- [ ] **Step 3: Run tests to verify they pass**
```bash
cd build && ctest --output-on-failure
Expected: 0 failed
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

