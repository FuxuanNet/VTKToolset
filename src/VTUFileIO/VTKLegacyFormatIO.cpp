#include <VTKLegacyFormatIO.h>

#include <VTUDataContainer.h>

#include <qstring.h>
#include <qfile.h>
#include <qtextstream.h>
#include <qfileinfo.h>
#include <qdebug.h>

/*
* VTULegacyFormatWriter
* Write data in format of .vtk unstructured grid
*/

VTKLegacyFormatWriter::VTKLegacyFormatWriter(const QString& path, VTUDataContainer* VTKData) {

	this->data = VTKData;
	QString targetPath = path;
	if (QFileInfo(path).suffix() != "vtk") targetPath = path + ".vtk";

	file = new QFile(targetPath);
	if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
		file = NULL;
		return;
	}
	stream = new QTextStream(file);
	stream->setRealNumberPrecision(8);
	stream->setRealNumberNotation(QTextStream::SmartNotation);
}

int VTKLegacyFormatWriter::Write() {

	if (!file) return ERRORTYPE_NOTEXIST;
	*stream << "# vtk DataFile Version 3.0\n";
	*stream << "SAMModel Output" << "\n";
	*stream << "ASCII\n";
	*stream << "DATASET UNSTRUCTURED_GRID\n";
	*stream << flush;

	currentState = HeaderWritten;
	currentState = WritePointsHeader();
	*stream << flush;

	currentState = WritePoints();
	*stream << flush;

	currentState = WriteCellsHeader();
	*stream << flush;

	currentState = WriteCells();
	*stream << flush;

	currentState = WriteCellTypesHeader();
	*stream << flush;

	currentState = WriteCellTypes();
	*stream << flush;

	currentState = WritePointData();
	*stream << flush;

	currentState = WriteCellData();
	*stream << flush;

	return 0;
}

FormatWriter::State VTKLegacyFormatWriter::WritePointsHeader() {
	if (currentState != HeaderWritten) {
		return currentState;
	}
	 
	*stream << "POINTS " << data->points.size() << " float" << "\n";
	nodesWritten = 0;
	return PointsWriting;
}

FormatWriter::State VTKLegacyFormatWriter::WritePoints() {
	if (currentState != PointsWriting) {
		return currentState;
	}

	const int pointsPerLine = 9;
	int count = 0;

	for (int i = 0; i < data->points.size(); ++i) {
		float x = data->points[i].x;
		float y = data->points[i].y;
		float z = data->points[i].z;

		*stream << x << " " << y << " " << z << " ";
		count++;

		if (count % pointsPerLine == 0) {
			*stream << "\n"; 
		}
	}

	if (count % pointsPerLine != 0) {
		*stream << "\n"; 
	}

	nodesWritten += data->points.size();
	return PointsWritten;
}

FormatWriter::State VTKLegacyFormatWriter::WriteCellsHeader() {
	if (currentState != PointsWritten) {
		return currentState;
	}

	*stream << "\nCELLS " << data->elems.size() << " " << data->elemVertices << '\n';
	cellsWritten = 0;

	return CellsWriting;
}

FormatWriter::State VTKLegacyFormatWriter::WriteCells() {
	if (currentState != CellsWriting) {
		return currentState;
	}

	const int cellsPerLine = 3; 
	int count = 0;

	for (int i = 0; i < data->elems.size(); ++i) {
		int numNodes = VTUElementHandler::GetArrayLengthByEnum(data->elems[i].type);
		*stream << numNodes;

		for (int j = 0; j < numNodes; ++j) {
			*stream << ' ' << data->elems[i].dataSet[j];
		}

		*stream << " "; 
		count++;

		if (count % cellsPerLine == 0) {
			*stream << "\n";
		}
	}

	if (count % cellsPerLine != 0) {
		*stream << "\n";
	}

	cellsWritten += data->elems.size();
	return CellsWritten;
}

FormatWriter::State VTKLegacyFormatWriter::WriteCellTypesHeader() {
	if (currentState != CellsWritten) {
		return currentState; 
	}

	*stream << "\nCELL_TYPES " << data->elems.size() << '\n';

	return CellTypesWriting;
}

FormatWriter::State VTKLegacyFormatWriter::WriteCellTypes() {
	if (currentState != CellTypesWriting) {
		return currentState; 
	}

	const int numCells = data->elems.size();
	const int typesPerLine = 20; 

	for (int i = 0; i < numCells; ++i) {
		*stream << data->elems[i].type << " ";
		if ((i + 1) % typesPerLine == 0) {
			*stream << "\n";
		}
	}

	if (numCells % typesPerLine != 0) *stream << "\n";

	return CellTypesWritten;
}


FormatWriter::State VTKLegacyFormatWriter::WritePointData() {
	if (currentState != CellTypesWritten) {
		return currentState;
	}

	int numPoints = data->points.size();
	*stream << "\nPOINT_DATA " << numPoints << "\n";

	for (auto it = data->pointData.constBegin(); it != data->pointData.constEnd(); ++it) {
		const QString& fieldName = it.key();
		const QVector<float>& values = it.value();

		int numComp = data->pointDataComponents.value(fieldName, 0);
		if (numComp <= 0 || values.size() < numPoints * numComp) continue;

		if (numComp == 1) {
			// 单分量字段写成 SCALARS，例如 S11、S_pressure。
			*stream << "SCALARS " << fieldName << " float 1\n";
			*stream << "LOOKUP_TABLE default\n";

			const int pointsPerLine = 10; 
			for (int i = 0; i < numPoints; ++i) {
				*stream << values[i] << " ";
				if ((i + 1) % pointsPerLine == 0) *stream << "\n";
			}
			if (numPoints % pointsPerLine != 0) *stream << "\n";
		}
		else if (numComp == 3) {
			// 三分量字段写成 VECTORS，例如 U、UR。
			*stream << "VECTORS " << fieldName << " float\n";
			const int pointsPerLine = 3; // 每行写 3 个点，让文本文件更容易阅读。
			for (int i = 0; i < numPoints; ++i) {
				*stream << values[3 * i] << " "
					<< values[3 * i + 1] << " "
					<< values[3 * i + 2] << " ";
				if ((i + 1) % pointsPerLine == 0) *stream << "\n";
			}
			if (numPoints % pointsPerLine != 0) *stream << "\n";
		}
		else if (numComp == 6) {
			// 六分量字段写成 TENSORS，例如 S、E。
			*stream << "TENSORS " << fieldName << " float\n";
			const int pointsPerLine = 2; // 每行写 2 个张量，避免一行过长。
			for (int i = 0; i < numPoints; ++i) {
				float xx = values[6 * i + 0];
				float yy = values[6 * i + 1];
				float zz = values[6 * i + 2];
				float xy = values[6 * i + 3];
				float yz = values[6 * i + 4];
				float xz = values[6 * i + 5];

				// VTK TENSORS 需要 3x3 矩阵，这里由 6 个工程分量展开。
				*stream << xx << " " << xy << " " << xz << "  "
					<< xy << " " << yy << " " << yz << "  "
					<< xz << " " << yz << " " << zz << "  ";

				if ((i + 1) % pointsPerLine == 0) *stream << "\n";
			}
			if (numPoints % pointsPerLine != 0) *stream << "\n";
		}
	}
	currentState = PointDataWritten;

	return PointDataWritten;
}


FormatWriter::State VTKLegacyFormatWriter::WriteCellData() {
	if (currentState != PointDataWritten) {
		return currentState;
	}

	int numCells = data->elems.size();  // 单元数量
	*stream << "\nCELL_DATA " << numCells << "\n";

	if (numCells)
	{
		for (auto it = data->cellData.constBegin(); it != data->cellData.constEnd(); ++it) {
			const QString& fieldName = it.key();
			const QVector<float>& values = it.value();

			int numComp = data->cellDataComponents.value(fieldName, 0);
			if (numComp <= 0 || values.size() < numCells * numComp) continue;

			if (numComp == 1) {
				// 单分量单元字段写成 SCALARS。
				*stream << "SCALARS " << fieldName << " float 1\n";
				*stream << "LOOKUP_TABLE default\n";

				const int cellsPerLine = 10; // 每行写 10 个标量单元值。
				for (int i = 0; i < numCells; ++i) {
					*stream << values[i] << " ";
					if ((i + 1) % cellsPerLine == 0) *stream << "\n";
				}
				if (numCells % cellsPerLine != 0) *stream << "\n";
			}
			else if (numComp == 3) {
				// 三分量单元字段写成 VECTORS。
				*stream << "VECTORS " << fieldName << " float\n";

				const int cellsPerLine = 3; // 每行写 3 个向量单元值。
				for (int i = 0; i < numCells; ++i) {
					*stream << values[3 * i] << " "
						<< values[3 * i + 1] << " "
						<< values[3 * i + 2] << " ";
					if ((i + 1) % cellsPerLine == 0) *stream << "\n";
				}
				if (numCells % cellsPerLine != 0) *stream << "\n";
			}
			else if (numComp == 6) {
				// 六分量单元字段写成 TENSORS。
				*stream << "TENSORS " << fieldName << " float\n";

				const int cellsPerLine = 2; // 每行写 2 个单元张量。
				for (int i = 0; i < numCells; ++i) {
					float xx = values[6 * i + 0];
					float yy = values[6 * i + 1];
					float zz = values[6 * i + 2];
					float xy = values[6 * i + 3];
					float yz = values[6 * i + 4];
					float xz = values[6 * i + 5];

					// VTK TENSORS 使用 3x3 矩阵顺序写出。
					*stream << xx << " " << xy << " " << xz << "  "
						<< xy << " " << yy << " " << yz << "  "
						<< xz << " " << yz << " " << zz << "  ";

					if ((i + 1) % cellsPerLine == 0) *stream << "\n";
				}
				if (numCells % cellsPerLine != 0) *stream << "\n";
			}
		}
	}

	currentState = CellDataWritten;

	return CellDataWritten;
}


FormatWriter::State VTKLegacyFormatWriter::WriteMaterialsHeader() {
	return MaterialsWriting;
}

FormatWriter::State VTKLegacyFormatWriter::WriteMaterials() {

	return MaterialsWriting;
}

FormatWriter::State VTKLegacyFormatWriter::BeginField() {
	return FieldHeaderWritten;
}

FormatWriter::State VTKLegacyFormatWriter::WriteFieldHeader()
{
	return FieldDataWriting;
}

FormatWriter::State VTKLegacyFormatWriter::WriteFieldData() {
	return FieldDataWriting;
}

FormatWriter::State VTKLegacyFormatWriter::WriteField(const QString& name, int numComponents, int numTuples,
	const QString& dataType, float* values)
{
	return FieldDataWriting;
}

//void VTKLegacyFormatWriter::End() {
//	if (stream) {
//		if (currentState == FieldDataWriting) {
//			// 如果正在写 FIELD 块，结束前补一个空行。
//			*stream << "\n\n";
//		}
//	}
//	currentState = Finished;
//}

// new


/*
* VTKLegacyFormatReader
* Read in .vtk unstructured grid files
*/

VTKLegacyFormatReader::VTKLegacyFormatReader(const QString& path, VTUDataContainer* VTKData) {
	this->data = VTKData;
	QString targetPath = path;
	if (QFileInfo(path).suffix() != "vtk") return;

	file = new QFile(targetPath);
	if (!file->open(QIODevice::ReadOnly | QIODevice::Text)) {
		file = NULL;
		return;
	}
	stream = new QTextStream(file);
	nodesRead = 0;
	cellsRead = 0;
	status = 0;
}

int VTKLegacyFormatReader::Read() {

	if (!file) return ERRORTYPE_NOTEXIST;

	if (stream->atEnd()) return ERRORTYPE_FILE_READ_FAILED;
	QString versionLine = stream->readLine();
	if (!versionLine.startsWith("# vtk DataFile Version")) {
		return ERRORTYPE_FILE_READ_FAILED;
	}

	// Comment line (skip)
	if (stream->atEnd()) return ERRORTYPE_FILE_READ_FAILED;

	stream->readLine();

	// Format (only ASCII supported)
	if (stream->atEnd()) return ERRORTYPE_FILE_READ_FAILED;
	QString format = stream->readLine().trimmed();
	if (format != "ASCII") {
		return ERRORTYPE_FILE_READ_FAILED;
	}

	// Dataset type
	if (stream->atEnd()) return ERRORTYPE_FILE_READ_FAILED;
	QString dataset = stream->readLine().trimmed();
	if (!dataset.startsWith("DATASET UNSTRUCTURED_GRID")) {
		return ERRORTYPE_FILE_READ_FAILED;
	}
	QString version = versionLine.split(' ', QString::SkipEmptyParts)[4];
	if (version == "3.0")
		return Read30();
	else if (version == "5.1")
		return Read51();
	else
		return ERRORTYPE_FILE_READ_FAILED;
}

int VTKLegacyFormatReader::Read30() {
	while (!stream->atEnd()) {
		QString line = stream->readLine().trimmed();

		if (line.startsWith("POINTS")) {
			QStringList parts = line.split(' ', QString::SkipEmptyParts);
			if (parts.size() < 2) return ERRORTYPE_WRONG_NODE_DATA;
			bool ok;
			int numPoints = parts[1].toInt(&ok);
			if (!ok) return ERRORTYPE_WRONG_NODE_DATA;

			if (parts[2] == "float" || parts[2] == "double" || parts[2] == "int" || parts[2] == "long")
				status |= ReadPoints(numPoints);
		}
		else if (line.startsWith("POINT_DATA")) {
			QStringList parts = line.split(' ', QString::SkipEmptyParts);
			if (parts.size() < 2) return ERRORTYPE_WRONG_NODE_DATA;
			bool ok;
			int numPoints = parts[1].toInt(&ok);
			if (!ok) return ERRORTYPE_WRONG_NODE_DATA;

			// POINT_DATA 这一行在 Legacy VTK 中只有 “POINT_DATA 点数”，
			// 类型信息在后面的 VECTORS/SCALARS 行里。原代码访问 parts[2] 会越界，
			// SAM 导入带结果的 VTK 时容易卡死或崩溃。
			status |= ReadPointData(numPoints);
		}
		else if (line.startsWith("CELLS")) {
			QStringList parts = line.split(' ', QString::SkipEmptyParts);
			if (parts.size() < 2) return ERRORTYPE_WRONG_ELEMENT_DATA;
			bool ok;
			int numCells = parts[1].toInt(&ok);
			if (!ok) return ERRORTYPE_WRONG_ELEMENT_DATA;
			status |= ReadCells30(numCells);

		}
		else if (line.startsWith("CELL_TYPES")) {
			QStringList parts = line.split(' ', QString::SkipEmptyParts);
			if (parts.size() < 2) continue;
			bool ok;
			int numTypes = parts[1].toInt(&ok);
			if (!ok) continue;
			status |= ReadCellTypes(numTypes);
		}
	}

	//state |= ReadMaterials();
	return status;
}

int VTKLegacyFormatReader::Read51() {
	while (!stream->atEnd()) {
		QString line = stream->readLine().trimmed();

		if (line.startsWith("POINTS")) {
			QStringList parts = line.split(' ', QString::SkipEmptyParts);
			if (parts.size() < 2) return ERRORTYPE_WRONG_NODE_DATA;
			bool ok;
			int numPoints = parts[1].toInt(&ok);
			if (!ok) return ERRORTYPE_WRONG_NODE_DATA;

			if (parts[2] == "float" || parts[2] == "double" || parts[2] == "int" || parts[2] == "long")
				status |= ReadPoints(numPoints);
		}
		else if (line.startsWith("CELLS")) {
			QStringList parts = line.split(' ', QString::SkipEmptyParts);
			if (parts.size() < 2) return ERRORTYPE_WRONG_ELEMENT_DATA;
			bool ok;
			int numOffsets = parts[1].toInt(&ok);
			if (!ok) return ERRORTYPE_WRONG_ELEMENT_DATA;
			status |= ReadCells51(numOffsets);
		}
	}

	//state |= ReadMaterials();
	return status;
}

int VTKLegacyFormatReader::ReadPoints(int numPoints) {
	for (int i = 0; i < numPoints; ++i) {
		if (stream->atEnd()) return ERRORTYPE_WRONG_NODE_DATA;

		float x, y, z;
		*stream >> x >> y >> z;

		data->InsertNextPoint(i, x, y, z);

		++nodesRead;
	}
	return 0;
}

int VTKLegacyFormatReader::ReadPointData(int numPoints) {
	if (!stream || stream->atEnd()) return ERRORTYPE_FILE_READ_FAILED;
	bool readAnyVector = false;

	while (!stream->atEnd()) {
		QString line = stream->readLine().trimmed();
		if (line.isEmpty()) continue;

		// CELL_DATA 是下一个大段。导入网格时当前只需要读 POINT_DATA 中的
		// 位移向量。VTK 导出的应力/应变在 CELL_DATA 中，当前 SAM 导入接口
		// 不把它恢复成云图结果，所以这里安全跳过后续结果段，避免大量
		// SCALARS/TENSORS 被当成网格段继续解析导致卡住。
		if (line.startsWith("CELL_DATA")) {
			while (!stream->atEnd()) {
				stream->readLine();
			}
			return 0;
		}

		QStringList parts = line.split(' ', QString::SkipEmptyParts);
		if (parts.isEmpty()) continue;
		if (parts[0] == "VECTORS") {
			if (parts.size() < 2) return ERRORTYPE_FILE_READ_FAILED;
			QString dataName = parts[1]; // 字段名，例如 "Displacement"。

			QVector<float> displacement;
			displacement.reserve(numPoints * 3); // 提前预留空间，减少 QVector 扩容次数。

			for (int i = 0; i < numPoints; ++i) {
				float u, v, w;
				*stream >> u >> v >> w;
				displacement.push_back(u);
				displacement.push_back(v);
				displacement.push_back(w);
			}

			// 写入 pointData，后续导入网格时可保留节点向量字段。
			data->pointData.insert(dataName, displacement);
			data->pointDataComponents.insert(dataName, 3);
			readAnyVector = true;

			// 一个 VTK 文件可能有多个 VECTORS，例如 U 和 UR。
			// 继续读，直到遇到 CELL_DATA 或文件结束。
			continue;
		}
		else if (parts[0] == "SCALARS") {
			// 当前导入主线只恢复几何和向量字段，标量字段先跳过。
			// 继续读下一段内容。
		}
		else {
			// 遇到暂不支持的 section，继续向后扫描。
		}
	}

	return readAnyVector ? 0 : ERRORTYPE_FILE_READ_FAILED;
}

int VTKLegacyFormatReader::ReadCells30(int numCells) {

	data->elems.resize(numCells);

	for (int i = 0; i < numCells; ++i) {

		if (stream->atEnd()) return ERRORTYPE_WRONG_ELEMENT_DATA;
		bool ok;

		// 导出器为了文件更短，会在一行里写多个单元：
		// “2 0 1 2 1 2 3 2 3 4”。原代码按“一行一个单元”读，
		// 遇到这种合法 VTK 会读错，后续建网格时就可能崩溃。
		int count = 0;
		*stream >> count;
		if (stream->status() != QTextStream::Ok || count <= 0) {
			return ERRORTYPE_WRONG_ELEMENT_DATA;
		}
		
		// Copy element connectivity
		int* conn = (int*)malloc(sizeof(int) * count);
		if (!conn) return ERRORTYPE_MEMORY_ALLOC_FAILED;
		for (int j = 0; j < count; ++j) {
			int value = 0;
			*stream >> value;
			if (stream->status() != QTextStream::Ok) {
				free(conn);
				return ERRORTYPE_WRONG_ELEMENT_DATA;
			}
			conn[j] = value;
		}
		// Add to data container
		data->elems[i].dataSet = conn;
		++cellsRead;
	}
	return 0;
}

int VTKLegacyFormatReader::ReadCells51(int numOffsets) {
	data->elems.resize(numOffsets - 1);
	int* offsets = (int*)malloc(numOffsets * sizeof(int));
	if (!offsets) return ERRORTYPE_MEMORY_ALLOC_FAILED;
	while (!stream->atEnd()) {
		QString line = stream->readLine().trimmed();
		
		
		if (line.startsWith("OFFSETS")) {
			for (int i = 0; i < numOffsets; ++i) {
				*stream >> offsets[i];
			}
		}
		else if (line.startsWith("CONNECTIVITY")) {
			for (int i = 0; i < numOffsets - 1; ++i) {

				// Copy element connectivity
				int distance = offsets[i + 1] - offsets[i];
				int* conn = (int*)malloc(sizeof(int) * distance);
				if (!conn) return ERRORTYPE_MEMORY_ALLOC_FAILED;
				for (int j = 0; j < distance; ++j) {
					*stream >> conn[j];
				}
				// Add to data container
				data->elems[i].dataSet = conn;
				++cellsRead;
			}

		}
		else if (line.startsWith("CELL_TYPES")) {
			if (ReadCellTypes(numOffsets - 1))
				return ERRORTYPE_WRONG_ELEMENT_DATA;
		}
	}
	free(offsets);
	return 0;
}

int VTKLegacyFormatReader::ReadCellTypes(int numTypes) {
	data->elems.resize(numTypes);
	for (int i = 0; i < numTypes; ++i) {
		int type;
		*stream >> type;
		// Add to data container
		data->elems[i].type = (VTUElementHandler::VTKType)type;
	}
	return 0;
}
