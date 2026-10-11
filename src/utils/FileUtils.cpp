#include "utils/FileUtils.h"
#include <QFileInfo>

namespace FileUtils {

QString uniquePath(const QString& desiredPath)
{
    QFileInfo fi(desiredPath);
    if (!fi.exists())
        return desiredPath;

    const QString dir  = fi.absolutePath();
    const QString base = fi.completeBaseName();
    const QString ext  = fi.suffix().isEmpty() ? QString() : "." + fi.suffix();

    int counter = 1;
    QString candidate;
    do {
        candidate = QString("%1/%2 (%3)%4").arg(dir, base).arg(counter).arg(ext);
        ++counter;
    } while (QFileInfo::exists(candidate));

    return candidate;
}

bool ensureParentDirExists(const QString& filePath)
{
    const QFileInfo fi(filePath);
    QDir d = fi.absoluteDir();
    if (d.exists())
        return true;
    return d.mkpath(".");
}

QString extensionLower(const QString& path)
{
    return QFileInfo(path).suffix().toLower();
}

bool isSubPath(const QDir& parent, const QDir& child)
{
    const QString p = QDir::cleanPath(parent.absolutePath());
    const QString c = QDir::cleanPath(child.absolutePath());
    return c == p || c.startsWith(p + '/');
}

} // namespace FileUtils
