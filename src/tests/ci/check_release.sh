#!/usr/bin/env bash
#
# 发布检查（Phase 5 主机侧可执行部分；移动端检查需在 CI/相应平台执行）。
#
# 用法：src/tests/ci/check_release.sh <build_dir>
#
set -euo pipefail

BUILD_DIR="${1:-build}"
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"

echo "==> [1/4] blacklist"
python "$REPO_ROOT/src/tests/ci/check_blacklist.py" "$BUILD_DIR"

echo "==> [2/4] upstream manifest"
python "$REPO_ROOT/src/tests/ci/check_upstream_manifest.py" --upstream-root "D:/Project/Sample/microvisionchain/src"

echo "==> [3/4] symbols"
"$REPO_ROOT/src/tests/ci/check_symbols.sh" "$BUILD_DIR"

echo "==> [4/4] ctest"
CTEST_BIN="ctest"
if ! command -v ctest >/dev/null 2>&1; then
  CTEST_BIN="/c/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe"
fi
if [ -d "$BUILD_DIR/src/tests/Debug" ]; then
  "$CTEST_BIN" --test-dir "$BUILD_DIR" -C Debug --output-on-failure
else
  "$CTEST_BIN" --test-dir "$BUILD_DIR" --output-on-failure
fi

echo "check_release: PASS (host-side)"
