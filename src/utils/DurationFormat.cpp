#include "utils/DurationFormat.h"
#include <QtGlobal>

namespace DurationFormat {

QString fromSeconds(double seconds)
{
    if (seconds < 0.0 || !qIsFinite(seconds))
        return QStringLiteral("—");

    const qint64 total = static_cast<qint64>(seconds + 0.5);
    const qint64 h = total / 3600;
    const qint64 m = (total % 3600) / 60;
    const qint64 s = total % 60;

    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

QString etaFromSeconds(double seconds)
{
    if (seconds < 0.0 || !qIsFinite(seconds))
        return QStringLiteral("—");
    return fromSeconds(seconds);
}

QString humanBytes(qint64 bytes)
{
    static const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    double v = static_cast<double>(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; ++u; }
    return QString("%1 %2").arg(v, 0, 'f', u == 0 ? 0 : 1).arg(units[u]);
}

} // namespace DurationFormat