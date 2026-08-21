#!/usr/bin/env python3
"""
mvclight 上游导入清单校验（Phase 0）。

用法：
  python3 src/tests/ci/check_upstream_manifest.py [--upstream-root D:/Project/Sample/microvisionchain/src]

行为：
- 读取 third_party/patches/upstream_manifest.json；
- 对 status == "imported" 的条目：
    * 校验上游文件存在；
    * 校验本仓库目标文件存在；
    * 若未关联 patch，校验 SHA256 与上游一致；
- 对 status == "pending" 的条目仅提示，不视为失败（用于记录后续导入计划）。
返回 0 表示通过。
"""

import argparse
import hashlib
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
MANIFEST = REPO_ROOT / "third_party" / "patches" / "upstream_manifest.json"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--upstream-root", default="D:/Project/Sample/microvisionchain/src")
    args = parser.parse_args()

    if not MANIFEST.exists():
        print(f"FAIL: manifest not found: {MANIFEST}")
        return 1

    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    upstream_root = Path(args.upstream_root)
    failed = False

    imports = data.get("imports", [])
    if not imports:
        print("WARN: manifest imports is empty")

    for entry in imports:
        src = entry["source"]
        dst = entry["dest"]
        status = entry.get("status", "imported")
        patch = entry.get("patch")

        upstream_file = upstream_root / src
        local_file = REPO_ROOT / dst

        if status == "pending":
            print(f"PENDING: {src} -> {dst} (planned for later phase)")
            continue

        if not upstream_file.exists():
            print(f"FAIL: upstream file missing: {upstream_file}")
            failed = True
            continue
        if not local_file.exists():
            print(f"FAIL: local file missing: {local_file}")
            failed = True
            continue

        if not patch:
            up_hash = sha256_file(upstream_file)
            local_hash = sha256_file(local_file)
            if up_hash != local_hash:
                print(f"FAIL: sha256 mismatch for {src} (upstream={up_hash} local={local_hash})")
                failed = True
            else:
                print(f"PASS: {src} matches upstream")
        else:
            print(f"PASS: {src} imported with patch {patch} (sha256 not compared)")

    if failed:
        print("check_upstream_manifest: FAILED")
        return 1
    print("check_upstream_manifest: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
