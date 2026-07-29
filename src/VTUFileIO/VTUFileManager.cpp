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

//�����ͷ�ļ���Ϊ��print����������ӵ�
#include<odbOdbRepository.h>
#include<odbmesh.h>
#include<odaOdbFragment.h>
#include <odbPartRepository.h>  // ����odbPartRepository����������
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
	
	wcsncpy(this->target.targetPath, reinterpret_cast<const wchar_t*>(path.utf16()),path.size() + 1);
	this->target.displayMode = display;
	wcsncpy(this->target.targetModel, reinterpret_cast<const wchar_t*>(modelName.utf16()), modelName.size() + 1);
	wcsncpy(this->target.targetPart, reinterpret_cast<const wchar_t*>(partName.utf16()), partName.size() + 1);
};

void VTUFileManager::Init(const QString& path, const QString& modelName) {

	wcsncpy(this->target.targetPath, reinterpret_cast<const wchar_t*>(path.utf16()), path.size() + 1);
	wcsncpy(this->target.targetModel, reinterpret_cast<const wchar_t*>(modelName.utf16()), modelName.size() + 1);
	QString baseName = QFileInfo(path).baseName();
	wcsncpy(this->target.targetPart, reinterpret_cast<const wchar_t*>(baseName.utf16()), baseName.size() + 1);
};

void VTUFileManager::Init(const QString& path, const QString& odbPath, const int& display, const QString& modelName) {

	wcsncpy(this->target.targetPath, reinterpret_cast<const wchar_t*>(path.utf16()), path.size() + 1);
	wcsncpy(this->target.odbPath, reinterpret_cast<const wchar_t*>(odbPath.utf16()), odbPath.size() + 1);
	this->target.displayMode = display;
	wcsncpy(this->target.targetModel, reinterpret_cast<const wchar_t*>(modelName.utf16()), modelName.size() + 1);
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
	//memset((void*)target.targetPart, 0, 128 * sizeof(wchar_t));
	wcsncpy(target.targetPart, reinterpret_cast<const wchar_t*>(targetP.utf16()), targetP.size() + 1);
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

	QString odbPath = target.TargetOdbPath();

	if (ExportSamH5ToVtkFrames(odbPath, writer) == 0) {
		return 0;
	}

	odbOdb& odb = odbOdbRepository::Instance().get(odbPath); 

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
	return QString::fromWCharArray(targetModel);
}

QString TargetList::TargetPart() {
	return QString::fromWCharArray(targetPart);
}

QString TargetList::TargetPath() {
	return QString::fromWCharArray(targetPath);
}

QString TargetList::TargetOdbPath() {
	return QString::fromWCharArray(odbPath);
}

const QString VTUFileManager::GetTargetPartName() {
	return target.TargetPart().trimmed();
}