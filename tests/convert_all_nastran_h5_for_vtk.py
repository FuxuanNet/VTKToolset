#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
递归转换 nastran_h5 目录中的所有 H5 文件，供 VTKToolset 集成测试使用。

NH2SH-v2 自带的 --input-dir 只处理目录第一层，不搜索子目录。
老师给的数据多数放在子目录中，所以这里逐个调用单文件命令：

    NH2SH.exe input.h5 output.sam.h5

输出文件统一放到 vtk_test_nh2sh_all/，文件名前加上原子目录名，避免重名。
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
NH2SH_EXE = ROOT / "NH2SH-v2" / "NH2SH" / "Release" / "NH2SH.exe"
INPUT_ROOT = ROOT / "nastran_h5"
OUTPUT_ROOT = ROOT / "vtk_test_nh2sh_all"


def safe_stem(path: Path) -> str:
    """把子目录路径拼到文件名里，避免不同目录下同名 H5 覆盖。"""
    relative = path.relative_to(INPUT_ROOT)
    parts = list(relative.parts)
    parts[-1] = path.stem
    return "__".join(parts)


def run_one(input_h5: Path, output_h5: Path) -> tuple[bool, str]:
    output_h5.parent.mkdir(parents=True, exist_ok=True)
    completed = subprocess.run(
        [str(NH2SH_EXE), str(input_h5), str(output_h5)],
        cwd=str(ROOT),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return completed.returncode == 0, completed.stdout


def main() -> int:
    if not NH2SH_EXE.exists():
        print(f"NH2SH.exe not found: {NH2SH_EXE}", file=sys.stderr)
        return 1
    if not INPUT_ROOT.exists():
        print(f"input directory not found: {INPUT_ROOT}", file=sys.stderr)
        return 1

    files = sorted(INPUT_ROOT.rglob("*.h5"))
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)

    print(f"Input H5 files: {len(files)}")
    print(f"Output directory: {OUTPUT_ROOT}")

    successes: list[Path] = []
    failures: list[tuple[Path, str]] = []

    for index, input_h5 in enumerate(files, start=1):
        output_h5 = OUTPUT_ROOT / f"{safe_stem(input_h5)}.sam.h5"
        print(f"\n[{index}/{len(files)}] {input_h5.relative_to(ROOT)}")
        ok, output = run_one(input_h5, output_h5)
        print(output.strip())
        if ok:
            successes.append(output_h5)
        else:
            failures.append((input_h5, output))

    summary_path = OUTPUT_ROOT / "conversion_summary.txt"
    with summary_path.open("w", encoding="utf-8") as f:
        f.write(f"Input H5 files: {len(files)}\n")
        f.write(f"Succeeded: {len(successes)}\n")
        f.write(f"Failed: {len(failures)}\n\n")
        f.write("Succeeded files:\n")
        for path in successes:
            f.write(f"  {path}\n")
        f.write("\nFailed files:\n")
        for path, output in failures:
            f.write(f"  {path}\n")
            f.write(output)
            f.write("\n")

    print("\nSummary")
    print(f"  Succeeded: {len(successes)}/{len(files)}")
    print(f"  Failed: {len(failures)}/{len(files)}")
    print(f"  Summary file: {summary_path}")

    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
