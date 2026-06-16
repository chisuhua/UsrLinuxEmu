#!/bin/bash
# scripts/install-hooks.sh - Install git hooks for UsrLinuxEmu
#
# Copies the tracked hook templates from scripts/hooks/ to .git/hooks/
# so pre-commit checks run automatically.
#
# Usage:
#   scripts/install-hooks.sh           # install all hooks
#   scripts/install-hooks.sh --uninstall  # remove UsrLinuxEmu hooks
#
# Exit codes:
#   0 - success
#   1 - not inside a git repository

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
HOOK_SRC_DIR="${REPO_ROOT}/scripts/hooks"
HOOK_DST_DIR="${REPO_ROOT}/.git/hooks"

# ---------------------------------------------------------------------------
# Pre-flight checks
# ---------------------------------------------------------------------------

if [ ! -d "${REPO_ROOT}/.git" ]; then
    echo "ERROR: not a git repository (${REPO_ROOT}/.git not found)" >&2
    echo "Run from the project root, or init git first." >&2
    exit 1
fi

if [ ! -d "${HOOK_SRC_DIR}" ]; then
    echo "ERROR: ${HOOK_SRC_DIR} not found" >&2
    echo "Hook templates are missing. Re-clone or restore scripts/hooks/." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Uninstall path
# ---------------------------------------------------------------------------

if [ "${1:-}" = "--uninstall" ]; then
    echo "Uninstalling UsrLinuxEmu hooks from ${HOOK_DST_DIR}/"
    for src in "${HOOK_SRC_DIR}"/*; do
        [ -f "${src}" ] || continue
        name=$(basename "${src}")
        target="${HOOK_DST_DIR}/${name}"
        if [ -f "${target}" ]; then
            rm -f "${target}"
            echo "  removed ${name}"
        fi
    done
    echo "Done."
    exit 0
fi

# ---------------------------------------------------------------------------
# Install path
# ---------------------------------------------------------------------------

echo "Installing UsrLinuxEmu git hooks..."
echo "  source: ${HOOK_SRC_DIR}/"
echo "  target: ${HOOK_DST_DIR}/"
echo ""

installed=0
skipped=0
for src in "${HOOK_SRC_DIR}"/*; do
    [ -f "${src}" ] || continue
    name=$(basename "${src}")
    target="${HOOK_DST_DIR}/${name}"

    # Don't clobber existing non-UsrLinuxEmu hooks unless --force
    if [ -f "${target}" ] && ! grep -q "UsrLinuxEmu pre-commit hook" "${target}" 2>/dev/null; then
        echo "  ⚠ ${name}: existing hook found (not UsrLinuxEmu), backing up to ${name}.bak"
        mv "${target}" "${target}.bak"
    fi

    cp "${src}" "${target}"
    chmod +x "${target}"
    echo "  ✓ installed ${name}"
    installed=$((installed + 1))
done

echo ""
echo "Installed ${installed} hook(s)."
echo ""
echo "Verify with:"
echo "  ls -la ${HOOK_DST_DIR}/pre-commit"
echo ""
echo "Test by staging a doc change and committing:"
echo "  touch docs/00_adr/test.md"
echo "  git add docs/00_adr/test.md"
echo "  SKIP_DOCS_AUDIT=1 git commit -m 'test'   # with docs-audit enabled"
echo ""
echo "To bypass docs-audit on a specific commit:"
echo "  SKIP_DOCS_AUDIT=1 git commit -m 'hotfix'"
echo ""
echo "To uninstall:"
echo "  scripts/install-hooks.sh --uninstall"
