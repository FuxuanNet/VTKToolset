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
	writer = NULL;
	fileWriter = NULL;
	reader = NULL;
	fileReader = NULL;
}

VTUFileManager::~VTUFileManager() {
	if (writer != NULL) delete(writer);
	if (fileWriter != NULL) delete(fileWriter);
}

TargetList::TargetList() {
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

/*
Read in mdb for extract nodes and elements and path to output VTK files
*/
void VTUFileManager::Init(const QString& path, const int& display, const QString& modelName, const QString& partName) {
	this->target.targetPath = path;
	this->target.displayMode = display;
	this->target.targetModel = modelName;
	this->target.targetPart = partName;
};

void VTUFileManager::Init(const QString& path, const QString& modelName) {

	this->target.targetPath = path;
	this->target.targetModel = modelName;
	QString baseName = QFileInfo(path).baseName();
	this->target.targetPart = baseName;
};

void VTUFileManager::Init(const QString& path, const QString& odbPath, const int& display, const QString& modelName) {

	this->target.targetPath = path;
	this->target.odbPath = odbPath;
	this->target.displayMode = display;
	this->target.targetModel = modelName;
};

int VTUFileManager::ReadToCache() {
	writer = new VTUContainerWriter();
	fileReader = new VTKLegacyFormatReader(target.TargetPath(), writer->GetContainerPointer());

	int status = fileReader->Read();

	MessageHandler::ReportImportInfo(fileReader->GetNodesRead(), fileReader->GetCellsRead());
	delete fileReader;

	if(status == 0) reader = new VTUContainerReader(writer->GetContainerPointer());

	return status;
}

int VTUFileManager::ReadToSAM() {
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
	QList<VTUDataContainer*> VTKDataFramesList = writer->GetVTKDataFramesList();
	qDebug() << "[WriteFile] Number of VTK data frames:" << VTKDataFramesList.size();

	QString baseName = target.TargetPath();

	int exportedNodes = 0;
	int exportedCells = 0;

	if (!VTKDataFramesList.isEmpty()) {
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

	delete writer;
	writer = nullptr;
	MessageHandler::ReportExportInfo(exportedNodes, exportedCells);

	return 0;
}
int VTUFileManager::writeSinglePart() {
	const ptoKPartRepository& parts = ConstGetModelParts(target.TargetModel());
	if (parts.IsEmpty())
		return ERRORTYPE_NOTEXIST;
	return writer->ReadVTKPart(parts.ConstGet(target.TargetPart()));
}

int VTUFileManager::writeAllParts() {
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
