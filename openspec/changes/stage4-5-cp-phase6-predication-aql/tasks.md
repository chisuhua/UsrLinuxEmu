## 1. Predication: Predicate Register & Entry

- [ ] 1.1 Add `PredicateState { bool enabled = true; uint64_t value = 0; } predicate_` to `HardwarePullerEmu`
- [ ] 1.2 Add `GPU_OP_SET_PREDICATE` enum to `gpu_queue.h` GPFIFO entry types
- [ ] 1.3 Implement SET operation: `predicate_.value = entry.payload[0]; predicate_.enabled = (value != 0)`
- [ ] 1.4 Implement AND operation: `predicate_.value &= entry.payload[0]; update enabled`
- [ ] 1.5 Implement OR operation: `predicate_.value |= entry.payload[0]; update enabled`
- [ ] 1.6 Implement XOR operation: `predicate_.value ^= entry.payload[0]; update enabled`

## 2. Predication: Puller DECODE Check

- [ ] 2.1 Add predicate flag detection in Puller DECODE phase
- [ ] 2.2 Skip entry dispatch when entry has predicate flag AND `predicate_.enabled == false`
- [ ] 2.3 Continue FETCH for next entry after predicate skip
- [ ] 2.4 Verify: predicate skip does not break IB jump_stack logic

## 3. Predication: Context Switch Persistence

- [ ] 3.1 Add `PredicateState predicate_snapshot_` to `ChannelState`
- [ ] 3.2 Wire save predicate state on preempt
- [ ] 3.3 Wire restore predicate state on resume

## 4. AQL: gpufifo_entry Format Field

- [ ] 4.1 Add `uint8_t format` field to `gpu_gpfifo_entry` (default 0=UsrNative)
- [ ] 4.2 Verify: existing UsrNative path unaffected (format=0 is no-op)
- [ ] 4.3 Verify: 1 byte size doesn't break struct alignment

## 5. AQL: Packet Parsing

- [ ] 5.1 Add `parseAqlPacket(const gpu_gpfifo_entry&)` to `GpfifoToLaunchParamsTranslator`
- [ ] 5.2 Map AQL `kernel_object` → `LaunchParams.kernel_addr`
- [ ] 5.3 Map AQL `kernarg_address` → `LaunchParams.kernargs`
- [ ] 5.4 Map AQL `grid_size_{x,y,z}` → `LaunchParams.grid_{x,y,z}`
- [ ] 5.5 Map AQL `workgroup_size_{x,y,z}` → `LaunchParams.block_{x,y,z}`
- [ ] 5.6 Add `format == FORMAT_AQL` branch in translator dispatch

## 6. AQL: Completion Signal → Timeline Semaphore

- [ ] 6.1 Map AQL `completion_signal` handle to `SemaphoreManager` handle
- [ ] 6.2 On AQL batch completion, call `sem_signal` with completion_signal handle
- [ ] 6.3 Handle completion_signal=0 as no-op (no signal)

## 7. PM4 Stub

- [ ] 7.1 Add `FORMAT_PM4 = 2` constant
- [ ] 7.2 Return -ENOSYS when format=2 is encountered in dispatcher

## 8. Standalone Tests

- [ ] 8.1 Write `test_predication_standalone`: SET/AND/OR/XOR operations
- [ ] 8.2 Write test: predicate skip in DECODE phase
- [ ] 8.3 Write test: predicate state survives preempt/resume
- [ ] 8.4 Write `test_aql_standalone`: AQL packet parsing → LaunchParams
- [ ] 8.5 Write test: AQL completion_signal triggers semaphore signal
- [ ] 8.6 Write test: PM4 format returns -ENOSYS

## 9. Sanitizer & Verification

- [ ] 9.1 Run `SANITIZER=asan-ubsan ./build.sh test` — all green
- [ ] 9.2 Run `SANITIZER=tsan ./build.sh test` — all green
- [ ] 9.3 Verify no new IOCTL numbers exposed
- [ ] 9.4 Run `tools/docs-audit.sh --strict` — PASS

## 10. Documentation & ADR Sync

- [ ] 10.1 Update ADR-051 status: PROPOSED → Accepted
- [ ] 10.2 Update ADR-052 status: PROPOSED → Accepted (PM4 deferred note)
- [ ] 10.3 Add changelog entry to roadmap.md (Stage 4.5 Phase 6 完成)