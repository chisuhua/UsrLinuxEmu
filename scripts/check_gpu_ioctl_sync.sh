#!/usr/bin/env bash
# scripts/check_gpu_ioctl_sync.sh — Verify System C GPU_IOCTL_* numbering stays
# in sync between UsrLinuxEmu (main repo) and TaskRunner (submodule mirror).
#
# Per Oracle D2 (Stage 1.2 review, 2026-07-02): TaskRunner's UsrLinuxEmu mirror
# of plugins/gpu_driver/shared/gpu_ioctl.h must stay byte-identical to avoid
# IOCTL ABI drift between emulator and real-driver consumers.
#
# Exit 0 = in sync; non-zero = drift detected (CI failure).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TASKRUNNER_MIRROR="$REPO_ROOT/external/TaskRunner/UsrLinuxEmu/plugins/gpu_driver/shared/gpu_ioctl.h"
MAIN_FILE="$REPO_ROOT/plugins/gpu_driver/shared/gpu_ioctl.h"

# 1. Extract only GPU_IOCTL_* #define lines (the numbering is what matters)
EXTRACT_REGEX='^#define[[:space:]]+GPU_IOCTL_[A-Z_]+[[:space:]]+(_IO|R|_IOW|_IOR|_IOWR)\('

if [ ! -f "$MAIN_FILE" ]; then
    echo "ERROR: UsrLinuxEmu mirror file missing: $MAIN_FILE"
    exit 1
fi
if [ ! -f "$TASKRUNNER_MIRROR" ]; then
    echo "ERROR: TaskRunner mirror file missing: $TASKRUNNER_MIRROR"
    echo "       Re-pull the TaskRunner submodule or restore the mirror."
    exit 1
fi

MAIN_DEFINES=$(grep -E "$EXTRACT_REGEX" "$MAIN_FILE"     | sort || true)
TR_DEFINES=$(   grep -E "$EXTRACT_REGEX" "$TASKRUNNER_MIRROR" | sort || true)

if [ "$MAIN_DEFINES" = "$TR_DEFINES" ]; then
    IOCTL_COUNT=$(echo "$MAIN_DEFINES" | grep -c . || echo 0)
    echo "OK: $IOCTL_COUNT GPU_IOCTL_* entries in sync"
    echo "  UsrLinuxEmu:  $MAIN_FILE"
    echo "  TaskRunner:   $TASKRUNNER_MIRROR"
    exit 0
else
    echo "FAIL: GPU_IOCTL_* numbering drift between UsrLinuxEmu and TaskRunner"
    echo
    echo "=== Only in UsrLinuxEmu: ==="
    diff <(echo "$MAIN_DEFINES") <(echo "$TR_DEFINES") | head -20
    echo
    echo "Per Oracle D2/ADR-035 §Rule 5.1: copy gpu_ioctl.h to TaskRunner mirror"
    echo "and obtain dual-approval before merging."
    exit 2
fi