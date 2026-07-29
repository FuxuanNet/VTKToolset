#include <VTUFormatIO.h>

#include <VTUDataContainer.h>

#include <qstring.h>
#include <QtXml/qxml.h>
#include <qfile.h>
#include <qtextstream.h>
#include <qfileinfo.h>

VTUFormatWriter::VTUFormatWriter(const QString& path, VTUDataContainer* VTKData) {

	this->data = VTKData;
	Q_UNUSED(path);

}

int VTUFormatWriter::Write() {
	return 0;
}

VTUFormatReader::VTUFormatReader(const QString& path, VTUDataContainer* VTKData) {

}
int VTUFormatReader::Read() {
	return 0;
}
