#include <VTUFileIOCommand.h>

#include <omuArguments.h>
#include <omuMethodCall.h>
#include <omuPrimExpr.h>

#include <SAMBoolKeyword.h>
#include <cmdGCommandDeliveryRole.h>
#include <sesGSessionState.h>

#include <qdebug.h>
#include <QFileInfo>


// 这个文件可以理解成“菜单界面”和“SAM 后端接口”之间的翻译层。
// 前端 Qt 菜单拿到的是 QString 路径、当前上下文这些界面信息，
// 但真正执行导入导出的，是 SAM 自己的命令系统。
// 所以这里做的事情就是：把 C++/Qt 里的参数，拼成一条 SAM/Python 命令，再发给命令窗口执行。

// 在 SAM 命令窗口里，最终会形成类似下面这样的调用：
// mdb.models['ModelName'].exportMdbToVtk(...)
// mdb.models['ModelName'].exportOdbToVtk(...)
// mdb.models['ModelName'].importVtk(...)

// SAM 不同版本返回的 OdbPathName 形式不完全相同：可能是纯路径，也可能是
// odbs['path'] 或 odbs["path"]。这里只提取真正存在的 H5 文件，避免把表达式
// 直接传给后端后误走 ODB 回退分支。
static QString ResolveOdbPath(const QString& expression)
{
	const QString raw = expression.trimmed();
	if (QFileInfo(raw).isFile()) return raw;

	const int bracket = raw.indexOf('[');
	if (bracket >= 0) {
		int quoteStart = -1;
		QChar quote;
		for (int i = bracket + 1; i < raw.size(); ++i) {
			if (raw[i] == '\'' || raw[i] == '"') {
				quoteStart = i;
				quote = raw[i];
				break;
			}
		}
		if (quoteStart >= 0) {
			const int quoteEnd = raw.indexOf(quote, quoteStart + 1);
			if (quoteEnd > quoteStart) {
				const QString candidate = raw.mid(quoteStart + 1, quoteEnd - quoteStart - 1);
				if (QFileInfo(candidate).isFile()) return candidate;
			}
		}
	}

	// 有些上下文字符串带 file:/// 前缀，QFileInfo 在当前 Qt 版本中不能直接
	// 把它当作本地文件名，因此再尝试去掉这个前缀。
	if (raw.startsWith("file:///", Qt::CaseInsensitive)) {
		const QString candidate = raw.mid(8);
		if (QFileInfo(candidate).isFile()) return candidate;
	}
	return raw;
}

// 打印模型信息
void VTUFileIOCommand::CommitPrint() {
	omuArguments args;

	const sesGVpContext& context = sesGSessionState::Instance()->ConstGetVpContext();
	QString pyt = QString("mdb.models['%1']").arg(context.ModelName());
	if (context.ModelName().isEmpty() || context.PartName().isEmpty()) return;

	// omuMethodCall 可以理解成“把对象名 + 方法名 + 参数，拼成一条可执行命令字符串”。
	omuMethodCall mc(pyt, "printAll", args);
	QString cmd;
	cmd.append(mc);
	cmdGCommandDeliveryRole::Instance().SendCommand(cmd);
	return;
}

// 导出 .vtk 文件，path 是保存路径
void VTUFileIOCommand::CommitSave(const QString& path) {

	const sesGVpContext& context = sesGSessionState::Instance()->ConstGetVpContext();
	// 当前用户在 SAM 里看到的内容，都会反映到这个 context 里。
	// 例如当前是 Part 模块还是 ODB 结果模块、当前模型名是什么、当前 Part 名是什么。
	QString modelName = context.ModelName();
	QString partName = context.PartName();
	QString odbExpr = context.OdbPathName();
	QString pyt = QString("mdb.models['%1']").arg(modelName);

	QString odbPath = ResolveOdbPath(odbExpr);
	qDebug() << "[VTK export] ODB context:" << odbExpr << "resolved to:" << odbPath;

	// omuArguments 就像一个“参数盒子”。
	// 后面 Put 进去的内容，会按照顺序传给后端接口函数 exportMdbToVtk / exportOdbToVtk。
	omuArguments args(4);

	if (context.TypeByModule() == omu_ODB) {
		// 用户当前处在后处理结果环境。
		// 这时导出的是“带结果的 VTK”。
		// 也就是会尝试写出 U、UR、S、E 等场量。
		args.Put(path);   // vtk 文件保存路径
		args.Put(odbPath);
		args.Put((int)(context.TypeByModule()));
		args.Put(modelName);

		omuMethodCall mc(pyt, "exportOdbToVtk", args);
		QString cmd;
		cmd.append(mc);

		// cmdGCommandDeliveryRole 是 SAM 的命令投递角色。
		// 你可以把它想成“把命令丢给 SAM 内部命令解释器去执行”。
		cmdGCommandDeliveryRole::Instance().SendCommand(cmd);
	}
	else {
		// 用户当前处在前处理/模型环境。
		// 这时导出的是纯网格 VTK，不一定有结果场。
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

// 导入 .vtk 文件
void VTUFileIOCommand::CommitOpen(const QString& path) {

	const sesGVpContext& context = sesGSessionState::Instance()->ConstGetVpContext();
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

// 加载 VTUFileIO 模块
void VTUFileIOCommand::CommitLoad() {
	// 这一句相当于让 SAM 命令系统执行 Python 里的：
	// import VTUFileIO
	// 模块加载之后，后端注册函数才会生效。
	cmdGCommandDeliveryRole::Instance().SendCommand("import VTUFileIO");
}
