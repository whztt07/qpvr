﻿#include "Tests.h"

#include <QImageReader>
#include <QImageWriter>

#include <QImage>
#include <QPainter>

#include <QTemporaryDir>
#include <QDir>

#include <zlib.h>

#include <QtTest>
#include <QDebug>

struct PVRTests::Options
{
	int compression;
	int quality;
	int ver;
	QByteArray subType;
	QImageIOHandler::Transformations transform;
	QImage::Format resultFormat;

	static const Options OPTIONS[];
};

const PVRTests::Options PVRTests::Options::OPTIONS[] = {
	{ Z_NO_COMPRESSION, -1, 2, QByteArray(),
		QImageIOHandler::TransformationNone, QImage::Format_RGBA8888 },
	{ Z_NO_COMPRESSION, -1, 2, QByteArray(),
		QImageIOHandler::TransformationFlip, QImage::Format_RGBA8888 },
	{ Z_NO_COMPRESSION, -1, 3, QByteArray(),
		QImageIOHandler::TransformationMirror, QImage::Format_RGBA8888 },
	{ Z_DEFAULT_COMPRESSION, 80, 2, QByteArrayLiteral("pvrtc2_2"),
		QImageIOHandler::TransformationNone, QImage::Format_RGBA8888 },
	{ Z_NO_COMPRESSION, 50, 2, QByteArrayLiteral("pvrtc2_4"),
		QImageIOHandler::TransformationFlip, QImage::Format_RGBA8888 },
	{ Z_BEST_COMPRESSION, 0, 3, QByteArrayLiteral("pvrtc1_2"),
		QImageIOHandler::TransformationMirror, QImage::Format_RGBA8888 },
	{ Z_BEST_SPEED, 100, 3, QByteArrayLiteral("pvrtc1_4"),
		QImageIOHandler::TransformationRotate180, QImage::Format_RGBA8888 },
	{ Z_BEST_SPEED, 100, 2, QByteArrayLiteral("etc1"),
		QImageIOHandler::TransformationNone, QImage::Format_RGB888 },
	{ Z_BEST_COMPRESSION, 0, 3, QByteArrayLiteral("etc2"),
		QImageIOHandler::TransformationMirror, QImage::Format_RGBA8888 }
};

template <typename CLASS>
static void checkSupportedOptions(const CLASS &io)
{
	// supported options
	QVERIFY(io.supportsOption(QImageIOHandler::SubType));
	QVERIFY(io.supportsOption(QImageIOHandler::SupportedSubTypes));
	QVERIFY(io.supportsOption(QImageIOHandler::CompressionRatio));
	QVERIFY(io.supportsOption(QImageIOHandler::Size));
	QVERIFY(io.supportsOption(QImageIOHandler::ScaledSize));
	QVERIFY(io.supportsOption(QImageIOHandler::Quality));
	QVERIFY(io.supportsOption(QImageIOHandler::ImageFormat));
	QVERIFY(io.supportsOption(QImageIOHandler::ImageTransformation));
	// unsupported options
	QVERIFY(!io.supportsOption(QImageIOHandler::Gamma));
	QVERIFY(!io.supportsOption(QImageIOHandler::Animation));
	QVERIFY(!io.supportsOption(QImageIOHandler::IncrementalReading));
	QVERIFY(!io.supportsOption(QImageIOHandler::Endianness));
	QVERIFY(!io.supportsOption(QImageIOHandler::BackgroundColor));
	QVERIFY(!io.supportsOption(QImageIOHandler::OptimizedWrite));
	QVERIFY(!io.supportsOption(QImageIOHandler::ProgressiveScanWrite));
	QVERIFY(!io.supportsOption(QImageIOHandler::Name));
	QVERIFY(!io.supportsOption(QImageIOHandler::Description));
	QVERIFY(!io.supportsOption(QImageIOHandler::ClipRect));
	QVERIFY(!io.supportsOption(QImageIOHandler::ScaledClipRect));
}

static const QImage &fetchImage()
{
	static bool imageInitialized = false;

	static QImage image(32, 32, QImage::Format_RGBA8888);
	if (!imageInitialized)
	{
		imageInitialized = true;

		QPainter painter(&image);
		painter.fillRect(QRect(0, 0, 16, 16), QColor(Qt::red));
		painter.fillRect(QRect(16, 0, 16, 16), QColor(Qt::green));
		painter.fillRect(QRect(0, 16, 16, 16), QColor(Qt::blue));
		painter.fillRect(QRect(16, 16, 16, 16), QColor(Qt::transparent));
	}

	return image;
}

void PVRTests::testInstallation()
{
	QList<QList<QByteArray>> supported;
	supported.append(QImageReader::supportedImageFormats());
	supported.append(QImageWriter::supportedImageFormats());

	for (auto &s : supported)
	{
		QVERIFY(s.indexOf("pvr") >= 0);
		QVERIFY(s.indexOf("pvr.ccz") >= 0);
	}
}

void PVRTests::testIO_data()
{
	QTest::addColumn<bool>("compressed");

	QTest::newRow("pvr") << false;
	QTest::newRow("pvr.ccz") << true;
}

void PVRTests::testIO()
{
	QFETCH(bool, compressed);

	QTemporaryDir tempDir;
	tempDir.setAutoRemove(true);
	QVERIFY(tempDir.isValid());

	QDir dir(tempDir.path());

	for (const Options &options : Options::OPTIONS)
	{
		testWrite(options, dir, compressed);
		testRead(options, dir, compressed);
	}
}

void PVRTests::testWrite(
	const Options &options, const QDir &dir, bool compressed)
{
	QImageWriter writer;

	auto subType = getSubType(options.ver, options.subType);

	writer.setFileName(filePathForSubType(dir, subType, compressed));
	QVERIFY(writer.canWrite());
	checkSupportedOptions(writer);

	auto supportedSubTypes = writer.supportedSubTypes();

	if (compressed)
		subType.append(QByteArrayLiteral(".ccz"));

	QVERIFY(supportedSubTypes.indexOf(subType) >= 0);

	writer.setCompression(options.compression);
	writer.setQuality(options.quality);
	writer.setSubType(subType);
	writer.setTransformation(options.transform);

	QVERIFY(writer.write(fetchImage()));
}

void PVRTests::testRead(
	const Options &options, const QDir &dir, bool compressed)
{
	auto &image = fetchImage();

	QImageReader reader;

	auto subType = getSubType(options.ver, options.subType);

	reader.setFileName(filePathForSubType(dir, subType, compressed));
	QVERIFY(reader.canRead());
	checkSupportedOptions(reader);

#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 1)
	// QImageReader::supportedSubTypes always empty in previous versions
	{
		QImageWriter writer;
		writer.setFormat(QByteArrayLiteral("pvr"));
		QCOMPARE(reader.supportedSubTypes(), writer.supportedSubTypes());
	}
#endif
	QCOMPARE(reader.imageFormat(), options.resultFormat);
	QCOMPARE(reader.size(), image.size());
	QCOMPARE(reader.supportsAnimation(), false);
	QCOMPARE(reader.transformation(), options.transform);
	QCOMPARE(reader.loopCount(), 0);
	QCOMPARE(reader.imageCount(), 1);

	subType = getSubType(3, options.subType);

	if (compressed)
		subType.append(QByteArrayLiteral(".ccz"));

	QCOMPARE(reader.subType(), subType);

	QImage readImage;

	QVERIFY(reader.read(&readImage));

	QCOMPARE(readImage.size(), image.size());
	QCOMPARE(readImage.format(), options.resultFormat);

	reader.setScaledSize(QSize(16, 16));

	QVERIFY(reader.read(&readImage));
	QCOMPARE(readImage.size(), reader.scaledSize());
	QCOMPARE(readImage.format(), options.resultFormat);
}

QByteArray PVRTests::getSubType(int ver, const QByteArray &str)
{
	auto subType = QByteArrayLiteral("pvr") + QByteArray().setNum(ver);

	if (!str.isEmpty())
		subType += '.' + str;

	return subType;
}

QString PVRTests::filePathForSubType(
	const QDir &dir, const QByteArray &subType, bool compressed)
{
	return dir.filePath(QString::fromLatin1(subType +
		(compressed ? QByteArrayLiteral(".pvr.ccz")
					: QByteArrayLiteral(".pvr"))));
}
