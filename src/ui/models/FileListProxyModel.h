#pragma once

#include <QSortFilterProxyModel>

// Сортировка/фильтрация без изменения исходной модели.
class FileListProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit FileListProxyModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setDynamicSortFilter(true);
    }
};
