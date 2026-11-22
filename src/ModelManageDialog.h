#pragma once

#include "ConfigDialog.h"

#include <QDialog>
#include <QFile>
#include <QHash>
#include <QModelIndex>
#include <QNetworkAccessManager>
#include <QSet>

#include "ui_modelmanagedialog.h"

class QNetworkReply;
class QSortFilterProxyModel;
class QTimer;
class QLabel;
class ModelListModel;

// 模型管理页面：从 Hugging Face 拉取最新模型列表、下载 GGUF 文件并设为当前模型。
// huggingface.co 直连失败时自动回退到 hf-mirror.com 镜像。
// 列表采用 model/view：ModelListModel（数据）+ QSortFilterProxyModel（排序）+ QTableView。
class ModelManageDialog : public QDialog {
    Q_OBJECT
public:
    explicit ModelManageDialog(const AppConfig & config, QWidget * parent = nullptr);

signals:
    // 下载完成（本地文件路径），主窗口会将其设为当前模型
    void modelDownloaded(const QString & localPath);

protected:
    void closeEvent(QCloseEvent * event) override;
    bool eventFilter(QObject * watched, QEvent * event) override;

private slots:
    void onRefresh();
    void onRowDoubleClicked(const QModelIndex & index);
    void onRowHovered(const QModelIndex & index);
    void onOpenPage();
    void onCopyId();
    void onDownloadGguf();
    void browseSaveDir();

private:
    QString apiHost(bool mirror) const;
    void fetchModels(bool useMirror);
    void handleModelsReply(QNetworkReply * reply, bool wasMirror);
    void fetchTree(bool useMirror);
    void handleTreeReply(QNetworkReply * reply, bool wasMirror);
    void startDownload(const QString & modelId, const QString & filePath,
                       bool useMirror = false, bool forceOverwrite = false);
    void setBusy(bool busy);
    void updateSelectionActions();

    // —— 悬停显示模型介绍（README 首段） ——
    QString hoverBaseHtml(const QModelIndex & index) const;
    void fetchDescription(const QString & id, bool useMirror);
    void handleDescriptionReply(QNetworkReply * reply, const QString & id, bool wasMirror);
    void showModelTooltip();
    QLabel * hoverTip(); // 自绘的不透明提示窗（QToolTip 在 macOS 下有系统级半透明）

    Ui::ModelManageDialog ui;
    QNetworkAccessManager m_nam;
    ModelListModel *        m_model = nullptr;
    QSortFilterProxyModel * m_proxy = nullptr;
    QString m_selectedId;
    bool m_downloading = false;
    QFile m_downloadFile;

    QTimer * m_descTimer = nullptr;   // hover 防抖
    QLabel * m_hoverTip = nullptr;    // 自绘悬停提示窗
    QString  m_hoverId;
    QString  m_hoverBase;             // 当前行的元信息 HTML
    bool     m_listMirror = false;    // 列表是在镜像站拉取成功的
    QHash<QString, QString> m_descCache;   // 模型介绍缓存（空串 = 无 README）
    QSet<QString>           m_descRequested;
};
