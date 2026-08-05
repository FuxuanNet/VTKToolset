# SAM VTK 接口导出拓展汇报

## 1. 项目目标

本项目在 SAM 原有 VTK 插件基础上，升级后处理 H5 到 Legacy VTK 的导出能力。它不只是“把文件后缀改成 `.vtk`”，而是要完成三类数据的转换：

> **当前版本已经通过 15 个真实 H5、55 个 Frame 验证，可导出 `U、UR、RF、RM、S、E` 六类源结果字段，即位移、转角、反力、反力矩、应力和应变；同时导出节点、单元、单元类型等网格数据。**

1. 导出网格几何：节点坐标、单元连接关系和 VTK 单元类型；
2. 导出每个 Step、Frame 中的有限元结果字段；
3. 保留字段的分量、所属位置以及可以验证的 Location/Record 信息。

整体数据流为：

```text
SAM H5
  ├─ 网格：节点、单元、单元类型
  └─ 结果：Step → Frame → Field
                  ↓
            VTUDataContainer
                  ↓
Legacy VTK：POINTS、CELLS、POINT_DATA、CELL_DATA、FIELD
```

### 1.1 已通过实际样例验证的结果字段

本次自动验收使用 15 个真实 H5、共 55 个 Frame。验证报告中实际出现并成功导出的源结果字段共有六类：

| 字段   | 中文含义      | 通常所属位置     | VTK 中的主要形式                                                                                        |
| ------ | ------------- | ---------------- | ------------------------------------------------------------------------------------------------------- |
| `U`  | 位移          | 节点             | `POINT_DATA` 中的整体向量 `U`，以及已验证的 `U1/U2/U3`                                            |
| `UR` | 转角/旋转位移 | 节点             | 整体向量`UR`，以及已验证的 `UR1/UR2/UR3`                                                            |
| `RF` | 反力          | 节点             | 整体字段`RF`，以及源 `ComponentLabels` 给出的分量；样例中包括 `RF1/RF2/RF3` 和 `RFR1/RFR2/RFR3` |
| `RM` | 反力矩        | 节点             | 当前样例已验证整体向量`RM`；不额外宣称未在结果中出现的 `RM1/RM2/RM3`                                |
| `S`  | 应力          | 单元及其结果位置 | 普通`S`、各应力分量、精确 Location/Record、`S_Mises` 和 `S_pressure`                              |
| `E`  | 应变          | 单元及其结果位置 | 普通`E`、各应变分量，以及精确 Location/Record                                                         |

### 1.2 同时导出的网格标识字段

除了物理结果，新版还写出用于核对和筛选网格的辅助字段：

| 辅助字段                | 作用                                           | 所属位置 |
| ----------------------- | ---------------------------------------------- | -------- |
| `NodeLabel`           | 保存 SAM/H5 原始节点编号                       | 节点     |
| `ElementLabel`        | 保存原始单元编号                               | 单元     |
| `ElementClass`        | 保存 H5 中的单元类别编号                       | 单元     |
| `ElementType__类型名` | 标记单元原始类型，例如`ElementType__SPRINGA` | 单元     |

这些字段不是位移、应力一类求解结果，而是导出程序附加的网格元数据。它们让我们能够检查“VTK 中的这个点或单元来自源文件哪里”，也用于证明 `SPRINGA` 等特殊单元没有丢失。

对应实现位置：`VTKToolset/src/VTUFileIO/SamH5VtkExporter.cpp:361-414`。

### 2. 原版基础与本次工作边界

原版已经实现：

- SAM 菜单入口和保存对话框；
- SAM 命令接口注册；
- 通过 ODB SDK 遍历 Step、Frame 和 Field Output；
- `VTUDataContainer` 中间容器；
- Legacy VTK 文件写入；
- VTK 网格导入。

因此，菜单系统、插件加载和基础 VTK writer 不属于本次从零开发的内容。

本次主要工作集中在：

```text
直接读取 SAM H5
  -> 保留源字段和精确位置记录
  -> 补齐特殊单元
  -> 保留原 SDK 回退
  -> 自动比较 H5 与 VTK
```

## 3. 证据来源

为避免只根据代码推测，本报告同时使用三类证据：

1. 原始源码：`OpenOLTranSim-main-VTKIO-Source`；
2. 升级源码：`VTKToolset`；
3. 源 H5、新版 VTK 文件及自动验收报告。

主要对照样例：

```text
字段样例：Job-S4R-TractionX__job-s4r-tractionx.h5
特殊单元样例：180K__180k.sam.h5
```

升级后导出结果：

```text
E:/SAM/Temp/VTK_All_20260803/
```

自动验收报告：

```text
E:/SAM/Temp/VTK_All_20260803/validation_report.md
E:/SAM/Temp/VTK_All_20260803/validation_report.json
```

## 4. 改进一：增加直接 H5 主路线，同时保留原 SDK 接口

### 4.1 原版代码证据

原版 `VTUFileManager::writeODB()` 直接取得 ODB 对象，再调用 SDK writer：

**源码位置：** `OpenOLTranSim-main-VTKIO-Source/VTKIO-Source/VTUFileIO/VTUFileManager.cpp:257-264`

```cpp
int VTUFileManager::writeODB() {
    QString odbPath = target.TargetOdbPath();
    odbOdb& odb = odbOdbRepository::Instance().get(odbPath);
    int status = writer->VTKExportODB(&odb);
    return status;
}
```

**代码注释（讲解）：**

- 第 259 行取得用户要导出的 ODB 路径；`QString` 是 Qt 提供的字符串类型。
- 第 261 行通过 SAM SDK 的 `odbOdbRepository` 查找并取得 ODB 对象；引用符号 `&` 表示这里使用仓库中的原对象，不复制一份新对象。
- 第 263 行把 ODB 对象交给原版 `VTKExportODB()`，所以原版只有“ODB SDK → VTK”这一条后处理路线。
- 第 264 行返回状态码，通常 `0` 表示成功，非 `0` 表示出现错误。

原版路径是：

```text
SAM H5/ODB
  -> ODB SDK 对象
  -> VTUContainerWriter
  -> VTK
```

它能够遍历 SDK 暴露的字段，但没有直接遍历源 H5 的字段层级。因此，源文件内容能否完整进入 VTK，还受 SDK 对象表达和旧 writer 规则影响。

### 4.2 新版解决方案

新版 `VTUFileManager::writeODB()` 先尝试直接 H5：

**源码位置：** `VTKToolset/src/VTUFileIO/VTUFileManager.cpp:279-310`（下面节选主路线与回退代码）

```cpp
if (ExportSamH5ToVtkFrames(odbPath, writer) == 0) {
    return 0;
}

odbOdb& odb = odbOdbRepository::Instance().get(odbPath);
int status = writer->VTKExportODB(&odb);
return status;
```

**代码注释（讲解）：**

- `ExportSamH5ToVtkFrames()` 是新增的直接 H5 入口；返回 `0` 时立即 `return`，优先采用信息更完整的新路线。
- 如果第一次读取失败，完整源码第 295—302 行还会通过 ODB 对象取得真实 H5 路径并重试，解决界面传入仓库别名而不是真实磁盘路径的问题。
- 两次直接读取都不适用时，程序继续执行原来的 `VTKExportODB()`，这就是“保留原接口”的具体代码证据。
- 这种写法属于“优先新路线、失败后回退”的兼容设计，不要求旧数据全部符合新增 H5 读取规则。

直接 H5 失败时仍执行原 `VTKExportODB()`。因此升级增加了新路线，但没有删除旧接口。

新版主流程：

```text
VTUFileManager::writeODB()
  -> ExportSamH5ToVtkFrames()
  -> ExportSamH5ToVtkContainers()
  -> 读取 Part、Instance、Step、Frame、Field
  -> VTUDataContainer
  -> Legacy VTK writer
```

### 4.3 实际结果证据

独立测试程序 `VTUFileIOSmokeTest.exe` 与正式插件调用同一个函数：

**源码位置：** `VTKToolset/tests/direct_h5_export_smoke.cpp:12-17`，核心调用位于第 13 行。

```cpp
QList<VTUDataContainer*> frames;
if (ExportSamH5ToVtkContainers(QString::fromLocal8Bit(argv[1]), &frames) != 0) return 3;
```

**代码注释（讲解）：**

- `QString::fromLocal8Bit(argv[1])` 把命令行传入的 H5 路径转换成 Qt 字符串。
- `frames` 是保存所有输出帧指针的 `QList` 容器；尖括号中的类型说明容器里装什么。
- `&frames` 表示把容器地址交给函数，函数才能把生成的帧写回调用者。
- 如果转换函数返回非 `0`，测试程序立即返回错误码 `3`，表示核心转换失败。
- 测试程序和正式插件最终都进入 `ExportSamH5ToVtkContainers()`，因此冒烟测试验证的是正式导出核心，而不是另写的一套模拟代码。

使用该核心批量转换 15 个真实 H5，共生成 55 个 VTK；55 帧全部通过网格与字段检查。这证明直接 H5 核心不仅存在于代码中，也实际完成了批量转换。

## 5. 改进二：保留普通兼容字段，并新增精确位置记录

### 5.1 原版代码证据

原版普通字段由 `WriteField()` 写入。对单元字段，它遍历 SDK 返回的每条 `value`，但每次都使用相同的 `fieldName` 和 `index`：

**源码位置：** `OpenOLTranSim-main-VTKIO-Source/VTKIO-Source/VTUFileIO/VTUContainerWriter.cpp:219-272`（下面保留与覆盖问题直接相关的语句）

```cpp
for (int i = 0; i < field.values().size(); ++i) {
    const odbFieldValue value = field.values(i);
    int elemLabel = value.elementLabel();
    int index = mesh.GetMeshElemIndexFromLabel(elemLabel);
    container->InsertCellData(fieldName, index, dataVec);
}
```

**代码注释（讲解）：**

- 循环逐条读取字段值；`value.elementLabel()` 取得这条结果所属的有限元单元编号。
- `GetMeshElemIndexFromLabel()` 把 SAM 单元编号转换成 VTK 容器中的连续下标。
- 最终写入时只使用 `fieldName` 和 `index`，没有把 Location、积分点或 Record 编入数组名称。
- 因此，同一字段、同一单元若有多条记录，它们会指向容器中的同一个存储位置。

原版 `InsertCellData()` 对同一个“字段名 + 单元位置”直接赋值：

**源码位置：** `OpenOLTranSim-main-VTKIO-Source/VTKIO-Source/VTUFileIO/VTUDataContainer.cpp:92-101`，直接赋值位于第 99—100 行。

```cpp
for (int i = 0; i < numComponents; ++i) {
    data[cellIndex * numComponents + i] = values[i];
}
```

**代码注释（讲解）：**

- `data` 是某一个字段对应的一维浮点数组；多分量数据按“单元 0 的所有分量、单元 1 的所有分量……”连续存放。
- `cellIndex * numComponents + i` 用于计算“某单元的第 `i` 个分量”在一维数组中的位置。
- 赋值符号 `=` 会替换该位置原来的数值，所以相同字段和相同单元位置被再次写入时，旧值会被覆盖。

因此，当同一单元存在多条位置记录时，原版没有为它们建立不同名称，后写入的记录可能覆盖先写入的记录。源码能够证明的问题不是“原版一定取了哪一种平均值”，而是 **Location/Record 身份没有被保留下来**。

原版源码没有生成包含 Location/Record 身份的字段名称；因此原版能力由上述源码判断，不引用迭代过程中的旧 VTK 文件代替原版结果。

### 5.2 新版精华代码

新版为每个位置和记录生成不会重名的字段：

**源码位置：** `VTKToolset/src/VTUFileIO/SamH5VtkExporter.cpp:645-657`（下面节选字段命名与写入部分）

```cpp
QString exactName = fieldName + "__" + vtkSafeName(QString::fromStdString(locationName));
if (recordsPerElement > 1) exactName += "__Record_" + QString::number(record + 1);
if (part == "Imag") exactName += "__Imag";
const int cellIndex = globalCellOffset + element;
container->InsertCellData(exactName, cellIndex, tuple);
```

**代码注释（讲解）：**

- `exactName` 从原字段名开始，再拼接 Location 名称，例如 `S__LocationIndex_1`。
- 当每个单元有多条记录时，再拼接 `__Record_1`、`__Record_2`，使记录拥有不同数组名。
- 如果数据属于复数虚部，再加 `__Imag`；这段能力目前缺少归档样例，所以报告不把它列为已验证成果。
- 最后用新名称写入容器，不同位置和记录不再落到同名数组中。

精确记录写完后，新版再生成一个普通名称的兼容字段。新版对同一单元的实部记录明确求平均：

**源码位置：** `VTKToolset/src/VTUFileIO/SamH5VtkExporter.cpp:676-683`

```cpp
QVector<float> average = sums[element];
for (int component = 0; component < average.size(); ++component) average[component] /= counts[element];
const int cellIndex = globalCellOffset + element;
insertCellTuple(container, fieldName, fieldName,
    labels, cellIndex, average);
```

**代码注释（讲解）：**

- `sums[element]` 保存该单元各条实部记录的分量总和，复制给 `average` 后不会破坏原累计值。
- 循环把每个分量除以记录数 `counts[element]`，得到新版明确定义的单元平均值。
- `insertCellTuple()` 仍使用普通字段名 `S` 或 `E` 写入，使原来只认识普通名称的查看方式继续可用。
- 精确字段负责“信息不丢”，普通平均字段负责“兼容原使用习惯”，两类字段同时存在。

设计含义：

- `S`、`E`：兼容旧查看方式的单元平均结果；
- `S__LocationIndex_*__Record_*`：源位置精确结果；
- `E__LocationIndex_*__Record_*`：源位置精确结果。

### 5.3 新版结果证据

新版 `Job-S4R-TractionX` VTK 中包含：

```text
E__LocationIndex_1__Record_1 ... Record_4
E__LocationIndex_3__Record_1 ... Record_4
E__LocationIndex_5__Record_1 ... Record_4

S__LocationIndex_1__Record_1 ... Record_4
S__LocationIndex_3__Record_1 ... Record_4
S__LocationIndex_5__Record_1 ... Record_4
```

证据对照：

| 检查项目                 | 原版源码             | 新版实际 VTK |
| ------------------------ | -------------------- | -----------: |
| Location/Record 独立命名 | 没有                 |           有 |
| 精确`S/E` 位置记录数组 | 源码没有对应命名逻辑 |           24 |
| 普通名称`S/E` 兼容字段 | 保留普通字段名       |           有 |

因此新版没有删除原接口使用者熟悉的普通 `S/E` 字段，同时新增了 24 个带 Location/Record 身份的精确位置数组。这里所说的“平均”只描述新版兼容字段的生成方式，不对原版普通字段的数值处理作超出源码证据的推断。

## 6. 改进三：补齐 SPRINGA，避免特殊单元丢失

### 6.1 原版代码证据

原版一维单元映射只包含：

**源码位置：** `OpenOLTranSim-main-VTKIO-Source/VTKIO-Source/VTUFileIO/VTUElementHandler.cpp:25-29`

```cpp
if (typeLabel == "B31" || typeLabel == "B33" || typeLabel == "T3D2") return VTK_LINE;
return VTK_NONE;
```

**代码注释（讲解）：**

- `typeLabel` 是 SAM 有限元类型名称，`VTK_LINE` 是 VTK 的两节点线单元类型。
- 原版只识别 `B31`、`B33` 和 `T3D2`；都不匹配时返回 `VTK_NONE`，表示没有找到可输出的 VTK 类型。
- 由于条件中没有 `SPRINGA`，该类型无法通过这里的一维单元映射。

其中没有 `SPRINGA`。

`180K` 源 H5 中共有：

```text
总单元数：31,853
SPRINGA：12
```

原版源码对 `SPRINGA` 返回 `VTK_NONE`，因此它没有对应的 VTK 单元类型映射。这里不使用迭代过程中的旧 VTK 文件代替原版结果证据。

### 6.2 新版精华代码

新版明确把弹簧和连接器保存为 VTK 线单元：

**源码位置：** `VTKToolset/src/VTUFileIO/VTUElementHandler.cpp:26-31`

```cpp
if (typeLabel == "B31" || typeLabel == "B33" || typeLabel == "T3D2"
    || typeLabel == "SPRINGA" || typeLabel == "SPRING1" || typeLabel == "SPRING2"
    || typeLabel.startsWith("CONN")) return VTK_LINE;
return VTK_NONE;
```

**代码注释（讲解）：**

- 新条件在原三种线单元基础上加入 `SPRINGA`、`SPRING1`、`SPRING2`。
- `startsWith("CONN")` 表示名称以 `CONN` 开头的连接器类型也按线单元保存。
- 多个条件用 `||` 连接，含义是“任意一个条件成立即可返回 `VTK_LINE`”。
- 这些单元本质上连接两个节点，用 VTK 线单元保留其拓扑关系，比直接丢弃更符合源网格。

同时保存原始类型标识：

**源码位置：** `VTKToolset/src/VTUFileIO/SamH5VtkExporter.cpp:410-415`

```cpp
const QString fieldName = "ElementType__" + elementType;
```

**代码注释（讲解）：**

- 这行把原始类型拼进字段名，例如生成 `ElementType__SPRINGA`。
- 完整源码随后为每个单元写入 `1` 或 `0`：属于该类型写 `1`，否则写 `0`。
- VTK 查看器因此不仅知道它是线单元，还可以通过该字段识别其原始 SAM 类型。

### 6.3 新版结果证据

新版文件：

```text
E:/SAM/Temp/VTK_All_20260803/
180K__180k.sam/180K__180k.sam_0.vtk
```

结果：

```text
CELLS 31853
CELL_TYPES 31853
SCALARS ElementType__SPRINGA float 1
```

对比：

| 检查项目         |  原版源码/源 H5 |            新版实际 VTK |
| ---------------- | --------------: | ----------------------: |
| `SPRINGA` 映射 |    原版源码没有 |            `VTK_LINE` |
| `SPRINGA` 数量 |     源 H5 为 12 | 12 个单元被类型字段标识 |
| 总单元数         | 源 H5 为 31,853 |                  31,853 |

新版单元数与源 H5 完全一致，并能在 VTK 中识别 12 个 `SPRINGA`。这项结论由原版映射源码、源 H5 统计和新版 VTK 共同证明。

## 7. 核心架构与主讲文件

课堂主线只需要讲五个文件。

### 7.1 `VTUFileManager.cpp`：路线调度

```text
VTKToolset/src/VTUFileIO/VTUFileManager.cpp
```

主讲：`WriteCache()`、`WriteFile()`、`writeODB()`。

**源码位置：**

- `WriteCache()`：`VTKToolset/src/VTUFileIO/VTUFileManager.cpp:168-184`
- `WriteFile()`：`VTKToolset/src/VTUFileIO/VTUFileManager.cpp:186-254`
- `writeODB()`：`VTKToolset/src/VTUFileIO/VTUFileManager.cpp:279-310`

```text
WriteCache：读取 SAM/H5，先整理到内存容器
WriteFile：把内存容器写成一个或多个 VTK
writeODB：直接 H5 优先，ODB SDK 回退
```

### 7.2 `SamH5VtkExporter.cpp`：升级核心

```text
VTKToolset/src/VTUFileIO/SamH5VtkExporter.cpp
```

主讲三个函数：

**源码位置：**

- `ExportSamH5ToVtkContainers()`：`VTKToolset/src/VTUFileIO/SamH5VtkExporter.cpp:726-817`
- `writeField()`：`VTKToolset/src/VTUFileIO/SamH5VtkExporter.cpp:694-722`
- `writeElementField()`：`VTKToolset/src/VTUFileIO/SamH5VtkExporter.cpp:615-692`

```text
ExportSamH5ToVtkContainers：遍历 Step、Frame、Field
writeField：节点/单元/全局字段分发
writeElementField：精确记录与平均兼容字段
```

### 7.3 `VTUDataContainer.h/.cpp`：中间数据容器

```text
VTKToolset/src/VTUFileIO/VTUDataContainer.h
VTKToolset/src/VTUFileIO/VTUDataContainer.cpp
```

它连接 reader 和 writer，可类比 Python 字典：

**对应源码位置：** `VTKToolset/src/VTUFileIO/VTUDataContainer.h:20-50`

下面不是项目中的 Python 源码，而是帮助初学者理解 C++ 容器结构的讲解用伪代码：

```python
container = {
    "points": [],
    "cells": [],
    "pointData": {},
    "cellData": {},
    "fieldData": {}
}
```

**代码注释（讲解）：**

- `points` 对应 C++ 中的 `QVector<Point>`，保存节点坐标。
- `cells` 对应 `QVector<Element>`，保存单元类型和节点连接关系。
- 三个 `Data` 字典按字段名保存节点数据、单元数据和全局数据。
- 这个类把“读取源数据”和“写出 VTK”隔开，是整个项目的数据中转站。

### 7.4 `VTKLegacyFormatIO.cpp`：最终 VTK 写出

```text
VTKToolset/src/VTUFileIO/VTKLegacyFormatIO.cpp
```

负责把容器写成：

```text
POINTS
CELLS
CELL_TYPES
POINT_DATA
CELL_DATA
FIELD
```

**对应源码位置：** `VTKToolset/src/VTUFileIO/VTKLegacyFormatIO.cpp:32-90`、`VTKToolset/src/VTUFileIO/VTKLegacyFormatIO.cpp:92-180` 及后续数据写出函数。

**格式注释（讲解）：** `POINTS` 保存坐标，`CELLS` 保存节点连接关系，`CELL_TYPES` 保存 VTK 单元类型；后三项分别承载节点字段、单元字段和不依附具体节点或单元的全局字段。这一块是 VTK 文件格式关键字，不是独立的 C++ 代码片段。

### 7.5 `VTUElementHandler.cpp`：有限元类型到 VTK 类型

```text
VTKToolset/src/VTUFileIO/VTUElementHandler.cpp
```

主讲 `SPRINGA -> VTK_LINE`，并用源 H5 与新版 VTK 均为 31,853 个单元、其中 12 个为 `SPRINGA` 来证明。

## 8. 工作量说明

本次不是简单增加几个 `if`，而是打通整条数据链：

```text
H5 字段和网格读取
  -> 字段分类与精确记录命名
  -> 统一容器扩展
  -> Legacy VTK 写出
  -> 特殊单元映射
  -> 自动验证
```

可复核规模：

- 新增 `SamH5VtkExporter.cpp/.h`，形成独立的 H5 读取与转换模块；
- 扩展 `VTUFileManager`、`VTUDataContainer`、`VTKLegacyFormatIO`、`VTUContainerWriter`、`VTUElementHandler` 等原有后端文件；
- 增加独立 H5 转换测试入口；
- 增加 H5 源字段完整性验证；
- 生成并保留 55 个 VTK 和逐帧 JSON 报告。

代码行数只表示规模，实际工作重点是字段语义和网格数量能够从源 H5 一直保持到最终 VTK。

## 9. 全量验证结果

测试范围：

```text
VTKToolset/test_cases/SAM_H5/
```

测试结果：

| 项目    | 结果 |
| ------- | ---: |
| H5 文件 |   15 |
| VTK 帧  |   55 |
| 通过帧  |   55 |
| 失败帧  |    0 |

每帧检查：

- H5 Frame 数与 VTK 文件数一致；
- 节点数一致；
- 单元数一致；
- `CELL_TYPES` 数与单元数一致；
- 单元连接下标全部位于有效节点范围；
- H5 源字段在 VTK 中存在同名兼容字段或精确记录字段。

报告位置：

```text
E:/SAM/Temp/VTK_All_20260803/validation_report.md
E:/SAM/Temp/VTK_All_20260803/validation_report.json
E:/SAM/Temp/VTK_All_20260803/vtk_file_manifest.csv
```

本次补充中文注释后，已重新执行 Release 编译：

**执行位置：** `E:/WuXi/Example_new/VTKToolset`。这是构建命令，不对应某个 C++ 源码行号。

```powershell
cmake --build build --config Release
```

**命令注释（讲解）：** `cmake --build` 表示让 CMake 调用已经配置好的编译器；`build` 是构建目录；`--config Release` 表示生成用于部署的 Release 版本。

构建成功生成：

```text
VTKToolset/bin/Release/VTUFileIO.pyd
VTKToolset/bin/Release/SAM.Pre.VTUFileIOToolset.dll
VTKToolset/bin/Release/VTUFileIOSmokeTest.exe
```

这一步证明新增讲解注释没有破坏 C++ 编译；数据正确性则由前述 15 个 H5、55 帧验证结果证明。

## 11. 当前边界

直接 H5 路线当前只对单 Part、单 Instance 文件直接转换。检测到多 Part 或多 Instance 时，不强行拼接，而是返回失败并调用原 ODB SDK 路线。

当前 15 个归档测试文件均为单 Part、单 Instance，因此全量测试覆盖的是当前直接路线的明确适用范围。

代码还实现了复数虚部、全局字段和任意分量 `FIELD` 写法，但当前归档 H5 没有相应样例形成完整结果证据。因此这些能力不作为本次汇报的已验证成果，只作为后续补充测试方向。

## 12. 汇报结论

本次升级保留了原 ODB SDK 接口，新增直接 H5 主路线，并完成两项有原版源码和实际结果证明的数据拓展：

1. `Job-S4R-TractionX` 新增 24 个精确 `S/E` 位置记录，同时保留普通名称的兼容字段；
2. 原版没有 `SPRINGA` 的 VTK 类型映射；新版导出 12 个 `SPRINGA`，总单元数与源 H5 的 31,853 一致。

最终 15 个 H5、55 帧全部通过自动验证。问题、解决代码和实际导出结果能够逐项对应。
