#pragma once

#include "ConfigDialog.h"

#include <QDialog>
#include <QFile>
#include <QNetworkAccessManager>

#include "ui_modelmanagedialog.h"

class QNetworkReply;

// 模型管理页面：从 Hugging Face 拉取最新模型列表、下载 GGUF 文件并设为当前模型。
// huggingface.co 直连失败时自动回退到 hf-mirror.com 镜像。
class ModelManageDialog : public QDialog {
    Q_OBJECT
public:
    explicit ModelManageDialog(const AppConfig & config, QWidget * parent = nullptr);

signals:
    // 下载完成（本地文件路径），主窗口会将其设为当前模型
    void modelDownloaded(const QString & localPath);

protected:
    void showEvent(QShowEvent * event) override;
    void closeEvent(QCloseEvent * event) override;

private slots:
    void onRefresh();
    void onRowDoubleClicked(int row, int column);
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

    Ui::ModelManageDialog ui;
    QNetworkAccessManager m_nam;
    QString m_selectedId;
    bool m_downloading = false;
    QFile m_downloadFile;
};
