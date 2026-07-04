# UVM Error Semantics

Stage 1.3 UVM/HMM errno mapping reference. Documents the Linux kernel error code contract
for the `mmu_notifier` and `hmm_range` APIs in the user-space simulation layer.

## Error Code Table

| Error Code | Value | API Context | Meaning |
|-----------|-------|-------------|---------|
| `0` | 0 | All | Success |
| `-ENOMEM` | -12 | `mmu_notifier_register` | Internal bookkeeping allocation failed |
| `-EFAULT` | -14 | `hmm_range_fault` | Range contains invalid/unmapped address |
| `-EBUSY` | -16 | `hmm_range_fault` | Concurrent invalidation detected; caller must retry |
| `-EINVAL` | -22 | `mmu_notifier_register`, `hmm_range_fault` | Invalid argument (NULL pointer, bad range) |
| `-ENOSPC` | -28 | `mmu_interval_notifier_insert` | No resources available (reserved) |

## Verification

All errno values are verified in `tests/test_mmu_notifier_standalone.cpp` §8.1
(catch test tag: `[uvm][mmu_notifier][errno]`).

## References

- `include/linux_compat/types.h`: errno macro definitions
- `include/linux_compat/mmu_notifier.h`: API error semantics
- `include/linux_compat/hmm.h`: HMM range fault error semantics
- tasks.md §8.1-§8.2: errno mapping requirements