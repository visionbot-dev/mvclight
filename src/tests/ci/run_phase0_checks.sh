#!/usr/bin/env bash
#
# Phase 0 DoD 自动校验入口。
# 用法：src/tests/ci/run_phase0_checks.sh <build_dir>
#
set -euo pipefail

BUILD_DIR="${1:-build}"
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"

# 适配多配置生成器（Visual Studio）
CTEST_CONFIG=""
if [ -d "$BUILD_DIR/src/tests/Debug" ]; then
  CTEST_CONFIG="-C Debug"
fi

# ctest 可能未加入 PATH（VS 自带 CMake 场景）
if command -v ctest >/dev/null 2>&1; then
  CTEST_BIN="ctest"
else
  CTEST_BIN="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe"
fi

echo "==> [1/5] check_blacklist.py"
python "$REPO_ROOT/src/tests/ci/check_blacklist.py" "$BUILD_DIR"

echo "==> [2/5] check_upstream_manifest.py"
python "$REPO_ROOT/src/tests/ci/check_upstream_manifest.py" --upstream-root "D:/Project/Sample/microvisionchain/src"

echo "==> [3/5] ctest unit"
"$CTEST_BIN" --test-dir "$BUILD_DIR" $CTEST_CONFIG --output-on-failure

echo "==> [4/5] check_symbols.sh"
"$REPO_ROOT/src/tests/ci/check_symbols.sh" "$BUILD_DIR"

echo "==> [5/5] init(NULL) behavior"
TEST_API="$(find "$BUILD_DIR" -maxdepth 5 -name 'test_mvc_light_api.exe' -o -name 'test_mvc_light_api' 2>/dev/null | head -n 1)"
if [ -z "$TEST_API" ] || [ ! -x "$TEST_API" ]; then
  echo "FAIL: test_mvc_light_api not found"
  exit 1
fi
"$TEST_API"

echo "Phase 0 DoD: ALL PASS"
