#pragma once

#include <QString>

namespace nlsi {

bool writePdfReport(const QString& filePath, const QString& html, QString* error = nullptr);

} // namespace nlsi
