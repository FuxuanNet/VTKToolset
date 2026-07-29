import json
import math
from pathlib import Path


OUT_DIR = Path(__file__).resolve().parent / "generated_vtk_cases"


def mises(v):
    s11, s22, s33, s12, s23, s13 = v
    return math.sqrt(
        0.5 * ((s11 - s22) ** 2 + (s22 - s33) ** 2 + (s33 - s11) ** 2)
        + 3.0 * (s12 ** 2 + s23 ** 2 + s13 ** 2)
    )


def pressure(v):
    return -(v[0] + v[1] + v[2]) / 3.0


def tensor_matrix(v):
    s11, s22, s33, s12, s23, s13 = v
    return [s11, s12, s13, s12, s22, s23, s13, s23, s33]


def fmt(value):
    return f"{value:.6g}"


def point_vectors(points, scale):
    values = []
    for i, (x, y, z) in enumerate(points):
        values.append((scale * (x + 0.1 * i), scale * (y + 0.05 * i), scale * (z + 0.02 * i)))
    return values


def make_case(name, description, points, cells, stress_rows, strain_rows):
    return {
        "name": name,
        "description": description,
        "points": points,
        "cells": cells,
        "stress": stress_rows,
        "strain": strain_rows,
        "u": point_vectors(points, 0.01),
        "ur": point_vectors(points, 0.001),
    }


def write_vectors(f, title, rows):
    f.write(f"VECTORS {title} float\n")
    for row in rows:
        f.write(" ".join(fmt(x) for x in row) + "\n")


def write_tensor(f, title, rows):
    f.write(f"TENSORS {title} float\n")
    for row in rows:
        f.write(" ".join(fmt(x) for x in tensor_matrix(row)) + "\n")


def write_scalar(f, title, rows):
    f.write(f"SCALARS {title} float 1\n")
    f.write("LOOKUP_TABLE default\n")
    f.write(" ".join(fmt(x) for x in rows) + "\n")


def write_vtk(case):
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    path = OUT_DIR / f"{case['name']}.vtk"
    points = case["points"]
    cells = case["cells"]
    cell_size = sum(len(conn) + 1 for conn, _ in cells)

    with path.open("w", encoding="utf-8", newline="\n") as f:
        f.write("# vtk DataFile Version 3.0\n")
        f.write(case["description"] + "\n")
        f.write("ASCII\n")
        f.write("DATASET UNSTRUCTURED_GRID\n")
        f.write(f"POINTS {len(points)} float\n")
        for x, y, z in points:
            f.write(f"{fmt(x)} {fmt(y)} {fmt(z)}\n")

        f.write(f"\nCELLS {len(cells)} {cell_size}\n")
        for conn, _ in cells:
            f.write(str(len(conn)) + " " + " ".join(str(i) for i in conn) + "\n")

        f.write(f"\nCELL_TYPES {len(cells)}\n")
        f.write(" ".join(str(t) for _, t in cells) + "\n")

        f.write(f"\nPOINT_DATA {len(points)}\n")
        write_vectors(f, "U", case["u"])
        write_vectors(f, "UR", case["ur"])

        f.write(f"\nCELL_DATA {len(cells)}\n")
        write_tensor(f, "S", case["stress"])
        for idx, label in enumerate(["S11", "S22", "S33", "S12", "S23", "S13"]):
            write_scalar(f, label, [row[idx] for row in case["stress"]])
        write_scalar(f, "S_Mises", [mises(row) for row in case["stress"]])
        write_scalar(f, "S_pressure", [pressure(row) for row in case["stress"]])

        write_tensor(f, "E", case["strain"])
        for idx, label in enumerate(["E11", "E22", "E33", "E12", "E23", "E13"]):
            write_scalar(f, label, [row[idx] for row in case["strain"]])

    return path


def build_cases():
    return [
        make_case(
            "01_beam_rod_line_static",
            "Line elements: beam and truss style static fields",
            [(0, 0, 0), (1, 0, 0), (2, 0.2, 0), (3, 0.2, 0.1)],
            [([0, 1], 3), ([1, 2], 3), ([2, 3], 3)],
            [(100, 60, 20, 5, 0, 0), (80, 40, 10, 0, 3, 0), (30, 20, 15, 2, 1, 4)],
            [(0.001, 0.0006, 0.0002, 0.00005, 0, 0), (0.0008, 0.0004, 0.0001, 0, 0.00003, 0), (0.0003, 0.0002, 0.00015, 0.00002, 0.00001, 0.00004)],
        ),
        make_case(
            "02_shell_tri_quad_planar",
            "Shell elements: triangles and quads with planar stress",
            [(0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 1, 0), (2, 0, 0), (2, 1, 0)],
            [([0, 1, 2], 5), ([1, 3, 2], 5), ([1, 4, 5, 3], 9)],
            [(120, 45, 0, 12, 0, 0), (95, 30, 0, 9, 0, 0), (60, 25, 0, 5, 0, 0)],
            [(0.0012, 0.00045, 0, 0.00012, 0, 0), (0.00095, 0.0003, 0, 0.00009, 0, 0), (0.0006, 0.00025, 0, 0.00005, 0, 0)],
        ),
        make_case(
            "03_solid_tetra_wedge_hex",
            "Solid elements: tetrahedron, wedge and hexahedron",
            [(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1), (2, 0, 0), (3, 0, 0), (2, 1, 0), (2, 0, 1), (3, 0, 1), (2, 1, 1), (4, 0, 0), (5, 0, 0), (5, 1, 0), (4, 1, 0), (4, 0, 1), (5, 0, 1), (5, 1, 1), (4, 1, 1)],
            [([0, 1, 2, 3], 10), ([4, 5, 6, 7, 8, 9], 13), ([10, 11, 12, 13, 14, 15, 16, 17], 12)],
            [(200, 150, 100, 20, 10, 5), (160, 110, 90, 12, 8, 6), (90, 85, 70, 4, 3, 2)],
            [(0.002, 0.0015, 0.001, 0.0002, 0.0001, 0.00005), (0.0016, 0.0011, 0.0009, 0.00012, 0.00008, 0.00006), (0.0009, 0.00085, 0.0007, 0.00004, 0.00003, 0.00002)],
        ),
        make_case(
            "04_mixed_beam_shell_solid",
            "Mixed model: line, shell and solid elements in one file",
            [(0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 1, 0), (0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1), (2, 0, 0), (3, 0, 0)],
            [([8, 9], 3), ([0, 1, 2], 5), ([1, 3, 2, 0], 9), ([0, 1, 3, 2, 4, 5, 6, 7], 12)],
            [(50, 20, 10, 1, 0, 0), (70, 35, 0, 8, 0, 0), (65, 30, 0, 6, 0, 0), (140, 90, 45, 15, 6, 3)],
            [(0.0005, 0.0002, 0.0001, 0.00001, 0, 0), (0.0007, 0.00035, 0, 0.00008, 0, 0), (0.00065, 0.0003, 0, 0.00006, 0, 0), (0.0014, 0.0009, 0.00045, 0.00015, 0.00006, 0.00003)],
        ),
        make_case(
            "05_compression_pressure",
            "Compression case: negative normal stress gives positive pressure",
            [(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1), (2, 0, 0), (3, 0, 0), (3, 1, 0), (2, 1, 0), (2, 0, 1), (3, 0, 1), (3, 1, 1), (2, 1, 1)],
            [([0, 1, 2, 3], 10), ([4, 5, 6, 7, 8, 9, 10, 11], 12)],
            [(-100, -80, -60, 5, 0, 0), (-200, -150, -120, 10, 5, 2)],
            [(-0.001, -0.0008, -0.0006, 0.00005, 0, 0), (-0.002, -0.0015, -0.0012, 0.0001, 0.00005, 0.00002)],
        ),
        make_case(
            "06_shear_dominant",
            "Shear-dominant fields for Mises verification",
            [(0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 1, 0), (0, 0, 1), (1, 0, 1), (0, 1, 1)],
            [([0, 1], 3), ([0, 1, 2], 5), ([0, 1, 2, 4], 10)],
            [(10, 10, 10, 50, 20, 30), (5, 5, 5, 40, 0, 0), (0, 0, 0, 25, 25, 25)],
            [(0.0001, 0.0001, 0.0001, 0.0005, 0.0002, 0.0003), (0.00005, 0.00005, 0.00005, 0.0004, 0, 0), (0, 0, 0, 0.00025, 0.00025, 0.00025)],
        ),
        make_case(
            "07_shell_zero_out_of_plane",
            "Shell plane stress: S33/S23/S13 are zero",
            [(0, 0, 0), (1, 0, 0), (2, 0, 0), (0, 1, 0), (1, 1, 0), (2, 1, 0)],
            [([0, 1, 4, 3], 9), ([1, 2, 5, 4], 9), ([0, 1, 3], 5)],
            [(30, 15, 0, 3, 0, 0), (45, 20, 0, 4, 0, 0), (20, 10, 0, 2, 0, 0)],
            [(0.0003, 0.00015, 0, 0.00003, 0, 0), (0.00045, 0.0002, 0, 0.00004, 0, 0), (0.0002, 0.0001, 0, 0.00002, 0, 0)],
        ),
        make_case(
            "08_quadratic_elements",
            "Quadratic VTK cell types for high-order coverage",
            [(0, 0, 0), (0.5, 0, 0), (1, 0, 0), (0, 1, 0), (0.5, 0.5, 0), (0, 0.5, 0), (2, 0, 0), (3, 0, 0), (3, 1, 0), (2, 1, 0), (2.5, 0, 0), (3, 0.5, 0), (2.5, 1, 0), (2, 0.5, 0), (0, 0, 1), (1, 0, 1), (0, 1, 1), (0, 0, 2), (0.5, 0, 1), (0.5, 0.5, 1), (0, 0.5, 1), (0, 0, 1.5), (0.5, 0, 1.5), (0, 0.5, 1.5)],
            [([0, 1, 2], 21), ([0, 2, 3, 1, 4, 5], 22), ([6, 7, 8, 9, 10, 11, 12, 13], 23), ([14, 15, 16, 17, 18, 19, 20, 21, 22, 23], 24)],
            [(25, 15, 5, 1, 0, 0), (35, 18, 4, 2, 0, 0), (55, 25, 0, 5, 0, 0), (75, 50, 25, 6, 4, 2)],
            [(0.00025, 0.00015, 0.00005, 0.00001, 0, 0), (0.00035, 0.00018, 0.00004, 0.00002, 0, 0), (0.00055, 0.00025, 0, 0.00005, 0, 0), (0.00075, 0.0005, 0.00025, 0.00006, 0.00004, 0.00002)],
        ),
        make_case(
            "09_dense_many_elements",
            "Dense mixed mesh with many cells",
            [(i % 5, i // 5, (i % 3) * 0.2) for i in range(20)],
            [([0, 1], 3), ([1, 2], 3), ([0, 1, 5], 5), ([1, 6, 5], 5), ([2, 3, 8, 7], 9), ([3, 4, 9, 8], 9), ([5, 6, 10, 11], 9), ([6, 7, 12, 11], 9), ([0, 1, 5, 10], 10), ([1, 2, 6, 11], 10), ([10, 11, 12, 15, 16, 17], 13), ([12, 13, 14, 17, 18, 19], 13)],
            [(20 + i * 7, 10 + i * 3, 5 + i, i % 4, (i + 1) % 3, (i + 2) % 5) for i in range(12)],
            [(0.0002 + i * 0.00007, 0.0001 + i * 0.00003, 0.00005 + i * 0.00001, (i % 4) * 0.00001, ((i + 1) % 3) * 0.00001, ((i + 2) % 5) * 0.00001) for i in range(12)],
        ),
        make_case(
            "10_zero_and_extreme_fields",
            "Zero and high contrast values for robustness",
            [(0, 0, 0), (10, 0, 0), (0, 10, 0), (0, 0, 10), (10, 10, 0), (10, 0, 10), (0, 10, 10), (10, 10, 10)],
            [([0, 1], 3), ([0, 1, 2], 5), ([0, 1, 4, 2], 9), ([0, 1, 2, 3], 10), ([0, 1, 4, 2, 3, 5, 7, 6], 12)],
            [(0, 0, 0, 0, 0, 0), (1000, -500, 250, 100, 50, 25), (-300, 600, -150, -30, 15, -10), (1, 2, 3, 4, 5, 6), (500, 500, 500, 0, 0, 0)],
            [(0, 0, 0, 0, 0, 0), (0.01, -0.005, 0.0025, 0.001, 0.0005, 0.00025), (-0.003, 0.006, -0.0015, -0.0003, 0.00015, -0.0001), (0.00001, 0.00002, 0.00003, 0.00004, 0.00005, 0.00006), (0.005, 0.005, 0.005, 0, 0, 0)],
        ),
    ]


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    manifest = []
    for case in build_cases():
        path = write_vtk(case)
        manifest.append({
            "file": path.name,
            "description": case["description"],
            "points": len(case["points"]),
            "cells": len(case["cells"]),
            "cell_types": sorted(set(t for _, t in case["cells"])),
            "fields": ["U", "UR", "S", "E", "S11", "S22", "S33", "S12", "S23", "S13", "S_Mises", "S_pressure", "E11", "E22", "E33", "E12", "E23", "E13"],
        })

    (OUT_DIR / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"Generated {len(manifest)} VTK cases in {OUT_DIR}")


if __name__ == "__main__":
    main()
