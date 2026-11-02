#include <iostream>

#include <QBuffer>
#include <QCoreApplication>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QByteArray>
#include <QLibraryInfo>

namespace {

bool check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

bool canDecode(const QByteArray& bytes, const char* format)
{
    QBuffer buffer;
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly))
    {
        return false;
    }
    QImageReader reader(&buffer);
    reader.setFormat(format);
    const QImage image = reader.read();
    if (image.isNull())
    {
        std::cerr << "Could not decode " << format << ": "
                  << reader.errorString().toStdString() << '\n';
        return false;
    }
    if (reader.format().compare(format, Qt::CaseInsensitive) != 0)
    {
        std::cerr << "Decoded image format was " << reader.format().toStdString()
                  << ", expected " << format << ".\n";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QString webpPluginRoot = qEnvironmentVariable("NLSI_QT_WEBP_PLUGIN_ROOT");
    if (!webpPluginRoot.isEmpty())
    {
        QCoreApplication::addLibraryPath(webpPluginRoot);
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::addLibraryPath(QLibraryInfo::path(QLibraryInfo::PluginsPath));
#else
    QCoreApplication::addLibraryPath(QLibraryInfo::location(QLibraryInfo::PluginsPath));
#endif

    bool ok = true;

    QImage source(4, 4, QImage::Format_RGB32);
    source.fill(Qt::cyan);
    QByteArray webp;
    QBuffer webpOutput(&webp);
    if (!webpOutput.open(QIODevice::WriteOnly))
    {
        std::cerr << "Could not open the WebP encoder buffer.\n";
        ok = false;
    }
    else
    {
        QImageWriter writer(&webpOutput, "webp");
        if (!writer.canWrite() || !writer.write(source))
        {
            std::cerr << "Could not encode the WebP test fixture: "
                      << writer.errorString().toStdString() << '\n';
            ok = false;
        }
        else
        {
            ok = check(canDecode(webp, "webp"),
                       "the installed Qt WebP plugin decodes a WebP image") && ok;
        }
    }

    for (const char* format : {"PNG", "JPEG"})
    {
        QByteArray encoded;
        QBuffer output(&encoded);
        if (!output.open(QIODevice::WriteOnly))
        {
            std::cerr << "Could not open the " << format << " encoder buffer.\n";
            ok = false;
        }
        else if (!source.save(&output, format))
        {
            std::cerr << "Could not encode " << format << " test image.\n";
            ok = false;
        }
        else if (!canDecode(encoded, format))
        {
            std::cerr << "FAILED: existing " << format
                      << " image support remains available\n";
            ok = false;
        }
    }

    return ok ? 0 : 1;
}
