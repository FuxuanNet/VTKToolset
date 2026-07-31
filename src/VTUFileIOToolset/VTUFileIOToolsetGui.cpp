#include <VTUFileIOToolsetGui.h>
#include <VTUFileIOForm.h>
#include <VTUFileIODb.h>
#include <VTUFileIOCommand.h>

#include <SAMApp.h>
#include <SAMMenuPane.h>
#include <SAMMainWindow.h>
#include <qmenubar.h>
#include <SAMMenuCommand.h>
#include <SAMMainWindow.h>

#include <cmdGCommandDeliveryRole.h>
#include <SAMFileDialog.h>

#include <ubiFileToolsetGui.h>


VTUFileIOToolsetGui::VTUFileIOToolsetGui()
	: SAMToolsetGui("Test")
{
	// 构造函数可以理解成“对象刚创建时自动执行的初始化函数”。
	// 这里一创建工具集界面，就马上去创建菜单项。
	createMenuItems();
	createToolboxItems();

}

VTUFileIOToolsetGui::~VTUFileIOToolsetGui()
{

}

void VTUFileIOToolsetGui::activate()
{
	// SAM 激活当前工具集时会调用这里。目前只沿用父类默认行为。
	SAMToolsetGui::activate();
}

void VTUFileIOToolsetGui::deactivate()
{
	// SAM 切换到别的工具集时会调用这里。目前只沿用父类默认行为。
	SAMToolsetGui::deactivate();
}

void VTUFileIOToolsetGui::createMenuItems()
{
	// 这部分是 Qt/SAM 菜单代码。
	// 目标很简单：找到 SAM 自带的 File 菜单，再找到它下面的 Import 和 Export 子菜单，
	// 然后分别插入一个 "VTK..." 按钮。
	auto app = SAMApp::getSAMApp();
	auto mw = app->getSAMMainWindow();
	auto menuBar = mw->getMenubar();

	SAMMenu* fileMenu = nullptr;
	SAMMenu* importMenu = nullptr;
	SAMMenu* exportMenu = nullptr;

	// Qt 菜单栏里每一项都是 QAction。这里逐个查看，找到对象名叫 FileMenu 的菜单。
	// objectName 是程序内部识别名，界面上显示的文字可能会随语言变化。
	for (QAction* action : menuBar->actions()) {
		QMenu* menu = action->menu();
		if (menu)
		{
			QString name = menu->objectName();
			if (name.compare("FileMenu") == 0)
			{
				fileMenu = (SAMMenu*)menu;
				break;
			}
		}
	}
	if (fileMenu == nullptr) return;

	// 找到 File 菜单后，再继续向下找 Import 和 Export。
	// 老师要求入口统一放在官方菜单 File -> Import / Export，所以这里不再额外创建 Test 菜单。
	for (QAction* action : fileMenu->actions()) {
		QMenu* menu = action->menu();
		if (menu)
		{
			QString name = menu->objectName();

			if (name.compare("FileImportMenu") == 0)
				importMenu = (SAMMenu*)menu;

			if (name.compare("FileExportMenu") == 0)
				exportMenu = (SAMMenu*)menu;
		}
	}
	if (exportMenu == nullptr) return;
	if (importMenu == nullptr) return;

	SAMMenuCommand* SaveVTKCmd = new SAMMenuCommand(this, exportMenu, "VTK...");
	exportMenu->addAction(SaveVTKCmd);

	//SAMMenuCommand* SaveCmd = new SAMMenuCommand(this, fileMenu, tr("&VTU.."));
	//fileMenu->addAction(SaveVTUCmd);

	SAMMenuCommand* ImportCmd = new SAMMenuCommand(this, importMenu, "VTK...");
	importMenu->addAction(ImportCmd);

	// Qt 的 connect 是“信号槽”机制：
	// - triggered(bool) 是用户点击菜单时发出的信号；
	// - SaveDialog/OpenDialog 是我们自己写的槽函数；
	// 用户点 Export -> VTK...，程序就会自动进入 SaveDialog()。
	connect(SaveVTKCmd, SIGNAL(triggered(bool)), this, SLOT(SaveDialog()));
	connect(ImportCmd, SIGNAL(triggered(bool)), this, SLOT(OpenDialog()));
}


void VTUFileIOToolsetGui::createToolboxItems()
{
	// 当前只做菜单入口，工具栏按钮暂时不放内容。
}

void VTUFileIOToolsetGui::PrintMsg() {
	VTUFileIOCommand::CommitPrint();
}

void VTUFileIOToolsetGui::SaveDialog() {
	
	// 弹出保存文件对话框。这里只允许用户选择 Legacy ASCII .vtk。
	// SAMFileDialog 是 SAM 对 Qt 文件对话框的封装，用法和 QFileDialog 很像。
	fileDialog = new SAMFileDialog("Export VTK file", 0);
	fileDialog->setFileMode(QFileDialog::AnyFile);
	fileDialog->setAcceptMode(QFileDialog::AcceptSave);
	//fileDialog->setNameFilters(QStringList("VTK XML Unstructured Grid(*.vtu)"));
	fileDialog->setNameFilters(QStringList("VTK Legacy(*.vtk)"));

	// 用户选完保存路径后，SAMFileDialog 会发出 onFileSelected 信号。
	// 这个信号携带路径参数，然后进入 OnSave(const QString& path)。
	QObject::connect(fileDialog, SIGNAL(onFileSelected(const QString&)), this, SLOT(OnSave(const QString&)));
	fileDialog->show();
}

void VTUFileIOToolsetGui::OnSave(const QString& path) {
	// 只在第一次导出时 import VTUFileIO。
	// static 局部变量会在函数调用结束后继续保留值，所以 loaded 可以记住模块是否已经加载过。
	// 如果每次都 import，命令窗口会变得很乱，也可能重复初始化后端对象。
	static bool loaded = false;
	if (!loaded) {
		VTUFileIOCommand::CommitLoad();
		loaded = true;
	}

	VTUFileIOCommand::CommitSave(path);
	//delete fileDialog;
	fileDialog = NULL;
}

void VTUFileIOToolsetGui::OpenDialog() {

	// 导入入口沿用官方已有能力：读取 Legacy VTK 的几何网格。
	// 当前课题主线是“导出网格和结果”，导入结果云图不是这次扩展重点。
	fileDialog = new SAMFileDialog("Import VTK file", 0);
	fileDialog->setFileMode(QFileDialog::AnyFile);
	fileDialog->setAcceptMode(QFileDialog::AcceptOpen);
	//fileDialog->setNameFilters(QStringList("VTK XML Unstructured Grid(*.vtu)"));
	fileDialog->setNameFilters(QStringList("VTK Legacy(*.vtk)"));

	QObject::connect(fileDialog, SIGNAL(onFileSelected(const QString&)), this, SLOT(OnOpen(const QString&)));
	fileDialog->show();
}


void VTUFileIOToolsetGui::OnOpen(const QString& path) {
	// 导入也要先加载 Python 后端模块，然后把路径交给命令桥接层。
	static bool loaded = false;
	if (!loaded) {
		VTUFileIOCommand::CommitLoad();
		loaded = true;
	}

	VTUFileIOCommand::CommitOpen(path);
	// TODO: Whether to delete fileDialog?
	// delete fileDialog;
	fileDialog = NULL;
}

