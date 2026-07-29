#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
统计批量生成的 SAM H5：节点数、单元类型、是否包含 U/UR/S/E。

用于从一批真实转换结果中挑选 VTKToolset 的 SAM 集成测试文件。
"""

from __future__ import annotations

import sys
from pathlib import Path

import h5py


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_DIR = ROOT / "vtk_test_nh2sh_all"


def attr_text(value) -> str:
    if hasattr(value, "__len__") and not isinstance(value, (str, bytes)):
        if len(value) == 0:
            return ""
        value = value[0]
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return str(value)


def summarize_one(path: Path) -> dict[str, object]:
    with h5py.File(str(path), "r") as h5:
        nodes = int(h5["/Parts/Part-1/Nodes/Labels"].shape[0]) if "/Parts/Part-1/Nodes/Labels" in h5 else 0
        element_types: list[str] = []
        elements_root = h5.get("/Parts/Part-1/Elements")
        if elements_root is not None:
            for name in sorted(elements_root.keys()):
                group = elements_root[name]
                element_types.append(attr_text(group.attrs.get("ElementType", "")))

        frame = "/Steps/Step-1/Frames/Frame:0"
        fields = []
        for field in ["U", "UR", "RF", "RM", "S", "E"]:
            if f"{frame}/{field}" in h5:
                fields.append(field)

    return {
        "file": path.name,
        "nodes": nodes,
        "classes": len(element_types),
        "types": ",".join(element_types),
        "fields": ",".join(fields),
    }


def main() -> int:
    target_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_DIR
    files = sorted(target_dir.glob("*.sam.h5"))
    if not files:
        print(f"No .sam.h5 files found: {target_dir}", file=sys.stderr)
        return 1

    rows = [summarize_one(path) for path in files]
    out = target_dir / "sam_h5_field_summary.md"
    with out.open("w", encoding="utf-8") as f:
        f.write("| 文件 | 节点数 | 单元类数 | 单元类型 | 结果字段 |\n")
        f.write("|---|---:|---:|---|---|\n")
        for row in rows:
            f.write(f"| {row['file']} | {row['nodes']} | {row['classes']} | {row['types']} | {row['fields']} |\n")

    print(out)
    for row in rows:
        print(f"{row['file']}: nodes={row['nodes']} classes={row['classes']} types={row['types']} fields={row['fields']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
