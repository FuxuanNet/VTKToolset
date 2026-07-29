#ifndef VTUCONTAINERWRITER
#define VTUCONTAINERWRITER

#include <MessageHandler.h>
#include <qlist.h>


class QString;
class ptoKPart;
class QFile;
class bmeMesh;
class bmeElementClass;
class VTUDataContainer;
class odbOdb;
class odiKMesh;
class odbFieldOutput;
class odbFrame;

class VTUContainerWriter
{

public:
	VTUDataContainer* VTKData;
	QList<VTUDataContainer*> VTKDataFramesList;
	
	VTUContainerWriter();
	~VTUContainerWriter();

	int ReadVTKPart(ptoKPart part);
	int ReadVTKMesh(const bmeMesh* mesh);
	VTUDataContainer* GetContainerPointer();
	int VTKExportODB(odbOdb* odb);

	QList<VTUDataContainer*> GetVTKDataFramesList();

	//virtual int WriteFormat();
	
private:
	// odb write
	int BuildFramesList(odbOdb* odb);

	int WriteNodes(const odiKMesh& mesh, VTUDataContainer* container);
	int WriteElements(const odiKMesh& mesh, VTUDataContainer* container);
	int WriteStress(odbOdb* odb, const odiKMesh& mesh, VTUDataContainer* container, odbFrame& frame);
	int WriteCellTensorField(const QString& fieldName, const odiKMesh& mesh, VTUDataContainer* container, odbFrame& frame);

	int WriteField(const odbFieldOutput& field, const odiKMesh& mesh, VTUDataContainer* container);
};

#endif // !VTUCONTAINERWRITER
