#include <VTUFileIOToolsetPlugin.h>
#include <VTUFileIOToolsetGui.h>

#include <SAMApp.h>
#include <SAMMainWindow.h>
#include <SAMModuleGui.h>

VTUFileIOToolsetPlugin::VTUFileIOToolsetPlugin()
{

}

VTUFileIOToolsetPlugin::~VTUFileIOToolsetPlugin()
{

}

void VTUFileIOToolsetPlugin::registerToolset()
{
	// 这是插件进入 SAM 的第一站。
	// SAM 启动后会加载 FilePlugin 目录下的插件 DLL，然后调用 registerToolset()。
	// 这里拿到 SAM 主窗口，再把我们的 VTUFileIOToolsetGui 注册进去。
	// 注册成功后，Gui 类才有机会往 File -> Import / Export 菜单里添加 VTK... 入口。
	SAMApp* app = SAMApp::getSAMApp();
	SAMMainWindow* mw = app->getSAMMainWindow();
	if (mw->getModule("Part"))
	{
		// "Part" 是 SAM 的前处理/部件模块名称。
		// GUI_IN_MENUBAR 表示显示在菜单栏，GUI_IN_TOOLBAR 表示也允许显示在工具栏。
		// new 出来的 Gui 对象由 SAM 插件框架接管，后续用户点击菜单时会调用 Gui 中的槽函数。
		mw->getModule("Part")->registerToolset(new VTUFileIOToolsetGui, GUI_IN_MENUBAR | GUI_IN_TOOLBAR);
	}

}
