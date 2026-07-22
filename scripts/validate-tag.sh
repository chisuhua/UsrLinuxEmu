#!/bin/bash
# validate-tag.sh - Validate a git tag name against strict semver pattern
#
# Usage: validate-tag.sh <tag-name>
#   validate-tag.sh v1.0.0     # exit 0
#   validate-tag.sh v1.5       # exit 1
#   validate-tag.sh v1.0.0-beta # exit 1
#
# Pattern: ^v[0-9]+\.[0-9]+\.[0-9]+$
# Per ADR-065 version policy.
# Can be used both locally and in CI.

set -euo pipefail

TAG="${1:-}"

if [ -z "${TAG}" ]; then
  echo "Usage: $0 <tag-name>"
  echo "Validates that a tag name matches strict semver: v<major>.<minor>.<patch>"
  echo "Examples: v1.0.0, v2.3.4"
  exit 2
fi

if [[ "${TAG}" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "✅ '${TAG}' matches strict semver pattern"
  exit 0
else
  echo "❌ '${TAG}' does NOT match strict semver pattern (expected v<major>.<minor>.<patch>)"
  exit 1
fi