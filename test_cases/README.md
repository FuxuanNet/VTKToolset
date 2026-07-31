# 测试案例

`SAM_H5` 目录中包含 15 个 SAM H5 文件。这些文件均已在 SAM 中实际导出为 Legacy ASCII VTK，并完成网格与结果字段对比验证。

## 测试步骤

1. 先按 `deploy/README.md` 安装 VTKToolset 运行包。
2. 启动 SAM，打开或导入 `SAM_H5` 中任意一个 `.h5` 文件。
3. 切换到模型或后处理结果环境。
4. 点击 `File -> Export -> VTK...`。
5. 选择空的输出目录，导出 `.vtk` 文件。
6. 打开 <https://fuxuannet.github.io/vtk_geometry_viewer/>，选择或拖入导出的 `.vtk` 文件。
7. 在网页左侧的“结果着色”中切换 `U`、`UR`、`S`、`E`、`S11`、`S_Mises`、`S_pressure` 等字段。

## 预期结果

- VTK 中的节点数量、单元数量和连接关系与 SAM H5 保持一致。
- 源结果帧包含 `U`、`UR` 时，VTK 输出中包含相应节点结果。
- 源结果帧包含 `S`、`E` 时，VTK 输出中包含张量、各分量、`S_Mises` 和 `S_pressure`。
- 多帧 H5 会按帧导出多个 VTK 文件。

## 测试案例覆盖范围

| 文件 | 主要覆盖内容 |
|---|---|
| `180K__180k.sam.h5` | 大型壳模型，11 个结果帧 |
| `1Part-Bushing__1part-bushing.h5` | S4I 壳模型，11 个结果帧，覆盖网格、`U` 与 `UR` |
| `Beam-B31OS__beam-b31os.sam.h5` | B31 梁模型 |
| `Beam-L-LoadZ-RotAlongX__beam-l-loadz-rotalongx.sam.h5` | 梁位移与转角 |
| `Beam-OneMesh-Box-B33__beam-onemesh-box-b33.sam.h5` | B33 箱形截面梁 |
| `Beam-OneMesh-Circular-B33__beam-onemesh-circular-b33.sam.h5` | B33 圆形截面梁 |
| `Beam-OneMesh-L-B33__beam-onemesh-l-b33.sam.h5` | B33 L 形截面梁 |
| `Beam-OneMesh-Pipe-B33__beam-onemesh-pipe-b33.sam.h5` | B33 管形截面梁 |
| `Beam-OneMesh-Rect-B33__beam-onemesh-rect-b33.sam.h5` | B33 矩形截面梁 |
| `Beam-OneMesh-T-B33__beam-onemesh-t-b33.sam.h5` | B33 T 形截面梁 |
| `jiajinban-orient__jiajinban-orient.sam.h5` | 定向壳模型，11 个结果帧 |
| `Job-S4R-TractionX__job-s4r-tractionx.h5` | S4I 受拉壳模型，覆盖 `U`、`UR`、`S`、`E`、应力分量、Mises 与压力 |
| `Job-ship-test__job-ship-test.sam.h5` | 船体混合结构，11 个结果帧 |
| `Nastran_newItem0702.sam.h5` | 大型 Nastran 转换模型，包含 `U`、`UR`、`S`、`E` |
| `testp__testp.sam.h5` | 包含 `U`、`UR`、`S`、`E` 的壳模型 |

## 验证记录

本批测试一共对比了 57 个 VTK 文件，结果为 57 个通过、0 个失败。`verified_cases.json` 记录了纳入本目录的 15 个 H5 文件名。
