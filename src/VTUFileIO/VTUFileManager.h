#ifndef VTUFILEMANAGER
#define VTUFILEMANAGER

#include <ptoKPartRepository.h>
#include <basShortcut.h>
#include <QtPlugin>
#include <QString>

class VTUContainerWriter;
class VTUContainerReader;
class FormatWriter;
class FormatReader;
class QString;
class basMdb;

enum omuObjectToDisplayTypeEnm;
enum VTKType {
	VTKLegacy,
	VTU
};

struct TargetList{
	// 导出路径可能包含较深的项目目录。用 QString 保存，避免原来固定 128 个
	// wchar_t 的数组截断路径，导致后端找不到实际的 .sam.h5 文件。
	QString targetPath;
	QString odbPath;
	QString targetModel;
	QString targetPart;
	int targetPartID;
	VTKType type;
	int displayMode;//int type of omuObjectToDisplayTypeEnm

	bool withOdb;
	TargetList();
	QString TargetList::TargetModel();
	QString TargetList::TargetPart();
	QString TargetList::TargetPath();
	QString TargetList::TargetOdbPath();

};

class VTUFileManager
{
private:
	TargetList target;
	VTUContainerWriter* writer;
	VTUContainerReader* reader;
	FormatWriter* fileWriter;
	FormatReader* fileReader;

	static const cowListString& GetAssemblyParts(const QString& model);
	static ptoKPartRepository& GetModelParts(const QString& model);
	static const ptoKPartRepository& ConstGetModelParts(const QString& model);
	static basMdb& GetbasMdb();
	int writeSinglePart();
	int writeAllParts();
	int writeODB();

public:
	VTUFileManager();
	virtual ~VTUFileManager();

	void Init(const QString& targetPath, const int& display, const QString& modelName, const QString& partName);
	void Init(const QString& targetPath, const QString& modelName);
	void Init(const QString& targetPath, const QString& odbPath, const int& display, const QString& modelName);
	int WriteCache();
	int WriteFile();
	int ReadToCache();
	int ReadToSAM();
	void SyncSAM(basNewModelShortcut& modelShortcut);

	const QString GetTargetPartName();
};

#endif // !VTUFILEMANAGER
