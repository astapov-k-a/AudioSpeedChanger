#pragma once

#include <QString>

namespace DurationFormat {

QString fromSeconds(double seconds);         // "00:03:24" или "—" при неизвестном
QString humanBytes(qint64 bytes);            // "34.2 MB"
QString etaFromSeconds(double seconds);      // "00:00:41"
} // namespace DurationFormat