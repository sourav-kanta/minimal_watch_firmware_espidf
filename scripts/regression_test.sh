#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-host"

cd "$ROOT_DIR"

echo "==> Configuring host regression build"
cmake -S tests/host -B "$BUILD_DIR" \
    -DHOST_COVERAGE=OFF \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache

echo
echo "==> Building host tests"
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo
echo "==> Running regression tests"
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo
echo "==> Regression tests passed"
