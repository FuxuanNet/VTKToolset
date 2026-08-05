#include "SamH5VtkExporter.h"

#include <VTUContainerWriter.h>
#include <VTUDataContainer.h>
#include <VTUElementHandler.h>
#include <hdf5.h>

#include <QDebug>
#include <QFileInfo>
#include <QMap>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

// 这个文件负责“直接读取 SAM H5，再整理成 VTK 数据帧”。
// 它不直接创建界面，也不直接决定输出文件名；它只完成 H5 -> VTUDataContainer。
// VTUFileManager 随后把每个 VTUDataContainer 交给 Legacy VTK writer 落盘。
//
// 为什么增加直接 H5 路线：
// 原项目通过 ODB SDK 取得结果。SDK 适合兼容 SAM 对象，但源 H5 中的字段层级、
// ComponentLabels、积分点和重复记录经过 SDK 封装后不容易原样控制。
// 本文件直接遍历 H5 中实际存在的 Step、Frame 和字段，因此不是只处理 U/UR/S/E；
// RF、RM 或以后新增的字段只要结构符合规则，也会交给统一的 writeField() 处理。
//
// 初学者可把 H5 理解成“文件内部还有目录和数组”：
// group 类似目录，dataset 类似数组，attribute 类似写在目录或数组旁边的说明信息。

struct ElementClassInfo {
	// H5 里通常按 ElementClass 分组保存单元。
	// 一个 ElementClass 可以理解成“一批同类型单元”，例如一批四边壳、一批梁、一批实体。
	QString name;
	QString type;
	int h5ClassIndex = 0;
	bool connectivityUsesNodeIndex = true;
	int vtkType = VTUElementHandler::VTK_NONE;
	int nodesPerElement = 0;
	std::vector<int> labels;
	std::vector<int> connectivity;
};

struct SamMeshData {
	QString partName;
	QString instanceName;
	// nodeLabels 是 SAM/Nastran 用户看到的节点编号，例如 101、102。
	std::vector<int> nodeLabels;

	// coordinates 按 x/y/z 连续保存，长度通常是节点数 * 3。
	std::vector<float> coordinates;

	// VTK 要求单元连接关系使用 points 数组下标，例如 0、1、2。
	// 这里提前建立“节点编号 -> points 下标”的索引，后面写单元时直接查。
	QMap<int, int> nodeLabelToPosition;
	std::vector<ElementClassInfo> classes;
};

int readIntAttribute(hid_t group, const char* name, int defaultValue = 0)
{
	// hid_t 是 HDF5 C API 的句柄，可理解为“已打开对象的编号”。
	// attribute 不存在或打开失败时返回默认值，避免一个可选说明项导致整个导出失败。
	if (H5Aexists(group, name) <= 0) return defaultValue;
	hid_t attr = H5Aopen(group, name, H5P_DEFAULT);
	if (attr < 0) return defaultValue;
	int value = defaultValue;
	// H5T_NATIVE_INT 表示把 H5 中的整数转换成当前 C++ 平台的 int。
	H5Aread(attr, H5T_NATIVE_INT, &value);
	// HDF5 句柄需要显式关闭，类似 C 语言 fopen() 后必须 fclose()。
	H5Aclose(attr);
	return value;
}

bool exists(hid_t file, const std::string& path)
{
	// H5Lexists 用来判断 HDF5 文件里某个路径是否存在。
	// HDF5 很像一个“文件系统”：里面有 group，也有 dataset。
	return H5Lexists(file, path.c_str(), H5P_DEFAULT) > 0;
}

std::vector<hsize_t> datasetDims(hid_t dataset)
{
	// dataset 可能是一维 [节点数]，也可能是二维 [节点数, 分量数]。
	// 本函数只读取形状，不读取数组内容。例如返回 [4, 3] 表示 4 行、每行 3 个值。
	std::vector<hsize_t> dims;
	hid_t space = H5Dget_space(dataset);
	if (space < 0) return dims;
	const int rank = H5Sget_simple_extent_ndims(space);
	if (rank > 0) {
		dims.resize(rank);
		H5Sget_simple_extent_dims(space, dims.data(), nullptr);
	}
	H5Sclose(space);
	return dims;
}

std::vector<int> readIntDataset(hid_t file, const std::string& path)
{
	// 读取 int 类型 dataset。很多 H5 数组可能是一维，也可能是二维；
	// 这里先把各维度相乘，得到总元素数量，再一次性读成扁平数组。
	std::vector<int> values;
	if (!exists(file, path)) return values;
	hid_t dataset = H5Dopen2(file, path.c_str(), H5P_DEFAULT);
	if (dataset < 0) return values;
	std::vector<hsize_t> dims = datasetDims(dataset);
	hsize_t count = 1;
	for (hsize_t dim : dims) count *= dim;
	values.resize(static_cast<std::size_t>(count));
	if (count > 0) H5Dread(dataset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, values.data());
	H5Dclose(dataset);
	return values;
}

std::vector<float> readFloatDataset(hid_t file, const std::string& path, std::vector<hsize_t>* shape = nullptr)
{
	// 返回值统一为一维 std::vector<float>，方便后续用“行下标 * 分量数 + 分量下标”访问。
	// shape 是可选输出参数：调用者需要原二维/多维形状时传入地址，不需要时可省略。
	std::vector<float> values;
	if (!exists(file, path)) return values;
	hid_t dataset = H5Dopen2(file, path.c_str(), H5P_DEFAULT);
	if (dataset < 0) return values;
	std::vector<hsize_t> dims = datasetDims(dataset);
	if (shape) *shape = dims;
	hsize_t count = 1;
	for (hsize_t dim : dims) count *= dim;
	values.resize(static_cast<std::size_t>(count));
	if (count > 0) H5Dread(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, values.data());
	H5Dclose(dataset);
	return values;
}

QString readStringAttribute(hid_t group, const char* name)
{
	// HDF5 字符串有两种存法：变长字符串和固定长度字符串。
	// 两个分支最终都转换为 Qt 的 QString，后续才能方便拼路径和字段名。
	if (H5Aexists(group, name) <= 0) return QString();

	hid_t attr = H5Aopen(group, name, H5P_DEFAULT);
	if (attr < 0) return QString();
	hid_t type = H5Aget_type(attr);
	QString result;

	if (H5Tis_variable_str(type)) {
		char* value = nullptr;
		if (H5Aread(attr, type, &value) >= 0 && value) {
			result = QString::fromUtf8(value);
			H5free_memory(value);
		}
	}
	else {
		const std::size_t size = H5Tget_size(type);
		std::vector<char> buffer(size + 1, '\0');
		if (H5Aread(attr, type, buffer.data()) >= 0) {
			result = QString::fromUtf8(buffer.data());
		}
	}

	H5Tclose(type);
	H5Aclose(attr);
	return result;
}

QStringList readStringListAttribute(hid_t group, const char* name)
{
	// 读取字符串数组，例如 ComponentLabels = ["S11", "S22", "S12"]。
	// 这些标签决定每个数值分量的工程含义，不能只按数组位置盲猜。
	QStringList result;
	if (H5Aexists(group, name) <= 0) return result;

	hid_t attr = H5Aopen(group, name, H5P_DEFAULT);
	if (attr < 0) return result;
	hid_t type = H5Aget_type(attr);
	hid_t space = H5Aget_space(attr);
	hssize_t count = H5Sget_simple_extent_npoints(space);

	if (count > 0 && H5Tis_variable_str(type)) {
		std::vector<char*> values(static_cast<std::size_t>(count), nullptr);
		if (H5Aread(attr, type, values.data()) >= 0) {
			for (char* value : values) {
				result << QString::fromUtf8(value ? value : "");
			}
			H5Dvlen_reclaim(type, space, H5P_DEFAULT, values.data());
		}
	}
	else if (count > 0) {
		const std::size_t itemSize = H5Tget_size(type);
		std::vector<char> buffer(static_cast<std::size_t>(count) * itemSize + 1, '\0');
		if (H5Aread(attr, type, buffer.data()) >= 0) {
			for (hssize_t i = 0; i < count; ++i) {
				const char* item = buffer.data() + static_cast<std::size_t>(i) * itemSize;
				result << QString::fromUtf8(item);
			}
		}
	}

	H5Sclose(space);
	H5Tclose(type);
	H5Aclose(attr);
	return result;
}

herr_t collectGroupNames(hid_t group, const char* name, const H5L_info_t*, void* opData)
{
	// H5Literate 要求传入 C 风格回调函数。每发现一个子项，就调用一次本函数。
	// void* 是“没有具体类型的指针”；这里再转回 vector<string>*，把名称保存进去。
	std::vector<std::string>* names = static_cast<std::vector<std::string>*>(opData);
	names->push_back(name);
	return 0;
}

std::vector<std::string> groupNames(hid_t file, const std::string& path)
{
	// 枚举某个 group 下的全部直接子项，并按名称排序。
	// 本项目用它枚举 Parts、Instances、Steps、Frames，更重要的是枚举每帧实际字段。
	std::vector<std::string> names;
	if (!exists(file, path)) return names;
	hid_t group = H5Gopen2(file, path.c_str(), H5P_DEFAULT);
	if (group < 0) return names;
	hsize_t index = 0;
	H5Literate(group, H5_INDEX_NAME, H5_ITER_INC, &index, collectGroupNames, &names);
	H5Gclose(group);
	std::sort(names.begin(), names.end());
	return names;
}

int classIndexFromName(const std::string& name)
{
	const std::string prefix = "ElementClass:";
	if (name.rfind(prefix, 0) != 0) return 0;
	return std::atoi(name.substr(prefix.size()).c_str());
}

VTUElementHandler::VTKType vtkTypeFromSamType(const QString& type)
{
	return VTUElementHandler::SimplifiedConvertor(type, 0);
}

bool detectConnectivityUsesNodeIndex(const ElementClassInfo& cls, const SamMeshData& mesh)
{
	// SAM H5 的单元连接关系有两种常见写法：
	// 1. 直接使用节点数组下标，例如 0、1、2；
	// 2. 使用用户节点编号，例如 1001、1002、1003。
	// VTK 最终需要的是 points 下标，所以这里先判断输入是哪一种。
	if (cls.connectivity.empty()) return true;

	bool allAsLabels = true;
	bool allAsIndices = true;
	bool hasZero = false;

	for (int rawNode : cls.connectivity) {
		if (rawNode == 0) hasZero = true;
		if (rawNode < 0 || rawNode >= static_cast<int>(mesh.nodeLabels.size())) {
			allAsIndices = false;
		}
		if (!mesh.nodeLabelToPosition.contains(rawNode)) {
			allAsLabels = false;
		}
	}

	// NH2SH 写出的 Connectivities 是 0-based 节点行号，通常会出现 0。
	// 原生 SAM H5 或其他文件可能直接写用户节点编号，例如 1、2、1001。
	// 两种格式都兼容；能明确识别成节点编号时优先按节点编号处理。
	if (hasZero && allAsIndices) return true;
	if (allAsLabels) return false;
	return allAsIndices;
}

QString vtkSafeName(QString value);

SamMeshData readMesh(hid_t file, const QString& partName, const QString& instanceName)
{
	// 读取网格的总入口：
	// - Nodes/Labels 读节点编号；
	// - Nodes/Coordinates 读节点坐标；
	// - Elements/ElementClass:* 读不同类型的单元和连接关系。
	SamMeshData mesh;
	mesh.partName = partName;
	mesh.instanceName = instanceName;
	const std::string partRoot = "/Parts/" + partName.toStdString();
	mesh.nodeLabels = readIntDataset(file, partRoot + "/Nodes/Labels");
	mesh.coordinates = readFloatDataset(file, partRoot + "/Nodes/Coordinates");
	for (int i = 0; i < static_cast<int>(mesh.nodeLabels.size()); ++i) {
		mesh.nodeLabelToPosition.insert(mesh.nodeLabels[i], i);
	}

	const std::string root = partRoot + "/Elements";
	std::vector<std::string> classNames = groupNames(file, root);
	std::sort(classNames.begin(), classNames.end(), [](const std::string& a, const std::string& b) {
		return classIndexFromName(a) < classIndexFromName(b);
	});

	for (const std::string& className : classNames) {
		const std::string classPath = root + "/" + className;
		hid_t group = H5Gopen2(file, classPath.c_str(), H5P_DEFAULT);
		if (group < 0) continue;

		ElementClassInfo info;
		info.name = QString::fromStdString(className);
		info.h5ClassIndex = classIndexFromName(className);
		info.type = readStringAttribute(group, "ElementType");
		info.vtkType = vtkTypeFromSamType(info.type);
		info.nodesPerElement = VTUElementHandler::GetArrayLengthByEnum(static_cast<VTUElementHandler::VTKType>(info.vtkType));
		H5Gclose(group);

		info.labels = readIntDataset(file, classPath + "/Labels");
		info.connectivity = readIntDataset(file, classPath + "/Connectivities");
		if (info.labels.empty() || info.connectivity.empty()) continue;
		if (info.vtkType == VTUElementHandler::VTK_NONE || info.nodesPerElement <= 0) {
			if (info.connectivity.size() % info.labels.size() != 0) {
				qDebug() << "[SamH5VtkExporter] Skip malformed unknown element type:" << info.type;
				continue;
			}
			info.nodesPerElement = static_cast<int>(info.connectivity.size() / info.labels.size());
			info.vtkType = info.nodesPerElement == 1 ? VTUElementHandler::VTK_VERTEX
				: info.nodesPerElement == 2 ? VTUElementHandler::VTK_LINE
				: VTUElementHandler::VTK_CONVEX_POINT_SET;
			qDebug() << "[SamH5VtkExporter] Preserve unknown element type as generic VTK cell:"
				<< info.type << "nodes:" << info.nodesPerElement;
		}
		info.connectivityUsesNodeIndex = detectConnectivityUsesNodeIndex(info, mesh);
		mesh.classes.push_back(info);
	}

	bool fileUsesNodeIndexConnectivity = false;
	for (const ElementClassInfo& cls : mesh.classes) {
		for (int rawNode : cls.connectivity) {
			if (rawNode == 0) {
				fileUsesNodeIndexConnectivity = true;
				break;
			}
		}
		if (fileUsesNodeIndexConnectivity) break;
	}

	if (fileUsesNodeIndexConnectivity) {
		for (ElementClassInfo& cls : mesh.classes) {
			// NH2SH 写出的一个 H5 文件内，各 ElementClass 使用同一种连接编号规则。
			// 只要任意单元类出现节点 0，就说明连接关系是 0-based 节点行号。
			// 不能按每个单元类单独判断，否则某些壳/梁类没有用到节点 0 时，
			// 会被误当成真实节点编号，导入后几何会整体错位并出现乱面。
			cls.connectivityUsesNodeIndex = true;
		}
	}

	return mesh;
}

void fillMeshContainer(const SamMeshData& mesh, VTUDataContainer* container)
{
	// 把 SamMeshData 转成项目内部统一容器 VTUDataContainer。
	// 注意：这里会把 SAM/Nastran 节点编号转换成 VTK 需要的连续点下标。
	for (int i = 0; i < static_cast<int>(mesh.nodeLabels.size()); ++i) {
		const int base = i * 3;
		if (base + 2 >= static_cast<int>(mesh.coordinates.size())) break;
		container->InsertNextPoint(mesh.nodeLabels[i], mesh.coordinates[base], mesh.coordinates[base + 1], mesh.coordinates[base + 2]);
		container->InsertPointData("NodeLabel", i, QVector<float>{static_cast<float>(mesh.nodeLabels[i])});
	}

	QStringList cellElementTypes;
	for (const ElementClassInfo& cls : mesh.classes) {
		for (int e = 0; e < static_cast<int>(cls.labels.size()); ++e) {
			int* nodes = static_cast<int*>(std::malloc(sizeof(int) * cls.nodesPerElement));
			if (!nodes) continue;

			bool ok = true;
			for (int n = 0; n < cls.nodesPerElement; ++n) {
				const int sourceIndex = e * cls.nodesPerElement + n;
				if (sourceIndex >= static_cast<int>(cls.connectivity.size())) {
					ok = false;
					break;
				}

				const int rawNode = cls.connectivity[sourceIndex];

				// SAM H5/NH2SH 里常见的单元连接关系保存的是“节点数组下标”，从 0 开始。
				// VTK 也需要这种从 0 开始的点下标，所以这种情况可以直接使用。
				// 也兼容另一类文件：连接关系保存真实节点编号，例如 1001、1002。
				int vtkNodeIndex = -1;
				if (cls.connectivityUsesNodeIndex && rawNode >= 0 && rawNode < static_cast<int>(mesh.nodeLabels.size())) {
					vtkNodeIndex = rawNode;
				}
				else if (container->Index2PositionMap.contains(rawNode)) {
					vtkNodeIndex = container->Index2PositionMap.value(rawNode);
				}

				if (vtkNodeIndex < 0) {
					ok = false;
					break;
				}
				nodes[n] = vtkNodeIndex;
			}

			if (ok) {
				container->InsertNextElement(static_cast<VTUElementHandler::VTKType>(cls.vtkType), nodes, cls.nodesPerElement);
				const int cellIndex = container->GetNumberOfCells() - 1;
				container->InsertCellData("ElementLabel", cellIndex, QVector<float>{static_cast<float>(cls.labels[e])});
				container->InsertCellData("ElementClass", cellIndex, QVector<float>{static_cast<float>(cls.h5ClassIndex)});
				cellElementTypes.append(vtkSafeName(cls.type));
			}
			else {
				std::free(nodes);
			}
		}
	}
	const QStringList uniqueTypes = QSet<QString>::fromList(cellElementTypes).values();
	for (const QString& elementType : uniqueTypes) {
		const QString fieldName = "ElementType__" + elementType;
		for (int cellIndex = 0; cellIndex < cellElementTypes.size(); ++cellIndex) {
			container->InsertCellData(fieldName, cellIndex, QVector<float>{cellElementTypes[cellIndex] == elementType ? 1.0f : 0.0f});
		}
	}
}

QVector<float> normalizeTensor6(const float* data, int count)
{
	// VTK 的 TENSORS 写出时按 3x3 张量理解。
	// 我们内部统一先整理成 6 个工程分量：
	// [11, 22, 33, 12, 23, 13]。
	// 如果壳单元只有平面分量，例如 S11/S22/S12，缺失的 S33/S23/S13 用 0 补齐。
	QVector<float> value(6, 0.0f);
	for (int i = 0; i < count && i < 6; ++i) value[i] = data[i];
	return value;
}

int tensorComponentSlot(const QString& label, const QString& fieldName)
{
	QString clean = label.trimmed().toUpper();
	clean.remove(" ");
	if (clean.startsWith(fieldName.toUpper())) {
		clean = clean.mid(fieldName.size());
	}

	if (clean == "11" || clean == "XX") return 0;
	if (clean == "22" || clean == "YY") return 1;
	if (clean == "33" || clean == "ZZ") return 2;
	if (clean == "12" || clean == "XY") return 3;
	if (clean == "23" || clean == "YZ") return 4;
	if (clean == "13" || clean == "XZ") return 5;
	return -1;
}

QVector<float> normalizeTensor6ByLabels(const float* data, int count, const QStringList& labels, const QString& fieldName)
{
	QVector<float> value(6, 0.0f);
	if (!data || count <= 0) return value;

	if (!labels.isEmpty()) {
		for (int i = 0; i < count && i < labels.size(); ++i) {
			const int slot = tensorComponentSlot(labels[i], fieldName);
			if (slot >= 0 && slot < 6) {
				value[slot] = data[i];
			}
		}
		return value;
	}

	// 没有 ComponentLabels 时按 SAM 常见顺序兜底：
	// 3 分量通常是梁/杆 [11,22,12]；4 分量通常是壳 [11,22,33,12]。
	if (count == 3) {
		value[0] = data[0];
		value[1] = data[1];
		value[3] = data[2];
		return value;
	}
	if (count == 4) {
		value[0] = data[0];
		value[1] = data[1];
		value[2] = data[2];
		value[3] = data[3];
		return value;
	}

	return normalizeTensor6(data, count);
}

float mises(const QVector<float>& v)
{
	const double s11 = v.value(0);
	const double s22 = v.value(1);
	const double s33 = v.value(2);
	const double s12 = v.value(3);
	const double s23 = v.value(4);
	const double s13 = v.value(5);
	return static_cast<float>(std::sqrt(0.5 * ((s11 - s22) * (s11 - s22)
		+ (s22 - s33) * (s22 - s33)
		+ (s33 - s11) * (s33 - s11))
		+ 3.0 * (s12 * s12 + s23 * s23 + s13 * s13)));
}

QString vtkSafeName(QString value)
{
	// Legacy VTK 字段名不能安全包含空格、路径分隔符等字符。
	// 统一替换成下划线，只改变输出名称，不改变数值。
	for (int i = 0; i < value.size(); ++i) {
		const QChar ch = value.at(i);
		if (ch.isSpace() || ch == '/' || ch == '\\' || ch == ':' || ch == ';') value[i] = '_';
	}
	return value;
}

QString componentFieldName(const QString& outputName, const QString& fieldName, const QStringList& labels, int component)
{
	// 为一个多分量字段生成单独分量名。
	// 例：U 的三个分量生成 U1/U2/U3；S 统一生成 S11/S22/S33/S12/S23/S13。
	// 如果是精确记录 S__LocationIndex_1，则同样把后缀带到分量名上，避免不同位置互相覆盖。
	QString label;
	if (fieldName == "S" || fieldName == "E") {
		const QStringList tensorSuffixes{"11", "22", "33", "12", "23", "13"};
		if (component < tensorSuffixes.size()) label = fieldName + tensorSuffixes[component];
	}
	else if (component < labels.size()) {
		label = vtkSafeName(labels[component]);
	}
	if (label.isEmpty()) label = fieldName + QString::number(component + 1);
	QString suffix;
	if (outputName.startsWith(fieldName)) suffix = outputName.mid(fieldName.size());
	else suffix = "__" + outputName;
	return label + suffix;
}

int componentCount(const std::vector<float>& values, const std::vector<hsize_t>& shape, int records, const QStringList& labels)
{
	// 按可靠程度推断每条记录有几个分量：
	// 1. 优先使用 dataset 最后一维；2. 再用 ComponentLabels 数量；3. 最后用总数/记录数。
	// 例如 shape=[4,3]，表示 4 条记录、每条 3 个分量。
	if (shape.size() >= 2 && shape.back() > 0) return static_cast<int>(shape.back());
	if (!labels.isEmpty()) return labels.size();
	if (records > 0 && values.size() % records == 0) return static_cast<int>(values.size()) / records;
	return 1;
}

void insertPointTuple(VTUDataContainer* container, const QString& outputName, const QString& fieldName,
	const QStringList& labels, int pointIndex, const QVector<float>& tuple)
{
	// tuple 表示“一个节点的一整条结果”，例如 U=[U1,U2,U3]。
	// 先保存整体 U，便于按向量/模长显示；再保存 U1/U2/U3，便于按单分量着色。
	container->InsertPointData(outputName, pointIndex, tuple);
	if (tuple.size() <= 1) return;
	for (int component = 0; component < tuple.size(); ++component) {
		container->InsertPointData(componentFieldName(outputName, fieldName, labels, component), pointIndex,
			QVector<float>{tuple[component]});
	}
}

void insertCellTuple(VTUDataContainer* container, const QString& outputName, const QString& fieldName,
	const QStringList& labels, int cellIndex, const QVector<float>& tuple)
{
	// 与节点版本相同，只是目标换成 CELL_DATA。例如同时保存 S 和 S11/S22/...。
	container->InsertCellData(outputName, cellIndex, tuple);
	if (tuple.size() <= 1) return;
	for (int component = 0; component < tuple.size(); ++component) {
		container->InsertCellData(componentFieldName(outputName, fieldName, labels, component), cellIndex,
			QVector<float>{tuple[component]});
	}
}

void writeNodalDataset(hid_t file, const std::string& datasetPath, const QString& outputName,
	const QString& fieldName, const QStringList& labels, VTUDataContainer* container)
{
	// 节点字段常见布局为 [节点数, 分量数]。
	// HDF5 读出后是一维数组，因此第 node 行第 component 列的位置是：
	// node * components + component。这和 C 语言二维数组按行连续存储相同。
	std::vector<hsize_t> shape;
	std::vector<float> values = readFloatDataset(file, datasetPath, &shape);
	if (values.empty()) return;
	const int nodes = container->GetNumberOfPoints();
	const int components = componentCount(values, shape, nodes, labels);
	if (components <= 0 || static_cast<int>(values.size()) < nodes * components) return;
	for (int node = 0; node < nodes; ++node) {
		QVector<float> tuple;
		tuple.reserve(components);
		for (int component = 0; component < components; ++component) {
			tuple.append(values[node * components + component]);
		}
		insertPointTuple(container, outputName, fieldName, labels, node, tuple);
	}
}

void writeGlobalDataset(hid_t file, const std::string& datasetPath, const QString& outputName,
	const QStringList& labels, VTUDataContainer* container)
{
	// 全局结果不属于某个节点或单元，不能硬塞进 POINT_DATA/CELL_DATA。
	// 保存到 fieldData，最终由 Legacy writer 写成标准 FIELD FieldData。
	std::vector<hsize_t> shape;
	std::vector<float> values = readFloatDataset(file, datasetPath, &shape);
	if (values.empty()) return;
	int components = !labels.isEmpty() ? labels.size() : 1;
	if (shape.size() >= 2 && shape.back() > 0) components = static_cast<int>(shape.back());
	if (components <= 0 || values.size() % components != 0) components = 1;
	const int tuples = static_cast<int>(values.size()) / components;
	QVector<float> data;
	data.reserve(static_cast<int>(values.size()));
	for (float value : values) data.append(value);
	container->InsertFieldData(outputName, components, tuples, data);
}

QVector<float> normalizedTuple(const float* data, int components, const QStringList& labels, const QString& fieldName)
{
	// S/E 是张量，需要按 ComponentLabels 放到统一六分量顺序。
	// 其他字段不改变分量顺序，尽量保持源文件语义。
	if (fieldName == "S" || fieldName == "E") {
		return normalizeTensor6ByLabels(data, components, labels, fieldName);
	}
	QVector<float> tuple;
	tuple.reserve(components);
	for (int component = 0; component < components; ++component) tuple.append(data[component]);
	return tuple;
}

void writeElementField(hid_t file, const std::string& framePath, const QString& fieldName,
	const QString& instanceName, const QStringList& labels, const SamMeshData& mesh, VTUDataContainer* container)
{
	// 单元字段比节点字段复杂：一个单元可能有多个积分点、截面点或单元节点记录。
	// 本函数采取“双份保留”策略：
	// 1. 每个位置/记录单独保存为精确字段，防止信息丢失；
	// 2. 同一单元的实部记录再求平均，继续生成旧用户熟悉的 S/E 兼容字段。
	const std::string fieldRoot = framePath + "/" + fieldName.toStdString() + "/" + instanceName.toStdString();
	if (!exists(file, fieldRoot)) return;
	// container 中所有 ElementClass 的单元放在一个连续数组里。
	// globalCellOffset 记录当前单元类在全局 CELL_DATA 中从哪里开始。
	int globalCellOffset = 0;
	for (const ElementClassInfo& cls : mesh.classes) {
		const std::string classRoot = fieldRoot + "/ElementClass:" + std::to_string(cls.h5ClassIndex);
		const int elementCount = static_cast<int>(cls.labels.size());
		// sums/counts 只用于计算兼容平均值；精确记录会在内层循环立即写入，不会被平均替代。
		std::vector<QVector<float>> sums(elementCount);
		std::vector<int> counts(elementCount, 0);

		// locationName 可能代表积分点、截面位置等。Real/Imag 分别是复数的实部和虚部。
		for (const std::string& locationName : groupNames(file, classRoot)) {
			for (const QString& part : QStringList{"Real", "Imag"}) {
				std::vector<hsize_t> shape;
				const std::string datasetPath = classRoot + "/" + locationName + "/" + part.toStdString();
				std::vector<float> values = readFloatDataset(file, datasetPath, &shape);
				if (values.empty() || elementCount <= 0) continue;
				const int components = componentCount(values, shape, elementCount, labels);
				if (components <= 0 || values.size() % components != 0) continue;
				const int recordCount = static_cast<int>(values.size()) / components;
				if (recordCount < elementCount || recordCount % elementCount != 0) continue;
				// 总记录数能够整除单元数时，商就是每个单元拥有的记录数。
				const int recordsPerElement = recordCount / elementCount;
				for (int element = 0; element < elementCount; ++element) {
					for (int record = 0; record < recordsPerElement; ++record) {
						const int offset = (element * recordsPerElement + record) * components;
						QVector<float> tuple = normalizedTuple(values.data() + offset, components, labels, fieldName);
						// 名称编码数据来源，例：S__LocationIndex_1__Record_2__Imag。
						// 这样多个位置、多个记录和虚部可以同时存在，不会写入同名数组互相覆盖。
						QString exactName = fieldName + "__" + vtkSafeName(QString::fromStdString(locationName));
						if (recordsPerElement > 1) exactName += "__Record_" + QString::number(record + 1);
						if (part == "Imag") exactName += "__Imag";
						const int cellIndex = globalCellOffset + element;
						container->InsertCellData(exactName, cellIndex, tuple);
						if (fieldName == "S" && part == "Real") {
							const QString suffix = exactName.mid(fieldName.size());
							container->InsertCellData("S_Mises" + suffix, cellIndex, QVector<float>{mises(tuple)});
							container->InsertCellData("S_pressure" + suffix, cellIndex,
								QVector<float>{-(tuple[0] + tuple[1] + tuple[2]) / 3.0f});
						}
						if (part == "Real") {
							if (sums[element].isEmpty()) sums[element] = QVector<float>(tuple.size(), 0.0f);
							if (sums[element].size() == tuple.size()) {
								for (int component = 0; component < tuple.size(); ++component) sums[element][component] += tuple[component];
								counts[element] += 1;
							}
						}
					}
				}
			}
		}

		// 精确数据全部写完后，再为每个单元生成一个平均实部字段。
		// 这一步保留旧版 S/E 使用方式；需要精确结果时选择带双下划线的字段。
		for (int element = 0; element < elementCount; ++element) {
			if (counts[element] <= 0 || sums[element].isEmpty()) continue;
			QVector<float> average = sums[element];
			for (int component = 0; component < average.size(); ++component) average[component] /= counts[element];
			const int cellIndex = globalCellOffset + element;
			insertCellTuple(container, fieldName, fieldName, labels, cellIndex, average);
			if (fieldName == "S") {
				container->InsertCellData("S_Mises", cellIndex, QVector<float>{mises(average)});
				container->InsertCellData("S_pressure", cellIndex,
					QVector<float>{-(average[0] + average[1] + average[2]) / 3.0f});
			}
		}
		globalCellOffset += elementCount;
	}
}

void writeField(hid_t file, const std::string& framePath, const QString& fieldName,
	const SamMeshData& mesh, VTUDataContainer* container)
{
	// 所有字段统一从这里分发。它不写死 U、UR、RF、RM 等名字，而是查看 H5 结构和 Position。
	// 可以类比 Python 路由函数：识别数据属于哪里，再调用对应处理函数。
	const std::string fieldPath = framePath + "/" + fieldName.toStdString();
	hid_t fieldGroup = H5Gopen2(file, fieldPath.c_str(), H5P_DEFAULT);
	if (fieldGroup < 0) return;
	// ComponentLabels 说明分量含义；Position 说明字段属于节点、单元还是全局区域。
	const QStringList labels = readStringListAttribute(fieldGroup, "ComponentLabels");
	const int position = readIntAttribute(fieldGroup, "Position", 0);
	H5Gclose(fieldGroup);
	const std::string instanceRoot = fieldPath + "/" + mesh.instanceName.toStdString();
	if (!exists(file, instanceRoot)) return;
	// 节点/全局字段的 Real/Imag dataset 直接位于 Instance 下；
	// 单元字段下面还会继续出现 ElementClass 和 Location 层级，因此走最后的分支。
	if (exists(file, instanceRoot + "/Real") || exists(file, instanceRoot + "/Imag")) {
		if (position == 1 || position == 2 || position == 13) {
			writeNodalDataset(file, instanceRoot + "/Real", fieldName, fieldName, labels, container);
			writeNodalDataset(file, instanceRoot + "/Imag", fieldName + "__Imag", fieldName, labels, container);
		}
		else {
			writeGlobalDataset(file, instanceRoot + "/Real", fieldName, labels, container);
			writeGlobalDataset(file, instanceRoot + "/Imag", fieldName + "__Imag", labels, container);
		}
		return;
	}
	writeElementField(file, framePath, fieldName, mesh.instanceName, labels, mesh, container);
}

} // namespace

int ExportSamH5ToVtkContainers(const QString& h5Path, QList<VTUDataContainer*>* framesOutput)
{
	// 这是 SAM H5 -> VTK 多帧导出的总入口。
	// VTUFileManager 会先尝试调用这里；成功后 writer->VTKDataFramesList 里会得到多帧容器，
	// 后续 WriteFile 会把每一帧分别写成一个 .vtk 文件。
	// 返回 0 表示直接 H5 路线成功；返回 -1 表示“不适用或读取失败”，上层会回退 ODB SDK。
	// 因此这里的失败不是整个导出必然失败，而是告诉调度器继续尝试兼容路线。
	if (!framesOutput) {
		qDebug() << "[SamH5VtkExporter] No output container writer was supplied.";
		return -1;
	}

	// QFileInfo 只负责检查路径和扩展名；真正读取文件使用下面的 H5Fopen。
	const QFileInfo h5File(h5Path);
	if (!h5File.exists() || !h5File.isFile()) {
		qDebug() << "[SamH5VtkExporter] The ODB reference is not an existing file:" << h5Path;
		return -1;
	}
	if (h5File.suffix().toLower() != "h5") {
		qDebug() << "[SamH5VtkExporter] The ODB file is not an H5 file:" << h5Path;
		return -1;
	}

	const QString absolutePath = h5File.absoluteFilePath();
	qDebug() << "[SamH5VtkExporter] Reading SAM H5 directly:" << absolutePath;
	// H5F_ACC_RDONLY 表示只读打开，导出过程不会修改用户的源 H5。
	hid_t file = H5Fopen(absolutePath.toLocal8Bit().constData(), H5F_ACC_RDONLY, H5P_DEFAULT);
	if (file < 0) {
		qDebug() << "[SamH5VtkExporter] H5Fopen failed:" << absolutePath;
		return -1;
	}

	const std::vector<std::string> parts = groupNames(file, "/Parts");
	const std::vector<std::string> instances = groupNames(file, "/Assembly/Instances");
	// 当前直接路线只在单 Part、单 Instance 时建立了一一对应关系。
	// 多 Part/Instance 若强行拼接，节点下标、Instance 结果和单元偏移可能对应错误。
	// 所以检测到复杂结构时主动返回 -1，让成熟的 ODB SDK 路线接管，而不是输出错误 VTK。
	if (parts.size() != 1 || instances.size() != 1) {
		qDebug() << "[SamH5VtkExporter] Direct H5 export requires one part and one instance; using ODB fallback."
			<< "parts:" << parts.size() << "instances:" << instances.size();
		H5Fclose(file);
		return -1;
	}
	SamMeshData mesh = readMesh(file, QString::fromStdString(parts.front()), QString::fromStdString(instances.front()));
	if (mesh.nodeLabels.empty() || mesh.classes.empty()) {
		qDebug() << "[SamH5VtkExporter] Required mesh groups are absent or contain no supported cells."
			<< "nodes:" << mesh.nodeLabels.size() << "element classes:" << mesh.classes.size();
		H5Fclose(file);
		return -1;
	}

	// 不假设 Step 名为 Step-1，也不只处理第一步；实际有几个 Step 就遍历几个。
	std::vector<std::string> steps = groupNames(file, "/Steps");
	for (const std::string& stepName : steps) {
		const std::string framesRoot = "/Steps/" + stepName + "/Frames";
		std::vector<std::string> frames = groupNames(file, framesRoot);
		// 字符串排序会把 Frame:10 放在 Frame:2 前面，所以取冒号后的数字再比较。
		// [](...) { ... } 是 C++ lambda，可理解为临时写在这里的“小比较函数”。
		std::sort(frames.begin(), frames.end(), [](const std::string& a, const std::string& b) {
			const std::string prefix = "Frame:";
			int ia = a.rfind(prefix, 0) == 0 ? std::atoi(a.substr(prefix.size()).c_str()) : 0;
			int ib = b.rfind(prefix, 0) == 0 ? std::atoi(b.substr(prefix.size()).c_str()) : 0;
			return ia < ib;
		});
		for (const std::string& frameName : frames) {
			// 每个 Frame 创建独立容器。同一份网格会复制到每帧，再附加该帧自己的结果字段。
			VTUDataContainer* container = new VTUDataContainer;
			fillMeshContainer(mesh, container);
			const std::string framePath = framesRoot + "/" + frameName;
			// 核心升级点：枚举源 Frame 下实际存在的全部字段，而不是写死 U/UR/S/E 白名单。
			// 新字段只要遵循相同 H5 层级，就会自动进入 writeField() 分类处理。
			for (const std::string& fieldName : groupNames(file, framePath)) {
				writeField(file, framePath, QString::fromStdString(fieldName), mesh, container);
			}
			framesOutput->append(container);
			qDebug() << "[SamH5VtkExporter] Step/frame" << QString::fromStdString(stepName)
				<< QString::fromStdString(frameName)
				<< "cells:" << container->GetNumberOfCells()
				<< "point fields:" << container->pointData.keys()
				<< "cell fields:" << container->cellData.keys();
		}
	}
	if (framesOutput->isEmpty()) {
		// 某些 H5 只有网格、没有 Step/Frame 结果。仍输出一帧纯几何，避免合法网格被判失败。
		VTUDataContainer* container = new VTUDataContainer;
		fillMeshContainer(mesh, container);
		framesOutput->append(container);
	}

	// 所有数据已经复制进 C++ 容器，关闭 H5 文件不会影响后续 VTK 写出。
	H5Fclose(file);
	qDebug() << "[SamH5VtkExporter] Exported frames from SAM H5:" << framesOutput->size();
	return 0;
}

int ExportSamH5ToVtkFrames(const QString& h5Path, VTUContainerWriter* writer)
{
	// 这是给正式 SAM 导出链使用的薄封装。
	// 真正转换逻辑集中在 ExportSamH5ToVtkContainers()，独立 Smoke Test 也调用同一核心，
	// 因此测试通过的不是另一套模拟代码，而是正式插件使用的转换代码。
	if (!writer) return -1;
	return ExportSamH5ToVtkContainers(h5Path, &writer->VTKDataFramesList);
}
