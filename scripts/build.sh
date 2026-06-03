#!/usr/bin/env bash
set -euo pipefail

# Builds owplog with sensible defaults, plus optional interactive prompts.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="build"
BUILD_TYPE="Release"
GENERATOR=""
JOBS=""
BUILD_TESTS="ON"
BUILD_SHARED_C_API="ON"
BUILD_E2E_EXAMPLES="ON"
CLEAN_BUILD_DIR=0
RUN_TESTS=0
INTERACTIVE=0

usage() {
    cat <<'USAGE'
Usage: bash scripts/build.sh [options]

Options:
  -i, --interactive                Prompt for values interactively.
  -b, --build-dir <dir>            Build directory (default: build).
  -t, --build-type <type>          CMake build type (default: Release).
  -g, --generator <name>           CMake generator (default: CMake default).
  -j, --jobs <n>                   Parallel build jobs.
      --tests <on|off>             Set OWPLOG_BUILD_TESTS (default: on).
      --shared-c-api <on|off>      Set OWPLOG_BUILD_SHARED_C_API (default: on).
      --e2e-examples <on|off>      Set OWPLOG_BUILD_E2E_EXAMPLES (default: on).
      --clean                      Remove build directory before configuring.
      --run-tests                  Run ctest after build.
  -h, --help                       Show this help.
USAGE
}

normalize_on_off() {
    local raw="${1:-}"
    local upper
    upper="$(printf '%s' "$raw" | tr '[:lower:]' '[:upper:]')"
    case "$upper" in
        ON|OFF) printf '%s\n' "$upper" ;;
        1|TRUE|YES|Y) printf 'ON\n' ;;
        0|FALSE|NO|N) printf 'OFF\n' ;;
        *)
            echo "Invalid boolean value: $raw (expected on/off)" >&2
            exit 1
            ;;
    esac
}

prompt_text() {
    local prompt="$1"
    local default="$2"
    local answer=""
    read -r -p "$prompt [$default]: " answer
    if [[ -z "$answer" ]]; then
        printf '%s\n' "$default"
    else
        printf '%s\n' "$answer"
    fi
}

prompt_on_off() {
    local prompt="$1"
    local default="$2"
    local answer=""
    read -r -p "$prompt [$default]: " answer
    if [[ -z "$answer" ]]; then
        normalize_on_off "$default"
    else
        normalize_on_off "$answer"
    fi
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -i|--interactive)
            INTERACTIVE=1
            ;;
        -b|--build-dir)
            BUILD_DIR="$2"
            shift
            ;;
        -t|--build-type)
            BUILD_TYPE="$2"
            shift
            ;;
        -g|--generator)
            GENERATOR="$2"
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift
            ;;
        --tests)
            BUILD_TESTS="$(normalize_on_off "$2")"
            shift
            ;;
        --shared-c-api)
            BUILD_SHARED_C_API="$(normalize_on_off "$2")"
            shift
            ;;
        --e2e-examples)
            BUILD_E2E_EXAMPLES="$(normalize_on_off "$2")"
            shift
            ;;
        --clean)
            CLEAN_BUILD_DIR=1
            ;;
        --run-tests)
            RUN_TESTS=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            exit 1
            ;;
    esac
    shift
done

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake is required but was not found in PATH." >&2
    exit 1
fi

if [[ "$INTERACTIVE" -eq 1 ]]; then
    echo "Interactive build setup"
    echo "-----------------------"
    BUILD_DIR="$(prompt_text "Build directory" "$BUILD_DIR")"
    BUILD_TYPE="$(prompt_text "Build type (Debug/Release/RelWithDebInfo/MinSizeRel)" "$BUILD_TYPE")"
    GENERATOR="$(prompt_text "Generator (leave blank for default)" "$GENERATOR")"
    JOBS="$(prompt_text "Parallel jobs (leave blank for CMake default)" "$JOBS")"
    BUILD_TESTS="$(prompt_on_off "Build tests? (on/off)" "$BUILD_TESTS")"
    BUILD_SHARED_C_API="$(prompt_on_off "Build shared C API? (on/off)" "$BUILD_SHARED_C_API")"
    BUILD_E2E_EXAMPLES="$(prompt_on_off "Build e2e examples? (on/off)" "$BUILD_E2E_EXAMPLES")"
    if [[ "$(prompt_on_off "Clean build directory first? (on/off)" "OFF")" == "ON" ]]; then
        CLEAN_BUILD_DIR=1
    fi
    if [[ "$(prompt_on_off "Run tests after build? (on/off)" "OFF")" == "ON" ]]; then
        RUN_TESTS=1
    fi
fi

cd "$REPO_ROOT"

if [[ "$CLEAN_BUILD_DIR" -eq 1 && -d "$BUILD_DIR" ]]; then
    echo "Removing existing build directory: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

CONFIGURE_CMD=(
    cmake
    -S .
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DOWPLOG_BUILD_TESTS="$BUILD_TESTS"
    -DOWPLOG_BUILD_SHARED_C_API="$BUILD_SHARED_C_API"
    -DOWPLOG_BUILD_E2E_EXAMPLES="$BUILD_E2E_EXAMPLES"
)

if [[ -n "$GENERATOR" ]]; then
    CONFIGURE_CMD+=(-G "$GENERATOR")
fi

BUILD_CMD=(cmake --build "$BUILD_DIR")
if [[ -n "$JOBS" ]]; then
    BUILD_CMD+=(--parallel "$JOBS")
fi

echo "Configuring with:"
printf '  %q ' "${CONFIGURE_CMD[@]}"
echo
"${CONFIGURE_CMD[@]}"

echo "Building with:"
printf '  %q ' "${BUILD_CMD[@]}"
echo
"${BUILD_CMD[@]}"

if [[ "$RUN_TESTS" -eq 1 ]]; then
    TEST_CMD=(ctest --test-dir "$BUILD_DIR" --output-on-failure)
    echo "Running tests with:"
    printf '  %q ' "${TEST_CMD[@]}"
    echo
    "${TEST_CMD[@]}"
fi

