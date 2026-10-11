#pragma once

#include <QString>
#include <QDir>

namespace FileUtils {

// Возвращает уникальный путь: если файл существует — добавляет " (1)", " (2)"...
QString uniquePath(const QString& desiredPath);

// Создаёт все промежуточные каталоги для файла.
bool ensureParentDirExists(const QString& filePath);

// Возвращает расширение без точки в нижнем регистре.
QString extensionLower(const QString& path);

// Есть ли dst внутри src (рекурсивно).
bool isSubPath(const QDir& parent, const QDir& child);

} // namespace FileUtils
