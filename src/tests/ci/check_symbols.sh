#!/usr/bin/env bash
#
# mvclight 符号可见性校验（Phase 0）。
#
# 用法：
#   src/tests/ci/check_symbols.sh <build_dir> [lib_path]
#
# 平台策略：
#   - MSVC：使用 dumpbin /exports（VS 环境）
#   - GCC/Clang：使用 nm -D
#
# 仅允许导出 mvc_light_* 符号。

set -u

BUILD_DIR="${1:-build}"
LIB_PATH="${2:-}"

if [ -z "$LIB_PATH" ]; then
  # 自动查找 libmvclight
  LIB_PATH="$(find "$BUILD_DIR" -maxdepth 4 \( -name 'libmvclight.so' -o -name 'libmvclight.dylib' -o -name 'mvclight.dll' \) 2>/dev/null | head -n 1)"
fi

if [ -z "$LIB_PATH" ] || [ ! -f "$LIB_PATH" ]; then
  echo "FAIL: library not found under $BUILD_DIR"
  exit 1
fi

echo "Checking symbols in: $LIB_PATH"

if command -v nm >/dev/null 2>&1; then
  EXPORTED="$(nm -D --defined-only "$LIB_PATH" 2>/dev/null | awk '{print $3}' | grep -v '^$' || true)"
elif command -v dumpbin >/dev/null 2>&1; then
  # dumpbin /exports 输出：仅在 ordinal 表头之后、以 "ordinal hint RVA name" 格式的行才是导出项
  EXPORTED="$(dumpbin /exports "$LIB_PATH" 2>/dev/null \
    | awk '/^[[:space:]]*ordinal[[:space:]]/{in_table=1; next}
           in_table && /^[[:space:]]*[0-9]+[[:space:]]+[0-9A-Fa-f]+[[:space:]]+[0-9A-Fa-f]+[[:space:]]/{print $4}' \
    | grep -E '^[A-Za-z_@?]' || true)"
else
  echo "FAIL: neither nm nor dumpbin available"
  exit 1
fi

BAD=""
while IFS= read -r sym; do
  [ -z "$sym" ] && continue
  case "$sym" in
    mvc_light_*) ;;
    *) BAD="$BAD $sym" ;;
  esac
done <<< "$EXPORTED"

if [ -n "$BAD" ]; then
  echo "FAIL: unexpected exported symbols:$BAD"
  exit 1
fi

echo "check_symbols: PASS (only mvc_light_* exported)"
