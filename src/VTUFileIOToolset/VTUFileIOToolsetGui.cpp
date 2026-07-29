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
	createMenuItems();
	createToolboxItems();

}

VTUFileIOToolsetGui::~VTUFileIOToolsetGui()
{

}

void VTUFileIOToolsetGui::activate()
{
	// todo
	SAMToolsetGui::activate();
}

void VTUFileIOToolsetGui::deactivate()
{
	// todo
	SAMToolsetGui::deactivate();
}

void VTUFileIOToolsetGui::createMenuItems()
{
	auto app = SAMApp::getSAMApp();
	auto mw = app->getSAMMainWindow();
	auto menuBar = mw->getMenubar();

	SAMMenu* fileMenu = nullptr;
	SAMMenu* importMenu = nullptr;
	SAMMenu* exportMenu = nullptr;

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

	connect(SaveVTKCmd, SIGNAL(triggered(bool)), this, SLOT(SaveDialog()));
	connect(ImportCmd, SIGNAL(triggered(bool)), this, SLOT(OpenDialog()));
}


void VTUFileIOToolsetGui::createToolboxItems()
{
	// todo
}

void VTUFileIOToolsetGui::PrintMsg() {
	VTUFileIOCommand::CommitPrint();
}

void VTUFileIOToolsetGui::SaveDialog() {
	
	fileDialog = new SAMFileDialog("Export VTK file", 0);
	fileDialog->setFileMode(QFileDialog::AnyFile);
	fileDialog->setAcceptMode(QFileDialog::AcceptSave);
	//fileDialog->setNameFilters(QStringList("VTK XML Unstructured Grid(*.vtu)"));
	fileDialog->setNameFilters(QStringList("VTK Legacy(*.vtk)"));

	QObject::connect(fileDialog, SIGNAL(onFileSelected(const QString&)), this, SLOT(OnSave(const QString&)));
	fileDialog->show();
}

void VTUFileIOToolsetGui::OnSave(const QString& path) {
	// ֻ��ִ��ʱ����ģ��
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

	fileDialog = new SAMFileDialog("Import VTK file", 0);
	fileDialog->setFileMode(QFileDialog::AnyFile);
	fileDialog->setAcceptMode(QFileDialog::AcceptOpen);
	//fileDialog->setNameFilters(QStringList("VTK XML Unstructured Grid(*.vtu)"));
	fileDialog->setNameFilters(QStringList("VTK Legacy(*.vtk)"));

	QObject::connect(fileDialog, SIGNAL(onFileSelected(const QString&)), this, SLOT(OnOpen(const QString&)));
	fileDialog->show();
}


void VTUFileIOToolsetGui::OnOpen(const QString& path) {
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
