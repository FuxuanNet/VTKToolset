#ifndef FORMATIO 
#define FORMATIO

#include <MessageHandler.h>

class VTUDataContainer;
class QString;
class QFile;
class QTextStream;

class FormatWriter {
public:
	enum State{
		NotInitialized,   
		HeaderWritten,      
		PointsWriting,      
		PointsWritten,
		CellsWriting,       
		CellsWritten,
		CellTypesWriting,   
		CellTypesWritten,
		PointDataWriting,
		PointDataWritten,
		CellDataWriting,
		CellDataWritten,
		MaterialsWriting,   
		FieldHeaderWritten, 
		FieldDataWriting,   
		Finished           
	};

	FormatWriter();
	~FormatWriter();

protected:
	
	QFile* file;
	QTextStream* stream;

	State currentState;
	int nodesWritten;
	int cellsWritten;
	int materialsWritten;
	
	VTUDataContainer* data;

	int fieldDataWritten;    
	int currentNumTuples;    
	int currentNumComponents;

public:
	virtual int Write();
	FormatWriter::State GetCurrentState() const;

	int GetNodesWritten() const;

	int GetCellsWritten() const;
};

class FormatReader {
public:
	
	FormatReader();
	~FormatReader();

protected:
	
	QFile* file;
	QTextStream* stream;

	int status;
	int nodesRead;
	int cellsRead;
	int materialsRead;
	
	VTUDataContainer* data;

	int fieldDataRead;    
	int currentNumTuples;    
	int currentNumComponents;
	int currentDataPerLine;  

public:
	virtual int Read();

	int GetNodesRead() const;

	int GetCellsRead() const;
};

#endif //FORMATIO