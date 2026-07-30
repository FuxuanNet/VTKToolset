<div align="center">

# SAM VTKToolset

SAM 的 VTK 导入与导出扩展，支持梁、壳、体网格及常用后处理结果写出。

![C++](https://img.shields.io/badge/C%2B%2B-14-00599C?logo=cplusplus)
![CMake](https://img.shields.io/badge/CMake-3.28+-064F8C?logo=cmake)
![Platform](https://img.shields.io/badge/Platform-Windows-0078D4?logo=windows)
![SAM](https://img.shields.io/badge/SAM-SDK-1F6FEB)
![VTK](https://img.shields.io/badge/VTK-Legacy_ASCII-EF553B)

</div>

---

## 快速开始：直接部署，无需源码编译

> **可直接部署文件：** `deploy/Release/VTUFileIO.pyd` 和 `deploy/Release/FilePlugin/SAM.Pre.VTUFileIOToolset.dll`
> **12 个已验证 SAM H5 测试案例：** `test_cases/SAM_H5/`
> **本项目配套的 VTK.js 可视化网站：** [https://fuxuannet.github.io/vtk_geometry_viewer/](https://fuxuannet.github.io/vtk_geometry_viewer/)

本项目可以直接部署到兼容版本的 SAM 中使用，不需要编译 C++ 源码。运行包不包含单独的 `.exe`，因为它由 SAM 启动时加载的两个模块组成。

### 安装运行包

1. 完全退出 SAM。
2. 打开项目中的 `deploy` 目录。
3. 将其中的 `Release` 文件夹复制到 SAM 安装根目录，例如 `E:/SAM`。
4. 允许 Windows 合并 `Release` 和 `Release/FilePlugin` 文件夹；更新版本时允许替换同名旧文件。
5. 启动 SAM。
6. 在菜单中确认出现 `File -> Import -> VTK...` 与 `File -> Export -> VTK...`。

复制后，SAM 目录应包含：

```text
E:/SAM/
└── Release/
    ├── VTUFileIO.pyd
    └── FilePlugin/
        └── SAM.Pre.VTUFileIOToolset.dll
```

### 测试运行包

1. 按上述步骤部署运行包。
2. 启动 SAM，打开或导入 `test_cases/SAM_H5` 中任意一个 `.sam.h5` 文件。
3. 进入模型或后处理结果环境。
4. 点击 `File -> Export -> VTK...`，选择一个空的输出文件夹。
5. 将导出的 `.vtk` 文件拖入网页查看器：[https://fuxuannet.github.io/vtk_geometry_viewer/](https://fuxuannet.github.io/vtk_geometry_viewer/)。
6. 在网页中检查网格，并从结果场列表选择 `U`、`UR`、`S`、`E`、`S11`、`S_Mises` 或 `S_pressure`。

这 12 个 H5 均已完成 SAM 到 VTK 的实际对比验证。验证结果包含 44 个 VTK 文件，44 个通过，0 个失败。详细覆盖范围见 `test_cases/README.md`。

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

仓库在 `bin/Release` 和 `deploy/Release` 中保留了当前已验证的两个运行文件：

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

## 常见问题

### CMake 找不到 Qt 或 SAM 库

确认 `LIBS_SAMSDK_ROOT` 指向完整 SDK 根目录，并检查 `ThridPartys/Qt-5.12.6-VC14-64`、`include` 和 `libs` 是否存在。

### 构建成功但 SAM 菜单中没有 VTK

确认菜单 DLL 位于 `<SAM>/Release/FilePlugin`，后端模块位于 `<SAM>/Release`，然后完全退出并重新启动 SAM。

### 队友需要完整 SDK 吗

参与源码编译需要 SAMSDK。只使用已编译插件时，可以部署两个 Release 文件，但仍需版本匹配的 SAM 软件环境。
