#include <iniPythonModuleRegistrar.h>
#include <VTUFileIOPytModule.h>
#include <SAMVTUFileIOFragment.h>
#include <omuAtom.h>

static VTUFileIOPytModule* VTUFileIOPytModulePtr = 0;
static SAMVTUFileIOFragment* SAMVTUFileIOFragmentPtr = 0;
static int VTUFileIOReferenceCount = 0;

void VTUFileIOInitialize(int& count)
{
	if (!VTUFileIOReferenceCount++)
	{
		// 这里是真正创建后端模块对象的地方。
		// import VTUFileIO 之后，SAM 会通过注册器走到这里。
		// VTUFileIOPytModule 负责注册 Python 模块名；
		// SAMVTUFileIOFragment 负责注册 mdb.models['xx'] 上可调用的方法。
		VTUFileIOPytModulePtr = new VTUFileIOPytModule;
		SAMVTUFileIOFragmentPtr = new SAMVTUFileIOFragment;
		omuAtom::CreateAtom("kefKLine");
		++count;
	}
}

void VTUFileIOFinalize(int& count)
{
	if (!--VTUFileIOReferenceCount)
	{
		// SAM 卸载模块时会走到这里，把初始化时 new 出来的对象释放掉。
		if (VTUFileIOPytModulePtr)
		{
			delete VTUFileIOPytModulePtr;
			VTUFileIOPytModulePtr = 0;

			delete SAMVTUFileIOFragmentPtr;
			SAMVTUFileIOFragmentPtr = 0;
		}

		--count;
	}
}

extern "C" void initVTUFileIO(void)
{
	// extern "C" 的意思是：用 C 风格导出这个函数名，避免 C++ 名字改编。
	// 这样 SAM 在加载 .pyd 时，才能稳定找到 initVTUFileIO 这个入口。
	// 你可以把它理解成“插件模块对外公开的总入口函数”。
	iniPythonModuleRegistrar::Instance().Register(VTUFileIOInitialize, VTUFileIOFinalize);
}
