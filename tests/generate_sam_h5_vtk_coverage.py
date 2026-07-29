#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成一份小型 SAM H5 覆盖样例，用于 VTKToolset 集成测试。

这份文件的目标很明确：让 SAM 打开后，VTK 导出可以覆盖梁、杆、壳、体单元，
以及 U、UR、S、E、S11/S22/S33/S12、Mises、pressure 等结果字段。

注意：
- 这不是 Nastran 转换结果，只是测试用的 SAM H5。
- 结构参考 NH2SH-v2 当前 writer 生成的 SAM H5。
- 单元连接使用节点“用户编号”，因为 VTKToolset 从 SAM/ODB 网格读出时按用户节点号建立索引。
"""

from __future__ import annotations

from pathlib import Path

import h5py
import numpy as np


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "vtk_test_nh2sh" / "vtk_full_coverage.sam.h5"

PART_NAME = "Part-1"
INSTANCE_NAME = "Part-1-1"


def str_dtype():
    return h5py.special_dtype(vlen=str)


def ensure_group(h5: h5py.File, path: str):
    return h5.require_group(path)


def write_string_attr(obj, name: str, values: list[str]) -> None:
    obj.attrs.create(name, np.array(values, dtype=str_dtype()))


def write_int_attr(obj, name: str, value: int | list[int]) -> None:
    obj.attrs.create(name, np.array(value if isinstance(value, list) else [value], dtype=np.int32))


def write_float_attr(obj, name: str, value: float) -> None:
    obj.attrs.create(name, np.array([value], dtype=np.float32))


def write_dataset(h5: h5py.File, path: str, data, dtype=None) -> None:
    parent = path.rsplit("/", 1)[0]
    ensure_group(h5, parent)
    array = np.array(data, dtype=dtype)
    h5.create_dataset(path, data=array)


def write_string_vector(h5: h5py.File, path: str, values: list[str]) -> None:
    parent = path.rsplit("/", 1)[0]
    ensure_group(h5, parent)
    h5.create_dataset(path, data=np.array(values, dtype=str_dtype()))


def write_string_table(h5: h5py.File, path: str, rows: list[list[str]]) -> None:
    parent = path.rsplit("/", 1)[0]
    ensure_group(h5, parent)
    h5.create_dataset(path, data=np.array(rows, dtype=str_dtype()))


def build_nodes():
    # 每个单元尽量使用不重复的节点，方便在 ParaView 里直接看出不同单元形状。
    coords = [
        # B31
        (0.0, 0.0, 0.0), (1.0, 0.0, 0.0),
        # B33
        (0.0, 0.4, 0.0), (1.0, 0.4, 0.0),
        # T3D2
        (0.0, 0.8, 0.0), (1.0, 0.8, 0.0),
        # S3
        (0.0, 1.4, 0.0), (1.0, 1.4, 0.0), (0.5, 2.1, 0.0),
        # S4
        (1.5, 1.4, 0.0), (2.5, 1.4, 0.0), (2.5, 2.1, 0.0), (1.5, 2.1, 0.0),
        # C3D4
        (0.0, 2.8, 0.0), (1.0, 2.8, 0.0), (0.5, 3.6, 0.0), (0.5, 3.1, 0.8),
        # C3D6
        (1.5, 2.8, 0.0), (2.5, 2.8, 0.0), (2.0, 3.6, 0.0),
        (1.5, 2.8, 0.8), (2.5, 2.8, 0.8), (2.0, 3.6, 0.8),
        # C3D8
        (3.0, 2.8, 0.0), (4.0, 2.8, 0.0), (4.0, 3.8, 0.0), (3.0, 3.8, 0.0),
        (3.0, 2.8, 0.8), (4.0, 2.8, 0.8), (4.0, 3.8, 0.8), (3.0, 3.8, 0.8),
    ]
    labels = list(range(1001, 1001 + len(coords)))
    return labels, coords


def build_element_blocks():
    # connectivity 写用户节点号；labels 写用户看到的单元编号。
    return [
        ("B31", "beam<L Profile>", 2001, 101, [1001, 1002]),
        ("B33", "beam<I Profile>", 2002, 102, [1003, 1004]),
        ("T3D2", "", 2003, 103, [1005, 1006]),
        ("S3", "shell", 2004, 201, [1007, 1008, 1009]),
        ("S4", "shell", 2005, 202, [1010, 1011, 1012, 1013]),
        ("C3D4", "solid", 2006, 301, [1014, 1015, 1016, 1017]),
        ("C3D6", "solid", 2007, 302, [1018, 1019, 1020, 1021, 1022, 1023]),
        ("C3D8", "solid", 2008, 303, [1024, 1025, 1026, 1027, 1028, 1029, 1030, 1031]),
    ]


def tensor_row(element_number: int, location_number: int, scale: float) -> list[float]:
    # 只写 4 个分量，模拟 SAM/NH2SH 当前常见的 [11,22,33,12]。
    # VTKToolset 会自动补成 6 分量，并输出 S13/S23 为 0。
    base = float(element_number)
    return [
        scale * (10.0 + base),
        scale * (20.0 + 0.5 * base),
        scale * (5.0 + location_number),
        scale * (1.0 + 0.25 * base),
    ]


def write_part(h5: h5py.File, node_labels: list[int], coordinates: list[tuple[float, float, float]], blocks) -> None:
    root = f"/Parts/{PART_NAME}"
    write_dataset(h5, f"{root}/Nodes/Labels", node_labels, np.int32)
    write_dataset(h5, f"{root}/Nodes/Coordinates", coordinates, np.float32)

    for class_index, (element_type, section_category, element_label, _pid, conn) in enumerate(blocks):
        class_root = f"{root}/Elements/ElementClass:{class_index}"
        group = ensure_group(h5, class_root)
        write_string_attr(group, "ElementType", [element_type])
        if section_category:
            # SAM 原始格式里这个单词就是 Catergory，照格式写，避免 SAM 读不到。
            write_string_attr(group, "SectionCatergory", [section_category])
        write_dataset(h5, f"{class_root}/Labels", [element_label], np.int32)
        write_dataset(h5, f"{class_root}/Connectivities", [conn], np.int32)


def write_assembly(h5: h5py.File, node_labels: list[int], blocks) -> None:
    root = f"/Assembly/Instances/{INSTANCE_NAME}"
    group = ensure_group(h5, root)
    write_int_attr(group, "Dependent", 1)
    write_string_attr(group, "PartName", [PART_NAME])
    write_dataset(h5, f"{root}/NodeSets/ALL/{INSTANCE_NAME}", node_labels, np.int32)

    for _class_index, (_etype, _category, element_label, pid, _conn) in enumerate(blocks):
        write_dataset(h5, f"{root}/ElementSets/PID_{pid}/{INSTANCE_NAME}", [element_label], np.int32)


def write_materials_and_sections(h5: h5py.File, blocks) -> None:
    for mid in [1]:
        write_dataset(h5, f"/Materials/MAT1_{mid}/Elastic/Real", [[210000.0, 0.3]], np.float32)
        write_dataset(h5, f"/Materials/MAT1_{mid}/Density/Real", [7.85e-9], np.float32)

    categories = {
        "beam<L Profile>": ([1, 5, 9], ["Section point 1", "Section point 5", "Section point 9"]),
        "beam<I Profile>": ([1, 5, 21, 25], ["Section point 1", "Section point 5", "Section point 21", "Section point 25"]),
        "shell": ([1, 3, 5], ["SNEG, (fraction = -1.0)", "Mid, (fraction = 0.0)", "SPOS, (fraction = 1.0)"]),
        "solid": ([1], ["Centroid"]),
    }
    for name, (numbers, descriptions) in categories.items():
        root = f"/SectionCategories/{name}"
        group = ensure_group(h5, root)
        write_string_attr(group, "Description", [name])
        write_dataset(h5, f"{root}/SectionPointNumber", numbers, np.int32)
        write_string_vector(h5, f"{root}/SectionPointDescription", descriptions)

    # Beam 表 10 列，Section Assignments 表 12 列，和 NH2SH-v2 writer 保持一致。
    write_string_table(h5, "/Sections/Beam", [
        ["L", "", "", "0.1", "0.08", "0.01", "0.01", "", "", ""],
        ["I", "", "", "0.2", "0.1", "0.01", "0.01", "0.008", "", ""],
    ])

    assignments = []
    beam_row_by_pid = {101: "0", 102: "1"}
    for _etype, category, _label, pid, _conn in blocks:
        if not category or category == "":
            continue
        row = [""] * 12
        row[0] = PART_NAME
        row[1] = f"PID_{pid}"
        if category == "shell":
            row[2] = "Shell"
        elif category == "solid":
            row[2] = "Solid"
        elif category.startswith("beam<"):
            row[2] = "Beam"
            row[3] = beam_row_by_pid.get(pid, "0")
            row[5], row[6], row[7] = "0", "0", "-1"
            row[9], row[10], row[11] = "0", "0", "0"
        assignments.append(row)
    write_string_table(h5, "/Sections/Assignments", assignments)


def write_step_results(h5: h5py.File, node_labels: list[int], blocks) -> None:
    step_root = "/Steps/Step-1"
    step = ensure_group(h5, step_root)
    write_string_attr(step, "Description", ["VTK coverage static test"])
    write_int_attr(step, "Domain", 0)
    write_int_attr(step, "Index", 1)
    write_string_attr(step, "Procedure", ["*STATIC"])
    write_string_vector(h5, f"{step_root}/LoadCases", ["LoadCase-1"])

    frame_root = f"{step_root}/Frames/Frame:0"
    frame = ensure_group(h5, frame_root)
    write_string_attr(frame, "Description", ["VTK coverage frame"])
    write_int_attr(frame, "Inc/Mode", 1)
    write_float_attr(frame, "Time/Freq", 1.0)
    write_string_attr(frame, "LoadCase", ["LoadCase-1"])

    u = []
    ur = []
    for index, _label in enumerate(node_labels):
        u.append([0.01 * index, 0.02 * index, 0.03 * index])
        ur.append([0.001 * index, 0.002 * index, 0.003 * index])

    for name, labels, values in [
        ("U", ["U1", "U2", "U3"], u),
        ("UR", ["UR1", "UR2", "UR3"], ur),
    ]:
        group = ensure_group(h5, f"{frame_root}/{name}")
        write_string_attr(group, "ComponentLabels", labels)
        write_int_attr(group, "Invariants", [8])
        write_int_attr(group, "Position", 1)
        write_int_attr(group, "Type", 3)
        write_dataset(h5, f"{frame_root}/{name}/{INSTANCE_NAME}/Real", values, np.float32)

    for field_name, is_engineering, scale in [("S", False, 1.0), ("E", True, 0.001)]:
        group = ensure_group(h5, f"{frame_root}/{field_name}")
        labels = ["E11", "E22", "E33", "E12"] if is_engineering else ["S11", "S22", "S33", "S12"]
        write_string_attr(group, "ComponentLabels", labels)
        write_int_attr(group, "Invariants", [1, 2, 3, 11, 9, 10] if is_engineering else [4, 5, 6, 7, 1, 2, 3, 11, 9, 10])
        write_int_attr(group, "Position", 3)
        write_int_attr(group, "Type", 6)
        write_int_attr(group, "isEngineeringTensor", 1 if is_engineering else 0)

        for class_index, (_etype, category, element_label, _pid, _conn) in enumerate(blocks):
            if category.startswith("beam<"):
                locations = [1, 5, 9] if "L Profile" in category else [1, 5, 21, 25]
            elif category == "shell":
                locations = [1, 3, 5]
            elif category == "solid":
                locations = [1]
            else:
                # T3D2 不带截面结果，只验证线单元网格导出。
                continue

            for location in locations:
                data = [tensor_row(element_label, location, scale)]
                write_dataset(
                    h5,
                    f"{frame_root}/{field_name}/{INSTANCE_NAME}/ElementClass:{class_index}/LocationIndex:{location}/Real",
                    data,
                    np.float32,
                )


def main() -> int:
    OUT.parent.mkdir(parents=True, exist_ok=True)
    node_labels, coordinates = build_nodes()
    blocks = build_element_blocks()

    if OUT.exists():
        OUT.unlink()

    with h5py.File(str(OUT), "w") as h5:
        write_part(h5, node_labels, coordinates, blocks)
        write_assembly(h5, node_labels, blocks)
        write_materials_and_sections(h5, blocks)
        write_step_results(h5, node_labels, blocks)

    print(f"Generated: {OUT}")
    print(f"Nodes: {len(node_labels)}")
    print(f"Element classes: {len(blocks)}")
    print("Fields: U, UR, S, E")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
