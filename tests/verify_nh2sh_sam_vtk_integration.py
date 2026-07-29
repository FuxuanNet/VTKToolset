#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
NH2SH-v2 + VTKToolset 集成检查脚本。

这个脚本做三件事：
1. 调用 NH2SH.exe，把真实 Nastran H5 转成 SAM H5。
2. 用 h5dump 检查 SAM H5 里有没有网格、U/UR、S/E 等结果数据。
3. 如果用户已经在 SAM 里导出了 VTK 文件，则继续检查 VTK 文件字段。

说明：
- SAM GUI 的“打开 H5、点击 File -> Export -> VTK...”仍需要人工操作。
- 这个脚本负责自动检查 GUI 前后的文件是否具备验收条件。
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


DEFAULT_NH2SH_EXE = ROOT / "NH2SH-v2" / "NH2SH" / "Release" / "NH2SH.exe"
DEFAULT_NASTRAN_H5 = ROOT / "nastran_h5" / "Nastran_newItem0702.h5"
DEFAULT_OUTPUT_DIR = ROOT / "vtk_test_nh2sh"
DEFAULT_SAM_H5 = DEFAULT_OUTPUT_DIR / "Nastran_newItem0702.sam.h5"
DEFAULT_VTK = DEFAULT_OUTPUT_DIR / "Nastran_newItem0702_export.vtk"
DEFAULT_H5DUMP = Path("D:/Anaconda3/Library/bin/h5dump.exe")


def sam_input_path(sam_h5: Path) -> Path:
    """NH2SH 约定 foo.sam.h5 对应 foo.sam.inp。"""
    name = sam_h5.name
    if name.endswith(".sam.h5"):
        return sam_h5.with_name(name[:-3] + ".inp")
    return sam_h5.with_name(name + ".sam.inp")


def run_command(args: list[str], cwd: Path | None = None) -> str:
    """运行外部命令，并把 stdout/stderr 合在一起返回，方便失败时定位原因。"""
    completed = subprocess.run(
        args,
        cwd=str(cwd) if cwd else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    output = completed.stdout
    if completed.returncode != 0:
        raise RuntimeError(
            "command failed with exit code {}\n{}\n{}".format(
                completed.returncode,
                " ".join(args),
                output,
            )
        )
    return output


def require_file(path: Path, description: str) -> None:
    if not path.exists():
        raise RuntimeError(f"{description} not found: {path}")


def convert_nastran_to_sam(nh2sh_exe: Path, nastran_h5: Path, sam_h5: Path) -> None:
    """调用 NH2SH。H5 和 INP 必须来自同一次转换，避免模型和结果不一致。"""
    require_file(nh2sh_exe, "NH2SH executable")
    require_file(nastran_h5, "Nastran H5 input")
    sam_h5.parent.mkdir(parents=True, exist_ok=True)
    print("[1/4] Converting Nastran H5 to SAM H5...")
    print(run_command([str(nh2sh_exe), str(nastran_h5), str(sam_h5)]).strip())


def dump_h5_names(h5dump: Path, sam_h5: Path) -> str:
    require_file(h5dump, "h5dump executable")
    require_file(sam_h5, "SAM H5 output")
    return run_command([str(h5dump), "-n", str(sam_h5)])


def check_tokens(text: str, required: list[str], optional: list[str] | None = None) -> bool:
    """检查关键字符串是否存在。这里只查路径/字段名，不解析二进制 HDF5 内容。"""
    ok = True
    for token in required:
        if token in text:
            print(f"  OK       {token}")
        else:
            print(f"  MISSING  {token}")
            ok = False
    for token in optional or []:
        if token in text:
            print(f"  OK       {token}")
        else:
            print(f"  OPTIONAL {token} not found")
    return ok


def check_sam_h5(h5dump: Path, sam_h5: Path) -> None:
    print("[2/4] Checking SAM H5 structure...")
    names = dump_h5_names(h5dump, sam_h5)
    required = [
        "/Parts/Part-1/Nodes/Coordinates",
        "/Parts/Part-1/Nodes/Labels",
        "/Parts/Part-1/Elements/ElementClass:0/Connectivities",
        "/Parts/Part-1/Elements/ElementClass:0/Labels",
        "/Steps/Step-1/Frames/Frame:0/U/Part-1-1/Real",
        "/Steps/Step-1/Frames/Frame:0/UR/Part-1-1/Real",
        "/Steps/Step-1/Frames/Frame:0/S",
        "/Steps/Step-1/Frames/Frame:0/S/Part-1-1",
    ]
    optional = [
        "/Steps/Step-1/Frames/Frame:0/E",
        "/Steps/Step-1/Frames/Frame:0/E/Part-1-1",
    ]
    if not check_tokens(names, required, optional):
        raise RuntimeError("SAM H5 is missing required VTK export inputs")


def check_coverage_sam_h5(h5dump: Path, sam_h5: Path) -> None:
    """检查专用覆盖样例是否包含梁、壳、体单元类型。"""
    print("[coverage] Checking SAM H5 element coverage...")
    # -A 能看到 ElementType 属性值，-n 能看到结果路径；两者合起来才完整。
    header = run_command([str(h5dump), "-A", str(sam_h5)])
    names = dump_h5_names(h5dump, sam_h5)
    combined = header + "\n" + names
    required = [
        '"B31"',
        '"B33"',
        '"T3D2"',
        '"S3"',
        '"S4"',
        '"C3D4"',
        '"C3D6"',
        '"C3D8"',
        "/Steps/Step-1/Frames/Frame:0/S",
        "/Steps/Step-1/Frames/Frame:0/E",
    ]
    if not check_tokens(combined, required):
        raise RuntimeError("coverage SAM H5 is missing expected element or result coverage")


def check_plugin_deployment(sam_release: Path) -> None:
    print("[3/4] Checking VTKToolset deployment...")
    required = [
        sam_release / "VTUFileIO.pyd",
        sam_release / "FilePlugin" / "SAM.Pre.VTUFileIOToolset.dll",
    ]
    for path in required:
        require_file(path, "deployed VTKToolset file")
        print(f"  OK       {path}")


def check_vtk_file(vtk_file: Path) -> bool:
    print("[4/4] Checking exported VTK file...")
    if not vtk_file.exists():
        print(f"  PENDING  VTK file not found: {vtk_file}")
        print("           Open SAM, load the SAM H5, then use File -> Export -> VTK... to generate it.")
        return False

    text = vtk_file.read_text(encoding="utf-8", errors="replace")
    required = [
        "POINTS",
        "CELLS",
        "CELL_TYPES",
        "POINT_DATA",
        "CELL_DATA",
        "VECTORS U float",
        "VECTORS UR float",
        "TENSORS S float",
        "SCALARS S11 float",
        "SCALARS S22 float",
        "SCALARS S12 float",
        "SCALARS S_Mises float",
        "SCALARS S_pressure float",
    ]
    optional = [
        "TENSORS E float",
        "SCALARS E11 float",
        "SCALARS E22 float",
        "SCALARS E12 float",
    ]
    if not check_tokens(text, required, optional):
        raise RuntimeError("exported VTK is missing required fields")
    return True


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Verify NH2SH-v2 SAM H5 and VTKToolset export inputs.")
    parser.add_argument("--nh2sh", type=Path, default=DEFAULT_NH2SH_EXE)
    parser.add_argument("--input", type=Path, default=DEFAULT_NASTRAN_H5)
    parser.add_argument("--output", type=Path, default=DEFAULT_SAM_H5)
    parser.add_argument("--h5dump", type=Path, default=DEFAULT_H5DUMP)
    parser.add_argument("--sam-release", type=Path, default=Path("E:/SAM/Release"))
    parser.add_argument("--vtk", type=Path, default=DEFAULT_VTK)
    parser.add_argument("--skip-convert", action="store_true", help="Only inspect existing SAM H5 and optional VTK file.")
    parser.add_argument("--coverage", action="store_true", help="Also check element type coverage in a generated SAM H5 sample.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if not args.skip_convert:
            convert_nastran_to_sam(args.nh2sh, args.input, args.output)
        else:
            print("[1/4] Skipping conversion.")
        check_sam_h5(args.h5dump, args.output)
        if args.coverage:
            check_coverage_sam_h5(args.h5dump, args.output)
        check_plugin_deployment(args.sam_release)
        vtk_ready = check_vtk_file(args.vtk)
        print("\nResult:")
        print(f"  SAM H5 ready: {args.output}")
        print(f"  SAM INP path: {sam_input_path(args.output)}")
        if vtk_ready:
            print(f"  VTK export verified: {args.vtk}")
        else:
            print("  VTK export verification waits for manual SAM export.")
        return 0
    except Exception as error:
        print(f"\nFAILED: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
