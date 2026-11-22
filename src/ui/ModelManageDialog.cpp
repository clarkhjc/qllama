#include "ui/ModelManageDialog.h"
#include "common/I18n.h"
#include "ui/ModelListModel.h"
#include "common/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QTimer>
#include <QUrlQuery>
#include <QDebug>

#include <algorithm>

namespace {

// 排序代理：数字列（下载量/点赞）按原始值排序，其余列按显示文本排序
class ModelSortProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool lessThan(const QModelIndex & left, const QModelIndex & right) const override {
        const int col = left.column();
        if (col == ModelListModel::DownloadsColumn || col == ModelListModel::LikesColumn) {
            return left.data(ModelListModel::RawValueRole).toLongLong()
                 < right.data(ModelListModel::RawValueRole).toLongLong();
        }
        return QSortFilterProxyModel::lessThan(left, right);
    }
};

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

// 从 README 提取一段纯文本介绍：去 YAML front matter / HTML 注释 / 图片 /
// Markdown 标记，压缩空白，截断到约 280 字。
static QString readmeSummary(QString md) {
    if (md.startsWith(QStringLiteral("---"))) {
        const int end = md.indexOf(QStringLiteral("\n---"), 3);
        if (end >= 0) {
            const int nl = md.indexOf('\n', end + 4);
            md = nl >= 0 ? md.mid(nl + 1) : QString();
        }
    }
    static const QRegularExpression reComment("<!--.*?-->",
                                              QRegularExpression::DotMatchesEverythingOption);
    md.remove(reComment);
    static const QRegularExpression reImage(R"(!\[[^\]]*\]\([^)]*\))");
    md.remove(reImage);
    static const QRegularExpression reLink(R"(\[([^\]]*)\]\([^)]*\))");
    md.replace(reLink, QStringLiteral("\\1"));
    static const QRegularExpression reHtml("<[^>]+>");
    md.remove(reHtml);
    static const QRegularExpression reHeading(R"(^\s{0,3}#{1,6}\s*)",
                                              QRegularExpression::MultilineOption);
    md.remove(reHeading);
    static const QRegularExpression reBullet(R"(^\s*[-*+]\s+)",
                                             QRegularExpression::MultilineOption);
    md.remove(reBullet);
    md.remove(QLatin1Char('`'));
    md.remove(QStringLiteral("**"));
    md.remove(QStringLiteral("__"));
    static const QRegularExpression reSpaces("\\s+");
    md.replace(reSpaces, QStringLiteral(" "));
    md = md.trimmed();
    if (md.size() > 280) {
        md = md.left(280) + QStringLiteral("…");
    }
    return md;
}

} // namespace

ModelManageDialog::ModelManageDialog(const AppConfig & config, QWidget * parent) : QDialog(parent) {
    ui.setupUi(this); // 界面布局见 ui/modelmanagedialog.ui（Qt Designer 编辑）

    // 列表：model/view（数据模型 + 排序代理 + QTableView）
    m_model = new ModelListModel(this);
    m_proxy = new ModelSortProxy(this);
    m_proxy->setSourceModel(m_model);
    ui.table->setModel(m_proxy);
    ui.table->setSortingEnabled(true);
    ui.table->sortByColumn(ModelListModel::UpdatedColumn, Qt::DescendingOrder); // 默认按最近更新
    ui.table->verticalHeader()->setVisible(false);
    ui.table->horizontalHeader()->setSectionResizeMode(ModelListModel::IdColumn, QHeaderView::Stretch);
    for (int i = 1; i < ModelListModel::ColumnCount; i++) {
        ui.table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }

    // 悬停行 → 显示元信息并拉取 README 介绍（300ms 防抖，结果缓存）
    ui.table->viewport()->setMouseTracking(true);
    ui.table->viewport()->installEventFilter(this);
    connect(ui.table, &QTableView::entered, this, &ModelManageDialog::onRowHovered);
    m_descTimer = new QTimer(this);
    m_descTimer->setSingleShot(true);
    m_descTimer->setInterval(300);
    connect(m_descTimer, &QTimer::timeout, this, [this] {
        if (m_hoverId.isEmpty()) {
            return;
        }
        const auto it = m_descCache.constFind(m_hoverId);
        if (it != m_descCache.constEnd()) {
            showModelTooltip(); // 已缓存
            return;
        }
        if (!m_descRequested.contains(m_hoverId)) {
            fetchDescription(m_hoverId, m_listMirror);
        }
    });

    ui.searchEdit->setClearButtonEnabled(true);
    ui.refreshBtn->setIcon(Theme::icon("play-circle"));
    ui.downloadBtn->setIcon(Theme::icon("download"));
    ui.openBtn->setIcon(Theme::icon("message-circle"));
    ui.closeBtn->setIcon(Theme::icon("x-circle"));
    ui.progressBar->setVisible(false);

    // 默认保存目录：~/Documents/agents/models（可在界面上修改）
    ui.saveDirEdit->setText(QDir::home().filePath("Documents/agents/models"));
    ui.saveDirEdit->setClearButtonEnabled(true);
    ui.browseSaveDirBtn->setIcon(Theme::icon("folder"));
    connect(ui.browseSaveDirBtn, &QPushButton::clicked, this, &ModelManageDialog::browseSaveDir);

    connect(ui.refreshBtn, &QPushButton::clicked, this, &ModelManageDialog::onRefresh);
    connect(ui.table, &QTableView::doubleClicked, this, &ModelManageDialog::onRowDoubleClicked);
    connect(ui.table->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex &, const QModelIndex &) { updateSelectionActions(); });
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

    I18n::retranslateTree(this); // .ui 静态文字按当前语言翻译
}

QString ModelManageDialog::apiHost(bool mirror) const {
    return mirror ? "https://hf-mirror.com" : "https://huggingface.co";
}

// 下载期间禁止关闭窗口（否则会中止下载留下半截文件）
void ModelManageDialog::closeEvent(QCloseEvent * event) {
    if (m_downloading) {
        event->ignore();
        ui.statusLabel->setText(I18n::t("正在下载，请等待完成或取消……"));
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

// 根据当前选中行更新按钮可用状态与 m_selectedId
void ModelManageDialog::updateSelectionActions() {
    const QModelIndex current = ui.table->currentIndex();
    m_selectedId = current.isValid()
                       ? current.sibling(current.row(), ModelListModel::IdColumn).data().toString()
                       : QString();
    const bool has = !m_selectedId.isEmpty();
    ui.openBtn->setEnabled(has);
    ui.copyBtn->setEnabled(has);
    ui.downloadBtn->setEnabled(has && !m_downloading);
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

    ui.statusLabel->setText(useMirror ? I18n::t("官方站不可达，正在通过镜像 hf-mirror.com 拉取…")
                                      : I18n::t("正在从 Hugging Face 拉取最新模型…"));
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
        ui.statusLabel->setText(I18n::t("拉取失败：%1").arg(reply->errorString()));
        return;
    }

    const QByteArray data = reply->readAll();
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isArray()) {
        ui.statusLabel->setText(I18n::t("数据解析失败"));
        return;
    }

    const QJsonArray arr = doc.array();
    QVector<ModelEntry> models;
    models.reserve(arr.size());
    for (const QJsonValue & value : arr) {
        const QJsonObject obj = value.toObject();
        ModelEntry entry;
        entry.id           = obj["id"].toString();
        entry.pipelineTag  = obj["pipeline_tag"].toString();
        entry.downloads    = (qlonglong) obj["downloads"].toDouble();
        entry.likes        = (qlonglong) obj["likes"].toDouble();
        entry.lastModified = obj["lastModified"].toString().left(10);
        models.append(entry);
    }
    m_model->setModels(models);
    ui.table->setCurrentIndex(QModelIndex()); // 清空选中行（触发按钮状态刷新）
    updateSelectionActions();

    // 列表来源决定 README 用哪个站点（官方 / 镜像）
    m_listMirror = wasMirror;
    m_hoverId.clear();
    if (m_descTimer) {
        m_descTimer->stop();
    }
    if (m_hoverTip) {
        m_hoverTip->hide();
    }

    // 调试：QLLAMA_TEST_HOVER=1 拉取第一条模型的介绍；=某个仓库 ID 则拉取指定模型
    if (qEnvironmentVariableIsSet("QLLAMA_TEST_HOVER")) {
        const QString testId = qEnvironmentVariable("QLLAMA_TEST_HOVER");
        QTimer::singleShot(300, this, [this, testId] {
            const QString id = (testId.isEmpty() || testId == "1")
                                   ? m_model->idAt(0)
                                   : testId;
            if (id.isEmpty()) {
                return;
            }
            m_hoverId = id;
            m_hoverBase = QStringLiteral("<b>%1</b>").arg(id.toHtmlEscaped());
            fetchDescription(id, m_listMirror);
        });
    }

    ui.statusLabel->setText(I18n::t("已拉取 %1 条模型（按最近更新排序）%2")
                                .arg(arr.size())
                                .arg(wasMirror ? I18n::t(" · 经镜像 hf-mirror.com") : QString()));

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
    ui.statusLabel->setText(I18n::t("正在查询仓库文件列表…"));
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
        ui.statusLabel->setText(I18n::t("查询文件列表失败：%1").arg(reply->errorString()));
        return;
    }

    const QByteArray data = reply->readAll();
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isArray()) {
        ui.statusLabel->setText(I18n::t("文件列表解析失败"));
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
        ui.statusLabel->setText(I18n::t("该仓库没有 GGUF 文件（试试搜索 gguf 找量化仓库）"));
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
    const QString choice = QInputDialog::getItem(this, I18n::t("选择要下载的 GGUF 文件"),
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
    const QString dir = QFileDialog::getExistingDirectory(this, I18n::t("选择模型保存目录"),
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
        ui.statusLabel->setText(I18n::t("无法创建目录：%1").arg(targetDir));
        return;
    }
    const QString fileName = QFileInfo(filePath).fileName();
    const QString localPath = QDir(targetDir).filePath(fileName);

    if (qEnvironmentVariableIsSet("QLLAMA_DUMP_GEOMETRY")) {
        qInfo().noquote() << "DLDIR: saveDirEdit='" << ui.saveDirEdit->text()
                          << "' targetDir='" << targetDir << "' localPath='" << localPath << "'";
    }

    if (QFileInfo::exists(localPath) && !forceOverwrite) {
        const auto answer = QMessageBox::question(this, I18n::t("文件已存在"),
            I18n::t("模型文件已存在：\n%1\n\n是否覆盖？").arg(localPath),
            QMessageBox::Yes | QMessageBox::No);
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
        ui.statusLabel->setText(I18n::t("无法写入文件：%1").arg(localPath));
        return;
    }

    m_downloading = true;
    setBusy(true);
    ui.progressBar->setVisible(true);
    ui.progressBar->setRange(0, 100);
    ui.progressBar->setValue(0);
    ui.statusLabel->setText(I18n::t("开始下载：%1%2").arg(
        fileName, useMirror ? I18n::t("（经镜像）") : QString()));

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
        ui.statusLabel->setText(I18n::t("下载中 %1 / %2 · %3")
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
                ui.statusLabel->setText(I18n::t("官方站不可达，改用镜像 hf-mirror.com 重试…"));
                startDownload(modelId, filePath, true, true);
                return;
            }
            m_downloading = false;
            setBusy(false);
            ui.progressBar->setVisible(false);
            ui.statusLabel->setText(I18n::t("下载失败：%1（已删除不完整文件）")
                                        .arg(reply->errorString()));
            QFile::remove(localPath);
        } else {
            m_downloading = false;
            setBusy(false);
            ui.progressBar->setVisible(false);
            ui.statusLabel->setText(I18n::t("下载完成：%1").arg(localPath));
            qInfo().noquote() << "HF 下载完成:" << localPath;
            emit modelDownloaded(localPath);
            accept(); // 完成后关闭页面，回到主窗口加载模型
        }
        reply->deleteLater();
    });
}

void ModelManageDialog::onRowDoubleClicked(const QModelIndex & index) {
    const QString id = index.sibling(index.row(), ModelListModel::IdColumn).data().toString();
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
        ui.statusLabel->setText(I18n::t("已复制模型 ID：%1").arg(m_selectedId));
    }
}

// —— 悬停显示模型介绍 ——

// 鼠标离开列表 / 关闭页面时收起提示
bool ModelManageDialog::eventFilter(QObject * watched, QEvent * event) {
    if (watched == ui.table->viewport() && event->type() == QEvent::Leave) {
        m_hoverId.clear();
        if (m_descTimer) {
            m_descTimer->stop();
        }
        if (m_hoverTip) {
            m_hoverTip->hide();
        }
    }
    return QDialog::eventFilter(watched, event);
}

// 悬停行的元信息（标题 + 类型/下载/点赞/更新），作为 tooltip 头部
QString ModelManageDialog::hoverBaseHtml(const QModelIndex & index) const {
    const int row = index.row();
    auto cell = [&index, row](int column) {
        return index.sibling(row, column).data().toString();
    };
    const QString meta = I18n::t("类型 %1 · 下载 %2 · 点赞 %3 · 更新 %4")
                             .arg(cell(ModelListModel::TagColumn).toHtmlEscaped(),
                                  cell(ModelListModel::DownloadsColumn),
                                  cell(ModelListModel::LikesColumn),
                                  cell(ModelListModel::UpdatedColumn));
    return QStringLiteral("<b>%1</b><br><span style=\"color:#b0b0c0;\">%2</span>")
        .arg(cell(ModelListModel::IdColumn).toHtmlEscaped(), meta);
}

void ModelManageDialog::onRowHovered(const QModelIndex & index) {
    if (!index.isValid()) {
        return;
    }
    const QString id = index.sibling(index.row(), ModelListModel::IdColumn).data().toString();
    if (id.isEmpty() || id == m_hoverId) {
        return;
    }
    m_hoverId = id;
    m_hoverBase = hoverBaseHtml(index);
    if (m_hoverTip && m_hoverTip->isVisible()) {
        m_hoverTip->hide(); // 移到新行时先收起旧提示，防抖后再显示新行
    }
    m_descTimer->start(); // 防抖：停留 300ms 才拉取 README
}

// 拉取模型 README（先按列表来源站点，失败自动换另一个站点）
void ModelManageDialog::fetchDescription(const QString & id, bool useMirror) {
    m_descRequested.insert(id);
    QNetworkRequest req(QUrl(apiHost(useMirror) + "/" + id + "/raw/main/README.md"));
    req.setTransferTimeout(8000);
    QNetworkReply * reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, id, useMirror] {
        handleDescriptionReply(reply, id, useMirror);
    });
}

void ModelManageDialog::handleDescriptionReply(QNetworkReply * reply, const QString & id,
                                               bool wasMirror) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        if (!wasMirror) {
            fetchDescription(id, true); // 官方站失败 → 镜像重试一次
            return;
        }
        m_descCache.insert(id, QString()); // 两个站点都没有 README
    } else {
        m_descCache.insert(id, readmeSummary(QString::fromUtf8(reply->readAll())));
    }
    if (qEnvironmentVariableIsSet("QLLAMA_TEST_HOVER")) {
        qInfo().noquote() << "DESC:" << id << "=>" << m_descCache.value(id).left(120);
    }
    if (id == m_hoverId) {
        showModelTooltip(); // 鼠标仍停留在该行
    }
}

// 自绘提示窗：QToolTip 在 macOS 下是系统半透明窗口，文字会被下方内容干扰，
// 因此用无边框 ToolTip 类型的 QLabel 自己显示，背景完全由 QSS 控制（不透明）。
QLabel * ModelManageDialog::hoverTip() {
    if (!m_hoverTip) {
        m_hoverTip = new QLabel(this, Qt::ToolTip | Qt::FramelessWindowHint);
        m_hoverTip->setObjectName("modelHoverTip");
        m_hoverTip->setTextFormat(Qt::RichText);
        m_hoverTip->setWordWrap(true); // 固定宽度内自动换行（富文本 table width 不可靠）
        m_hoverTip->setAttribute(Qt::WA_ShowWithoutActivating);
        m_hoverTip->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_hoverTip->setFocusPolicy(Qt::NoFocus);
    }
    return m_hoverTip;
}

void ModelManageDialog::showModelTooltip() {
    if (m_hoverId.isEmpty()) {
        return;
    }
    const QString desc = m_descCache.value(m_hoverId);
    const QString body = desc.isEmpty() ? I18n::t("（暂无模型介绍）")
                                        : desc.toHtmlEscaped();
    const QString html = QStringLiteral("%1<br><br>%2").arg(m_hoverBase, body);
    QLabel * tip = hoverTip();
    tip->setFixedWidth(430);
    tip->setText(html);
    tip->adjustSize();
    // 显示在光标右下方，并保证不超出屏幕
    QPoint pos = QCursor::pos() + QPoint(14, 18);
    if (const QScreen * scr = screen()) {
        const QRect avail = scr->availableGeometry();
        pos.setX(qBound(avail.left(), pos.x(), avail.right() - tip->width()));
        pos.setY(qBound(avail.top(), pos.y(), avail.bottom() - tip->height()));
    }
    tip->move(pos);
    tip->show();
    tip->raise();
}
