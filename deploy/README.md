# SAM 直接部署包

本目录包含已经编译好的 SAM VTKToolset 运行文件。使用兼容版本的 SAM 时，直接复制即可使用，无需安装 CMake、Visual Studio 或 SAMSDK。

## 文件说明

```text
deploy/
└── Release/
    ├── VTUFileIO.pyd
    └── FilePlugin/
        └── SAM.Pre.VTUFileIOToolset.dll
```

| 文件 | 用途 |
|---|---|
| `VTUFileIO.pyd` | SAM 加载的 VTK 后端模块，负责模型与结果数据转换 |
| `SAM.Pre.VTUFileIOToolset.dll` | SAM 文件菜单插件，提供 VTK 导入与导出入口 |

本运行包没有独立的 `.exe`。两个文件均由 SAM 在启动时读取。

## 安装步骤

1. 完全退出 SAM。
2. 复制本目录中的 `Release` 文件夹。
3. 粘贴到 SAM 安装根目录，例如 `E:/SAM`。
4. 允许 Windows 合并文件夹；升级时允许替换同名文件。
5. 启动 SAM。
6. 确认 SAM 菜单包含 `File -> Import -> VTK...` 和 `File -> Export -> VTK...`。

安装后的目录结构：

```text
<SAM 安装目录>/
└── Release/
    ├── VTUFileIO.pyd
    └── FilePlugin/
        └── SAM.Pre.VTUFileIOToolset.dll
```

## 兼容性

运行包适用于与当前构建环境一致的 64 位 SAM 版本。目标电脑的 SAM 版本不一致时，请使用对应版本的 SAMSDK 重新编译源码。

## 下一步

部署完成后，可使用项目的 `test_cases/SAM_H5` 测试案例验证导出功能，并使用网页查看器检查导出的 VTK 文件：<https://fuxuannet.github.io/vtk_geometry_viewer/>。
