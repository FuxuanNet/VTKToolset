<div align="center">

---

## 项目简介

VTKToolset 基于 SAM 已有 VTK 接口扩展开发，在 SAM 的 `File -> Import` 和 `File -> Export` 菜单中提供 VTK 文件入口。项目由两个动态模块组成：

| 模块         | 生成文件                         | 用途                                           |
| ------------ | -------------------------------- | ---------------------------------------------- |
| VTK 后端模块 | `VTUFileIO.pyd`                | 调用 SAM SDK 读取模型和结果，完成 VTK 数据转换 |
| 菜单插件     | `SAM.Pre.VTUFileIOToolset.dll` | 在 SAM 的文件菜单中提供英文导入、导出入口      |

`.pyd` 是供 SAM 内置 Python 加载的 Windows 动态模块，本质上与 DLL 类似。项目当前不生成独立的 `.exe`。

## 功能范围

- 导出梁、杆等线单元网格。
- 导出三角形和四边形壳单元网格。
- 导出四面体、楔体和六面体等实体单元网格。
- 导出节点位移 `U` 和转角位移 `UR`。
- 导出应力张量 `S`、应变张量 `E` 及各分量。
- 导出 `S_Mises` 和 `S_pressure` 派生结果。
- 保留官方 Legacy ASCII `.vtk` 网格导入入口。

## 环境要求

| 软件或开发包 | 要求                                                |
| ------------ | --------------------------------------------------- |
| 操作系统     | Windows 64 位                                       |
| 编译环境     | Visual Studio 2022，安装“使用 C++ 的桌面开发”     |
| CMake        | 3.28 或更高版本                                     |
| SAM          | 与 SDK 版本匹配的 SAM 安装目录                      |
| SAMSDK       | 包含`include`、`libs`、`ThridPartys` 三个目录 |
| Python 与 Qt | 使用 SAM 和 SAMSDK 自带版本，无需单独安装           |

SAMSDK 是 SAM 二次开发包。`include` 存放接口声明，`libs` 存放链接库，`ThridPartys` 存放 Qt、HDF5、TCMalloc 等依赖。本项目调用 SDK，不修改 SDK 文件。

## 推荐目录

将 `SAMSDK` 和 `VTKToolset` 放在同一父目录：

```text
SAMExtensionWorkspace/
├── SAMSDK/
│   ├── include/
│   ├── libs/
│   └── ThridPartys/
└── VTKToolset/
    ├── CMakeLists.txt
    ├── src/
    ├── tests/
    └── bin/Release/
```

采用该结构时，CMake 会默认在 `../SAMSDK` 查找 SDK。VTKToolset 也可以放在其他位置，此时需要通过 `LIBS_SAMSDK_ROOT` 指定 SDK 的绝对路径。

## 配置路径

构建前需要确认两个路径：

| CMake 参数           | 指向位置                                                        | 示例                       |
| -------------------- | --------------------------------------------------------------- | -------------------------- |
| `LIBS_SAM_ROOT`    | SAM 软件安装根目录，目录下应有`Release` 和 `Python27`       | `E:/SAM`                 |
| `LIBS_SAMSDK_ROOT` | SAMSDK 根目录，目录下应有`include`、`libs`、`ThridPartys` | `D:/SAMWorkspace/SAMSDK` |

路径通过 CMake 命令传入，无需编辑 `CMakeLists.txt`：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DLIBS_SAM_ROOT="E:/SAM" `
  -DLIBS_SAMSDK_ROOT="D:/SAMWorkspace/SAMSDK"
```

如果 SDK 与 VTKToolset 同级，可以省略 `LIBS_SAMSDK_ROOT`：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DLIBS_SAM_ROOT="E:/SAM"
```

首次配置后，路径会保存在 `build/CMakeCache.txt`。更换目录时重新执行带 `-D` 参数的配置命令即可。

## 编译与自动部署

在 VTKToolset 根目录执行：

```powershell
cmake --build build --config Release
```

构建成功后会生成并自动复制以下文件：

| 构建产物        | 项目内位置                                   | SAM 部署位置                                              |
| --------------- | -------------------------------------------- | --------------------------------------------------------- |
| Python 后端模块 | `bin/Release/VTUFileIO.pyd`                | `<SAM>/Release/VTUFileIO.pyd`                           |
| 菜单插件        | `bin/Release/SAM.Pre.VTUFileIOToolset.dll` | `<SAM>/Release/FilePlugin/SAM.Pre.VTUFileIOToolset.dll` |

自动复制失败时，可以关闭 SAM 后手动复制这两个文件。重新启动 SAM 后，新模块才会被加载。

## 使用方法

### 导出 VTK

1. 启动 SAM 并打开包含模型或后处理结果的工程。
2. 进入需要导出的模型或结果帧。
3. 点击 `File -> Export -> VTK...`。
4. 选择输出目录和文件名。
5. 使用 ParaView、VTK.js 查看器或文本检查工具打开 `.vtk` 文件。

多帧结果会按帧生成多个 VTK 文件。结果字段是否出现取决于当前 SAM 模型中实际保存的场量。

### 导入 VTK

1. 点击 `File -> Import -> VTK...`。
2. 选择 Legacy ASCII `.vtk` 文件。
3. 在 SAM 的 Part 或 Mesh 视图中查看导入网格。

导入入口主要恢复网格。SAM 的完整后处理步骤、帧和云图上下文仍以原 SAM H5 或 ODB 数据为准。

## 已编译文件

仓库的 `bin/Release` 保留了当前已验证的两个运行文件：

```text
bin/Release/VTUFileIO.pyd
bin/Release/SAM.Pre.VTUFileIOToolset.dll
```

同版本 SAM、SAMSDK 和 64 位运行环境可以直接复制使用。版本不一致时建议在目标电脑重新编译。

## 测试

`tests/generated_vtk_cases` 中包含 10 组 Legacy ASCII VTK 样例，覆盖梁、壳、体、混合网格、应力、应变和极值场景。

```powershell
python tests/generate_vtk_cases.py
python tests/verify_generated_vtk_cases.py
```

SAM H5 到 VTK 的实际导出测试还需要本机安装 SAM，并在 SAM 图形界面中执行导出。

## Git 与 SVN 提交规则

仓库提交源码、CMake 文件、测试脚本、轻量测试数据，以及 `bin/Release` 中的两个可部署文件。以下内容由 `.gitignore` 排除：

- `build`、`obj`、CMake 缓存和 Visual Studio 临时目录。
- 编译产生的 `.lib`、`.exp`、`.pdb`、`.ilk` 等中间文件。
- 本地 H5、ODB、OP2 和临时导出数据。
- 超过 100 MB 的计算数据或测试结果。

SVN 不读取 `.gitignore`。提交 SVN 时应按照同样规则手动排除，并在提交前检查大文件：

```powershell
Get-ChildItem -Recurse -File |
  Where-Object Length -ge 100MB |
  Select-Object FullName, Length
```

推荐提交内容：

```text
CMakeLists.txt
src/
tests/
README.md
.gitignore
bin/Release/VTUFileIO.pyd
bin/Release/SAM.Pre.VTUFileIOToolset.dll
```

## 常见问题

### CMake 找不到 Qt 或 SAM 库

确认 `LIBS_SAMSDK_ROOT` 指向完整 SDK 根目录，并检查 `ThridPartys/Qt-5.12.6-VC14-64`、`include` 和 `libs` 是否存在。

### 构建成功但 SAM 菜单中没有 VTK

确认菜单 DLL 位于 `<SAM>/Release/FilePlugin`，后端模块位于 `<SAM>/Release`，然后完全退出并重新启动 SAM。

### 队友需要完整 SDK 吗

参与源码编译需要 SAMSDK。只使用已编译插件时，可以部署两个 Release 文件，但仍需版本匹配的 SAM 软件环境。
