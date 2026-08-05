#include <SAMVTUFileIOFragment.h>

#include "VTUDataContainer.h"  

//#include <gdyScene.h>
//#include <gdyEditor.h>
//#include <cowList.T>

#include <ptoKPartRepository.h>
#include <ptoKUtils.h>
#include <ptoKPart.h> 
#include <ftrFeatureList.h>
#include <basMdb.h>
#include <basBasis.h>
#include <basNewModel.h>
#include <cmdKCommandDeliveryRole.h>

#include <bmeMesh.h> 
#include <bmeElementClass.h>
#include <bmeElementClassList.h>
#include <ptsKSceneManager.h>

#include <MessageHandler.h>


//下面的头文件是为了print后处理结果添加的
#include<odbOdbRepository.h>
#include<odbNode.h>
#include<odbmesh.h>
#include<odaOdbFragment.h>
#include <odbPartRepository.h>
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


static omuInterfaceObj::methodTable SAMVTUFileIOFMethods[] =
{
	// 这里是“接口注册表”。
	// 前端命令里写 mdb.models['ModelName'].exportOdbToVtk(...)，
	// SAM 就是在这张表里找到 "exportOdbToVtk" 对应的 C++ 成员函数。
	{"printAll", (omuInterfaceObj::methodFunc)&SAMVTUFileIOFragment::printAll},
	{"exportMdbToVtk",(omuInterfaceObj::methodFunc)&SAMVTUFileIOFragment::exportMdbToVtk},
	{"importVtk",(omuInterfaceObj::methodFunc)&SAMVTUFileIOFragment::importVtk},

	{"exportOdbToVtk",(omuInterfaceObj::methodFunc)&SAMVTUFileIOFragment::exportOdbToVtk},
	{ 0, 0 }
};

static omuInterfaceObj::memberTable SAMVTUFileIOFMembers[] =
{
	{ 0, 0, 0 }
};

SAMVTUFileIOFragment::SAMVTUFileIOFragment()
	: ptsKModelFragment()
{
	// DescribeType 把上面的接口表交给 SAM 的脚本系统。
	// 注册完成后，这个 Fragment 就像挂在 mdb.models['xxx'] 上的一组扩展方法。
	omuInterfaceObj::DescribeType("SAMVTUFileIOFragment", SAMVTUFileIOFMethods, SAMVTUFileIOFMembers);
	fileManager = NULL;
}
SAMVTUFileIOFragment::~SAMVTUFileIOFragment()
{
	if (fileManager != NULL) delete(fileManager);
}

omuPrimitive* SAMVTUFileIOFragment::Copy() const
{
	return new SAMVTUFileIOFragment(*this);
}

// ODB文件读取数据是否正常
omuPrimitive* SAMVTUFileIOFragment::printAll(omuArguments& args)
{
	// 打开 ODB 文件
	odbOdb* odb = 0;
	odb = &odbOdbRepository::Instance().get("D:/Temp/test/Plate1X1Buckling-1ElemRes.h5");
	qDebug() << "ODB file opened successfully. Analysis Title:" << odb->analysisTitle();

	odbStepRepositoryIT odbStepRepositoryIter(odb->steps());

	for (odbStepRepositoryIter.first(); !odbStepRepositoryIter.isDone(); odbStepRepositoryIter.next())
	{
		const QString& stepName = odbStepRepositoryIter.currentKey();
		const odbStep& step = odbStepRepositoryIter.currentValue();

		const odbSequenceFrame& frames = step.frames();
		odbFrame& frame = frames.get(0);
		frame.update();

		//get fields
		QString fieldName = 'U';
		const odbFieldOutputRepository& fields = frame.fieldOutputs();
		const odbSequenceString fieldNames = fields.getFieldOutputNames();
		int fieldsNum = fieldNames.size();
		const odbFieldOutput& field = fields[fieldName];
	}
	odbStepRepository& stepsRep = odb->steps();
	if (!stepsRep.isMember("Step-1")) {
		qDebug() << "Error: Step-1 not found in ODB";
		return nullptr;
	}

	// 获取Step-1
	odbStep& step = stepsRep.get("Step-1");

	//get frame
	int frameIndex = 0;
	odbSequenceFrame& frames = step.frames();
	odbFrame& frame = frames.get(frameIndex);
	frame.update();

	//get fields
	QString fieldName = 'U';
	const odbFieldOutputRepository& fields = frame.fieldOutputs();
	const odbSequenceString fieldNames = fields.getFieldOutputNames();
	int fieldsNum = fieldNames.size();
	qDebug() << "Number of field outputs in frame:" << fieldsNum;
	const odbFieldOutput& uField = fields[fieldName];
	int numValues = uField.values().size();

	qDebug() << "Frame" << frameIndex << "dispalcement field 'U' has" << numValues << "nodes";

	// 遍历每个节点
	for (int n = 0; n < numValues; ++n) {
		const odbFieldValue& val = uField.values(n);
		int nodeLabel = val.nodeLabel();
		int numComp = 0;
		const float* const U = val.data(numComp);  // 获取 x/y/z 位移

		if (numComp < 3) {
			qDebug() << "Node" << nodeLabel << "displacement components < 3!";
			continue;
		}

		float ux = U[0];
		float uy = U[1];
		float uz = U[2];

		qDebug() << "Node" << nodeLabel << "Displacement:" << ux << uy << uz;
	}

	qDebug() << "Finished reading displacement data!";
	return nullptr;
}

omuPrimitive* SAMVTUFileIOFragment::exportMdbToVtk(omuArguments& args) {

	QString path;
	int display;
	QString modelName;
	QString partName;

	// args.Begin/Get/End 是 SAM SDK 读取脚本参数的固定写法。
	// 这里的读取顺序必须和 VTUFileIOCommand::CommitSave 里 Put 的顺序一致。
	args.Begin();
	args.Get(path);
	args.Get(display);
	args.Get(modelName);
	args.Get(partName);
	args.End();


	int status = 0;
	fileManager = new VTUFileManager;
	if (fileManager != NULL)
	{

		// 前处理导出：path 是 VTK 输出路径，modelName/partName 指定当前模型和部件。
		// Init 只保存目标信息，真正读取网格和写文件在 WriteCache / WriteFile 中完成。
		fileManager->Init(path, display, modelName, partName);

		// WriteCache：先从 SAM 模型里读网格，整理到 VTUDataContainer 中。
		// WriteFile：再把容器中的点、单元、场量写成 Legacy VTK 文本文件。
		status |= fileManager->WriteCache();

		if (status == 0) {
			fileManager->WriteFile();
		}
		else {
			qDebug() << "[exportMdbToVtk] WriteCache failed. Status =" << status;
		}

		delete fileManager;
		fileManager = NULL;
	}
	else {
		status |= ERRORTYPE_MEMORY_ALLOC_FAILED;
	}

	MessageHandler::ReportExportErr(status);

	return 0;
}

omuPrimitive* SAMVTUFileIOFragment::exportOdbToVtk(omuArguments& args) {

	QString vtkpath;
	QString odbpath;
	int display;
	QString modelName;

	// 后处理导出参数来自 VTUFileIOCommand::CommitSave：
	// 1. vtkpath：用户选择的输出路径；
	// 2. odbpath：当前打开的 SAM H5/ODB 结果文件路径；
	// 3. display：当前模块类型；
	// 4. modelName：当前模型名。
	args.Begin();
	args.Get(vtkpath);
	args.Get(odbpath);
	args.Get(display);
	args.Get(modelName);
	args.End();


	int status = 0;
	fileManager = new VTUFileManager;
	if (fileManager != NULL)
	{

		// 后处理导出会优先尝试直接读 SAM H5。
		// 如果直接读 H5 成功，可以完整控制 U、UR、S、E、分量和 pressure 的写出。
		// 如果直接读 H5 失败，会回退到 SAM ODB SDK 读取路径。
		fileManager->Init(vtkpath, odbpath, display, modelName);

		status |= fileManager->WriteCache();
		qDebug() << "[exportOdbToVtk] Cache write status:" << status;

		if (status == 0) {
			fileManager->WriteFile();
			qDebug() << "[exportOdbToVtk] File write completed with status:" << status;
		}
		else {
		}

		delete fileManager;
		fileManager = NULL;
	}
	else {
		status |= ERRORTYPE_MEMORY_ALLOC_FAILED;
	}

	MessageHandler::ReportExportErr(status);

	return 0;
}

omuPrimitive* SAMVTUFileIOFragment::importVtk(omuArguments& args) {

	QString path;
	QString modelName;

	// 导入接口当前主要用于读 Legacy VTK 的几何网格。
	// 它可以帮我们确认导出的 VTK 文件拓扑是否能被 SAM 重新识别，
	// 但导入后的显示效果不等同于原 SAM H5 的后处理云图效果。
	args.Begin();
	args.Get(path);
	args.Get(modelName);
	args.End();

	int status = 0;
	fileManager = new VTUFileManager;
	if (fileManager != NULL)
	{
		fileManager->Init(path, modelName);
		if (!(status |= fileManager->ReadToCache())) {
			status |= fileManager->ReadToSAM();
			fileManager->SyncSAM(modelShortcut);
		}
		delete fileManager;
	}
	else status |= ERRORTYPE_MEMORY_ALLOC_FAILED;

	//TODO:Complete error report
	MessageHandler::ReportImportErr(status);
	return 0;
}
