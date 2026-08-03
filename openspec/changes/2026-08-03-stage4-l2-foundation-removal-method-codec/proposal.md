## Why

L2 portability gate (ADR-072 §L2) is blocked by 12 B-class violations. B-class foundation change `2026-08-03-stage4-l2-foundation-hal-fence-method-heap` added 5 new fn-ptrs to `struct gpu_hal_ops`. Fence-id removal change (commit 5072083) cleared 2 of 12. This is removal change #2 of 3 Phase-1 changes.

**This change (Change 2 of 3)**:
- Removes 1 of 12 L2 violations: `gpgpu_device.cpp:17` (`#include "sim/hardware/method_codec.h"`)
- Migrates 1 call site: `method_codec_encode(pkt, nullptr)` → `hal_method_codec_encode(hal_, pkt, nullptr)`
- L2 violation count: 10 → 9

**Why this change**:
- `method_codec.h`: 1 violation, 1 call site — single function, smallest change
- Follows the same pattern as fence-id removal: include removal + 1 fn-ptr call migration
- Unblocks Change 3 (hal-user removal), which is the last Phase-1 change

**Why now**:
- Fence-id removal shipped (commit dc5b9a1)
- Pattern is established and validated (tests pass, 0 regression)
- L2: 10 → 9; after Change 3, L2: 9 → 8 (Phase 1 complete)

**Architectural basis**:
- **ADR-023 §Decision 4** (append-only HAL extension) — fn-ptr added in foundation
- **ADR-072 §Decision 4 revised** (1 foundation + N removal pattern) — this is removal #2
- **ADR-043 §D5** (12 B-class violations) — 1 more removed by this change

## What Changes

- `plugins/gpu_driver/drv/gpgpu_device.cpp`:
  - Remove `#include "sim/hardware/method_codec.h"` (line 17)
  - Replace `method_codec_encode(pkt, nullptr)` with `hal_method_codec_encode(hal_, pkt, nullptr)` (line 362)

## Capabilities

### New Capabilities

(none — uses capabilities from B-class foundation change)

## Impact

- `plugins/gpu_driver/drv/gpgpu_device.cpp` (1 include removed, 1 call site migrated)
- L2 violation count: 10 → 9 (1 violation removed)
- (Not in scope: 9 remaining B-class violations — 1 more Phase-1 + Phase-2 + final removals)

## Out of Scope

- ❌ Removal of `sim/hardware/method_codec.h` from sim/ itself (still needed by hal_user.cpp)
- ❌ `hc->heap` field access migration (separate change: `removal-hal-user`)
- ❌ 9 remaining B-class violations
