#pragma once

#include <QAbstractTableModel>
#include <QList>


#include "core/Core.h"

class FileListModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        ColChecked = 0,
        ColName,
        ColFormat,
        ColDuration,
        ColSize,
        ColStatus,
        ColProgress,
        ColumnCount,
    };

    enum Role {
        IdRole = Qt::UserRole + 1,
    };

    explicit FileListModel(QObject* parent = nullptr);

    void setFiles(const QList<AudioFileInfo>& files);
    void clear();

    const QList<AudioFileInfo>& files() const { return m_files; }
    AudioFileInfo fileAt(int row) const;
    QString displayNameAt(int row) const;

    void setStatus(int row, ProcessingStatus status, const QString& detail = {});
    void setProgress(int row, int percent);
    void setAllChecked(bool checked);
    QList<int> checkedRows() const;

    // QAbstractTableModel
    int      rowCount(const QModelIndex& parent = {}) const override;
    int      columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    bool     setData(const QModelIndex& idx, const QVariant& v, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation o, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& idx) const override;

private:
    struct Row {
        AudioFileInfo    info;
        bool             checked  = true;
        ProcessingStatus status   = ProcessingStatus::Pending;
        int              progress = 0;
        QString          detail;
    };

    QList<AudioFileInfo> m_files;   // источник истины (для передачи в AudioSpeedChanger)
    QList<Row>           m_rows;
};
