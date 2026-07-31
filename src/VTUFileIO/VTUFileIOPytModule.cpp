#include <omuArguments.h>
#include <omuPrimNumber.h>

#include <cmdCWIP.h>
#include <VTUFileIOPytModule.h>

static omuInterfaceObj::methodTable VTUFileIOPytModuleMethods[] =
{ 
	{0, 0} 
};

VTUFileIOPytModule::VTUFileIOPytModule()
	: pyoModule("VTUFileIO", VTUFileIOPytModuleMethods, pyoModule::NO_IMPORT)
{
	// 这里把当前 .pyd 模块注册成 Python 世界里的 "VTUFileIO"。
	// 所以前端第一次导出时执行的 import VTUFileIO，最终就会加载到这里。
	// 当前这个模块本身没有直接暴露额外 Python 函数，
	// 真正有用的方法注册在 SAMVTUFileIOFragment 里。
}




VTUFileIOPytModule::~VTUFileIOPytModule()
{
	// 当前没有额外资源需要释放。
}

void VTUFileIOPytModule::DefineConstants()
{
	// 当前没有额外常量需要注册。
}
