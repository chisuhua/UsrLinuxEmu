## 1. CHANGELOG & Release Notes

- [x] 1.1 Generate CHANGELOG.md from `git log --oneline --no-decorate` grouped by Conventional Commits types
- [x] 1.2 Write RELEASE_NOTES.md with v1.0 summary, features, known issues, system requirements
- [x] 1.3 Add CHANGELOG.md and RELEASE_NOTES.md to `tools/docs-audit.sh` file presence check

## 2. Migration Guide

- [x] 2.1 Create migration guide at `docs/10-migration/v0-to-v1.md`
- [x] 2.2 Document System B → System C ioctl mapping (`GPGPU_*` → `GPU_IOCTL_*`)
- [x] 2.3 Document kernel SHARED requirement (Issue #11)
- [x] 2.4 Document directory restructuring (Phase 1.5 drv/hal/sim separation, archive/)
- [x] 2.5 Document test framework change (GTest → Catch2 if applicable)

## 3. Binary Release Workflow

- [x] 3.1 Create `.github/workflows/release.yml` triggered by `v*.*.*` tags
- [x] 3.2 Configure Release build (`-DCMAKE_BUILD_TYPE=Release`) with static linking
- [x] 3.3 Add GitHub Release upload step for `build/bin/cli`, `build/lib/libkernel.so`, `plugins/*.so`
- [ ] 3.4 Test release workflow with dry-run tag on fork (需 GitHub fork 环境)

## 4. Docker (Optional)

- [x] 4.1 Create `Dockerfile` based on `ubuntu:22.04`
- [ ] 4.2 Build and test Docker image locally (需 Docker 环境)

## 5. plan-handoff Update

- [x] 5.1 Update `.rddf/state/.plan-handoff.json` (marked v1-0-release-prep as completed)