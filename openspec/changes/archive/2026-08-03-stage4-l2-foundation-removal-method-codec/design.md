## Context

B-class foundation change `2026-08-03-stage4-l2-foundation-hal-fence-method-heap` (merged 1b2cbac) added 1 new fn-ptr to `struct gpu_hal_ops`:
- `int (*method_codec_encode)(void *ctx, const gpu_method_packet* pkt, const uint32_t* data)` → wraps `method_codec_encode()`

Inline wrapper in `gpu_hal.h`:
- `hal_method_codec_encode(hal, pkt, data)` → `hal->method_codec_encode(hal->ctx, pkt, data)`

Fence-id removal change (merged dc5b9a1) established the pattern (1 foundation + N removal) and cleared 2 L2 violations. This change follows the same pattern for method_codec.

**Current drv/ usage of `method_codec_encode`** (per `grep -rn 'method_codec' plugins/gpu_driver/drv/`):
- `gpgpu_device.cpp:17` — `#include "sim/hardware/method_codec.h"`
- `gpgpu_device.cpp:354` — comment: "Stage 4.3 Task 1.5: validate method_codec_encode for each valid entry"
- `gpgpu_device.cpp:362` — `method_codec_encode(pkt, nullptr);` (validation call in handleSubmitGraph)

Total: 1 call site in drv/ (in 1 file).

**Architectural basis**:
- **ADR-023 §Decision 4** (append-only HAL extension) — fn-ptr already in struct
- **ADR-072 §Decision 4 revised** — 1 foundation + N removal pattern; this is removal #2
- **ADR-043 §D5** — 12 B-class violations; this change removes 1

## Goals / Non-Goals

**Goals:**

- Remove `#include "sim/hardware/method_codec.h"` from `gpgpu_device.cpp`
- Migrate 1 call site: `method_codec_encode(pkt, nullptr)` → `hal_method_codec_encode(hal_, pkt, nullptr)`
- Update 1 comment (line 354): `method_codec_encode` → `hal_method_codec_encode`
- L2 violation count: 10 → 9 (1 violation removed)
- 0 functional change (same behavior; different call path)
- 0 regression in existing tests

**Non-Goals:**

- ❌ Removal of `sim/hardware/method_codec.h` from sim/ itself (still needed by `hal_user.cpp` to implement the fn-ptr)
- ❌ `hc->heap` field access migration (separate change: `removal-hal-user`)
- ❌ 9 remaining B-class violations (subsequent changes)

## Approach

### Step 1: Read `gpgpu_device.cpp` to find exact call site

```bash
cd /workspace/project/UsrLinuxEmu/.rddf/wt/2026-08-03-stage4-l2-foundation-removal-method-codec
grep -n "method_codec" plugins/gpu_driver/drv/gpgpu_device.cpp
```

### Step 2: Verify `hal_` is accessible in scope

`handleSubmitGraph` (where the call is) has access to `hal_` via the enclosing class (`GpgpuDevice::hal_`).

### Step 3: Migrate gpgpu_device.cpp

- Remove `#include "sim/hardware/method_codec.h"` (line 17)
- Update comment line 354: `method_codec_encode` → `hal_method_codec_encode` (cosmetic)
- Replace line 362: `method_codec_encode(pkt, nullptr);` → `hal_method_codec_encode(hal_, pkt, nullptr);`

### Step 4: Verify

- `make kernel gpu_hal hal_mock gpu_hal_mock gpu_sim gpu_drv gpu_driver_plugin -j4` — clean build
- `make test_context_type_standalone test_pdl_standalone test_priority_sched_standalone -j4` — build + run, all PASS
- `grep '#include.*"sim/hardware/method_codec"' plugins/gpu_driver/drv/` — should return 0 matches
- L2 violation count: `grep -rn '#include.*"sim/\|#include.*"hal/hal_user' plugins/gpu_driver/drv/ | wc -l` — should be 9 (was 10)

## Risks

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| `hal_` is nullptr at call site | Very Low | Call is in `handleSubmitGraph` which uses `hal_` throughout |
| Signature mismatch | Low | Both defined in same header file; compile error catches |
| `bool*` / `gpu_method_packet*` parameter compatibility | Low | All types are POD; safe across boundary |
