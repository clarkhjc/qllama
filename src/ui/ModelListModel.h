#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

// Hugging Face 模型条目
struct ModelEntry {
    QString id;
    QString pipelineTag;
    qlonglong downloads = 0;
    qlonglong likes = 0;
    QString lastModified; // YYYY-MM-DD
};

// 模型列表数据模型（配合 QTableView + QSortFilterProxyModel 使用）。
// 数字列在显示时为 1.2M / 345K 形式，排序使用 RawValueRole 的原始值。
class ModelListModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        IdColumn = 0,
        TagColumn,
        DownloadsColumn,
        LikesColumn,
        UpdatedColumn,
        ColumnCount,
    };
    enum Role {
        RawValueRole = Qt::UserRole + 1, // 数字列原始值（排序用）
    };

    explicit ModelListModel(QObject * parent = nullptr);

    int      rowCount(const QModelIndex & parent = QModelIndex()) const override;
    int      columnCount(const QModelIndex & parent = QModelIndex()) const override;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void setModels(const QVector<ModelEntry> & models);
    QString idAt(int row) const;

private:
    QVector<ModelEntry> m_models;
};
