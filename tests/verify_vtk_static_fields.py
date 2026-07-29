import math
import sys
from pathlib import Path


def read_tokens(path):
    tokens = []
    for raw in Path(path).read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        tokens.extend(line.split())
    return tokens


def expect(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name("sample_static_fields.vtk")
    tokens = read_tokens(path)
    text = path.read_text(encoding="utf-8")

    expect("DATASET UNSTRUCTURED_GRID" in text, "missing unstructured grid header")
    expect("POINTS 10 float" in text, "wrong point count")
    expect("CELLS 4 19" in text, "wrong cell count")

    cell_types_pos = tokens.index("CELL_TYPES")
    cell_count = int(tokens[cell_types_pos + 1])
    cell_types = [int(tokens[cell_types_pos + 2 + i]) for i in range(cell_count)]
    expect(cell_types == [3, 5, 9, 10], "expected line, triangle, quad and tetra cell types")

    required_fields = [
        "VECTORS U float",
        "VECTORS UR float",
        "TENSORS S float",
        "TENSORS E float",
        "SCALARS S11 float 1",
        "SCALARS S22 float 1",
        "SCALARS S33 float 1",
        "SCALARS S12 float 1",
        "SCALARS S_Mises float 1",
        "SCALARS S_pressure float 1",
        "SCALARS E11 float 1",
        "SCALARS E12 float 1",
    ]
    for field in required_fields:
        expect(field in text, "missing field: " + field)

    s11, s22, s33, s12, s23, s13 = 100.0, 50.0, 25.0, 10.0, 0.0, 0.0
    expected_mises = math.sqrt(0.5 * ((s11 - s22) ** 2 + (s22 - s33) ** 2 + (s33 - s11) ** 2)
                               + 3.0 * (s12 ** 2 + s23 ** 2 + s13 ** 2))
    expected_pressure = -(s11 + s22 + s33) / 3.0

    expect("68.3740" in text and abs(expected_mises - 68.3740) < 1e-3, "S_Mises formula check failed")
    expect("-58.3333" in text and abs(expected_pressure + 58.3333) < 1e-3, "S_pressure formula check failed")

    print("VTK sample verification passed:", path)


if __name__ == "__main__":
    main()
