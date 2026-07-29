#include "SamH5VtkExporter.h"

#include <VTUContainerWriter.h>
#include <VTUDataContainer.h>
#include <VTUElementHandler.h>
#include <hdf5.h>

#include <QDebug>
#include <QFileInfo>
#include <QMap>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

struct ElementClassInfo {
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
	std::vector<int> nodeLabels;
	std::vector<float> coordinates;
	QMap<int, int> nodeLabelToPosition;
	std::vector<ElementClassInfo> classes;
};

bool exists(hid_t file, const std::string& path)
{
	return H5Lexists(file, path.c_str(), H5P_DEFAULT) > 0;
}

std::vector<hsize_t> datasetDims(hid_t dataset)
{
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

std::vector<float> readFloatDataset(hid_t file, const std::string& path)
{
	std::vector<float> values;
	if (!exists(file, path)) return values;
	hid_t dataset = H5Dopen2(file, path.c_str(), H5P_DEFAULT);
	if (dataset < 0) return values;
	std::vector<hsize_t> dims = datasetDims(dataset);
	hsize_t count = 1;
	for (hsize_t dim : dims) count *= dim;
	values.resize(static_cast<std::size_t>(count));
	if (count > 0) H5Dread(dataset, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, values.data());
	H5Dclose(dataset);
	return values;
}

QString readStringAttribute(hid_t group, const char* name)
{
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
	std::vector<std::string>* names = static_cast<std::vector<std::string>*>(opData);
	names->push_back(name);
	return 0;
}

std::vector<std::string> groupNames(hid_t file, const std::string& path)
{
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

SamMeshData readMesh(hid_t file)
{
	SamMeshData mesh;
	mesh.nodeLabels = readIntDataset(file, "/Parts/Part-1/Nodes/Labels");
	mesh.coordinates = readFloatDataset(file, "/Parts/Part-1/Nodes/Coordinates");
	for (int i = 0; i < static_cast<int>(mesh.nodeLabels.size()); ++i) {
		mesh.nodeLabelToPosition.insert(mesh.nodeLabels[i], i);
	}

	const std::string root = "/Parts/Part-1/Elements";
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

		if (info.vtkType == VTUElementHandler::VTK_NONE || info.nodesPerElement <= 0) {
			qDebug() << "[SamH5VtkExporter] Skip unsupported element type:" << info.type;
			continue;
		}

		info.labels = readIntDataset(file, classPath + "/Labels");
		info.connectivity = readIntDataset(file, classPath + "/Connectivities");
		if (info.labels.empty() || info.connectivity.empty()) continue;
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
	for (int i = 0; i < static_cast<int>(mesh.nodeLabels.size()); ++i) {
		const int base = i * 3;
		if (base + 2 >= static_cast<int>(mesh.coordinates.size())) break;
		container->InsertNextPoint(mesh.nodeLabels[i], mesh.coordinates[base], mesh.coordinates[base + 1], mesh.coordinates[base + 2]);
	}

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
				container->InsertNextElement(static_cast<VTUElementHandler::VTKType>(cls.vtkType), nodes);
			}
			else {
				std::free(nodes);
			}
		}
	}
}

QVector<float> normalizeTensor6(const float* data, int count)
{
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

void writeNodalField(hid_t file, const std::string& framePath, const QString& name, VTUDataContainer* container)
{
	std::vector<float> values = readFloatDataset(file, framePath + "/" + name.toStdString() + "/Part-1-1/Real");
	if (values.empty()) return;
	const int nodes = container->GetNumberOfPoints();
	for (int i = 0; i < nodes; ++i) {
		const int base = i * 3;
		if (base + 2 >= static_cast<int>(values.size())) break;
		container->InsertPointData(name, i, QVector<float>{values[base], values[base + 1], values[base + 2]});
	}
}

void writeTensorField(hid_t file, const std::string& framePath, const QString& fieldName, const SamMeshData& mesh, VTUDataContainer* container)
{
	const std::string fieldRoot = framePath + "/" + fieldName.toStdString() + "/Part-1-1";
	if (!exists(file, fieldRoot)) return;

	QStringList componentLabels;
	const std::string fieldGroupPath = framePath + "/" + fieldName.toStdString();
	hid_t fieldGroup = H5Gopen2(file, fieldGroupPath.c_str(), H5P_DEFAULT);
	if (fieldGroup >= 0) {
		componentLabels = readStringListAttribute(fieldGroup, "ComponentLabels");
		H5Gclose(fieldGroup);
	}

	int globalCellOffset = 0;
	for (int classIndex = 0; classIndex < static_cast<int>(mesh.classes.size()); ++classIndex) {
		const ElementClassInfo& cls = mesh.classes[classIndex];

		// 结果数据在 H5 中仍然使用原始 ElementClass 编号。
		// mesh.classes 会跳过 SPRINGA 等 VTK 不支持的单元，所以这里不能使用过滤后的 classIndex。
		const std::string classRoot = fieldRoot + "/ElementClass:" + std::to_string(cls.h5ClassIndex);
		const int elementCount = static_cast<int>(cls.labels.size());
		std::vector<QVector<float>> sums(elementCount, QVector<float>(6, 0.0f));
		std::vector<int> counts(elementCount, 0);

		for (const std::string& locationName : groupNames(file, classRoot)) {
			std::vector<float> values = readFloatDataset(file, classRoot + "/" + locationName + "/Real");
			if (values.empty() || elementCount <= 0) continue;
			const int components = static_cast<int>(values.size()) / elementCount;
			if (components <= 0) continue;
			for (int e = 0; e < elementCount; ++e) {
				QVector<float> tensor = normalizeTensor6ByLabels(values.data() + e * components, components, componentLabels, fieldName);
				for (int c = 0; c < 6; ++c) sums[e][c] += tensor[c];
				counts[e] += 1;
			}
		}

		for (int e = 0; e < elementCount; ++e) {
			if (counts[e] <= 0) continue;
			QVector<float> avg(6, 0.0f);
			for (int c = 0; c < 6; ++c) avg[c] = sums[e][c] / counts[e];
			const int cellIndex = globalCellOffset + e;
			container->InsertCellData(fieldName, cellIndex, avg);

			QStringList componentNames;
			componentNames << fieldName + "11" << fieldName + "22" << fieldName + "33"
				<< fieldName + "12" << fieldName + "23" << fieldName + "13";
			for (int c = 0; c < componentNames.size(); ++c) {
				container->InsertCellData(componentNames[c], cellIndex, QVector<float>{avg[c]});
			}
			if (fieldName == "S") {
				container->InsertCellData("S_Mises", cellIndex, QVector<float>{mises(avg)});
				container->InsertCellData("S_pressure", cellIndex, QVector<float>{-(avg[0] + avg[1] + avg[2]) / 3.0f});
			}
		}

		globalCellOffset += elementCount;
	}
}

} // namespace

int ExportSamH5ToVtkFrames(const QString& h5Path, VTUContainerWriter* writer)
{
	if (!writer || !QFileInfo(h5Path).exists() || QFileInfo(h5Path).suffix().toLower() != "h5") {
		return -1;
	}

	hid_t file = H5Fopen(h5Path.toLocal8Bit().constData(), H5F_ACC_RDONLY, H5P_DEFAULT);
	if (file < 0) return -1;

	SamMeshData mesh = readMesh(file);
	if (mesh.nodeLabels.empty() || mesh.classes.empty()) {
		H5Fclose(file);
		return -1;
	}

	std::vector<std::string> frames = groupNames(file, "/Steps/Step-1/Frames");
	std::sort(frames.begin(), frames.end(), [](const std::string& a, const std::string& b) {
		const std::string prefix = "Frame:";
		int ia = a.rfind(prefix, 0) == 0 ? std::atoi(a.substr(prefix.size()).c_str()) : 0;
		int ib = b.rfind(prefix, 0) == 0 ? std::atoi(b.substr(prefix.size()).c_str()) : 0;
		return ia < ib;
	});
	if (frames.empty()) frames.push_back("Frame:0");

	for (const std::string& frameName : frames) {
		VTUDataContainer* container = new VTUDataContainer;
		fillMeshContainer(mesh, container);
		const std::string framePath = "/Steps/Step-1/Frames/" + frameName;
		writeNodalField(file, framePath, "U", container);
		writeNodalField(file, framePath, "UR", container);
		writeTensorField(file, framePath, "S", mesh, container);
		writeTensorField(file, framePath, "E", mesh, container);
		writer->VTKDataFramesList.append(container);
	}

	H5Fclose(file);
	qDebug() << "[SamH5VtkExporter] Exported frames from SAM H5:" << writer->VTKDataFramesList.size();
	return 0;
}
