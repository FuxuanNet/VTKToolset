#include <VTUFileIOCommand.h>

#include <omuArguments.h>
#include <omuMethodCall.h>
#include <omuPrimExpr.h>

#include <SAMBoolKeyword.h>
#include <cmdGCommandDeliveryRole.h>
#include <sesGSessionState.h>

#include <qdebug.h>


// 在控制台中可以调用mdb.models['ModelName'].printAll()类似的功能

// 打印模型信息
void VTUFileIOCommand::CommitPrint() {
	omuArguments args;

	const sesGVpContext& context = sesGSessionState::Instance()->ConstGetVpContext();
	QString pyt = QString("mdb.models['%1']").arg(context.ModelName());
	if (context.ModelName().isEmpty() || context.PartName().isEmpty()) return;

	omuMethodCall mc(pyt, "printAll", args);
	QString cmd;
	cmd.append(mc);
	cmdGCommandDeliveryRole::Instance().SendCommand(cmd);
	return;
}

// 保存.vtu文件 path代表存储的路径
void VTUFileIOCommand::CommitSave(const QString& path) {

    const sesGVpContext& context = sesGSessionState::Instance()->ConstGetVpContext();
    //TODO:Print Messages when there's nothing in context or else
	QString modelName = context.ModelName();
	QString partName = context.PartName();
	QString odbExpr = context.OdbPathName();
	QString pyt = QString("mdb.models['%1']").arg(modelName);

	// ---------- 直接在函数里处理 odbExpr ----------
	QString odbPath = odbExpr;
	int start = odbExpr.indexOf("['");
	int end = odbExpr.lastIndexOf("']");
	if (start != -1 && end != -1 && end > start + 2) {
		odbPath = odbExpr.mid(start + 2, end - (start + 2));
	}

	omuArguments args(4);

	if (context.TypeByModule() == omu_ODB) {
		args.Put(path);   // vtk文件存储的路径
		args.Put(odbPath);
		args.Put((int)(context.TypeByModule()));
		args.Put(modelName);

		omuMethodCall mc(pyt, "exportOdbToVtk", args);
		QString cmd;
		cmd.append(mc);

		cmdGCommandDeliveryRole::Instance().SendCommand(cmd);
	}
	else {
		args.Put(path);
		args.Put((int)(context.TypeByModule()));
		args.Put(modelName);
		args.Put(partName);

		omuMethodCall mc(pyt, "exportMdbToVtk", args);
		QString cmd;
		cmd.append(mc);
		cmdGCommandDeliveryRole::Instance().SendCommand(cmd);
	}
} 

// 打开.vtu文件
void VTUFileIOCommand::CommitOpen(const QString& path) {

	const sesGVpContext& context = sesGSessionState::Instance()->ConstGetVpContext();
	//TODO:Print Messages when there's nothing in context or else
	QString pyt = QString("mdb.models['%1']").arg(context.ModelName());
	if (context.ModelName().isEmpty()) return;
	omuArguments args(2);
	args.Put(path);
	args.Put(context.ModelName());

	omuMethodCall mc(pyt, "importVtk", args);
	QString cmd;
	cmd.append(mc);
	cmdGCommandDeliveryRole::Instance().SendCommand(cmd);
}

// 加载VTUFileIO模块
void VTUFileIOCommand::CommitLoad() {
	cmdGCommandDeliveryRole::Instance().SendCommand("import VTUFileIO");
}