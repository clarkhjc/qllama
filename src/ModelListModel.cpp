#include "ModelListModel.h"
#include "I18n.h"

namespace {

// 下载量/点赞量友好格式：1.2M / 345K / 890
QString formatCount(qlonglong n) {
    if (n >= 1000000) {
        return QStringLiteral("%1M").arg(n / 1000000.0, 0, 'f', 1);
    }
    if (n >= 1000) {
        return QStringLiteral("%1K").arg(n / 1000.0, 0, 'f', 1);
    }
    return QString::number(n);
}

} // namespace

ModelListModel::ModelListModel(QObject * parent) : QAbstractTableModel(parent) {}

int ModelListModel::rowCount(const QModelIndex & parent) const {
    return parent.isValid() ? 0 : m_models.size();
}

int ModelListModel::columnCount(const QModelIndex & parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ModelListModel::data(const QModelIndex & index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_models.size()) {
        return {};
    }
    const ModelEntry & e = m_models.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case IdColumn:        return e.id;
        case TagColumn:       return e.pipelineTag.isEmpty() ? QStringLiteral("—") : e.pipelineTag;
        case DownloadsColumn: return formatCount(e.downloads);
        case LikesColumn:     return formatCount(e.likes);
        case UpdatedColumn:   return e.lastModified;
        default:              break;
        }
        break;
    case RawValueRole: // 排序用原始值
        if (index.column() == DownloadsColumn) {
            return QVariant::fromValue<qlonglong>(e.downloads);
        }
        if (index.column() == LikesColumn) {
            return QVariant::fromValue<qlonglong>(e.likes);
        }
        break;
    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case TagColumn:
            return QVariant(Qt::AlignCenter);
        case DownloadsColumn:
        case LikesColumn:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    default:
        break;
    }
    return {};
}

QVariant ModelListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
    case IdColumn:        return I18n::t("模型 ID");
    case TagColumn:       return I18n::t("类型");
    case DownloadsColumn: return I18n::t("下载量");
    case LikesColumn:     return I18n::t("点赞");
    case UpdatedColumn:   return I18n::t("更新时间");
    default:              return {};
    }
}

void ModelListModel::setModels(const QVector<ModelEntry> & models) {
    beginResetModel();
    m_models = models;
    endResetModel();
}

QString ModelListModel::idAt(int row) const {
    return (row >= 0 && row < m_models.size()) ? m_models.at(row).id : QString();
}
