#include <VTUFileIODB.h>
#include <VTUFileIOForm.h>

#include <QValidator>
#include <QLabel>
#include <QLineEdit>
#include <QGroupBox>

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <QMessageBox>

#include <omuArguments.h>
#include <omuMethodCall.h>

#include <cmdGCommandDeliveryRole.h>

// 这个类曾用于生成一个简单的 SAM 对话框，目前主入口已经改为 File 菜单。

VTUFileIODB::VTUFileIODB(VTUFileIOForm* form)
	:SAMDataDialog(form, tr("VTUFileIO"), OK | CANCEL)
{
	setMinimumSize(470, 250);

	/*************************Parameters*************************************/
	QGroupBox* geomGroup = new QGroupBox(tr("VTUFileIOGroupBox"));
	QDoubleValidator* doubleValidator = new QDoubleValidator(geomGroup);

	QFormLayout* geomLayout = new QFormLayout(geomGroup);

	QVBoxLayout* paramLayout = new QVBoxLayout;
	paramLayout->addWidget(geomGroup);

	QHBoxLayout* contentLayout = new QHBoxLayout(contentArea);
	contentLayout->addLayout(paramLayout);

}

/// Destructor.
VTUFileIODB::~VTUFileIODB()
{

}


void VTUFileIODB::onCmdOk(int id)
{	
	static bool loaded = false;
	if (!loaded) {
		cmdGCommandDeliveryRole::Instance().SendCommand("import VTUFileIO");
		loaded = true;
	}

	// 执行调试命令：调用 printAll 接口，主要用于早期验证后端模块是否能被调用。
	omuArguments args;
	omuMethodCall mc("mdb.models['Model-1'].parts['Part-1']", "printAll", args);
	QString cmd;
	cmd.append(mc);
	cmdGCommandDeliveryRole::Instance().SendCommand(cmd);

	cmdGCommandDeliveryRole::Instance().SendCommand("session.viewports['Viewport: 1'].view.fitView()");

	this->deleteLater();
}


//wasted
void VTUFileIODB::trans() {
	omuArguments args;

	cmdGCommandDeliveryRole::Instance().SendCommand("import VTUFileIO");

	omuMethodCall mc("mdb.models['Model-1'].parts['Part-1']", "printAll", args);

	QString cmd;
	cmd.append(mc);

	cmdGCommandDeliveryRole::Instance().SendCommand(cmd);

	cmdGCommandDeliveryRole::Instance().SendCommand("session.viewports['Viewport: 1'].view.fitView()");

	this->deleteLater();
}
