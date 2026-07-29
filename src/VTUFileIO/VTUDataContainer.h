#include <qvector.h>
#include <qmap.h>
#include <VTUElementHandler.h>

class shpShape;

struct Point {
	float x;
	float y;
	float z;
};
struct Element {
	VTUElementHandler::VTKType type;
	int* dataSet;
};

class QString;

class VTUDataContainer {
public:

	QVector<Point> points;
	QVector<Element> elems;
	QMap<int, int> Index2PositionMap;

	int elemVertices;

	// ---------------------------------------------------
	QMap<QString, QVector<float>> pointData;     
	QMap<QString, QVector<float>> cellData;   
	QMap<QString, int> pointDataComponents;
	QMap<QString, int> cellDataComponents;
	// QMap<QString, QVector<float>> fieldData;     // 全局场，不依赖单元或节点

	// ---------------------------------------------------               

	void VTUDataContainer::InsertNextPoint(int index, float x, float y, float z);
	int VTUDataContainer::InsertNextElement(VTUElementHandler::VTKType type, int* dataSet);

	void InsertPointData(const QString& fieldName, int pointInd, const QVector<float>& values);
	void InsertCellData(const QString& fieldName, int cellInd, const QVector<float>& values);

	// ----------------------------------------------------
	inline int GetNumberOfPoints() const { return points.size(); }
	inline int GetNumberOfCells() const { return elems.size(); }

	VTUDataContainer();
	~VTUDataContainer();
};
