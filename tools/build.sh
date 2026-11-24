#!/bin/bash
# Builds Fallout 2 Community Edition with Stimpack from the engine/ submodule.
#
#   tools/build.sh [Release|Debug] [build directory]
#
# Needs CMake 3.19+, a C++17 compiler, Python 3.8+ and git (see docs/BUILDING.md).
set -euo pipefail

STIMPACK_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG="${1:-Release}"
BUILD_DIR="${2:-$STIMPACK_DIR/build/$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')}"

if [[ ! -f "$STIMPACK_DIR/engine/CMakeLists.txt" ]]; then
    git -C "$STIMPACK_DIR" submodule update --init engine
fi

GENERATOR=()
if command -v ninja >/dev/null 2>&1; then
    GENERATOR=(-G Ninja)
fi

cmake -S "$STIMPACK_DIR/engine" -B "$BUILD_DIR" "${GENERATOR[@]}" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    "-DCMAKE_PROJECT_fallout2-ce_INCLUDE=$STIMPACK_DIR/cmake/StimpackProjectInclude.cmake"
cmake --build "$BUILD_DIR" --config "$CONFIG" --target fallout2-ce
echo "built: $BUILD_DIR"
