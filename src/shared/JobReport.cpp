#include "JobReport.h"

#include <QFile>
#include <QFileInfo>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QSaveFile>
#include <QTextDocument>

namespace nlsi {

bool writePdfReport(const QString& filePath, const QString& html, QString* error)
{
    if (error)
    {
        error->clear();
    }
    QSaveFile outputFile(filePath);
    if (!outputFile.open(QIODevice::WriteOnly))
    {
        if (error)
        {
            *error = QString("Cannot create PDF report %1: %2")
                .arg(filePath, outputFile.errorString());
        }
        return false;
    }
    {
        QPdfWriter writer(&outputFile);
        writer.setResolution(96);
        writer.setPageSize(QPageSize(QPageSize::A4));
        QTextDocument document;
        document.setHtml(html);
        document.setPageSize(writer.pageLayout().paintRect(QPageLayout::Point).size());
        document.print(&writer);
    }

    if (!outputFile.commit())
    {
        if (error)
        {
            *error = QString("Cannot atomically save PDF report %1: %2")
                .arg(filePath, outputFile.errorString());
        }
        return false;
    }
    const QFileInfo outputInfo(filePath);
    QFile file(filePath);
    if (!outputInfo.exists() || outputInfo.size() <= 0 || !file.open(QIODevice::ReadOnly))
    {
        if (error)
        {
            *error = QString("Could not create PDF report %1.").arg(filePath);
        }
        return false;
    }
    if (!file.read(5).startsWith("%PDF-"))
    {
        if (error)
        {
            *error = QString("Generated report is not a valid PDF: %1").arg(filePath);
        }
        return false;
    }
    return true;
}

} // namespace nlsi
