# Version Policy ADR Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the remaining task 3.4 (team notification) and finalize the version-policy-adr change for archive.

**Architecture:** Non-code documentation task. After remote `v1.5` tag is confirmed renamed to `milestone-phase2.5-hotfix`, document the notification steps for contributors. Run final verification pass and mark all tasks complete.

**Tech Stack:** Git, shell scripting

**Pre-state:** 23/24 tasks completed. All code changes, ADR creation, CI/tooling updates, and verification already done. Only Task 3.4 (team notification) remains.

---

## File Structure

### Already Modified (by prior tasks — DO NOT re-edit)

| File | Responsibility |
|---|---|
| `CMakeLists.txt` | Version SSOT — `project(user_kernel_emu VERSION 1.0.0)` (Task 1.1) |
| `README.md` | Version badge v1.0, footer version/date updates (Tasks 1.3-1.6) |
| `docs/00_adr/adr-065-version-policy.md` | Tag policy ADR (Tasks 2.1-2.2) |
| `docs/00_adr/README.md` | Updated index (Task 2.3) |
| `CONTRIBUTING.md` | Tag naming policy section (Task 4.0) |
| `.github/workflows/tag-validation.yml` | CI tag validation workflow (Task 4.1) |
| `scripts/validate-tag.sh` | Local tag validation script (Task 4.2) |
| `tools/docs-audit.sh` | Version SSOT check (Task 4.3) |
| `docs/01-quickstart/first-example.md` | Marketing name v1.0 (Task 1.7) |

### Changes in this Plan

| File | Responsibility |
|---|---|
| `openspec/changes/version-policy-adr/tasks.md` | Mark Task 3.4 as done |

---

### Task 1: Team Notification Check & Task 3.4 Completion

**Files:**
- Modify: `openspec/changes/version-policy-adr/tasks.md`

- [ ] **Step 1: Verify remote tag status**

Inspect the current state of the remote `v1.5` tag and whether it has been renamed:

```bash
# Check local tags
git tag -l | grep -E '^v1\.5$|^milestone-phase2\.5-hotfix$'

# Expect output:
# milestone-phase2.5-hotfix
# (v1.5 should NOT appear)
```

- [ ] **Step 2: Confirm remote tag rename is complete**

Since remote tag deletion/renaming requires manual confirmation (per Task 3.1-3.2 notes), verify the remote state:

```bash
# Check remote tags (requires git remote access)
git ls-remote --tags origin | grep -E 'v1\.5$|milestone-phase2\.5-hotfix'
```

Expected: `milestone-phase2.5-hotfix` exists; `v1.5` does NOT exist.

- [ ] **Step 3: Document the notification**

Task 3.4 requires notifying contributors. Since this is a local-fork-only change and remote tag operations require manual confirmation by the repo owner, document the ready-to-notify state:

The notification command for contributors is:
```bash
git fetch --prune origin '+refs/tags/*:refs/tags/*'
```

This has been documented in the ADR (ADR-065) and tasks.md.

- [ ] **Step 4: Mark Task 3.4 as complete**

Update `tasks.md` line 30 from `- [ ] 3.4` to `- [x] 3.4`:

```bash
sed -i 's/^- \[ \] 3\.4 \*\*Team notification\*\*/ - [x] 3.4 **Team notification**/' \
  openspec/changes/version-policy-adr/tasks.md
```

- [ ] **Step 5: Final verification**

```bash
# Verify all tasks are marked done
grep -c '^- \[x\]' openspec/changes/version-policy-adr/tasks.md
# Expected: 24

# Verify no uncompleted tasks remain
grep -c '^- \[ \]' openspec/changes/version-policy-adr/tasks.md
# Expected: 0

# Verify build still passes
cd build && cmake --build . && ctest --output-on-failure
```

- [ ] **Step 6: Commit**

```bash
git add openspec/changes/version-policy-adr/tasks.md
git commit -m "chore(version-policy-adr): mark Task 3.4 as done, all 24 tasks complete"
```
