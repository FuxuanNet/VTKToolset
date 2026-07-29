import json
import math
import sys
from pathlib import Path


CASE_DIR = Path(__file__).resolve().parent / "generated_vtk_cases"


def fail(message):
    raise AssertionError(message)


def read_text(path):
    return Path(path).read_text(encoding="utf-8")


def tokens(text):
    values = []
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        values.extend(line.split())
    return values


def scalar_values(all_tokens, name, count):
    marker = None
    for i in range(len(all_tokens) - 3):
        if all_tokens[i:i + 4] == ["SCALARS", name, "float", "1"]:
            marker = i
            break
    if marker is None:
        fail("missing scalar field " + name)
    start = marker + 6
    return [float(all_tokens[start + i]) for i in range(count)]


def tensor_rows(all_tokens, name, count):
    marker = None
    for i in range(len(all_tokens) - 2):
        if all_tokens[i:i + 3] == ["TENSORS", name, "float"]:
            marker = i
            break
    if marker is None:
        fail("missing tensor field " + name)
    start = marker + 3
    rows = []
    for row in range(count):
        raw = [float(all_tokens[start + row * 9 + j]) for j in range(9)]
        rows.append([raw[0], raw[4], raw[8], raw[1], raw[5], raw[2]])
    return rows


def mises(row):
    s11, s22, s33, s12, s23, s13 = row
    return math.sqrt(
        0.5 * ((s11 - s22) ** 2 + (s22 - s33) ** 2 + (s33 - s11) ** 2)
        + 3.0 * (s12 ** 2 + s23 ** 2 + s13 ** 2)
    )


def close(a, b, eps=1e-4):
    return abs(a - b) <= eps * max(1.0, abs(a), abs(b))


def verify_one(path):
    text = read_text(path)
    all_tokens = tokens(text)

    if "DATASET UNSTRUCTURED_GRID" not in text:
        fail(f"{path.name}: missing DATASET UNSTRUCTURED_GRID")

    point_pos = all_tokens.index("POINTS")
    point_count = int(all_tokens[point_pos + 1])
    cell_pos = all_tokens.index("CELLS")
    cell_count = int(all_tokens[cell_pos + 1])
    type_pos = all_tokens.index("CELL_TYPES")
    type_count = int(all_tokens[type_pos + 1])
    cell_types = [int(all_tokens[type_pos + 2 + i]) for i in range(type_count)]

    if point_count <= 0 or cell_count <= 0:
        fail(f"{path.name}: empty mesh")
    if type_count != cell_count:
        fail(f"{path.name}: CELL_TYPES count does not match CELLS")

    for required in ["POINT_DATA", "CELL_DATA", "VECTORS U float", "VECTORS UR float", "TENSORS S float", "TENSORS E float"]:
        if required not in text:
            fail(f"{path.name}: missing {required}")

    for field in ["S11", "S22", "S33", "S12", "S23", "S13", "S_Mises", "S_pressure", "E11", "E22", "E33", "E12", "E23", "E13"]:
        scalar_values(all_tokens, field, cell_count)

    stress = tensor_rows(all_tokens, "S", cell_count)
    s11 = scalar_values(all_tokens, "S11", cell_count)
    s_mises = scalar_values(all_tokens, "S_Mises", cell_count)
    s_pressure = scalar_values(all_tokens, "S_pressure", cell_count)

    for i, row in enumerate(stress):
        if not close(row[0], s11[i]):
            fail(f"{path.name}: S tensor and S11 mismatch at cell {i}")
        if not close(mises(row), s_mises[i]):
            fail(f"{path.name}: S_Mises formula mismatch at cell {i}")
        if not close(-(row[0] + row[1] + row[2]) / 3.0, s_pressure[i]):
            fail(f"{path.name}: S_pressure formula mismatch at cell {i}")

    return {
        "file": path.name,
        "points": point_count,
        "cells": cell_count,
        "cell_types": sorted(set(cell_types)),
    }


def main():
    case_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else CASE_DIR
    manifest_path = case_dir / "manifest.json"
    if not manifest_path.exists():
        fail("manifest not found, run generate_vtk_cases.py first")

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if len(manifest) != 10:
        fail("expected 10 generated cases")

    summary = []
    for item in manifest:
        summary.append(verify_one(case_dir / item["file"]))

    print("Generated VTK cases verification passed:")
    for item in summary:
        print(f"  {item['file']}: points={item['points']} cells={item['cells']} cell_types={item['cell_types']}")


if __name__ == "__main__":
    main()
