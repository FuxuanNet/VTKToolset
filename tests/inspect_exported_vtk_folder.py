#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
检查 SAM 导出的 Legacy VTK 文件夹。

用途：
- 统计每个 .vtk 的 POINTS / CELLS / CELL_TYPES。
- 统计 POINT_DATA 和 CELL_DATA 字段。
- 对比多个帧之间网格数量是否一致。

这个脚本只读取文本 VTK，不依赖 ParaView。
"""

from __future__ import annotations

import argparse
from pathlib import Path


def parse_vtk(path: Path) -> dict[str, object]:
    text = path.read_text(encoding="utf-8", errors="replace")
    tokens = text.split()

    result: dict[str, object] = {
        "file": path.name,
        "size": path.stat().st_size,
        "points": None,
        "cells": None,
        "cell_size": None,
        "cell_types": None,
        "point_data_count": None,
        "cell_data_count": None,
        "point_fields": [],
        "cell_fields": [],
    }

    section = None
    i = 0
    while i < len(tokens):
        token = tokens[i]
        if token == "POINTS" and i + 1 < len(tokens):
            result["points"] = int(tokens[i + 1])
            i += 3
            continue
        if token == "CELLS" and i + 2 < len(tokens):
            result["cells"] = int(tokens[i + 1])
            result["cell_size"] = int(tokens[i + 2])
            i += 3
            continue
        if token == "CELL_TYPES" and i + 1 < len(tokens):
            result["cell_types"] = int(tokens[i + 1])
            i += 2
            continue
        if token == "POINT_DATA" and i + 1 < len(tokens):
            section = "POINT_DATA"
            result["point_data_count"] = int(tokens[i + 1])
            i += 2
            continue
        if token == "CELL_DATA" and i + 1 < len(tokens):
            section = "CELL_DATA"
            result["cell_data_count"] = int(tokens[i + 1])
            i += 2
            continue
        if token in {"SCALARS", "VECTORS", "TENSORS"} and i + 1 < len(tokens):
            field_name = tokens[i + 1]
            if section == "POINT_DATA":
                result["point_fields"].append(field_name)
            elif section == "CELL_DATA":
                result["cell_fields"].append(field_name)
            i += 2
            continue
        i += 1

    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="Inspect exported VTK folder.")
    parser.add_argument("folder", type=Path)
    parser.add_argument("--report", type=Path, default=None)
    args = parser.parse_args()

    files = sorted(args.folder.glob("*.vtk"), key=lambda p: p.name)
    if not files:
        print(f"No .vtk files found: {args.folder}")
        return 1

    rows = [parse_vtk(path) for path in files]
    first = rows[0]

    lines: list[str] = []
    lines.append(f"VTK folder: {args.folder}")
    lines.append(f"VTK files: {len(rows)}")
    lines.append("")
    lines.append("| 文件 | 大小 | 点数 | 单元数 | CellTypes数 | POINT_DATA | CELL_DATA | 点字段 | 单元字段 |")
    lines.append("|---|---:|---:|---:|---:|---:|---:|---|---|")
    for row in rows:
        lines.append(
            "| {file} | {size} | {points} | {cells} | {cell_types} | {point_data_count} | {cell_data_count} | {point_fields} | {cell_fields} |".format(
                file=row["file"],
                size=row["size"],
                points=row["points"],
                cells=row["cells"],
                cell_types=row["cell_types"],
                point_data_count=row["point_data_count"],
                cell_data_count=row["cell_data_count"],
                point_fields=",".join(row["point_fields"]) or "-",
                cell_fields=",".join(row["cell_fields"]) or "-",
            )
        )

    lines.append("")
    same_mesh = all(row["points"] == first["points"] and row["cells"] == first["cells"] for row in rows)
    lines.append(f"Same mesh in all frames: {same_mesh}")

    expected_point_fields = {"U", "UR"}
    frames_with_point_results = [
        row["file"] for row in rows if expected_point_fields.issubset(set(row["point_fields"]))
    ]
    lines.append(f"Frames with U and UR: {len(frames_with_point_results)}/{len(rows)}")

    output = "\n".join(lines)
    print(output)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(output + "\n", encoding="utf-8")
        print(f"\nReport written: {args.report}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
