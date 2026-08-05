#include <SamH5VtkExporter.h>
#include <VTKLegacyFormatIO.h>
#include <VTUDataContainer.h>

#include <QCoreApplication>
#include <QString>

int main(int argc, char* argv[])
{
	QCoreApplication application(argc, argv);
	if (argc != 3) return 2;
	QList<VTUDataContainer*> frames;
	if (ExportSamH5ToVtkContainers(QString::fromLocal8Bit(argv[1]), &frames) != 0) return 3;
	for (int frame = 0; frame < frames.size(); ++frame) {
		const QString output = QString::fromLocal8Bit(argv[2]) + "_" + QString::number(frame) + ".vtk";
		VTKLegacyFormatWriter fileWriter(output, frames[frame]);
		if (fileWriter.Write() != 0) return 4;
	}
	return frames.isEmpty() ? 5 : 0;
}
