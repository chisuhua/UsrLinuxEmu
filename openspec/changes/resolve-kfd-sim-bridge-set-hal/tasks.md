## 1. Preflight

- [ ] 1.1 Confirm zero call sites: `git grep -n kfd_sim_bridge_set_hal plugins/ tests/ src/ tools/ docs/` — expect only the definition (kfd_sim_bridge.cpp) and declaration (kfd_sim_bridge.h)
- [ ] 1.2 Read the function body at `plugins/gpu_driver/drv/kfd_sim_bridge.cpp:261` to understand what state it touches (so the deletion isn't surprising — confirm it only sets a member that nobody reads)
- [ ] 1.3 Check for any doc references: `git grep -ln kfd_sim_bridge_set_hal docs/` — should be empty

## 2. Implement: delete the dead setter

- [ ] 2.1 Open `plugins/gpu_driver/drv/kfd_sim_bridge.cpp`, locate the `kfd_sim_bridge_set_hal` function definition (line 261 area), delete the function (including its preceding comment if any)
- [ ] 2.2 Open `plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h`, locate the declaration (line 25), delete the line
- [ ] 2.3 Verify the deletion didn't leave a dangling forward reference (the function might be declared elsewhere — `git grep kfd_sim_bridge_set_hal` should now return zero hits)
- [ ] 2.4 If the deleted function was the last user of any state field in `kfd_sim_bridge.cpp`, also remove that field (no longer needed); otherwise leave the field for future use

## 3. Verify

- [ ] 3.1 `cmake --build build -j4` — no errors (the header is included by other TUs; deleted declaration won't break consumers)
- [ ] 3.2 `ctest` — 157/157 still PASS (no test referenced the deleted function)

## 4. Commit + cleanup

- [ ] 4.1 `git add plugins/gpu_driver/drv/kfd_sim_bridge.cpp plugins/gpu_driver/drv/kfd/kfd_sim_bridge.h`
- [ ] 4.2 Commit message: `chore(kfd): delete unused kfd_sim_bridge_set_hal setter (zero call sites)`
- [ ] 4.3 Optional: grep for `set_hal` in the rest of the codebase to confirm no other variants are equally dead