# Stage 1.3 HAL uvm-ops Audit

> Per Decision 4 (design.md) + ADR-035 Rule 3 + ADR-027 spec-driven principle.
> Guardrail: HAL extensions are FORBIDDEN unless KFD driver code in
> stage 1.4 demonstrably requires them.

## Decision: Zero `hal_uvm_*` ops added in Stage 1.3

**Status**: ✅ Zero ops added.

Stage 1.3 (UVM/HMM) does **NOT** add any `hal_uvm_*` ops to
`struct gpu_hal_ops`. The HAL interface remains at 11 ops (per ADR-023 cap).

## Rationale

- **ADR-027**: spec-driven incremental principle — no pre-adding
  ops without demonstrable KFD call site evidence
- **ADR-035 Rule 3**: every HAL op addition requires a full ADR
  process (proposal + review + approval)
- **Decision 4 (design.md §Decision 4)**: defer all `hal_uvm_*`
  additions to Stage 1.4 (KFD integration), triggered ONLY when
  real KFD code references a missing op

## Verification Method

Every candidate `hal_uvm_*` op requires:

1. Call site evidence in real KFD code (file:line)
2. Compile error / linker error log proving the op is referenced
3. ADR process approval (ADR-035 Rule 3) before adding to `struct gpu_hal_ops`

## Current Count

| op | KFD call site | evidence | decision |
|---|---|---|---|
| _(none)_ | - | - | - |

**Total: 0 ops** (verified by `git diff plugins/gpu_driver/hal/`
remaining at zero lines through Stage 1.3 implementation).

## Enforcement Guard Rails

- `git diff plugins/gpu_driver/hal/include/hal_ops.h` must remain zero lines
- `git diff plugins/gpu_driver/hal/` must remain zero lines
- `git diff include/linux_compat/types.h` must contain only 1.3
  governance annotations, no new type definitions for HAL
- Each future HAL op addition requires a new ADR entry referencing
  this audit document

## Stage 1.4 Trigger

If KFD integration (Stage 1.4) reveals a missing `hal_uvm_*` op:

1. Document call site (`kfd_svm.c:123` or similar)
2. File a new ADR (ADR-038+) with compile error trace
3. Add the op only after ADR is Accepted

## References

- Decision 4 (design.md): HAL conditional expansion
- [`ADR-023`](../../../../docs/00_adr/adr-023-hal-interface.md):
  HAL interface contract, 11 ops cap
- [`ADR-027`](../../../../docs/00_adr/adr-027-linux-compat-strategy.md):
  Linux compat, spec-driven
- [`ADR-035`](../../../../docs/00_adr/adr-035-governance-policy.md):
  governance