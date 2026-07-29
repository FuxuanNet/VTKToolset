#pragma once

class QString;

class VTUElementHandler
{
public:
    static enum VTKType {
        VTK_NONE = 0,

        // 0D
        VTK_VERTEX = 1,
        VTK_POLY_VERTEX = 2,

        // 1D
        VTK_LINE = 3,
        VTK_POLY_LINE = 4,
        VTK_QUADRATIC_EDGE = 21,

        // 2D
        VTK_TRIANGLE = 5,
        VTK_QUADRATIC_TRIANGLE = 22,
        VTK_QUAD = 9,
        VTK_QUADRATIC_QUAD = 23,
        VTK_BIQUADRATIC_QUAD = 28,
        VTK_PIXEL = 8,
        VTK_POLYGON = 7,
        VTK_TRIANGLE_STRIP = 6,

        // 3D linear
        VTK_TETRA = 10,
        VTK_HEXAHEDRON = 12,
        VTK_WEDGE = 13,
        VTK_PYRAMID = 14,
        VTK_VOXEL = 11,

        // 3D quadratic
        VTK_QUADRATIC_TETRA = 24,
        VTK_QUADRATIC_HEXAHEDRON = 25,
        VTK_QUADRATIC_WEDGE = 26,
        VTK_QUADRATIC_PYRAMID = 27,

        // high-order / special
        VTK_TRIQUADRATIC_HEXAHEDRON = 29,
        VTK_CONVEX_POINT_SET = 41,
        VTK_POLYHEDRON = 42,

        // user-defined
        VTK_USER_DEFINED_START = 100
    };

public:
	static VTKType SimplifiedConvertor(const QString& typeLabel, int dimension);

	static VTKType ConvertTo1DVTKType(const QString& typeLabel);

	static VTKType SimplyConvertTo1DVTKType(const QString& typeLabel);
	static VTKType SimplyConvertTo2DVTKType(const QString& typeLabel);
	static VTKType SimplyConvertTo3DVTKType(const QString& typeLabel);

	static bool Check3DVTKType(VTUElementHandler::VTKType type);

	static int GetArrayLengthByLabel(const QString& typeLabel);
	static int GetArrayLengthByEnum(VTKType typeEnum);

	/*
	* 由于 VTK 和 SAM 六面体节点编号顺序不同，需要在 VTUContainerWriter 写入 Container 时做转换。
	*/
	static bool IsCube(VTKType typeEnum);

	static QString GetSAMTypeByVTKType(VTKType typeEnum, int beamType = 0, int cubeType = 0, int quadType = 0);
};

