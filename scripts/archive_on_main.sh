#!/usr/bin/env bash
# scripts/archive_on_main.sh — Custom archive flow for "on main" mode
#
# The standard ship_archive.sh lightweight mode assumes:
#   - Branch openspec/<name> exists with commits
#   - Merge that branch into default (main)
#   - openspec archive <name> --yes
#   - Delete branch
#
# For "on main" mode (user wants no worktree, no branch):
#   - Work is committed directly to main
#   - No branch merge needed
#   - Just openspec archive + commit the archive moves
#
# Usage:
#   bash scripts/archive_on_main.sh <project_root> <change_name>

set -euo pipefail

PROJECT_ROOT="${1:-$(git rev-parse --show-toplevel)}"
CHANGE_NAME="${2:-}"

if [ -z "$CHANGE_NAME" ]; then
  echo "Usage: $0 <project_root> <change_name>" >&2
  exit 1
fi

cd "$PROJECT_ROOT" || { echo "❌ Cannot cd to $PROJECT_ROOT" >&2; exit 1; }

# Validate we're on main
CURRENT_BRANCH=$(git branch --show-current)
if [ "$CURRENT_BRANCH" != "main" ]; then
  echo "⚠️  Warning: not on main branch (current: $CURRENT_BRANCH)" >&2
  echo "   Continuing anyway — user explicitly opted for non-main mode" >&2
fi

# Validate change exists and is committed
if [ ! -d "openspec/changes/$CHANGE_NAME" ]; then
  echo "❌ Change directory not found: openspec/changes/$CHANGE_NAME" >&2
  exit 1
fi

# Check for uncommitted changes in the change scope
CHANGE_DIRTY=$(git status --porcelain "openspec/changes/$CHANGE_NAME/" 2>/dev/null | head -5)
if [ -n "$CHANGE_DIRTY" ]; then
  echo "❌ Change has uncommitted changes:" >&2
  echo "$CHANGE_DIRTY" >&2
  exit 1
fi

# Check for uncommitted changes elsewhere (warning only)
OTHER_DIRTY=$(git status --porcelain 2>/dev/null | grep -v "openspec/changes/$CHANGE_NAME/" | head -5)
if [ -n "$OTHER_DIRTY" ]; then
  echo "⚠️  Uncommitted changes outside change scope:" >&2
  echo "$OTHER_DIRTY" >&2
fi

echo "📦 Archiving $CHANGE_NAME (on main mode)..."

# Run openspec archive
if command -v openspec >/dev/null 2>&1; then
  openspec archive "$CHANGE_NAME" --yes || {
    echo "⚠️  openspec archive failed (CLI may not be installed); manually moving" >&2
    mkdir -p "openspec/changes/archive/"
    mv "openspec/changes/$CHANGE_NAME" "openspec/changes/archive/$CHANGE_NAME/"
  }
else
  echo "⚠️  openspec CLI not found; manually moving" >&2
  mkdir -p "openspec/changes/archive/"
  mv "openspec/changes/$CHANGE_NAME" "openspec/changes/archive/$CHANGE_NAME/"
fi

# Commit the archive move
git add -A
if git diff --cached --quiet; then
  echo "⚠️  Nothing to commit (archive move may have been a no-op)"
else
  git commit -m "archive: $CHANGE_NAME (on main mode)" || {
    echo "❌ Failed to commit archive move" >&2
    exit 1
  }
  echo "✅ Archive commit created"
fi

# Update proposal-approved.md status (best-effort)
PROPOSAL_APPROVED="proposal-approved.md"
if [ -f "$PROPOSAL_APPROVED" ]; then
  # Update status to archived using simple sed
  if grep -q "^### $CHANGE_NAME" "$PROPOSAL_APPROVED" 2>/dev/null; then
    sed -i "s/^### $CHANGE_NAME .*/### $CHANGE_NAME — archived ($(date +%Y-%m-%d))/" "$PROPOSAL_APPROVED" 2>/dev/null || true
    git add "$PROPOSAL_APPROVED"
    if ! git diff --cached --quiet; then
      git commit -m "docs(approved): mark $CHANGE_NAME as archived" || true
    fi
  fi
fi

# Cleanup plan file (if exists)
PLAN_FILE=".rddf/plans/${CHANGE_NAME}.md"
if [ -f "$PLAN_FILE" ]; then
  rm -f "$PLAN_FILE"
  git add "$PLAN_FILE" 2>/dev/null || true
  if ! git diff --cached --quiet; then
    git commit -m "chore: clean up plan file for archived change $CHANGE_NAME" || true
  fi
  echo "✅ Plan file cleaned up"
fi

echo ""
echo "✅ $CHANGE_NAME archived on main"
echo "   Latest commit: $(git log --oneline -1)"
echo "   Active changes: $(ls -d openspec/changes/*/ 2>/dev/null | grep -v archive/ | wc -l)"
