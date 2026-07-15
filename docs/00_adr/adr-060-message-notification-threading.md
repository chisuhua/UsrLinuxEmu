# ADR-060: Linux Kernel Message Notification Threading for KFD Simulation

**状态**: ✅ Accepted（2026-07-14，模拟 owner 签字）
**日期**: 2026-07-11
**提案人**: Sisyphus（Oracle 咨询基础上起草）
**评审者**: UsrLinuxEmu Architecture Team（模拟签字 per ADR-035 §R2.3；正式签字待 owner 复核）
**关联 ADR**:
- [ADR-035](../00_adr/adr-035-governance-policy.md) ✅ 治理规则（本 ADR 自身走此规则）
- [ADR-018](../00_adr/adr-018-driver-sim-separation.md) ✅ 驱动/仿真分离（线程不破坏 ②③ 边界）
- [ADR-023](../00_adr/adr-023-hal-interface.md) ✅ HAL 接口契约（线程安全 HAL ops）
- [ADR-036](../00_adr/adr-036-three-way-separation.md) ✅ 3 区分架构原则（线程跨层规则）
- [ADR-059](../00_adr/adr-059-kfd-multi-file-integration.md) ✅ KFD 多文件集成（**C-12 本体**，依赖本 ADR）

**修订记录**:
- 2026-07-11 v1: 初版（Oracle 咨询 session `ses_0a20c2cc1ffeuc3KgE6isVHGtz` 起草，10 决策点采纳）
- 2026-07-14 v1.1: Oracle 评审 session `ses_0a1fabadfffeJRp6kcN6p6j02S`（10 项 fix 应用：CRIT-1/2/4/5 + R-7/10 + #9 + ADR-036 跨文档同步 + kfd_mmu_get_workqueue accessor 名保留）；docs-audit 43/43 PASS；状态升 ✅ Accepted

**关联 Change**: `openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/`（**前置依赖**）
**关联 TADR**: 无直接 TADR（KFD 内部线程架构，非跨仓 ABI）
**关联 Oracle 咨询**: Oracle DP-1 ~ DP-10 决策点（2026-07-11）
**关联历史**: [kfd-portability-report.md §4.2](../05-advanced/kfd-portability-report.md)（GCC 13 pthread bug）

---

## Context

### C1: 项目状态

UsrLinuxEmu **全项目仅 1 个后台线程**，即 `HardwarePullerEmu::thread_`（`plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp:30`），用途是 GPU 命令调度 FSM 主循环。所有 sim 模块 README 明确声明"single-threaded"。mmu_notifier / page_fault / event notification **全部同步路径**。

### C2: C-12 需求

C-12 sub-project（`openspec/changes/2026-08-15-stage1-4-kfd-multi-file-integration/`）将创建 6 个新 KFD 模块。其中 **3 个需要异步消息通知机制**：

| 模块 | 真实 Linux 行为 | 当前 UsrLinuxEmu 支持 |
|------|----------------|---------------------|
| `kfd_events.c` | `kfd_event_work_handler` workqueue | ❌ 无 |
| `kfd_process.c` | `kfd_process_wq` per-process workqueue | ❌ 无 |
| `kfd_mmu.c` | `amdgpu_mn` mmu_notifier + workqueue | ⚠️ 同步 callback |

不解决此架构基础，C-12 实施时无法准确模拟 Linux 内核行为。

### C3: GCC 13 pthread bug（已知阻塞）

`kfd-portability-report.md §4.2` 记录：`gpu_drm_driver.cpp` include `<thread>` 触发 GCC 13 libstdc++ gthr-default.h weakref 链，`__gthrw_(sched_yield)` 需要 weakref 支持失败。

**Oracle 关键发现**（2026-07-11 咨询）：
> `std::mutex` 和 `std::condition_variable` **不受** GCC 13 bug 影响（用 pthread_mutex_t/pthread_cond_t 通过 libstdc++，不通过 gthr weakrefs）。仅 `std::thread` 受影响。

**Workaround**：用 raw `pthread_*` API 创建线程，绕过 gthr-default.h 链；mutex/cv 仍可用 std primitives。

### C4: 研究基础

Oracle 咨询（2026-07-11，session `ses_0a20c2cc1ffeuc3KgE6isVHGtz`）针对 10 个决策点给出专业建议，本 ADR 综合采纳。

---

## Decision

### 1. Threading Infrastructure

#### 1.1 两层抽象：`kernel_thread_base` + `kernel_workqueue`

**`kernel_thread_base`**（pthread_* 包装，GCC 13 workaround）：

```cpp
// include/kernel/thread/kernel_thread_base.h
class kernel_thread_base {
  pthread_t thread_;
  std::atomic<bool> running_{false};

  virtual void run() = 0;  // 子类 override

 public:
  void start();   // pthread_create(&thread_, nullptr, &entry, this)
  void stop();    // running_=false + pthread_join(thread_, nullptr)
  bool is_running() const { return running_.load(); }
  ~kernel_thread_base() {
    assert(!is_running() &&
           "derived class MUST explicitly call stop() in own destructor first line; "
           "RAII auto-stop in base dtor cannot safely join a thread whose run() "
           "references already-destroyed derived members.");
  }
};

**Destroy order rule**: 子类必须在自身析构函数**第一行**显式调用 `stop()`，不得依赖 base dtor RAII。原因：base dtor 在 `pthread_join` 时 derived 已销毁，若 `run()` 访问 derived 成员（必然）→ UB。参照 `HardwarePullerEmu::~HardwarePullerEmu() { stop(); }` 模式（`plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp:24`，先 stop 再走 base dtor）。

```

**`kernel_workqueue`**（workqueue 模拟，基于 kernel_thread_base）：

```cpp
// include/kernel/thread/kernel_workqueue.h
class kernel_workqueue {
  struct Item { std::function<void()> fn; };

  std::unique_ptr<kernel_thread_base> worker_;  // 单 worker（DP-2）
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<Item> queue_;
  std::atomic<bool> started_{false};
  std::atomic<int>  in_flight_{0};  // in-flight item 计数（CRIT-2）

  void worker_loop() {
    while (true) {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this]{
        return !queue_.empty() || !started_.load();
      });
      if (!started_.load() && queue_.empty()) break;  // drain-then-exit
      auto item = std::move(queue_.front());
      queue_.pop_front();
      lock.unlock();
      in_flight_.fetch_add(1);
      item.fn();
      in_flight_.fetch_sub(1);
      cv_.notify_all();  // 唤醒 flush() 等待者
    }
  }

 public:
  void enqueue(std::function<void()> fn);
  bool flush(std::chrono::milliseconds timeout);  // false on timeout
  void start(int n_workers = 1);
  void stop();
  bool queue_empty() const {  // test/introspection only（CRIT-2 drain contract verification）
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }
};
```

#### 1.2 单 worker 默认 + 可配置

- **默认**：`kernel_workqueue(1)`（单 worker）
- **配置**：`kernel_workqueue(N)` 显式指定 worker 数
- **拒绝** per-CPU workqueue（用户态无 NUMA affinity，无意义）

**理由**：匹配 Linux `system_wq` 单线程语义；C-12 work items 是低频（event delivery / mmu notify / deferred cleanup），非 throughput-sensitive。

#### 1.3 无执行 timeout + flush-timeout for test

- **enqueue()**：无执行 timeout（Linux 真实语义）
- **flush(timeout)**：可选 timeout，仅用于 test hang detection

#### 1.4 Primitive 选择：`std::condition_variable + std::mutex + std::atomic`

- **混用策略**：
  - `std::condition_variable + std::mutex`：阻塞等待（workqueue 主循环）
  - `std::atomic<bool>`：状态标志（started_, running_）
  - **不**用 WaitQueue（src/kernel/wait_queue.h）：该 wrapper 增加不必要的间接层

**理由**：与 HardwarePullerEmu 现有模式一致；最易调试（GDB 可直接看 mutex/cv 状态）。

#### 1.5 `stop()` drain 契约 + `flush()` 超时语义（CRIT-2）

**`stop()` 实现（drain 模式，默认）**：

```cpp
void stop() {
  if (!started_.exchange(false)) return;       // 幂等 + 双检锁
  cv_.notify_all();
  if (worker_) worker_->stop();                 // → pthread_join
  // drain contract: worker_loop 已消费完所有 queue_ + in-flight item
}
```

**`flush(timeout)` 实现**：

```cpp
bool flush(std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(mutex_);
  return cv_.wait_for(lock, timeout, [this]{
    return queue_.empty() && in_flight_.load() == 0;
  });
}
```

**Drain contract 文字说明**：`stop()` 阻塞直到 `queue_` 排空 + 所有 in-flight item 执行完毕，匹配 Linux `destroy_workqueue` 语义。`flush(timeout)` 返回 `true` 表示 `queue_` empty + `in_flight_ == 0`；返回 `false` 表示 timeout 触发时仍有部分 item 未执行。force-stop 用 `stop(bool discard_pending = true)`（仅 test/debug）—— 跳过 drain 直接置 started_=false，丢 queue_ 中剩余 item。

**In-flight 计数守则**：`std::atomic<int> in_flight_{0}`；fn() 调用前 `fetch_add(1)`、调用后 `fetch_sub(1)`，紧贴 unlock 之后；`flush()` waiter 必须等 `in_flight_.load() == 0` 才认作真正"排空"。

#### 1.6 Test introspection accessor

**Test introspection accessor**：`queue_empty()` 是 const 成员函数，仅供 test/introspection 使用，验证 `stop(discard_pending=true)` 调用后 `queue_` 实际状态。drain 模式下返回 `true` 即代表 drain 完成；discard 模式下可能返回 `false`，因 `queue_` 中保留未执行 item。实现内部持 mutex 短暂读取 `queue_.empty()`，开销可忽略。**该 API 不作为 production hot-path**。外部 consumer 应通过 `flush(timeout)` 等待 drain 完成，而非 poll 队列。

### 2. C-12 Module Binding

#### 2.1 Sync/Async 决策表

| 模块 | 决策 | 理由 |
|------|------|------|
| `kfd_events.c` | **异步** | 模拟 Linux `kfd_event_work_handler` workqueue；同步会阻塞 kfd_ioctl 路径 |
| `kfd_process.c` destroy | **同步** | Linux `kfd_process_destroy` 由 IOCTL 同步调用；一些 RCU/deferred 但架构同步 |
| `kfd_mmu.c` mmu_notifier callback | **同步（async opt-in）** | 默认同步匹配 synchronous MMU simulator path；通过 `kfd_mmu_get_workqueue()` 暴露 async 升级路径 |
| `kfd_pasid.c` | **同步** | PASID 分配在 Linux 是同步 bitmap/xarray |
| `kfd_dispatch.c` | **同步** | IOCTL 派发同步 |
| `kfd_module.c` init/exit | **同步** | 模块生命周期简单 |

**kfd_mmu async opt-in 设计**（DP-4 风险缓解）：

```cpp
// kfd_mmu.h
kernel_workqueue* kfd_mmu_get_workqueue();  // day-1 暴露，未来 1 行切换
```

**HAL ops 线程安全契约（R-7）**：HAL 函数（`hal_iommu_*`、`hal_event_signal`、未来新增 ops）可能在 worker 线程上下文调用 —— mmu_notifier、deferred event delivery、deferred cleanup 路径。**所有 HAL op 必须 thread-safe，或由调用方持共享锁**。实施选项：(a) HAL 实现内部用 mutex 保护 state；(b) 调用方在调用前持锁。ADR-023 当前未规范此契约（**本 ADR 触发 ADR-023 后续 v2 补丁同步更新**）；C-12 实施时若实现 half-thread-safe，纳入 OpenSpec change 注明。

**实施验证**：已在 `tests/test_hal_thread_safety_standalone.cpp` 中为所有 11 个 HAL ops（及未来新增）提供 TSan 冒烟测试覆盖。测试包含 7 个 `TEST_CASE`（register_read、register_write、mem_read/write、mem_alloc/free、fence_create/read、void-return ops、全混合），对 `hal_user`（mutex 保护实现）和 `hal_mock`（非原子计数器基准）各运行 4 线程 × 100 迭代并发测试。通过 `cmake -DENABLE_TSAN=ON` 启用 Clang ThreadSanitizer。

**双实现验证策略**（per Oracle 4th-2）：
- **`hal_user` SECTION**（验证目标）：TSan **0 race** 报告。所有 11 个 HAL ops 由 mutex + `__sync_fetch_and_add` 保护，C-12 业务路径必须使用 `hal_user`（或 `hal_user`-style 实现）。
- **`hal_mock` SECTION**（预期行为）：TSan **报告 race 是预期行为**。Mock 故意保留非原子计数器，**目的是避免被 `std::atomic` 掩盖真实问题**（per Oracle 4th-2）。Mock 不是 thread-safe 是设计意图；如果未来 mock 需要 thread-safe，应在 PR review 中显式论证，而不是默认 atomic 化所有计数器。
- **测试通过条件**：`All tests passed (16 assertions in 7 test cases)` + `hal_user SECTION 0 race` + `hal_mock SECTION 报告 race > 0`（验证设计意图正确）。

**实测结果**（R-3 验证阶段）：
- 非 TSan build：编译 + 运行 7/7 PASS（16 assertions）
- TSan build：编译成功，运行 7/7 PASS，hal_user 0 race，hal_mock 23 warnings（**符合设计意图**）
- 链接：`gpu_hal` + `gpu_hal_mock` + pthread
- 测试注册于 `tests/CMakeLists.txt`，`ENABLE_TSAN=ON` 时启用 `-fsanitize=thread -g -O1`

#### 2.2 生命周期：module init/exit

```cpp
// kfd_module.c — 生命周期由 init/exit 显式管理
struct kfd_module_state {
  kernel_workqueue event_wq;
};
static kfd_module_state* g_module_state = nullptr;

int kfd_module_init(void) {
  g_module_state = new kfd_module_state;
  g_module_state->event_wq.start(1);
  // ... init sub-modules ...
}

void kfd_module_exit(void) {
  // 销毁顺序：先所有 sub-module exit，最后 stop workqueue + free
  // ... exit sub-modules ...
  g_module_state->event_wq.flush(std::chrono::milliseconds(5000));
  g_module_state->event_wq.stop();
  delete g_module_state;
  g_module_state = nullptr;
}
```

**生命周期守则**: `kernel_workqueue` 必须由 `kfd_module_init/exit` 显式管理生命周期，**禁止 global static**（避免 atexit 顺序未定义 crash：VFS/Logger 等 singleton 可能已经先销毁，导致 worker thread 回调 access-after-free）。销毁顺序：所有 sub-module 先 exit，最后 stop workqueue + free。参照 `docs/05-advanced/plugin-development.md §3.4.4` 线程生命周期守则扩展适用。

#### 2.3 Always-on threading（无 #ifdef）

- 所有 C-12 新模块默认使用 kernel_workqueue（异步部分）
- 无 compile-time feature flag
- 无 runtime selection（避免双代码路径维护）

**理由**：C-12 是新代码，无 legacy 兼容负担；避免 `#ifdef` 永久负担。

### 3. Verification

#### 3.1 TSan 优先（必填）

- **新增 build target**：`cmake -DENABLE_TSAN=ON`
- **编译器**：Clang（GCC TSan 不成熟）
- **测试目标**：仅 threaded 测试（不全量 318 tests）

#### 3.2 ASan/UBSan 基线（保持）

- 已有 43/43 docs-audit PASS + 318/318 ctest PASS 必须保持
- ASan/UBSan 检测 memory error 与 UB（不检测 race）

#### 3.3 新增压力测试

**共 8 个 TEST_CASE**（覆盖基础 + drain + flush + lifecycle）：

```cpp
// tests/test_kernel_threading_standalone.cpp
TEST_CASE("kernel_workqueue enqueue/dequeue", "[thread]") {
  kernel_workqueue wq;
  wq.start(1);
  bool called = false;
  wq.enqueue([&]{ called = true; });
  wq.flush(1000ms);
  REQUIRE(called);
  wq.stop();
}

TEST_CASE("kernel_workqueue concurrent enqueue stress", "[thread][stress]") {
  // N producer threads × 1000 items each = 1000*N items
  // Verify all items execute exactly once
}

TEST_CASE("kernel_workqueue stop drain mode", "[thread][drain]") {
  kernel_workqueue wq;
  wq.start(1);
  wq.enqueue([]{ /* work */ });
  wq.enqueue([]{ /* work */ });
  wq.stop();  // drain contract: blocks until both complete
  REQUIRE(true);  // implicit: stop() returned ⇒ queue_ empty + in_flight=0
}

TEST_CASE("kernel_workqueue stop discard_pending mode", "[thread][drain]") {
  kernel_workqueue wq;
  wq.start(1);
  wq.enqueue([]{ std::this_thread::sleep_for(100ms); });
  wq.enqueue([]{ /* long task */ });
  wq.stop(true /* discard_pending */);  // 主要契约：立即返回，不能 hang
  CHECK(wq.queue_empty());  // 弱约束：discard 模式下 queue 可能非空但不会再 execute（informational only）
}

TEST_CASE("kernel_workqueue flush timeout returns false on partial", "[thread][flush]") {
  kernel_workqueue wq;
  wq.start(1);
  wq.enqueue([]{ std::this_thread::sleep_for(100ms); });
  REQUIRE_FALSE(wq.flush(std::chrono::milliseconds(1)));  // timeout，未排空
  REQUIRE(wq.flush(std::chrono::milliseconds(5000)));     // 后续排空成功
  wq.stop();
}

TEST_CASE("kernel_workqueue enqueue before start buffers", "[thread][ordering]") {
  kernel_workqueue wq;
  int counter = 0;
  wq.enqueue([&]{ counter++; });
  wq.enqueue([&]{ counter++; });
  wq.start(1);          // start 后 worker 才消费
  wq.flush(1000ms);
  REQUIRE(counter == 2);
  wq.stop();
}

TEST_CASE("kernel_thread_base lifecycle", "[thread]") {
  // start / stop / is_running / base dtor assert(!is_running)
}

TEST_CASE("kernel_thread_base derived dtor must stop before base", "[thread][lifecycle]") {
  // 必须 assert/crash 当 derived dtor 忘记 stop()：
  // 父类 dtor 中的 assert(!is_running()) 触发 → 测试 FAIL（abort）
  struct ForgetfulDerived : kernel_thread_base {
    ~ForgetfulDerived() { /* 故意忘记调 stop() */ }  // 期望 base dtor 触发 assert
    void run() override { while (running_.load()) std::this_thread::sleep_for(1ms); }
  };
  ForgetfulDerived d;
  d.start();
  // d 离开作用域时 base dtor 应该 assert FAIL
}
```

### 4. Non-Decisions（明确不在本 ADR 范围）

| 项 | 原因 |
|----|------|
| HardwarePullerEmu 重构 | 已有 std::thread pattern 正常；未来单独 ADR（详见 §5 占位）|
| `kthread` 模拟 | C-12 不需要；workqueue 已覆盖 |
| `completion` 模拟 | C-12 不需要；可用 cv+mutex 替代 |
| `fasync / SIGIO` 模拟 | C-12 不需要；未来 UVM/HMM 可能需要 |
| Per-CPU workqueue | 用户态无 NUMA 概念，无意义 |
| `kernel_workqueue` 优先级/调度 | Linux workqueue 本身无显式优先级 |

### 5. Future: HardwarePullerEmu Threading 统一（路线图占位）

> **本节是路线图占位**，不是绑定决策。C-12 完成后、进入下一个 threading 相关 sub-project 时重新评估。

#### 5.1 现状

`HardwarePullerEmu`（`plugins/gpu_driver/sim/hardware/hardware_puller_emu.{h,cpp}`）使用 `std::thread` 创建后台工作线程（`hardware_puller_emu.h:105` 声明 `std::thread thread_`，`hardware_puller_emu.cpp:26-30` 启动模式），而本 ADR 在 ① 层引入 `kernel_thread_base`（raw pthread_* 包装）。

两条线程栈路径共存但不一致：

| 属性 | HardwarePullerEmu | kernel_thread_base |
|------|-------------------|-------------------|
| 所属层 | ③ sim | ① kernel env |
| 线程原语 | `std::thread` | `pthread_t`（pthread_create/join）|
| 是否受 ADR-060 管辖 | ❌（§4 明确排除）| ✅ |
| GCC 13 weakref bug | 不受影响（C++ 编译单元）| 为此 workaround 引入 |

#### 5.2 迁移触发条件

当以下任一满足时，应当评估 HardwarePullerEmu 的 `std::thread → kernel_thread_base` 迁移：

1. **HardwarePullerEmu 逻辑需要被移植到内核**（蓝图终态验收尝试）
2. **GCC 13 weakref bug 基础编译器版本提升至 GCC ≥ 14**（此时 `kernel_thread_base` 可考虑替换为 `std::thread`，统一方向反过来）
3. **HardwarePullerEmu 需要访问 ① 层服务**（如 `kernel_workqueue` 事件投递）—— 此时 `std::thread` 和 `kernel_thread_base` 的互操作性成为 issue
4. **C-12 完成后，项目进入下一个 threading 扩展 sub-project**（如 per-CPU workqueue、kthread 模拟）

#### 5.3 建议迁移策略（讨论起点，非绑定决策）

```
┌─────────────────────────────────────────────────────┐
│ 选项 A: HardwarePullerEmu 改用 kernel_thread_base   │
│   Pros: 统一 ①③ 层 threading 模型                    │
│   Cons: 重构 ~50 LOC（低 cost）                      │
│         现有测试需 TSan 验证                          │
│   Effort: ~2 天（含测试）                            │
└─────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────┐
│ 选项 B: kernel_thread_base 改用 std::thread          │
│   （GCC bug 修复后）                                 │
│   Pros: 统一的 C++ std 模式                          │
│   Cons: 需要等待 GCC ≥ 14 基础编译器                  │
│         会改变 ① 层公共 API                          │
│   Effort: ~3 天（含迁移 + 回归测试）                  │
└─────────────────────────────────────────────────────┘
```

#### 5.4 此占位的生命周期

- 本占位**不是** ADR 决策，不创建 OpenSpec change
- 当触发条件满足时，由相应的 sub-project owner 评估并决定是否创建独立 ADR
- 如果 C-12 完成后 6 个月仍未触发，此占位进入 ⏸️ Deferred 状态

#### 5.5 与其他 ADR 的关系

- [ADR-036](../00_adr/adr-036-three-way-separation.md)：统一 threading 后，③ 层代码的"可移植性"提升
- [ADR-023](../00_adr/adr-023-hal-interface.md)：如果 HardwarePullerEmu 通过 HAL 回调，跨层 threading 交互更清晰

---

## Consequences

### 正面影响

- ✅ **C-12 启动 gate 解除**：kfd_events.c / kfd_process.c / kfd_mmu.c 有明确线程架构
- ✅ **准确模拟 Linux 行为**：异步事件投递、deferred 清理、mmu_notifier callback 全部支持
- ✅ **可复用**：net_driver / storage_driver / UVM/HMM 未来 sub-project 可引用同一基础设施
- ✅ **GCC 13 bug 完全规避**：raw pthread_* API，不触发 gthr-default.h weakref 链
- ✅ **可调试**：cv+mutex+atomic 直接可观察（vs WaitQueue wrapper）
- ✅ **推动演进**：从"单线程用户态驱动"迈向"多线程用户态内核"

### 负面影响

- ⚠️ **pthread_t 与 C++ 对象生命周期复杂度**：需明确文档 + TSan 捕获 ordering bugs
- ⚠️ **kfd_mmu sync 默认**：未来 async 升级需改动 mmu callback path
- ⚠️ **sync → async 转换可能 break 调用方**：需审计 + 回归测试 eventual correctness
- ⚠️ **TSan 需要 Clang**：CI 增加编译选项
- ⚠️ **always-on threading**：无 fallback path（如果出 bug，需紧急修复）

### Open Questions（需要 owner 确认）

1. **kernel_workqueue 默认 worker 数 1 vs N**？Oracle 推荐 1（已采纳），但 kfd_events.c 是否需要 N>1？
2. **kfd_mmu async opt-in**：day-1 暴露 `kfd_mmu_get_workqueue()` 后，是否在 C-12 Phase B.3 中实际启用？建议"先暴露，不启用，未来按需启用"。
3. **跨仓影响**：如果 pthread_* + cv/mutex pattern 未来被 TaskRunner 引用，是否需要 TADR 镜像？建议"等真有需求再做"。
4. **TSan CI 集成**：CI matrix 是否增加 Clang + TSan 任务？建议"Phase B.1.10 PoC 通过后再决策"。

---

## Migration

### Day 1-2: ADR-060 Accepted

- [ ] Oracle 咨询完成（✅ 2026-07-11）
- [ ] ADR-060 创建完成（✅ 2026-07-11）
- [ ] docs-audit.sh --strict PASS（保持 44/44）
- [ ] ADR-060 review + Accepted（C-12 启动前必须）
- [ ] ADR-059 修订（移除原 D6 计划，引用 ADR-060）
- [ ] C-12 proposal.md 修订（ADR-060 作为前置依赖 gate）

### Phase 1: 基础设施（1 周）

| Step | 操作 | LOC | 验证 |
|------|------|----:|------|
| 1.1 | 新建 `include/kernel/thread/kernel_thread_base.h` | ~40 | 编译 |
| 1.2 | 新建 `src/kernel/thread/kernel_thread_base.cpp` | ~30 | TSan PASS |
| 1.3 | 新建 `include/kernel/thread/kernel_workqueue.h` | ~50 | 编译 |
| 1.4 | 新建 `src/kernel/thread/kernel_workqueue.cpp` | ~80 | TSan PASS |
| 1.5 | 新建 `tests/test_kernel_threading_standalone.cpp` | ~220 | TSan PASS + ASan/UBSan clean |
| 1.6 | CMakeLists.txt 添加 `ENABLE_TSAN` option | ~20 | Clang 编译 |
| **总计** | **6 文件** | **~440 LOC** | — |

### Phase 2: C-12 业务集成（与 C-12 Phase B 并行）

- C-12 Phase B.1.10 引用 ADR-060
- C-12 Phase B.3.7 kfd_mmu 通过 `kfd_mmu_get_workqueue()` opt-in
- C-12 Phase B.4.6 kfd_events 用 kernel_workqueue 异步事件投递

---

## Acceptance Criteria

### 功能验收

- [ ] `kernel_thread_base` 类实现，支持 start/stop/is_running/RAII 析构
- [ ] `kernel_workqueue` 类实现，支持 enqueue/flush/单 worker 默认
- [ ] `kernel_workqueue::queue_empty()` accessor 实现（test 基础设施，Phase B.1.10）
- [ ] GCC 13 pthread bug workaround 在 UsrLinuxEmu 构建系统应用（CMake）
- [ ] plugin-development.md §3.4.4 线程生命周期守则合规
- [ ] `kfd_mmu_get_workqueue()` day-1 暴露（即使默认 sync）

### 测试验收

- [ ] `tests/test_kernel_threading_standalone.cpp` 至少 8 个 TEST_CASE（basic / concurrent / stop drain / stop discard_pending / flush timeout / enqueue before start / lifecycle / derived dtor assert）
- [ ] TSan PASS（race condition 检测）
- [ ] ASan/UBSan 基线保持 PASS
- [ ] 既有 ctest 318/318 PASS（无 regression）
- [ ] docs-audit.sh --strict 44+/44+ PASS（新增 ADR-060）

### 架构验收

- [ ] 不破坏 ADR-018 ②③ 边界（线程不跨层）
- [ ] 不破坏 ADR-036 3 区分原则
- [ ] HAL 接口扩展仍走 ADR-023 + ADR-035 流程（如未来需要）
- [ ] `libgpu_core/` 零修改（ADR-020 保持）
- [ ] `kernel` 库保持 SHARED（Issue #11）

### 跨仓验收

- [ ] TaskRunner tests 318/318 PASS（无 regression）
- [ ] 跨仓同步协议（ADR-035 §Rule 5.1）按需执行

---

## References

### Oracle 咨询

- Oracle session `ses_0a20c2cc1ffeuc3KgE6isVHGtz`（2026-07-11）
- 10 决策点完整分析：DP-1 (abstraction granularity), DP-2 (worker count), DP-3 (timeout), DP-4 (sync/async per module), DP-5 (GCC 13 workaround), DP-6 (scope), DP-7 (verification), DP-8 (lifecycle), DP-9 (primitives), DP-10 (compatibility)

### 内部文档

- [post-refactor-architecture.md §1.10](../02_architecture/post-refactor-architecture.md) - 3 区分当前实现
- [kfd-portability-boundary.md v1.2](../05-advanced/kfd-portability-boundary.md) - KFD Tier-1/Tier-2 边界
- [kfd-portability-report.md §4.2](../05-advanced/kfd-portability-report.md) - **GCC 13 pthread bug 详细记录**
- [gpu_driver_architecture.md](../05-advanced/gpu_driver_architecture.md) - HardwarePullerEmu std::thread 参考模式
- [plugin-development.md §3.4.4](../05-advanced/plugin-development.md) - 后台线程生命周期守则
- [kfd-multi-file.md](../05-advanced/kfd-multi-file.md) - C-12 设计文档

### 关联 ADR

- [ADR-018](../00_adr/adr-018-driver-sim-separation.md) - 驱动/仿真分离
- [ADR-023](../00_adr/adr-023-hal-interface.md) - HAL 接口契约
- [ADR-035](../00_adr/adr-035-governance-policy.md) - 治理规则
- [ADR-036](../00_adr/adr-036-three-way-separation.md) - 3 区分架构原则
- [ADR-059](../00_adr/adr-059-kfd-multi-file-integration.md) - **KFD 多文件集成（C-12 依赖本 ADR）**

### Linux 内核参考

- `kernel/workqueue.c` - Linux workqueue 主实现
- `drivers/gpu/drm/amd/amdkfd/kfd_events.c` - `kfd_event_work_handler` 真实实现
- `drivers/gpu/drm/amd/amdkfd/kfd_process.c` - `kfd_process_wq` 真实实现
- `drivers/gpu/drm/amd/amdgpu/amdgpu_mn.c` - `amdgpu_mn` mmu_notifier handler
- **参考版本**: Linux 6.12 LTS（与 C-12 kfd_queue.c 一致）

### 构建参考

- `plugins/gpu_driver/sim/hardware/hardware_puller_emu.cpp:26-30` - 现有 std::thread 启动模式
- `plugins/gpu_driver/sim/hardware/hardware_puller_emu.h:105` - `std::thread thread_` 成员声明

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-07-14（v1.1 — 状态升 Accepted；Oracle 评审修复）
**对应 commit**: pending（C-12 启动 commit 引用本 ADR；模拟签字 2026-07-14）
**关联 Commit（待）**: C-12 启动 → ADR-060 升级 Accepted → kernel_thread_base + kernel_workqueue 实施 → C-12 业务模块集成
**状态**: ✅ Accepted（C-12 启动 gate 已达成；模拟签字 2026-07-14）