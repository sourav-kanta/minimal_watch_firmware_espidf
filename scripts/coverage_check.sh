#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-host-coverage"
COVERAGE_DIR="$BUILD_DIR/coverage"

cd "$ROOT_DIR"

echo "==> Configuring coverage build"
cmake -S tests/host -B "$BUILD_DIR" \
    -DHOST_COVERAGE=ON \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache

echo
echo "==> Building coverage tests"
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo
echo "==> Running coverage test suite"
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo
echo "==> Generating coverage report"

mkdir -p "$COVERAGE_DIR"

if command -v lcov >/dev/null 2>&1; then
    lcov \
        --capture \
        --directory "$BUILD_DIR" \
        --output-file "$COVERAGE_DIR/coverage.info" \
        --ignore-errors mismatch

    lcov \
        --remove "$COVERAGE_DIR/coverage.info" \
        '/usr/*' \
        '*/third_party/*' \
        '*/tests/*' \
        --output-file "$COVERAGE_DIR/coverage.info" \
        --ignore-errors unused

    if command -v genhtml >/dev/null 2>&1; then
        genhtml \
            "$COVERAGE_DIR/coverage.info" \
            --output-directory "$COVERAGE_DIR/html"

        echo
        echo "==> HTML coverage report:"
        echo "    $COVERAGE_DIR/html/index.html"
    fi

    echo
    echo "==> Coverage summary"
    lcov --summary "$COVERAGE_DIR/coverage.info"
else
    echo
    echo "WARNING: lcov is not installed."
    echo "Showing gcov summary instead."

    find "$BUILD_DIR" \
        -name '*.gcda' \
        -print0 |
    while IFS= read -r -d '' file; do
        gcov "$file" >/dev/null 2>&1 || true
    done
fi

echo
echo "==> Coverage checks completed"
