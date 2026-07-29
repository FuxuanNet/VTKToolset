#include "VTUElementHandler.h"
#include <qstring.h>

/*
* Query the enum VTK type of the element type label.
* Function query types in all dimensions when dimension = 0.
*/
VTUElementHandler::VTKType VTUElementHandler::SimplifiedConvertor(const QString& typeLabel, int dimension) {
	// 先按元素类型名字判断，再参考几何维度。
	// SAM/ODB 里壳、梁都可能放在三维空间中，Shape()->NumGeometryDimensions()
	// 有时会返回 3。如果只按维度分流，S3I/S4I 这类壳单元会被误判成 3D，
	// 最后变成 VTK_NONE，导出时就被跳过。
	VTKType type = SimplyConvertTo1DVTKType(typeLabel);
	if (type != VTK_NONE) return type;

	type = SimplyConvertTo2DVTKType(typeLabel);
	if (type != VTK_NONE) return type;

	type = SimplyConvertTo3DVTKType(typeLabel);
	if (type != VTK_NONE) return type;

	if (dimension == 1) return VTK_LINE;
	return VTK_NONE;
}

VTUElementHandler::VTKType VTUElementHandler::SimplyConvertTo1DVTKType(const QString& typeLabel) {

	if (typeLabel == "B31" || typeLabel == "B33" || typeLabel == "T3D2") return VTK_LINE;
	return VTK_NONE;
}

VTUElementHandler::VTKType VTUElementHandler::SimplyConvertTo2DVTKType(const QString& typeLabel) {

	if (typeLabel == "S4R" || typeLabel == "S4I" || typeLabel == "S4") return VTK_QUAD;
	if (typeLabel == "S3I" || typeLabel == "S3" || typeLabel == "S3R") return VTK_TRIANGLE;
	return VTK_NONE;
}

VTUElementHandler::VTKType VTUElementHandler::SimplyConvertTo3DVTKType(const QString& typeLabel) {

	if (typeLabel == "C3D4" || typeLabel == "C3D4R") return VTK_TETRA;
	if (typeLabel == "C3D6" || typeLabel == "C3D6R") return VTK_WEDGE;
	if (typeLabel == "C3D8" || typeLabel == "C3D8R") return VTK_HEXAHEDRON;
	if (typeLabel == "C3D10" || typeLabel == "C3D10R") return VTK_QUADRATIC_TETRA;
	if (typeLabel == "C3D20" || typeLabel == "C3D20R") return VTK_QUADRATIC_HEXAHEDRON;

	if (typeLabel == "S4" || typeLabel == "S4R" || typeLabel == "S4I") return VTK_QUAD;
	if (typeLabel == "S3" || typeLabel == "S3R" || typeLabel == "S3I") return VTK_TRIANGLE;

	return VTK_NONE;
}

VTUElementHandler::VTKType VTUElementHandler::ConvertTo1DVTKType(const QString& typeLabel) {

	if (typeLabel == "B31") return VTK_LINE;
	else return VTK_POLY_LINE;
}

bool VTUElementHandler::Check3DVTKType(VTUElementHandler::VTKType type) {

	if (type > 10) return true;

	else return false;
}

//Best avoide using this function because it querys all type.
int  VTUElementHandler::GetArrayLengthByLabel(const QString& typeLabel) {

	return GetArrayLengthByEnum(SimplifiedConvertor(typeLabel, 0));

}

int VTUElementHandler::GetArrayLengthByEnum(VTKType typeEnum) {
	switch (typeEnum) {
	case VTK_VERTEX:
	case VTK_POLY_VERTEX:
		return 1;

	case VTK_LINE:
	case VTK_POLY_LINE:
		return 2;

	case VTK_QUADRATIC_EDGE:
		return 3;

	case VTK_TRIANGLE:
		return 3;

	case VTK_QUADRATIC_TRIANGLE:
		return 6;

	case VTK_QUAD:
	case VTK_PIXEL:
		return 4;

	case VTK_QUADRATIC_QUAD:
		return 8;

	case VTK_BIQUADRATIC_QUAD:
		return 9;

	case VTK_TETRA:
		return 4;

	case VTK_QUADRATIC_TETRA:
		return 10;

	case VTK_HEXAHEDRON:
	case VTK_VOXEL:
		return 8;

	case VTK_QUADRATIC_HEXAHEDRON:
		return 20;

	case VTK_WEDGE:
		return 6;

	case VTK_QUADRATIC_WEDGE:
		return 15;

	case VTK_PYRAMID:
		return 5;

	case VTK_QUADRATIC_PYRAMID:
		return 13;

	case VTK_TRIANGLE_STRIP:
		return 3;

	default:
		return 0;
	}
}


/*
* Return QString type of SAM elements type label.
* For beams, return B31 when beamType=0, B33 when 1, T3D2 when 2.
* For cubes, return C3D8 when cubeType=0, C3D8R when 1.
* For quads, return S4 when quadType=0, S4I when 1, S4R when 2.
*/
QString VTUElementHandler::GetSAMTypeByVTKType(VTKType typeEnum, int beamType, int cubeType, int quadType) {
	switch (typeEnum)
	{
		case VTK_NONE:
			return "";
		case VTK_VERTEX:
			return "";
		case VTK_POLY_VERTEX:
			return "";
		case VTK_LINE:
			return beamType ? (beamType ==1 ? "B33" : "T3D2") : "B31";
		case VTK_TRIANGLE:
			return "S3";
		case VTK_TRIANGLE_STRIP:
			return "";
		case VTK_POLYGON:
			return "";
		case VTK_PIXEL:case VTK_QUAD:
			return quadType ? (quadType == 1 ? "S4I" : "S4R") : "S4";
		case VTK_TETRA:
			return "";
		case VTK_VOXEL:
			return cubeType ? "C3D8R" : "C3D8";
		case VTK_HEXAHEDRON:
			return cubeType ? "C3D8R" : "C3D8";
		default:
			return "";
	}
}

bool VTUElementHandler::IsCube(VTKType typeEnum) {
	return (typeEnum == VTK_VOXEL) || (typeEnum == VTK_HEXAHEDRON);
}
