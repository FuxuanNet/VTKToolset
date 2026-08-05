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
	int nodeCount;
};

class QString;

class VTUDataContainer {
public:

	// VTUDataContainer 是本项目内部最重要的“中转盒子”。
	// 读 SAM 模型、读 SAM H5、读 ODB 的代码，最后都先把数据整理到这个类里；
	// 写 VTK 文件的代码只认这个容器，不需要关心数据最初来自哪里。

	// points 保存 VTK POINTS：每个点一组 x/y/z 坐标。
	QVector<Point> points;

	// elems 保存 VTK CELLS：每个单元有一个 VTK 类型和一组点下标。
	// dataSet 里存的是“点在 points 数组里的位置”，不是 SAM/Nastran 的用户节点编号。
	QVector<Element> elems;

	// Index2PositionMap 用来做“用户节点编号 -> VTK 点下标”的转换。
	// 这是避免 VTK 网格乱掉的关键：VTK 连接关系必须写 0、1、2 这种连续下标。
	QMap<int, int> Index2PositionMap;

	int elemVertices;

	// ---------------------------------------------------
	// pointData 写成 VTK 的 POINT_DATA，适合节点位移 U、转角 UR。
	// cellData 写成 VTK 的 CELL_DATA，适合单元应力 S、单元应变 E。
	// Components 记录每个字段有几个分量，例如 U 是 3，S 是 6，S11 是 1。
	QMap<QString, QVector<float>> pointData;     
	QMap<QString, QVector<float>> cellData;   
	QMap<QString, QVector<float>> fieldData;
	QMap<QString, int> pointDataComponents;
	QMap<QString, int> cellDataComponents;
	QMap<QString, int> fieldDataComponents;
	QMap<QString, int> fieldDataTuples;

	// ---------------------------------------------------               

	void VTUDataContainer::InsertNextPoint(int index, float x, float y, float z);
	int VTUDataContainer::InsertNextElement(VTUElementHandler::VTKType type, int* dataSet, int nodeCount = 0);

	void InsertPointData(const QString& fieldName, int pointInd, const QVector<float>& values);
	void InsertCellData(const QString& fieldName, int cellInd, const QVector<float>& values);
	void InsertFieldData(const QString& fieldName, int components, int tuples, const QVector<float>& values);

	// ----------------------------------------------------
	inline int GetNumberOfPoints() const { return points.size(); }
	inline int GetNumberOfCells() const { return elems.size(); }

	VTUDataContainer();
	~VTUDataContainer();
};
