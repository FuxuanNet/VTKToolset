#ifndef SAMH5VTKEXPORTER_H
#define SAMH5VTKEXPORTER_H

#include <QString>

class VTUContainerWriter;

// 从 NH2SH/SAM 后处理 H5 直接读取网格和结果，填充 VTKDataFramesList。
// 返回 0 表示成功；非 0 时调用方会回退到原 ODB API。
int ExportSamH5ToVtkFrames(const QString& h5Path, VTUContainerWriter* writer);

#endif
