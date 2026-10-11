#pragma once

#include <QString>

namespace AppPaths {

void     ensureInitialized();
QString  configDir();
QString  logDir();
QString  logFilePath();
QString  defaultSourceDir();
QString  defaultTargetDir();

} // namespace AppPaths
