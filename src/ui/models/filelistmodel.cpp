#include "ui/models/FileListModel.h"
#include "utils/DurationFormat.h"

#include <QFileInfo>
#include <QBrush>
#include <QColor>

namespace {

QString statusToString(ProcessingStatus s)
{
    switch (s) {
    case ProcessingStatus::Pending:   return QObject::tr("Ожидание");
    case ProcessingStatus::Running:   return QObject::tr("Обработка");
    case ProcessingStatus::Done:      return QObject::tr("Готово");
    case ProcessingStatus::Failed:    return QObject::tr("Ошибка");
    case ProcessingStatus::Skipped:   return QObject::tr("Пропущено");
    case ProcessingStatus::Cancelled: return QObject::tr("Отменено");
    }
    return {};
}

} // namespace

FileListModel::FileListModel(QObject* parent) : QAbstractTableModel(parent) {}

void FileListModel::setFiles(const QList<AudioFileInfo>& files)
{
    beginResetModel();
    m_files = files;
    m_rows.clear();
    m_rows.reserve(files.size());
    for (const auto& f : files) {
        Row r;
        r.info = f;
        m_rows.push_back(r);
    }
    endResetModel();
}

void FileListModel::clear()
{
    beginResetModel();
    m_files.clear();
    m_rows.clear();
    endResetModel();
}

AudioFileInfo FileListModel::fileAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) return {};
    return m_rows[row].info;
}

QString FileListModel::displayNameAt(int row) const
{
    if (row < 0 || row >= m_rows.size()) return {};
    return QFileInfo(m_rows[row].info.filePath).fileName();
}

void FileListModel::setStatus(int row, ProcessingStatus status, const QString& detail)
{
    if (row < 0 || row >= m_rows.size()) return;
    m_rows[row].status = status;
    m_rows[row].detail = detail;
    if (status == ProcessingStatus::Done)
        m_rows[row].progress = 100;
    emit dataChanged(index(row, ColStatus), index(row, ColStatus),
                     { Qt::DisplayRole, Qt::ToolTipRole, Qt::ForegroundRole });
}

void FileListModel::setProgress(int row, int percent)
{
    if (row < 0 || row >= m_rows.size()) return;
    percent = qBound(0, percent, 100);
    if (m_rows[row].progress == percent) return;
    m_rows[row].progress = percent;
    emit dataChanged(index(row, ColProgress), index(row, ColProgress), { Qt::DisplayRole });
}

void FileListModel::setAllChecked(bool checked)
{
    if (m_rows.isEmpty()) return;
    for (auto& r : m_rows) r.checked = checked;
    emit dataChanged(index(0, ColChecked), index(m_rows.size() - 1, ColChecked),
                     { Qt::CheckStateRole });
}

QList<int> FileListModel::checkedRows() const
{
    QList<int> out;
    for (int i = 0; i < m_rows.size(); ++i)
        if (m_rows[i].checked) out.push_back(i);
    return out;
}

int FileListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_rows.size();
}

int FileListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant FileListModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.row() >= m_rows.size()) return {};

    const Row& r = m_rows[idx.row()];

    switch (role) {
    case Qt::CheckStateRole:
        if (idx.column() == ColChecked)
            return r.checked ? Qt::Checked : Qt::Unchecked;
        return {};

    case Qt::DisplayRole:
        switch (idx.column()) {
        case ColName:     return QFileInfo(r.info.filePath).fileName();
        case ColFormat:   return r.info.format.toUpper();
        case ColDuration: return DurationFormat::fromSeconds(r.info.durationSec);
        case ColSize:     return DurationFormat::humanBytes(r.info.sizeBytes);
        case ColStatus:   return statusToString(r.status);
        case ColProgress: return QString("%1%").arg(r.progress);
        }
        return {};

    case Qt::ToolTipRole:
        if (!r.detail.isEmpty()) return r.detail;
        return r.info.filePath;

    case Qt::ForegroundRole:
        switch (r.status) {
        case ProcessingStatus::Failed:    return QBrush(QColor("#c0392b"));
        case ProcessingStatus::Done:      return QBrush(QColor("#27ae60"));
        case ProcessingStatus::Skipped:   return QBrush(QColor("#7f8c8d"));
        case ProcessingStatus::Cancelled: return QBrush(QColor("#7f8c8d"));
        default: return {};
        }

    case Qt::TextAlignmentRole:
        if (idx.column() == ColDuration || idx.column() == ColSize || idx.column() == ColProgress)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        if (idx.column() == ColChecked)
            return int(Qt::AlignCenter);
        return {};

    case IdRole:
        return idx.row();
    }

    return {};
}

bool FileListModel::setData(const QModelIndex& idx, const QVariant& v, int role)
{
    if (!idx.isValid() || idx.row() >= m_rows.size()) return false;
    if (role == Qt::CheckStateRole && idx.column() == ColChecked) {
        m_rows[idx.row()].checked = (v.toInt() == Qt::Checked);
        emit dataChanged(idx, idx, { Qt::CheckStateRole });
        return true;
    }
    return false;
}

QVariant FileListModel::headerData(int section, Qt::Orientation o, int role) const
{
    if (role != Qt::DisplayRole) return {};
    if (o == Qt::Horizontal) {
        switch (section) {
        case ColChecked:  return QString();
        case ColName:     return tr("Имя");
        case ColFormat:   return tr("Формат");
        case ColDuration: return tr("Длительность");
        case ColSize:     return tr("Размер");
        case ColStatus:   return tr("Статус");
        case ColProgress: return tr("Прогресс");
        }
    }
    return QAbstractTableModel::headerData(section, o, role);
}

Qt::ItemFlags FileListModel::flags(const QModelIndex& idx) const
{
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (idx.column() == ColChecked) f |= Qt::ItemIsUserCheckable;
    return f;
}
