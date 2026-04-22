#!/usr/bin/env bash
set -euo pipefail

# Convenience wrapper for configure + build + test.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

FORWARD_ARGS=()
INTERACTIVE=0

usage() {
    cat <<'USAGE'
Usage: bash scripts/test.sh [options]

This script forwards most options to scripts/build.sh and forces:
  --tests on --run-tests

Common options:
  -i, --interactive
  -b, --build-dir <dir>
  -t, --build-type <type>
  -g, --generator <name>
  -j, --jobs <n>
      --shared-c-api <on|off>
      --e2e-examples <on|off>
      --clean
  -h, --help
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -i|--interactive)
            INTERACTIVE=1
            FORWARD_ARGS+=("$1")
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            FORWARD_ARGS+=("$1")
            ;;
    esac
    shift
done

if [[ "$INTERACTIVE" -eq 0 ]]; then
    echo "Running non-interactive test build (pass --interactive to be prompted)."
fi

bash "${SCRIPT_DIR}/build.sh" "${FORWARD_ARGS[@]}" --tests on --run-tests

