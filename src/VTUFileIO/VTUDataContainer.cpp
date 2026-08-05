#include <VTUDataContainer.h>
#include <MessageHandler.h>
#include <qstring.h>

VTUDataContainer::~VTUDataContainer() {
	int loop = elems.size();
	for (int i = 0; i < loop; ++i) {
		{
			if (elems[i].dataSet != NULL) free(elems[i].dataSet);
			elems[i].dataSet = NULL;
		}
	}
}

VTUDataContainer::VTUDataContainer() {
	elemVertices = 0;
}

void VTUDataContainer::InsertNextPoint(int index, float x, float y, float z) {
	int VTKindex = points.size();
	points.push_back({ x,y,z });
	Index2PositionMap.insert(index, VTKindex);
}

int VTUDataContainer::InsertNextElement(VTUElementHandler::VTKType type, int* dataSet, int nodeCount){

	if (type == VTUElementHandler::VTK_NONE || dataSet == nullptr)
		return ERRORTYPE_WRONG_ELEMENT_DATA;

	Element e;
	e.type = type;
	e.dataSet = dataSet;
	e.nodeCount = nodeCount > 0 ? nodeCount : VTUElementHandler::GetArrayLengthByEnum(type);

	if (type == VTUElementHandler::VTK_VOXEL) {
		int temp;
		temp = dataSet[2];
		dataSet[2] = dataSet[3];
		dataSet[3] = temp;
		temp = dataSet[6];
		dataSet[6] = dataSet[7];
		dataSet[7] = temp;
	}

	elems.push_back(e);
	elemVertices += e.nodeCount + 1;

	return 0;
}

void VTUDataContainer::VTUDataContainer::InsertPointData(const QString& fieldName,
	int pointIndex, const QVector<float>& values)
{
	if (pointIndex < 0 || pointIndex >= points.size()) {
		// TODO:error report
		/*throw std::out_of_range("InsertPointData: pointIndex out of range");*/
	}

	int numComponents = values.size();

	if (!pointData.contains(fieldName)) {
		pointData[fieldName] = QVector<float>();
		pointDataComponents[fieldName] = numComponents;
	}

	QVector<float>& data = pointData[fieldName];
	numComponents = pointDataComponents.value(fieldName, numComponents);

	int requiredSize = points.size() * numComponents;
	if (requiredSize < (pointIndex + 1) * numComponents) {
		requiredSize = (pointIndex + 1) * numComponents;
	}
	if (data.size() < requiredSize) {
		data.resize(requiredSize);
	}

	for (int i = 0; i < numComponents; ++i) {
		data[pointIndex * numComponents + i] = values[i];
	}
}

void VTUDataContainer::InsertCellData(const QString& fieldName,
	int cellIndex,
	const QVector<float>& values)
{
	if (cellIndex < 0 || cellIndex >= elems.size()) {
		// TODO: error report
		/* throw std::out_of_range("InsertCellData: cellIndex out of range"); */
	}

	int numComponents = values.size();

	if (!cellData.contains(fieldName)) {
		cellData[fieldName] = QVector<float>();
		cellDataComponents[fieldName] = numComponents;
	}

	QVector<float>& data = cellData[fieldName];
	numComponents = cellDataComponents.value(fieldName, numComponents);

	int requiredSize = elems.size() * numComponents;
	if (requiredSize < (cellIndex + 1) * numComponents) {
		requiredSize = (cellIndex + 1) * numComponents;
	}
	if (data.size() < requiredSize) {
		data.resize(requiredSize);
	}

	for (int i = 0; i < numComponents; ++i) {
		data[cellIndex * numComponents + i] = values[i];
	}
}

void VTUDataContainer::InsertFieldData(const QString& fieldName, int components, int tuples, const QVector<float>& values)
{
	if (fieldName.isEmpty() || components <= 0 || tuples <= 0 || values.size() < components * tuples) return;
	fieldData[fieldName] = values;
	fieldDataComponents[fieldName] = components;
	fieldDataTuples[fieldName] = tuples;
}
