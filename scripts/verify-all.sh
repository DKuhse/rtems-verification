#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Running all verification targets ==="
echo ""

"${SCRIPT_DIR}/verify-thread-priority.sh" "$@"
echo ""

"${SCRIPT_DIR}/verify-edf-update-priority.sh" "$@"
echo ""

"${SCRIPT_DIR}/verify-edf-unblock.sh" "$@"
echo ""

"${SCRIPT_DIR}/verify-edf-release-cancel.sh" "$@"
echo ""

echo "=== All verification targets complete ==="
