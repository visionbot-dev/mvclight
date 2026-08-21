#!/usr/bin/env python3
"""
mvclight 黑名单校验脚本（Phase 0）。

用法：
  python3 src/tests/ci/check_blacklist.py [build_dir]

检查内容：
1. 解析 src/CMakeLists.txt 中的 MVCLIGHT_WHITELIST_SOURCES，确认不含黑名单路径；
2. 若提供 build_dir，扫描其中 CMakeFiles 生成的目标文件列表，确认不含黑名单源文件对应的目标。

返回 0 表示通过，非 0 表示失败。
"""

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
SRC_CMAKE = REPO_ROOT / "src" / "CMakeLists.txt"

BLACKLIST_REGEX = [
    re.compile(r"(^|/)(validation|txmempool|mempooltxdb|net_processing|init)\.cpp$"),
    re.compile(r"(^|/)(rpc|wallet|mining|zmq|qt|http|addrman)/"),
]


def extract_whitelist(cmake_text: str):
    """从 src/CMakeLists.txt 提取 MVCLIGHT_WHITELIST_SOURCES 列表（粗粒度）。"""
    sources = []
    in_block = False
    for raw in cmake_text.splitlines():
        line = raw.strip()
        if line.startswith("set(MVCLIGHT_WHITELIST_SOURCES"):
            in_block = True
            line = line.split("(", 1)[1] if "(" in line else ""
        elif in_block and line.startswith(")"):
            in_block = False
            continue
        if in_block:
            # 去掉注释
            line = line.split("#", 1)[0].strip()
            if line and not line.startswith("set("):
                sources.append(line)
    return sources


def main() -> int:
    failed = False

    if not SRC_CMAKE.exists():
        print(f"FAIL: {SRC_CMAKE} not found")
        return 1

    whitelist = extract_whitelist(SRC_CMAKE.read_text(encoding="utf-8"))
    print(f"Whitelist sources: {len(whitelist)}")
    for src in whitelist:
        for pat in BLACKLIST_REGEX:
            if pat.search(src):
                print(f"FAIL: blacklisted source in whitelist: {src}")
                failed = True

    if len(sys.argv) > 1:
        build_dir = Path(sys.argv[1])
        if build_dir.exists():
            for obj in build_dir.rglob("*"):
                name = obj.name
                for pat in BLACKLIST_REGEX:
                    if pat.search(name):
                        print(f"FAIL: blacklisted artifact in build dir: {obj}")
                        failed = True
        else:
            print(f"WARN: build dir not found: {build_dir}")

    if failed:
        print("check_blacklist: FAILED")
        return 1
    print("check_blacklist: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
