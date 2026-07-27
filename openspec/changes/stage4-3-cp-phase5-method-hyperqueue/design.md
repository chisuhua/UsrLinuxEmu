# Design: Stage 4.3 — GPU CP Phase 5

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│ ② Portable Driver (plugins/gpu_driver/drv/)                  │
│                                                               │
│ GpgpuDevice ioctl table:                                      │
│   - GPU_IOCTL_MQD_ACTIVATE / DEACTIVATE / PREEMPT            │
│   - GPU_IOCTL_INTERRUPT_REGISTER / UNREGISTER                 │
│                                                               │
│ shared/mqd.h: MQD struct (②↔③ shared contract)               │
│   - ring state (wptr, rptr, base, size)                      │
│   - batch state (gpfifo_addr, entries, index)                │
│   - scheduling (priority, timeslice, doorbell_id)            │
│   - preempt context (saved state for context switch)         │
│   - perf hooks (ts_queries, counters)                        │
└─────────────────────────────────────────────────────────────┘
                              │ HAL
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ ③ Hardware Sim (plugins/gpu_driver/sim/)                     │
│                                                               │
│ sim/hardware/:                                                 │
│   channel_manager ─── Channel arbitration (RR, 32 channels) │
│   method_codec    ─── UsrNative encode/decode                │
│   mqd_state       ─── MQD state machine + BAR0 HQD regs     │
│   timestamp_query ─── logical tick + handle-based query      │
│                                                               │
│ sim/bar_sim:                                                   │
│   BAR0 MMIO (HQD_ACTIVE, HQD_PREEMPT control bits)           │
│   BAR2 VRAM (MQD backing store)                              │
│                                                               │
│ Puller FSM (extended):                                        │
│   IDLE → CHANNEL_SWITCH → FETCH → DECODE → SEMAPHORE →       │
│   SCHEDULE → DISPATCH → COMPLETE                              │
└─────────────────────────────────────────────────────────────┘
```

## Component Design

### 1. Method Codec (ADR-042)

**File**: `sim/hardware/method_codec.{h,cpp}`

**UsrNative packet format** (pushbuffer abstraction layer):
```c
enum class GpuEngineType : uint8_t { COMPUTE=0, COPY=1, GRAPHICS=2, _RESERVED=3 };

struct gpu_method_packet {
  uint16_t method_addr;  // OP_LAUNCH_KERNEL=0x100, NOTIFY_INTR=0x200
  uint8_t  engine;       // GpuEngineType 
  uint8_t  data_count;   // number of u32 data words following
  uint32_t data[];       // FAM (flexible array member, C99)
};
```

**Entry format extension** (existing `gpu_gpfifo_entry` bitfield extension):
```c
// Bits repurposed from _reserved:15 in existing struct
// NI:1  (method non-incrementing, reserved Phase 5)
// ACQUIRE:1 (semaphore acquire, reserved Phase 5.5)
// PREDICATED:1 (conditional execution, reserved Phase 6)
// Current Phase 5: all reserved bits = 0
```

**Key design decisions**:
- Two-layer: `gpu_method_packet` (variable-length, pushbuffer) → `gpu_gpfifo_entry` (fixed 84 bytes, ring buffer)
- No control flow (deferred to ADR-050 Indirect Buffer)
- format field owned by ADR-052 (AQL/PM4), reserved for future
- encode: drv handler (`GpgpuDevice::handlePushbufferSubmitBatch`)
- decode: sim Puller DECODE stage, dispatch via method_addr lookup table

### 2. Channel Manager (ADR-044)

**File**: `sim/hardware/channel_manager.{h,cpp}`

```cpp
struct ChannelState {
  uint32_t channel_id;
  GpuQueueEmu* queue;         // bound ring buffer queue
  uint64_t gpfifo_addr;       // current batch GPFIFO VA
  uint32_t current_index;
  uint32_t total_entries;
  bool batch_in_flight;
  uint64_t pending_fence_id;  // per-channel fence (extends ADR-040)
};

class ChannelManager {
  static constexpr uint32_t MAX_CHANNELS = 32;  // HyperQueue alignment
  static constexpr uint32_t TIME_SLICE_ENTRIES = 1024;  // entries per RR slice
  
  ChannelState channels_[MAX_CHANNELS];
  uint32_t active_count_;
  uint32_t last_channel_;  // RR cursor
  std::mutex mutex_;       // ioctl write vs Puller read (ADR-044 §D2.2)
  
  void registerChannel(uint32_t id, GpuQueueEmu* queue);
  void submitBatch(uint32_t channel_id, uint64_t gpfifo_addr, uint32_t count, uint64_t fence_id);
  ChannelState* nextReadyChannel();  // Round-Robin, returns nullptr if all idle
  void yieldChannel(uint32_t channel_id);
};
```

**Puller FSM integration**:
```
IDLE → CHANNEL_SWITCH (find next ready) → FETCH (from channel's gpfifo) → 
DECODE → (SEMAPHORE if needed) → SCHEDULE → DISPATCH → COMPLETE → CHANNEL_SWITCH
```

### 3. MQD/HQD State Management (ADR-054)

**File**: `shared/mqd.h` (layout contract, visible to ② and ③)

```c
struct MQD {
  // Ring Buffer State
  uint64_t ring_base_va;
  uint64_t ring_size;
  uint64_t wptr;        // written by ② (driver, via BAR)
  uint64_t rptr;        // written by ③ (hardware, after consumption)
  
  // Batch State
  uint64_t gpfifo_addr;
  uint32_t entry_count;
  uint32_t current_index;
  
  // Scheduling
  uint8_t priority;
  uint16_t timeslice_remaining;
  uint32_t doorbell_id;
  
  // Preempt Context
  uint64_t saved_gpfifo_addr;
  uint32_t saved_index;
  uint32_t saved_entries;
  
  // Profiling
  uint64_t ts_queries[8];
  uint64_t cycle_count;
  
  uint32_t state;       // 0=IDLE, 1=ACTIVE, 2=PREEMPTED
  uint32_t _pad[3];
} __attribute__((packed));
static_assert(sizeof(MQD) == 128, "MQD must be 128 bytes");
static_assert(sizeof(MQD) % 8 == 0, "MQD must be 8-byte aligned");
```

**Key decisions**:
- MQD backing: DMA coherent pool (ADR-073), mirrors real amdgpu GART allocation
- HQD control bits: BAR0 MMIO registers at offset 0x4000+channel*64 via `writel`/`readl`
- State transitions per ADR-054 D4 table (IDLE/ACTIVE/PREEMPTED × activate/deactivate/preempt/destroy)

### 4. Interrupt Model (ADR-048)

**Files**: `sim/hardware/interrupt.{h,cpp}`, HAL extension in `gpu_hal.h`

```c
// HAL ops (ADR-023: append-don't-modify)
void (*interrupt_register)(void* ctx, InterruptVector vector, interrupt_handler_t handler);
void (*interrupt_raise_ex)(void* ctx, uint32_t vector, uint64_t user_data);
// Old interrupt_raise kept for backward compat, deprecated

enum class InterruptVector : uint8_t {
  FENCE_SIGNALED = 0,  // fence completion
  NOTIFY_INTR = 1,     // NOTIFY_INTR entry in pushbuffer
  GPU_FAULT = 2,       // reserved (Phase 6+ MMU)
  ENGINE_HANG = 3,     // reserved (ADR-055 Deferred-Never)
};
```

**Dispatch path** (ADR-048 D5-D6):
```
Puller DECODE (sim thread) →
  interrupt_raise_ex(vector, fence_id) →
    enqueue to kernel_workqueue (ADR-060) →
      workqueue thread calls drv handler →
        handler signals ① WaitQueue →
          WAIT_FENCE ioctl returns
```

### 5. Profiling Hooks (ADR-057)

**Files**: `sim/hardware/timestamp_query.{h,cpp}`

```c
// Sim C-ABI (test backdoor, ioctl exposure deferred to Phase 5.5)
struct SimTimestampQuery;
SimTimestampQuery* sim_timestamp_query_create();
void sim_timestamp_query_record(SimTimestampQuery* q, uint64_t entry_index, uint64_t tick);
int sim_timestamp_query_resolve(SimTimestampQuery* q, uint64_t timeout_ms);
void sim_timestamp_query_destroy(SimTimestampQuery* q);

// Single logical tick counter
extern std::atomic<uint64_t> g_sim_tick;  // incremented per DISPATCH
```

**Puller hook**: DISPATCH stage increments `g_sim_tick` and checks `entry.ts_query` for non-zero handles → records tick.

## Integration Points

### Puller FSM States (ADR-044 D4)
```
IDLE ──→ CHANNEL_SWITCH ──→ FETCH ──→ DECODE ──→ (SEMAPHORE) ──→ SCHEDULE ──→ DISPATCH ──→ COMPLETE ──┐
  ↑                                                                                                      │
  └──────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

### Data Flow: Pushbuffer Submission
```
User ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH)
  → GpgpuDevice::handlePushbufferSubmitBatch
    → method_codec::encode(user_entries, count)
      → ChannelManager::submitBatch(channel_id, gpfifo_addr, count, fence_id)
        → Puller.runLoop() picks up via CHANNEL_SWITCH
          → DECODE: method_codec::decode(entry) dispatches method
          → DISPATCH: GlobalScheduler::enqueue
          → COMPLETE: handleComplete() → interrupt_raise_ex(FENCE_SIGNALED, fence_id)
            → workqueue → handler → WaitQueue::wake → ioctl returns
```

## Migration Plan

1. **Phase 1**: Method codec + ChannelManager skeleton (no FSM change)
2. **Phase 2**: Puller FSM CHANNEL_SWITCH state + channel arbitration
3. **Phase 3**: MQD struct in shared/ + BAR0 HQD registers + state machine
4. **Phase 4**: Interrupt model (register + raise_ex + workqueue dispatch)
5. **Phase 5**: Profiling hooks (tick + timestamp query C-ABI)
6. **Phase 6**: Integration + ctest regression + docs-audit

## Risks

| Risk | Mitigation |
|------|-----------|
| ChannelManager thread safety (ioctl vs Puller) | mutex + Issue #21 snapshot pattern |
| FSM state explosion (5 new subsystems) | Incremental migration, keep existing states working |
| HAL ops signature change breaking build | ADR-023 "append-don't-modify": add new ops, keep old |
| ABI break across TaskRunner shared gpu_types.h | Symbolic link keeps both repos in sync automatically |
| Performance regression from new FSM states | Baseline ctest benchmark, gate on ≤20% regression |
