#include "ModelManageDialog.h"
#include "MacVibrancy.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidgetItem>
#include <QTimer>
#include <QUrlQuery>
#include <QDebug>

#include <algorithm>

// 下载量/点赞量友好格式：1.2M / 345K / 890
static QString formatCount(long n) {
    if (n >= 1000000) {
        return QStringLiteral("%1M").arg(n / 1000000.0, 0, 'f', 1);
    }
    if (n >= 1000) {
        return QStringLiteral("%1K").arg(n / 1000.0, 0, 'f', 1);
    }
    return QString::number(n);
}

// 文件大小友好格式
static QString formatBytes(qint64 n) {
    if (n >= 1073741824) {
        return QStringLiteral("%1 GB").arg(n / 1073741824.0, 0, 'f', 2);
    }
    if (n >= 1048576) {
        return QStringLiteral("%1 MB").arg(n / 1048576.0, 0, 'f', 1);
    }
    if (n >= 1024) {
        return QStringLiteral("%1 KB").arg(n / 1024.0, 0, 'f', 0);
    }
    return QStringLiteral("%1 B").arg(n);
}

// 数字列：存真实数值用于正确排序，显示格式化文本
static QTableWidgetItem * numericItem(long value) {
    auto * item = new QTableWidgetItem;
    item->setData(Qt::DisplayRole, (qlonglong) value);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

ModelManageDialog::ModelManageDialog(const AppConfig & config, QWidget * parent) : QDialog(parent) {
    ui.setupUi(this); // 界面布局见 ui/modelmanagedialog.ui（Qt Designer 编辑）
    macSetupVibrancy(this, config.vibrancy);
    if (!config.vibrancy) {
        setStyleSheet("QDialog { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                      "stop:0 #f2f4fa, stop:1 #e4e8f4); }");
    }

    // 表格列宽策略
    ui.table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i < ui.table->columnCount(); i++) {
        ui.table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }

    ui.searchEdit->setClearButtonEnabled(true);
    ui.refreshBtn->setIcon(QIcon(":/assets/icons/play-circle.svg"));
    ui.downloadBtn->setIcon(QIcon(":/assets/icons/download.svg"));
    ui.openBtn->setIcon(QIcon(":/assets/icons/message-circle.svg"));
    ui.closeBtn->setIcon(QIcon(":/assets/icons/x-circle.svg"));
    ui.progressBar->setVisible(false);

    // 默认保存目录：~/Documents/agents/models（可在界面上修改）
    ui.saveDirEdit->setText(QDir::home().filePath("Documents/agents/models"));
    ui.saveDirEdit->setClearButtonEnabled(true);
    ui.browseSaveDirBtn->setIcon(QIcon(":/assets/icons/folder.svg"));
    connect(ui.browseSaveDirBtn, &QPushButton::clicked, this, &ModelManageDialog::browseSaveDir);

    connect(ui.refreshBtn, &QPushButton::clicked, this, &ModelManageDialog::onRefresh);
    connect(ui.table, &QTableWidget::cellDoubleClicked, this, &ModelManageDialog::onRowDoubleClicked);
    connect(ui.table, &QTableWidget::itemSelectionChanged, this, [this] {
        const int row = ui.table->currentRow();
        m_selectedId = row >= 0 ? ui.table->item(row, 0)->text() : QString();
        const bool has = !m_selectedId.isEmpty();
        ui.openBtn->setEnabled(has);
        ui.copyBtn->setEnabled(has);
        ui.downloadBtn->setEnabled(has);
    });
    connect(ui.openBtn, &QPushButton::clicked, this, &ModelManageDialog::onOpenPage);
    connect(ui.copyBtn, &QPushButton::clicked, this, &ModelManageDialog::onCopyId);
    connect(ui.downloadBtn, &QPushButton::clicked, this, &ModelManageDialog::onDownloadGguf);
    connect(ui.closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    ui.openBtn->setEnabled(false);
    ui.copyBtn->setEnabled(false);
    ui.downloadBtn->setEnabled(false);

    // 打开页面即拉取一次
    QTimer::singleShot(200, this, &ModelManageDialog::onRefresh);

    // 调试：QLLAMA_TEST_DOWNLOAD="仓库id|文件名" 时自动下载该文件（测试下载管线），
    // QLLAMA_TEST_SAVEDIR 可指定保存目录
    const QByteArray testSaveDir = qgetenv("QLLAMA_TEST_SAVEDIR");
    if (!testSaveDir.isEmpty()) {
        ui.saveDirEdit->setText(QString::fromUtf8(testSaveDir));
    }
    const QByteArray test = qgetenv("QLLAMA_TEST_DOWNLOAD");
    const int sep = test.indexOf('|');
    if (sep > 0) {
        const QString testId = QString::fromUtf8(test.left(sep));
        const QString testFile = QString::fromUtf8(test.mid(sep + 1));
        QTimer::singleShot(600, this, [this, testId, testFile] {
            startDownload(testId, testFile);
        });
    }
}

QString ModelManageDialog::apiHost(bool mirror) const {
    return mirror ? "https://hf-mirror.com" : "https://huggingface.co";
}

void ModelManageDialog::showEvent(QShowEvent * event) {
    QDialog::showEvent(event);
    static bool vibrancyDone = false;
    if (!vibrancyDone) {
        vibrancyDone = true;
        macEnableVibrancy(this);
    }
}

// 下载期间禁止关闭窗口（否则会中止下载留下半截文件）
void ModelManageDialog::closeEvent(QCloseEvent * event) {
    if (m_downloading) {
        event->ignore();
        ui.statusLabel->setText("正在下载，请等待完成或取消……");
        return;
    }
    QDialog::closeEvent(event);
}

// 下载/查询期间禁用交互
void ModelManageDialog::setBusy(bool busy) {
    ui.refreshBtn->setEnabled(!busy);
    ui.downloadBtn->setEnabled(!busy && !m_selectedId.isEmpty());
    ui.searchEdit->setEnabled(!busy);
    ui.countSpin->setEnabled(!busy);
}

void ModelManageDialog::onRefresh() {
    fetchModels(false);
}

void ModelManageDialog::fetchModels(bool useMirror) {
    const QString host = apiHost(useMirror);
    QUrl url(host + "/api/models");
    QUrlQuery query;
    query.addQueryItem("sort", "lastModified");
    query.addQueryItem("direction", "-1");
    query.addQueryItem("limit", QString::number(ui.countSpin->value()));
    const QString search = ui.searchEdit->text().trimmed();
    if (!search.isEmpty()) {
        query.addQueryItem("search", search);
    }
    url.setQuery(query);

    ui.statusLabel->setText(useMirror ? "官方站不可达，正在通过镜像 hf-mirror.com 拉取…"
                                      : "正在从 Hugging Face 拉取最新模型…");
    ui.refreshBtn->setEnabled(false);

    QNetworkRequest req(url);
    req.setTransferTimeout(15000);
    QNetworkReply * reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, useMirror] {
        handleModelsReply(reply, useMirror);
    });
}

void ModelManageDialog::handleModelsReply(QNetworkReply * reply, bool wasMirror) {
    reply->deleteLater();
    ui.refreshBtn->setEnabled(true);

    if (reply->error() != QNetworkReply::NoError) {
        if (!wasMirror) {
            fetchModels(true); // 官方站失败（本机常见），自动回退镜像
            return;
        }
        ui.statusLabel->setText("拉取失败：" + reply->errorString());
        return;
    }

    const QByteArray data = reply->readAll();
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isArray()) {
        ui.statusLabel->setText("数据解析失败");
        return;
    }

    const QJsonArray arr = doc.array();
    ui.table->setRowCount(arr.size());
    ui.table->setSortingEnabled(false); // 填充时暂停排序，避免行错乱
    for (int i = 0; i < arr.size(); i++) {
        const QJsonObject obj = arr[i].toObject();
        auto * idItem = new QTableWidgetItem(obj["id"].toString());
        idItem->setToolTip(obj["id"].toString());
        ui.table->setItem(i, 0, idItem);

        const QString tag = obj["pipeline_tag"].toString();
        auto * tagItem = new QTableWidgetItem(tag.isEmpty() ? QStringLiteral("—") : tag);
        tagItem->setTextAlignment(Qt::AlignCenter);
        ui.table->setItem(i, 1, tagItem);

        ui.table->setItem(i, 2, numericItem((long) obj["downloads"].toDouble()));
        ui.table->setItem(i, 3, numericItem((long) obj["likes"].toDouble()));
        ui.table->setItem(i, 4, new QTableWidgetItem(obj["lastModified"].toString().left(10)));
    }
    ui.table->setSortingEnabled(true);

    ui.statusLabel->setText(QStringLiteral("已拉取 %1 条模型（按最近更新排序）%2")
                                .arg(arr.size())
                                .arg(wasMirror ? QStringLiteral(" · 经镜像 hf-mirror.com") : QString()));

    // 调试：QLLAMA_DUMP_GEOMETRY=1 时输出前几条模型
    if (qEnvironmentVariableIsSet("QLLAMA_DUMP_GEOMETRY")) {
        for (int i = 0; i < std::min<int>(5, arr.size()); i++) {
            qInfo().noquote() << "HFMODEL:" << arr[i].toObject()["id"].toString()
                              << "downloads:" << arr[i].toObject()["downloads"].toDouble();
        }
    }
}

void ModelManageDialog::onDownloadGguf() {
    if (m_selectedId.isEmpty() || m_downloading) {
        return;
    }
    fetchTree(false);
}

// 查询仓库文件树，找 GGUF 文件
void ModelManageDialog::fetchTree(bool useMirror) {
    const QString host = apiHost(useMirror);
    QUrl url(host + "/api/models/" + m_selectedId + "/tree/main");
    ui.statusLabel->setText("正在查询仓库文件列表…");
    setBusy(true);

    QNetworkRequest req(url);
    req.setTransferTimeout(15000);
    QNetworkReply * reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, useMirror] {
        handleTreeReply(reply, useMirror);
    });
}

void ModelManageDialog::handleTreeReply(QNetworkReply * reply, bool wasMirror) {
    reply->deleteLater();
    setBusy(false);

    if (reply->error() != QNetworkReply::NoError) {
        if (!wasMirror) {
            fetchTree(true); // 回退镜像
            return;
        }
        ui.statusLabel->setText("查询文件列表失败：" + reply->errorString());
        return;
    }

    const QByteArray data = reply->readAll();
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isArray()) {
        ui.statusLabel->setText("文件列表解析失败");
        return;
    }

    // 收集 GGUF 文件（含子目录路径），按大小降序
    struct GgufFile {
        QString path;
        qint64 size = 0;
    };
    QVector<GgufFile> ggufs;
    const QJsonArray arr = doc.array();
    for (const auto & v : arr) {
        const QJsonObject f = v.toObject();
        const QString path = f["path"].toString();
        if (f["type"].toString() == "file" && path.toLower().endsWith(".gguf")) {
            ggufs.append({path, (qint64) f["size"].toDouble()});
        }
    }
    std::sort(ggufs.begin(), ggufs.end(), [](const GgufFile & a, const GgufFile & b) {
        return a.size > b.size;
    });

    if (ggufs.isEmpty()) {
        ui.statusLabel->setText("该仓库没有 GGUF 文件（试试搜索 gguf 找量化仓库）");
        return;
    }

    if (qEnvironmentVariableIsSet("QLLAMA_DUMP_GEOMETRY")) {
        for (const auto & g : ggufs) {
            qInfo().noquote() << "HFGGUF:" << g.path << formatBytes(g.size);
        }
    }

    // 让用户挑选文件（最多列 20 个）
    QStringList choices;
    for (int i = 0; i < std::min<int>(20, ggufs.size()); i++) {
        choices << QStringLiteral("%1  （%2）").arg(ggufs[i].path, formatBytes(ggufs[i].size));
    }
    bool ok = false;
    const QString choice = QInputDialog::getItem(this, "选择要下载的 GGUF 文件",
        m_selectedId, choices, 0, false, &ok);
    if (!ok || choice.isEmpty()) {
        return;
    }
    const int idx = choices.indexOf(choice);
    if (idx < 0) {
        return;
    }
    startDownload(m_selectedId, ggufs[idx].path);
}

// 选择保存目录
void ModelManageDialog::browseSaveDir() {
    const QString dir = QFileDialog::getExistingDirectory(this, "选择模型保存目录",
        ui.saveDirEdit->text().isEmpty() ? QDir::homePath() : ui.saveDirEdit->text());
    if (!dir.isEmpty()) {
        ui.saveDirEdit->setText(dir);
    }
}

void ModelManageDialog::startDownload(const QString & modelId, const QString & filePath,
                                      bool useMirror, bool forceOverwrite) {
    // 目标目录：<用户选择的保存目录>/<仓库名>/
    const QString repoName = modelId.section('/', -1);
    QDir modelsDir(ui.saveDirEdit->text().trimmed());
    if (modelsDir.path().isEmpty()) {
        modelsDir = QDir(QDir::home().filePath("Documents/agents/models")); // 空则回退默认
    }
    const QString targetDir = modelsDir.filePath(repoName);
    if (!modelsDir.mkpath(repoName)) {
        ui.statusLabel->setText("无法创建目录：" + targetDir);
        return;
    }
    const QString fileName = QFileInfo(filePath).fileName();
    const QString localPath = QDir(targetDir).filePath(fileName);

    if (qEnvironmentVariableIsSet("QLLAMA_DUMP_GEOMETRY")) {
        qInfo().noquote() << "DLDIR: saveDirEdit='" << ui.saveDirEdit->text()
                          << "' targetDir='" << targetDir << "' localPath='" << localPath << "'";
    }

    if (QFileInfo::exists(localPath) && !forceOverwrite) {
        const auto answer = QMessageBox::question(this, "文件已存在",
            localPath + "\n已存在，是否覆盖？", QMessageBox::Yes | QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    // 下载 URL（保留路径斜杠，其余转义）
    const QString host = apiHost(useMirror);
    const QString urlPath = QStringLiteral("/%1/resolve/main/%2").arg(modelId, filePath);
    const QUrl url(host + QUrl::toPercentEncoding(urlPath, "/"));

    m_downloadFile.setFileName(localPath);
    if (!m_downloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        ui.statusLabel->setText("无法写入文件：" + localPath);
        return;
    }

    m_downloading = true;
    setBusy(true);
    ui.progressBar->setVisible(true);
    ui.progressBar->setRange(0, 100);
    ui.progressBar->setValue(0);
    ui.statusLabel->setText(QStringLiteral("开始下载：%1%2").arg(
        fileName, useMirror ? QStringLiteral("（经镜像）") : QString()));

    QNetworkRequest req(url);
    // 30 秒无数据传输即判失败（正常下载数据持续流动，不会误触；
    // 但官方站连不通时能快速失败并切镜像重试）
    req.setTransferTimeout(30000);
    QNetworkReply * reply = m_nam.get(req);
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        m_downloadFile.write(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, fileName](qint64 received, qint64 total) {
        if (total > 0) {
            ui.progressBar->setRange(0, 1000);
            ui.progressBar->setValue((int) (received * 1000 / total));
        }
        ui.statusLabel->setText(QStringLiteral("下载中 %1 / %2 · %3")
                                    .arg(formatBytes(received), formatBytes(total), fileName));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, modelId, filePath, useMirror, localPath] {
        // 收尾读一次剩余数据
        m_downloadFile.write(reply->readAll());
        m_downloadFile.close();

        if (reply->error() != QNetworkReply::NoError) {
            if (!useMirror) {
                // 官方站失败（本机常见），清掉残片改用镜像重试
                QFile::remove(localPath);
                reply->deleteLater();
                ui.statusLabel->setText("官方站不可达，改用镜像 hf-mirror.com 重试…");
                startDownload(modelId, filePath, true, true);
                return;
            }
            m_downloading = false;
            setBusy(false);
            ui.progressBar->setVisible(false);
            ui.statusLabel->setText(QStringLiteral("下载失败：%1（已删除不完整文件）")
                                        .arg(reply->errorString()));
            QFile::remove(localPath);
        } else {
            m_downloading = false;
            setBusy(false);
            ui.progressBar->setVisible(false);
            ui.statusLabel->setText("下载完成：" + localPath);
            qInfo().noquote() << "HF 下载完成:" << localPath;
            emit modelDownloaded(localPath);
            accept(); // 完成后关闭页面，回到主窗口加载模型
        }
        reply->deleteLater();
    });
}

void ModelManageDialog::onRowDoubleClicked(int row, int) {
    const QString id = ui.table->item(row, 0)->text();
    if (!id.isEmpty()) {
        QDesktopServices::openUrl(QUrl("https://huggingface.co/" + id));
    }
}

void ModelManageDialog::onOpenPage() {
    if (!m_selectedId.isEmpty()) {
        QDesktopServices::openUrl(QUrl("https://huggingface.co/" + m_selectedId));
    }
}

void ModelManageDialog::onCopyId() {
    if (!m_selectedId.isEmpty()) {
        QApplication::clipboard()->setText(m_selectedId);
        ui.statusLabel->setText("已复制模型 ID：" + m_selectedId);
    }
}
