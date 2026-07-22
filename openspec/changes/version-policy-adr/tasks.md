## 1. CMake VERSION SSOT

- [x] 1.1 Edit `CMakeLists.txt` to add `VERSION 1.0.0` to `project(user_kernel_emu VERSION 1.0.0)`
- [x] 1.2 Verify `cmake` configure outputs correct `PROJECT_VERSION` (run `cmake -B build` and check)
- [x] 1.3 Update README.md version badge from `v0.5+` to `v1.0`
- [x] 1.4 Update README.md footer "当前版本" from `v0.6+` to `v1.0`
- [x] 1.5 Update README.md footer "最后验证" date to current date
- [x] 1.6 Update README.md phase badge from `Phase 2 complete` to `Stage 3 / v1.0`
- [x] 1.7 Update `docs/01-quickstart/first-example.md:217` marketing_name version from `v0.5` to `v1.0`

## 2. Git Tag Policy ADR

- [x] 2.1 Determine next ADR number (check `docs/00_adr/` for max existing number, currently 064 → next is 065)
- [x] 2.2 Create ADR file at `docs/00_adr/adr-065-version-policy.md` documenting:
  - Version SSOT: `CMakeLists.txt` `project()` VERSION
  - Tag naming: strict semver `v<major>.<minor>.<patch>` (regex `^v[0-9]+\.[0-9]+\.[0-9]+$`)
  - Milestone tag convention: `milestone-<description>` for non-release tags
  - v1.5 tag history and resolution
- [x] 2.3 Update `docs/00_adr/README.md` index and status distribution to include ADR-065

## 3. v1.5 Tag Resolution

> **⚠️ 禁止远程操作**: 以下所有操作仅在本地 fork 验证。远程 v1.5 删除和新增 milestone tag 需人工确认后执行 `git push origin :refs/tags/v1.5 && git push origin milestone-phase2.5-hotfix`。

- [x] 3.1 Rename existing `v1.5` tag to `milestone-phase2.5-hotfix` with annotated message preserving tagger identity
- [x] 3.2 Local cleanup and verification:
      - `git tag -d v1.5` (local delete only — remote requires manual confirmation)
      - `git tag -l` shows `milestone-phase2.5-hotfix` but NOT `v1.5`
- [x] 3.3 Verify tag points to correct commit (6d090e6 - Phase 2.5 hotfix)
- [ ] 3.4 **Team notification**: After remote tag rename is confirmed, notify contributors to run:
      ```bash
      git fetch --prune origin '+refs/tags/*:refs/tags/*'
      ```

## 4. CI/Tooling Updates

> **执行顺序**: 建议先完成 §2 (ADR) 再执行 4.0 (CONTRIBUTING.md 需要与 ADR 措辞一致)；先完成 §1 (CMake VERSION) 再执行 4.3 (docs-audit 需要 CMake VERSION 存在)

- [x] 4.0 Update `CONTRIBUTING.md` to add git tag naming policy (strict semver `v<major>.<minor>.<patch>`) — 在"提交规范"之后新增 `## 版本与 Tag 策略` 小节。措辞须与 ADR (2.2) 一致。
- [x] 4.1 Create separate tag validation workflow at `.github/workflows/tag-validation.yml` (NOT merging into `cmake-multi-platform.yml` to avoid double CI runs on tag push):
      - Trigger: `on: push: tags:` with glob `v[0-9]*.[0-9]*.[0-9]*`
      - First job: validate `${{ github.ref_name }}` matches strict semver regex `^v[0-9]+\.[0-9]+\.[0-9]+$`, fail early on non-compliant tags
      - Validate that tag does NOT start with `milestone-` prefix (these are excluded)
      - Note: GitHub Actions `tags:` filter uses glob (not regex), regex validation goes inside the job body
- [x] 4.2 Add tag format validation script in `scripts/validate-tag.sh` callable from CI and local, that checks a given tag name matches `^v[0-9]+\.[0-9]+\.[0-9]+$`
- [x] 4.3 Add README version consistency check to `tools/docs-audit.sh`:
      - Create new section `version-ssot` (section 9) — named to align with capability name
      - Implement `subsection()`: parse CMakeLists.txt `project(... VERSION X.Y.Z)` via sed
      - Implement `subsection()`: parse README.md badge `version-v[0-9.]+` and footer current version
      - `check_fail()` if mismatch; output expected vs actual versions
- [x] 4.4 Update `.openspec.yaml` in `openspec/changes/version-policy-adr/` to include `status: proposed`

## 5. Verification

- [x] 5.1 Reconfigure build: `cmake -B build` succeeds with new version
- [x] 5.2 Verify `cmake --build build` succeeds (no regression)
- [x] 5.3 Run `ctest --output-on-failure` from project root (105/105 PASS)
- [x] 5.4 Run `tools/docs-audit.sh --strict` to verify version consistency check works
- [x] 5.5 Verify CONTRIBUTING.md has tag policy section