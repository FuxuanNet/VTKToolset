#include "VTUContainerWriter.h"

#include <ptoKPart.h> 
#include <ftrFeatureList.h>
#include <visKSceneManager.h>
#include <VTUDataContainer.h>

#include <bmeMesh.h> 
#include <bmeElementClass.h>
#include <bmeElementClassList.h>
#include <shpShape.h>


#include <odbOdbRepository.h>       
#include <odbPartRepository.h>      
#include <odbStepRepository.h>      
#include <odbStep.h>                

#include <odbMesh.h>                
#include <odbNode.h>                
#include <ddbMesh.h>               
#include <odiKMesh.h>               
#include <odiKMeshContainer.h>      

#include <odbFrame.h>               
#include <odbFieldOutputRepository.h> 
#include <odbFieldValue.h>          
#include <odbFieldValueData.h>      
#include <odbFieldValueList.h>      
#include <odbFieldOutput.h>
         
              
#include <QDebug>                            
#include <QStringList>

namespace {

// 这个文件是“走 SAM SDK / ODB 对象读取”的导出路线。
// 如果 VTUFileManager 不能直接读取 SAM H5，就会回退到这里读取 SAM 已经打开的 ODB 对象。
// 它和 SamH5VtkExporter.cpp 的目标相同：最终都把数据塞进 VTUDataContainer。

// VTK 的 TENSORS 需要 6 个分量。SAM/ODB 有些壳结果只有 3 或 4 个分量，
// 这里统一补成 [11,22,33,12,23,13]，缺的分量用 0，避免导出文件列数错位。
QVector<float> NormalizeTensor6(const float* data, int numComp)
{
	QVector<float> value(6, 0.0f);
	if (!data || numComp <= 0) return value;

	const int copyCount = qMin(numComp, 6);
	for (int i = 0; i < copyCount; ++i) {
		value[i] = data[i];
	}

	return value;
}

double CalculateMises(const QVector<float>& v)
{
	const double s11 = v.value(0);
	const double s22 = v.value(1);
	const double s33 = v.value(2);
	const double s12 = v.value(3);
	const double s23 = v.value(4);
	const double s13 = v.value(5);

	return sqrt(0.5 * ((s11 - s22) * (s11 - s22)
		+ (s22 - s33) * (s22 - s33)
		+ (s33 - s11) * (s33 - s11))
		+ 3.0 * (s12 * s12 + s23 * s23 + s13 * s13));
}

}


VTUContainerWriter::VTUContainerWriter() {
	this->VTKData = new VTUDataContainer;
}

VTUContainerWriter::~VTUContainerWriter() {

}

// get data of mesh
int VTUContainerWriter::ReadVTKPart(ptoKPart part) {
	ftrFeatureList* ftrlist = part.GetFeatureList();
	if (!(ftrlist->MeshExists(bdoDefaultInstId)))
		return ERRORTYPE_NOTEXIST;

	return ReadVTKMesh(ftrlist->ConstGetMesh(bdoDefaultInstId));
}

// insert data of point and mesh to VTKDatacontainer
int VTUContainerWriter::ReadVTKMesh(const bmeMesh* mesh) {
	if (!(mesh->NumNodes()))
		return ERRORTYPE_WRONG_NODE_DATA;

	// 从 SAM 前处理网格里读取节点。
	// InsertNextPoint 的第一个参数传用户节点编号，容器内部会建立“节点编号 -> VTK 下标”的映射。
	const bmeNodeData& nodeData = mesh->NodeData();
	utiCoordCont3D nodeContainer = nodeData.CoordContainer();
	cowListInt nodeList;
	nodeData.GetUserNodeLabels(nodeList);

	for (int n = 0; n < nodeList.Length(); ++n) {
		float x, y, z;
		nodeContainer.GetCoord(n, x, y, z);
		VTKData->InsertNextPoint(n, x, y, z);
	}

	int status = 0;
	const bmeElementData& elemData = mesh->ElementData();
	const bmeElementClassList& elemClasses = elemData.ConstGetClasses();
	int connectivityIndex = 0;
	for (int l = 0; l < elemClasses.Size(); ++l) {
		const bmeElementClass& elem = elemClasses.ConstGet(l);
		//TODO:Mutiple elements in a single class.Correct the visit function.
		const int* conn = elem.Connectivity();
		VTUElementHandler::VTKType type = VTUElementHandler::SimplifiedConvertor(elem.ElemTypeLabel(), elem.Shape()->NumGeometryDimensions());
		int length = VTUElementHandler::GetArrayLengthByEnum(type);
		for (int e = 0; e < elem.NumElements(); ++e) {
			int* dataSet = (int*)malloc(length * sizeof(int));
			if (dataSet == NULL)
				return ERRORTYPE_MEMORY_ALLOC_FAILED;

			if (conn == NULL) return ERRORTYPE_WRONG_ELEMENT_DATA;
			for (int i = length * e; i < length * (e + 1); ++i) {
				dataSet[i % length] = VTKData->Index2PositionMap.value(conn[i]);
			}
			status |= VTKData->InsertNextElement(type, dataSet);

		}
	}
	return status;
}

VTUDataContainer* VTUContainerWriter::GetContainerPointer() {
	return VTKData;
}

QList<VTUDataContainer*> VTUContainerWriter::GetVTKDataFramesList() {
	return VTKDataFramesList;
}

int VTUContainerWriter::VTKExportODB(odbOdb* odb) {
	int status = BuildFramesList(odb);
	return status;
}

int VTUContainerWriter::BuildFramesList(odbOdb* odb) {
	// ODB 后处理结果通常是 Step -> Frame -> FieldOutput 的层级。
	// 这里按这个层级遍历所有帧，每一帧生成一个 VTUDataContainer，
	// 最后写文件时就能得到 xxx_0.vtk、xxx_1.vtk 这样的多帧结果。
	const odiKModel& model = odb->model();
	const odiKMeshContainer& meshCont = model.ConstMeshContainer();

	odbStepRepository& stepRepo = odb->steps();
	odbStepRepositoryIT stepIt(stepRepo);
	for (stepIt.first(); !stepIt.isDone(); stepIt.next()) {
		const odbStep& step = stepIt.currentValue();

		const odbSequenceFrame& frameSeq = step.frames();
		for (int fidx = 0; fidx < frameSeq.size(); ++fidx) {
			odbFrame& frame = frameSeq[fidx];
			frame.update();

			VTUDataContainer* container = new VTUDataContainer;
			const odbFieldOutputRepository& fieldRepo = frame.fieldOutputs();

			odiKMeshContainerIT meshIt(meshCont);
			for (meshIt.First(); !meshIt.IsDone(); meshIt.Next()) {
				const odiKMesh& mesh = meshIt.CurrentValue();

				WriteNodes(mesh, container);
				WriteElements(mesh, container);
				WriteStress(odb, mesh, container, frame);
				WriteCellTensorField("E", mesh, container, frame);

				odbFieldOutputRepositoryIT fieldIt(fieldRepo);
				for (fieldIt.first(); !fieldIt.isDone(); fieldIt.next()) {
					const odbFieldOutput& field = fieldIt.currentValue();
					WriteField(field, mesh, container);
				}
			}

			VTKDataFramesList.append(container);
		}
	}

	return 0;
}

int VTUContainerWriter::WriteNodes(const odiKMesh& mesh, VTUDataContainer* container) {
	const bmeNodeData& nodeData = mesh.NodeData();
	int numNodes = nodeData.NumNodes();

	container->Index2PositionMap.clear();

	if (numNodes == 0) {
		return 0;
	}

	for (int n = 0; n < numNodes; ++n) {
		float x, y, z;
		if (!nodeData.GetNodalCoord(n, x, y, z)) {
			continue;
		}

		int userLabel = nodeData.GetUserNodeLabel(n);
		container->InsertNextPoint(userLabel, x, y, z);
	}
	return 0;
}

int VTUContainerWriter::WriteElements(const odiKMesh& mesh, VTUDataContainer* container) {
	uint numElemClasses = mesh.NumElementClasses();

	for (uint c = 0; c < numElemClasses; ++c) {
		const bmeElementClass& elemClass = mesh.ElementClass(c);
		uint numElems = elemClass.NumElements();

		const int* conn = elemClass.Connectivity();
		if (!conn) {
			return ERRORTYPE_WRONG_ELEMENT_DATA;
		}

		VTUElementHandler::VTKType type =
			VTUElementHandler::SimplifiedConvertor(elemClass.ElemTypeLabel(),
				elemClass.Shape()->NumGeometryDimensions());

		int length = VTUElementHandler::GetArrayLengthByEnum(type);

		for (int e = 0; e < numElems; ++e) {
			int* dataSet = (int*)malloc(length * sizeof(int));
			if (!dataSet) {
				return ERRORTYPE_MEMORY_ALLOC_FAILED;
			}

			for (int i = length * e; i < length * (e + 1); ++i) {
				int userNodeLabel = conn[i];
				// VTK 单元连接必须写“点数组下标”，不能写 SAM 的用户节点编号。
				// 例如节点号可能是 1001，但 VTK 只认识第 0、1、2... 个点。
				if (!container->Index2PositionMap.contains(userNodeLabel)) {
					free(dataSet);
					return ERRORTYPE_WRONG_ELEMENT_DATA;
				}
				dataSet[i % length] = container->Index2PositionMap.value(userNodeLabel);
			}

			container->InsertNextElement(type, dataSet);
		}
	}


	return 0;
}

int VTUContainerWriter::WriteField(const odbFieldOutput& field, const odiKMesh& mesh, VTUDataContainer* container) {
	QString fieldName = field.name();
	if (fieldName == "S" || fieldName == "E") {
		// S/E 是本课题重点字段，需要额外拆分张量和分量，
		// 所以它们交给 WriteCellTensorField 统一处理。
		return 0;
	}
	odbEnum::odbDataTypeEnum type = field.type();
	odbEnum::odbResultPositionEnum pos = field.position();

	bool isPointData = (pos == odbEnum::NODAL || pos == odbEnum::ELEMENT_NODAL);
	bool isCellData = (pos == odbEnum::CENTROID || pos == odbEnum::ELEMENT_FACE);

	const int valueCount = field.values().size();
	for (int i = 0; i < valueCount; ++i) {
		const odbFieldValue value = field.values(i);

		int index = -1;
		if (isPointData) {
			int nodeLabel = value.nodeLabel();
			index = container->Index2PositionMap.value(nodeLabel, -1);
			if (index < 0) {
				continue;
			}
		}
		else if (isCellData) {
			int elemLabel = value.elementLabel();
			index = mesh.GetMeshElemIndexFromLabel(elemLabel);
			if (index < 0) {
				continue;
			}
		}
		else {
			continue;
		}

		int numComp = 0;
		const float* data = value.data(numComp);
		if (!data || numComp <= 0) {
			continue;
		}

		QVector<float> dataVec;
		dataVec.reserve(numComp);

		if (type == odbEnum::SCALAR) {
			dataVec.append(data[0]);
		}
		else if (type == odbEnum::VECTOR) {
			for (int j = 0; j < numComp; ++j) {
				dataVec.append(data[j]);
			}
		}
		else if (type == odbEnum::MATRIX || type == odbEnum::TENSOR_3D_FULL || type == odbEnum::TENSOR_3D_PLANAR) {
			for (int j = 0; j < numComp; ++j) {
				dataVec.append(data[j]);
			}
		}
		else {
			dataVec.append(data[0]);
		}

		if (isPointData) {
			// 节点字段，例如 U、UR，写入 POINT_DATA。
			container->InsertPointData(fieldName, index, dataVec);
		}
		else if (isCellData) {
			// 单元字段写入 CELL_DATA。
			container->InsertCellData(fieldName, index, dataVec);
		}
	}

	return 0;
}

int VTUContainerWriter::WriteStress(odbOdb* odb, const odiKMesh& mesh, VTUDataContainer* container, odbFrame& frame)
{
	Q_UNUSED(odb);
	return WriteCellTensorField("S", mesh, container, frame);
}

int VTUContainerWriter::WriteCellTensorField(const QString& fieldName, const odiKMesh& mesh, VTUDataContainer* container, odbFrame& frame)
{
	// 这里把一个单元上的多个积分点/截面点结果平均成一个单元中心结果。
	// Legacy VTK 的 CELL_DATA 正好是一单元一行，平均后 ParaView/SAM 外部工具更容易读取。
	const odbFieldOutputRepository& fields = frame.fieldOutputs();
	if (!fields.isMember(fieldName)) {
		return -1;
	}
	const odbFieldOutput& tensorField = fields[fieldName];
	int numValues = tensorField.values().size();

	QMap<int, QVector<QVector<float>>> elemIpData;
	for (int i = 0; i < numValues; ++i) {
		const odbFieldValue& val = tensorField.values(i);
		int elemLabel = val.elementLabel();
		int meshElemIndex = mesh.GetMeshElemIndexFromLabel(elemLabel);
		if (meshElemIndex < 0) {
			qDebug() << "[WriteCellTensorField] Skip field" << fieldName << "element label not found:" << elemLabel;
			continue;
		}

		int numComp = 0;
		const float* raw = val.data(numComp);
		if (!raw || numComp <= 0) {
			qDebug() << "[WriteCellTensorField] Skip field" << fieldName << "empty data on element:" << elemLabel;
			continue;
		}

		elemIpData[meshElemIndex].push_back(NormalizeTensor6(raw, numComp));
	}

	for (auto it = elemIpData.constBegin(); it != elemIpData.constEnd(); ++it) {
		int meshElemIndex = it.key();
		const QVector<QVector<float>>& ipList = it.value();
		if (ipList.isEmpty()) continue;

		int elemClassIndex = mesh.GetClassIndex(meshElemIndex);
		if (elemClassIndex < 0) continue;
		const bmeElementClass& elemClass = mesh.ElementClass(elemClassIndex);

		int localIndex = meshElemIndex - (int)elemClass.FirstElementIndex();
		if (localIndex < 0 || (uint)localIndex >= elemClass.NumElements()) continue;

		QVector<float> avg(6, 0.0f);
		for (int ip = 0; ip < ipList.size(); ++ip) {
			const QVector<float>& comp = ipList[ip];
			for (int c = 0; c < 6; ++c) {
				avg[c] += comp.value(c);
			}
		}
		for (int c = 0; c < 6; ++c) {
			avg[c] /= ipList.size();
		}

		const int vtkCellIndex = meshElemIndex;
		container->InsertCellData(fieldName, vtkCellIndex, avg);

		// 除了 TENSORS S/E，也额外写出单独分量。
		// 这样 VTK.js、ParaView 或其他工具可以直接选 S11、S22、E12 这类标量着色。
		QStringList names;
		names << fieldName + "11"
			<< fieldName + "22"
			<< fieldName + "33"
			<< fieldName + "12"
			<< fieldName + "23"
			<< fieldName + "13";
		for (int c = 0; c < names.size(); ++c) {
			container->InsertCellData(names[c], vtkCellIndex, QVector<float>{avg[c]});
		}

		if (fieldName == "S") {
			// Mises 是等效应力；pressure 按常见符号约定取 -(S11+S22+S33)/3。
			// 这两个字段都作为单分量标量写入 CELL_DATA。
			container->InsertCellData("S_Mises", vtkCellIndex, QVector<float>{static_cast<float>(CalculateMises(avg))});
			container->InsertCellData("S_pressure", vtkCellIndex, QVector<float>{-(avg[0] + avg[1] + avg[2]) / 3.0f});
		}
	}

	return 0;
}
