#include "VTUFileManager.h"

#include <ptoKUtils.h>
#include <ptoKPart.h> 

#include <basMdb.h>
#include <basBasis.h>
#include <basNewModel.h>
#include <omuPrimType.h>
#include <ptoKPartShortcut.h>
#include <ftrPrimaryObjShortcut.h>
#include <asoKUtils.h>
#include <asoFPartInstance.h>

// 下面的头文件用于读取 ODB/后处理结果。
#include<odbOdbRepository.h>
#include <odbOdb.h>
#include<odbmesh.h>
#include<odaOdbFragment.h>
#include <odbPartRepository.h>  // 提供 ODB Part 仓库访问接口
#include <QDebug>
#include <QStringList>
#include <ddbMesh.h>
#include <iterator>
#include <cowbtree.h>
#include <ddbNode.h>
#include <ddbElementData.h>
#include <ddbElementClass.h>

#include <odbStep.h>
#include <odbStepRepository.h>
#include <ddbStep.h>
#include <ddbDdbStep.h>
#include <ddbDdbStepContainer.h>
#include <odbFrame.h>
#include <ddbFrameContainer.h>
#include <odbFieldValue.h>
#include <odbFieldValueData.h>
#include <odbFieldValueList.h>
#include <FEDdbFrame.h>
#include <ddbFieldContainer.h>
#include <odbFieldOutputRepository.h>
#include <ddbField.h>

#include <VTUContainerWriter.h>
#include <VTUContainerReader.h>
#include <VTUFormatIO.h>
#include <VTKLegacyFormatIO.h>
#include <SamH5VtkExporter.h>
#include <MessageHandler.h>
#include <VTUDataContainer.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qstring.h>
#include <qdebug.h>

VTUFileManager::VTUFileManager() {
	// 构造函数先把四个工作指针置空。此时只创建“管理器”，还没有读取或写出数据。
	// NULL/空指针表示未创建对象，析构时据此判断是否需要 delete。
	writer = NULL;
	fileWriter = NULL;
	reader = NULL;
	fileReader = NULL;
}

VTUFileManager::~VTUFileManager() {
	// 析构函数在管理器生命周期结束时释放仍由它持有的对象，避免内存泄漏。
	// delete 空指针本身是安全的，但这里保留显式判断，便于初学者看清所有权。
	if (writer != NULL) delete(writer);
	if (fileWriter != NULL) delete(fileWriter);
}

TargetList::TargetList() {
	// TargetList 保存一次导入/导出任务需要的参数。
	// 默认值表示：Legacy VTK、暂未指定场景、没有额外 ODB 标记。
	targetPartID = 0;
	type = VTKLegacy;
	withOdb = false;
	displayMode = omu_NONE;
}

const ptoKPartRepository& VTUFileManager::ConstGetModelParts(const QString& model) {
	basBasis* bas = basBasis::Instance();
	basMdb mdb = bas->Fetch();
	return ptoKConstGetPartRepos(mdb, model);
}

ptoKPartRepository& VTUFileManager::GetModelParts(const QString& model) {

	basBasis* bas = basBasis::Instance();
	basMdb mdb = bas->Fetch();
	basModelMap& mdoelMap = mdb.GetModels();
	basNewModel& targetModel = mdoelMap.Get(model);
	
	return dynamic_cast<ptoKPartRepository&>(targetModel.GetPart());
}

const cowListString& VTUFileManager::GetAssemblyParts(const QString& model) {

	basBasis* bas = basBasis::Instance();
	basMdb mdb = bas->Fetch();
	const basModelMap& models = mdb.ConstGetModels();

	basNewModel cur_model = models.ConstGet(model);
	return cur_model.ConstGetInstanceTable().Keys();
}

// 三个同名 Init() 参数不同，这是 C++“函数重载”。
// 编译器根据调用时的参数个数和类型选择版本，不是定义了三个不同类。
// 本版本用于导出一个前处理 Part：保存输出路径、显示场景、模型名和 Part 名。
void VTUFileManager::Init(const QString& path, const int& display, const QString& modelName, const QString& partName) {
	this->target.targetPath = path;
	this->target.displayMode = display;
	this->target.targetModel = modelName;
	this->target.targetPart = partName;
};

void VTUFileManager::Init(const QString& path, const QString& modelName) {
	// 本版本用于导入 VTK。输入文件名暂时作为新 Part 的默认名称。
	this->target.targetPath = path;
	this->target.targetModel = modelName;
	QString baseName = QFileInfo(path).baseName();
	this->target.targetPart = baseName;
};

void VTUFileManager::Init(const QString& path, const QString& odbPath, const int& display, const QString& modelName) {
	// 本版本用于后处理导出：path 是用户选择的 VTK 输出基础路径，
	// odbPath 是 SAM 当前后处理对象对应的 H5/ODB 路径。
	this->target.targetPath = path;
	this->target.odbPath = odbPath;
	// 后处理接口历史上仍传入 display，但这里必须固定成 omu_ODB。
	// Q_UNUSED 告诉编译器“参数是接口要求保留的，这里有意不用”，避免未使用警告。
	Q_UNUSED(display);
	this->target.displayMode = omu_ODB;
	this->target.targetModel = modelName;
};

int VTUFileManager::ReadToCache() {
	// 导入第一阶段：把磁盘上的 .vtk 读进统一容器，但暂时不创建 SAM Part。
	// “Cache”不是网络缓存，这里只是指内存中的 VTUDataContainer。
	writer = new VTUContainerWriter();
	fileReader = new VTKLegacyFormatReader(target.TargetPath(), writer->GetContainerPointer());

	int status = fileReader->Read();

	MessageHandler::ReportImportInfo(fileReader->GetNodesRead(), fileReader->GetCellsRead());
	delete fileReader;

	if(status == 0) reader = new VTUContainerReader(writer->GetContainerPointer());

	return status;
}

int VTUFileManager::ReadToSAM() {
	// 导入第二阶段：把内存容器转换成 SAM 网格对象，并创建新 Part。
	QString targetP = target.TargetPart();
	int status = reader->ConstructNewPart(target.TargetModel(), targetP, target.targetPartID);
	// ConstructNewPart 可能会调整最终 Part 名称，直接保存其返回的 QString。
	target.targetPart = targetP;
	//reader->ReleaseMemory();
	delete reader;
	delete writer;
	writer = NULL;
	reader = NULL;
	return status;
}

int VTUFileManager::WriteCache() {
	// 导出第一阶段：创建 writer，根据当前 SAM 场景把数据整理到内存容器。
	// 此函数不写磁盘文件。displayMode 决定数据来自单个 Part、Assembly 还是 ODB/H5。
	writer = new VTUContainerWriter();
	switch (target.displayMode) {
	case omu_PART: {
		return writeSinglePart();
	}
	case omu_ASSEMBLY: {
		return writeAllParts();
	}
	case omu_ODB: {
		return writeODB();
	}
	}
	return ERRORTYPE_WRONG_SCENE;
}

int VTUFileManager::WriteFile() {
	// 导出第二阶段：把 WriteCache() 产生的容器真正写成 .vtk 文件。
	// 后处理通常有多个 Frame，所以先检查帧列表；前处理网格通常只有一个普通容器。
	QList<VTUDataContainer*> VTKDataFramesList = writer->GetVTKDataFramesList();
	qDebug() << "[WriteFile] Number of VTK data frames:" << VTKDataFramesList.size();

	QString baseName = target.TargetPath();

	int exportedNodes = 0;
	int exportedCells = 0;

	if (!VTKDataFramesList.isEmpty()) {
		// 多帧结果按“用户基础路径_帧序号.vtk”命名，例如 result_0.vtk、result_1.vtk。
		exportedNodes = VTKDataFramesList[0]->GetNumberOfPoints();
		exportedCells = VTKDataFramesList[0]->GetNumberOfCells();

		for (int i = 0; i < VTKDataFramesList.size(); ++i) {
			QString fileName = QString("%1_%2.vtk").arg(baseName).arg(i);

			qDebug() << "[WriteFile] Writing VTK frame" << i;

			VTKLegacyFormatWriter* fileWriter = new VTKLegacyFormatWriter(fileName, VTKDataFramesList[i]);
			int status = fileWriter->Write();
			delete fileWriter;

			if (status != 0) {
				delete writer;
				writer = nullptr;
				return status; 
			}
		}
	}
	else {
		// 没有帧列表时，说明导出的是单个 Part/Assembly 网格，直接写普通容器。
		VTUDataContainer* container = writer->GetContainerPointer();
		if (container == nullptr) {
			delete writer;
			writer = nullptr;
			return ERRORTYPE_NOTEXIST;
		}

		if (container->GetNumberOfPoints() == 0 || container->GetNumberOfCells() == 0) {
			delete writer;
			writer = nullptr;
			return ERRORTYPE_NOTEXIST;
		}

		exportedNodes = container->GetNumberOfPoints();
		exportedCells = container->GetNumberOfCells();

		qDebug() << "[WriteFile] Writing single VTK file";
		VTKLegacyFormatWriter* fileWriter = new VTKLegacyFormatWriter(baseName, container);
		int status = fileWriter->Write();
		delete fileWriter;

		if (status != 0) {
			delete writer;
			writer = nullptr;
			return status; 
		}
	}

	// 文件已经写完，释放中间容器的拥有者；把指针置空，避免析构函数再次 delete。
	delete writer;
	writer = nullptr;
	MessageHandler::ReportExportInfo(exportedNodes, exportedCells);

	return 0;
}
int VTUFileManager::writeSinglePart() {
	// 从 SAM 模型仓库取得指定 Part，再交给 writer 提取节点和单元。
	const ptoKPartRepository& parts = ConstGetModelParts(target.TargetModel());
	if (parts.IsEmpty())
		return ERRORTYPE_NOTEXIST;
	return writer->ReadVTKPart(parts.ConstGet(target.TargetPart()));
}

int VTUFileManager::writeAllParts() {
	// Assembly 可能包含多个实例。逐个取得有效 mesh，并追加到同一个 writer 容器。
	asoKAssembly assm = asoKConstGetAssembly(target.TargetModel());
	ftrFeatureList* fl = assm.GetFeatureList();
	cowListInt instIDs = asoFPartInstanceAC::GetActiveIds(*fl);
	int status = 0;
	for (int i = 0; i < instIDs.Length(); ++i) {
		int meshInstID = instIDs.Get(i);
		if (!fl->MeshExists(meshInstID))
			return ERRORTYPE_NOTEXIST;
		const bmeMesh* mesh = fl->ConstGetMesh(meshInstID);
		status |= writer->ReadVTKMesh(mesh);
	}
	return status;
}

int VTUFileManager::writeODB() {
	// 后处理导出的路线调度中心：
	// 1. 优先直接读取真实 H5，保留源字段及位置记录；
	// 2. 路径是 SAM 别名时，从 ODB 对象解析真实文件路径后再试一次；
	// 3. 仍不适用时调用原 ODB SDK，保证旧接口没有被升级路线删除。
	const QString odbPath = target.TargetOdbPath().trimmed();

	// 优先按界面传入的真实 H5 路径直接读取。直接读取能保留 H5 中的
	// ComponentLabels，壳单元的 S11/S22/S12 等分量不会被旧 ODB 接口重排。
	if (ExportSamH5ToVtkFrames(odbPath, writer) == 0) {
		qDebug() << "[VTK export] Direct SAM H5 export succeeded:" << odbPath;
		return 0;
	}

	// OdbPathName 在某些 SAM 版本中返回的是仓库表达式或别名，不能直接
	// 当作 Windows 文件路径使用。ODB 对象自身的 path() 才是 SAM 实际打开的文件。
	odbOdb& odb = odbOdbRepository::Instance().get(odbPath);
	const QString actualH5Path = odb.path().trimmed();
	if (!actualH5Path.isEmpty() && actualH5Path != odbPath) {
		qDebug() << "[VTK export] Retrying direct SAM H5 export with ODB path:" << actualH5Path;
		if (ExportSamH5ToVtkFrames(actualH5Path, writer) == 0) {
			qDebug() << "[VTK export] Direct SAM H5 export succeeded after ODB path resolution.";
			return 0;
		}
	}

	// 只有文件路径无法取得或 H5 结构不受支持时才使用旧 ODB 读取路线。
	// 它保留为兼容入口，但其壳结果分量表达能力弱于直接 H5 路线。
	qDebug() << "[VTK export] Falling back to ODB API export. Requested path:" << odbPath
		<< "; resolved path:" << actualH5Path;
	int status = writer->VTKExportODB(&odb);
	return status;
}

void VTUFileManager::SyncSAM(basNewModelShortcut& modelShortcut) {
	ptoKPartReposInModelShortcut reposInModelSC(modelShortcut);
	ptoKPartReposShortcut reposSC(reposInModelSC);
	ptoKPartInReposShortcut inReposSC(reposSC, target.TargetPart(), target.targetPartID);
	ftrPrimaryObjShortcut sc(inReposSC);
}


QString TargetList::TargetModel() {
	return targetModel;
}

QString TargetList::TargetPart() {
	return targetPart;
}

QString TargetList::TargetPath() {
	return targetPath;
}

QString TargetList::TargetOdbPath() {
	return odbPath;
}

const QString VTUFileManager::GetTargetPartName() {
	return target.TargetPart().trimmed();
}
