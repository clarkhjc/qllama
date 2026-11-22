#include "MainWindow.h"
#include "I18n.h"
#include "Markdown.h"
#include "ModelManageDialog.h"
#include "Theme.h"

#include <QBuffer>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QShowEvent>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextEdit>
#include <QThread>
#include <QTime>
#include <QTimer>

#include <algorithm>

// 把本地图片转成 data URI，嵌入聊天记录的 HTML 中
static QString imageDataUri(const QString & path, int maxWidth = 320) {
    QImage img(path);
    if (img.isNull()) {
        return {};
    }
    if (img.width() > maxWidth) {
        img = img.scaledToWidth(maxWidth, Qt::SmoothTransformation);
    }
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return "data:image/png;base64," + QString::fromLatin1(png.toBase64());
}

// 圆形渐变头像：Feather 图标 + 渐变底色 → PNG data URI（按「图标+配色+尺寸」缓存）。
// 按目标显示尺寸的 3~4 倍渲染（Retina 2x 下依然锐利），避免小位图放大产生毛刺；
// 欢迎页 logo 显示 72px、消息头像显示 26px，分别取 256px / 128px。
static QString avatarDataUri(const QString & svgPath, const QColor & c1, const QColor & c2, int S) {
    static QHash<QString, QString> cache;
    const QString key = svgPath + c1.name() + c2.name() + QString::number(S);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }
    const int iconSize = S * 53 / 100; // 图标占圆的 53%，与原先 34/64 的比例一致
    QPixmap pm(S, S);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        QLinearGradient g(0, 0, S, S);
        g.setColorAt(0.0, c1);
        g.setColorAt(1.0, c2);
        p.setBrush(g);
        p.setPen(Qt::NoPen);
        p.drawEllipse(0, 0, S, S);
        // SVG 图标按高分辨率栅格化后再绘制（QIcon 内部走 SVG 矢量渲染）
        const QPixmap iconPm = QIcon(svgPath).pixmap(QSize(iconSize, iconSize));
        p.drawPixmap((S - iconSize) / 2, (S - iconSize) / 2, iconSize, iconSize, iconPm);
    }
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    pm.save(&buf, "PNG");
    const QString uri = "data:image/png;base64," + QString::fromLatin1(png.toBase64());
    cache.insert(key, uri);
    return uri;
}

// 空状态欢迎页（启动时 / 清空对话后）
static QString welcomeHtml() {
    const ThemePalette & t = Theme::palette();
    const QString logo = avatarDataUri(":/assets/icons/message-circle-white.svg",
                                       t.avatarAssistant1, t.avatarAssistant2, 256);
    const QString subtitle = I18n::t("选择模型并点击「加载模型」，然后开始对话")
                             + QStringLiteral("<br>")
                             + I18n::t("支持 Markdown 渲染 · 图片问答 · 长对话上下文复用");
    return QStringLiteral(
        "<div style=\"text-align:center;margin-top:70px;\">"
        "<img src=\"%1\" width=\"72\" height=\"72\">"
        "<div style=\"font-size:20px;font-weight:600;color:%2;margin-top:16px;\">%4</div>"
        "<div style=\"color:%3;margin-top:10px;font-size:13px;\">%5</div>"
        "</div>").arg(logo, Theme::css(t.welcomeTitle), Theme::css(t.welcomeSubtitle),
                       I18n::t("qllama · 本地大模型对话"), subtitle);
}

// 助手消息头（含/不含“正在生成”状态标记）
static QString assistantHeaderHtml(bool generating) {
    const ThemePalette & t = Theme::palette();
    const QString avatar = avatarDataUri(":/assets/icons/zap-white.svg",
                                         t.avatarAssistant1, t.avatarAssistant2, 128);
    const QString timeStr = QTime::currentTime().toString("HH:mm");
    return QStringLiteral(
        "<div style=\"margin:14px 0 6px 0;\">"
        "<img src=\"%1\" width=\"26\" height=\"26\" style=\"vertical-align:middle;margin-right:8px;\">"
        "<b style=\"color:%2;\">%6</b>"
        "<span style=\"color:%3;font-size:11px;margin-left:8px;\">%4</span>"
        "%5</div>")
        .arg(avatar, Theme::css(t.assistantName), Theme::css(t.textMuted), timeStr,
             generating ? QStringLiteral("<span style=\"color:%1;margin-left:8px;\">%2</span>")
                              .arg(Theme::css(t.textSecondary), I18n::t("正在生成…"))
                        : QString(),
             I18n::t("助手"));
}

// 助手消息气泡（Markdown HTML 外层）：左对齐，贴合内容宽度。
// 背景放在表格单元格上，保证块级元素的 margin 区域也被气泡背景覆盖
// （放 div 上时暗色主题会出现一条条透明缝隙）。
static QString assistantBubbleHtml(const QString & innerHtml) {
    const ThemePalette & t = Theme::palette();
    return QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\"><tr><td style=\"padding:0;\">"
        "<table cellspacing=\"0\" cellpadding=\"0\"><tr><td style=\"background:%1;border:1px solid %2;"
        "border-radius:4px 16px 16px 16px;padding:12px 16px;\">%3"
        "</td></tr></table></td></tr></table>")
        .arg(Theme::css(t.assistantBubble), Theme::css(t.assistantBubbleBorder), innerHtml);
}

// 历史摘要卡片（压缩上下文后显示在聊天记录顶部）
static QString summaryHtml(const QString & summary) {
    const ThemePalette & t = Theme::palette();
    return QStringLiteral(
               "<div style=\"margin:14px 0 6px 0;\">"
               "<span style=\"color:%1;font-weight:600;font-size:13px;\">%2</span></div>"
               "<table width=\"100%%\" cellspacing=\"0\" cellpadding=\"0\"><tr><td "
               "style=\"background-color:%3;border-left:3px solid %4;padding:8px 14px;"
               "color:%5;font-size:13px;\">%6</td></tr></table>")
        .arg(Theme::css(t.heading), I18n::t("历史摘要"), Theme::css(t.quoteBg),
             Theme::css(t.quoteBorder), Theme::css(t.quoteText), Markdown::toHtml(summary));
}

// 模型状态指示灯：状态样式在 QSS（QLabel#statusDot[state=...]）
// state: idle / loading / ok / error
static void setStatusDot(QLabel * dot, const char * state) {
    dot->setProperty("state", QLatin1String(state));
    dot->style()->unpolish(dot);
    dot->style()->polish(dot);
}

// 按占用率给监控数值着色：绿 <50%，黄 <80%，红 ≥80%
static QString coloredPct(double pct, bool withDot = true) {
    const ThemePalette & t = Theme::palette();
    const QColor color = pct < 50.0 ? t.monitorOk : pct < 80.0 ? t.monitorWarn : t.monitorBad;
    return QStringLiteral("<span style=\"color:%1;\">%2</span>")
        .arg(Theme::css(color))
        .arg(withDot ? QStringLiteral("● %1%").arg(pct, 0, 'f', 0) : QString::number(pct, 'f', 0));
}

MainWindow::MainWindow(bool selfTest, const QString & selfTestPrompt, const QString & selfTestImage,
                       bool configScreenshot, QWidget * parent)
    : QMainWindow(parent), m_selfTest(selfTest), m_selfTestPrompt(selfTestPrompt), m_selfTestImage(selfTestImage) {
    ui.setupUi(this); // 界面布局见 ui/mainwindow.ui（Qt Designer 编辑）
    setWindowIcon(QIcon(":/assets/icons/appicon.png"));
    m_config.theme = Theme::currentName(); // 启动主题由 main() 应用，这里同步到配置
    m_config.language = I18n::current();   // 启动语言同样由 main() 应用
    if (qEnvironmentVariableIsSet("QLLAMA_TEST_CTX")) {
        m_config.chat.n_ctx = (uint32_t) qEnvironmentVariableIntValue("QLLAMA_TEST_CTX");
    }

    // 滚动区域透明，露出窗口背景
    ui.chatView->viewport()->setAutoFillBackground(false);
    ui.input->viewport()->setAutoFillBackground(false);

    // —— 按钮图标（Feather 图标，按当前主题着色，见 Theme::icon） ——
    refreshIcons();
    const QList<QPushButton *> iconBtns = {
        ui.browseModelBtn, ui.loadBtn, ui.unloadBtn, ui.manageModelsBtn, ui.clearBtn,
        ui.configBtn, ui.addImageBtn, ui.sendBtn, ui.stopBtn,
    };
    for (QPushButton * b : iconBtns) {
        b->setIconSize(QSize(15, 15));
    }

    // —— 工作线程 ——
    m_thread = new QThread(this);
    m_worker = new ChatWorker; // 无父对象，由线程结束时 deleteLater
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread->start();

    connect(this, &MainWindow::loadRequested,     m_worker, &ChatWorker::loadModel);
    connect(this, &MainWindow::unloadRequested,   m_worker, &ChatWorker::unloadModel);
    connect(this, &MainWindow::generateRequested, m_worker, &ChatWorker::generate);
    connect(this, &MainWindow::compressRequested, m_worker, &ChatWorker::compressContext);
    connect(this, &MainWindow::stopRequested,     m_worker, &ChatWorker::stopGeneration,
            Qt::DirectConnection); // 必须直连：生成阻塞期间队列信号无法被处理

    connect(m_worker, &ChatWorker::modelLoaded,        this, &MainWindow::onModelLoaded);
    connect(m_worker, &ChatWorker::modelLoadFailed,    this, &MainWindow::onModelLoadFailed);
    connect(m_worker, &ChatWorker::modelUnloaded,      this, &MainWindow::onModelUnloaded);
    connect(m_worker, &ChatWorker::tokenReady,         this, &MainWindow::onTokenReady);
    connect(m_worker, &ChatWorker::contextUpdated,     this, &MainWindow::updateContextLabel);
    connect(m_worker, &ChatWorker::generationFinished, this, &MainWindow::onGenerationFinished);
    connect(m_worker, &ChatWorker::generationFailed,   this, &MainWindow::onGenerationFailed);
    connect(m_worker, &ChatWorker::compressionFinished, this, &MainWindow::onCompressionFinished);
    connect(m_worker, &ChatWorker::compressionFailed,  this, &MainWindow::onCompressionFailed);

    // —— 控件连接 ——
    connect(ui.browseModelBtn, &QPushButton::clicked, this, &MainWindow::browseModel);
    connect(ui.loadBtn,        &QPushButton::clicked, this, &MainWindow::onLoadModel);
    connect(ui.unloadBtn,      &QPushButton::clicked, this, &MainWindow::onUnloadModel);
    connect(ui.clearBtn,       &QPushButton::clicked, this, &MainWindow::onClearChat);
    connect(ui.configBtn,      &QPushButton::clicked, this, &MainWindow::onConfig);
    connect(ui.manageModelsBtn, &QPushButton::clicked, this, &MainWindow::onManageModels);
    connect(ui.addImageBtn,    &QPushButton::clicked, this, &MainWindow::onAddImages);
    connect(ui.sendBtn,        &QPushButton::clicked, this, &MainWindow::onSend);
    connect(ui.stopBtn,        &QPushButton::clicked, this, &MainWindow::onStop);
    ui.input->installEventFilter(this); // Enter 直接发送
    // 发送按钮随输入内容动态启用
    connect(ui.input, &QTextEdit::textChanged, this, [this] {
        ui.sendBtn->setEnabled(!ui.input->toPlainText().trimmed().isEmpty()
                               && m_modelLoaded && !m_generating && !m_compressing);
    });
    ui.modelPath->setClearButtonEnabled(true);
    // 模型路径占满模型行所有剩余空间
    ui.modelRowLayout->setStretchFactor(ui.modelPath, 1);

    ui.chatView->append(welcomeHtml());

    // 模型状态指示灯（灰=未加载，黄=加载中，绿=已加载，红=失败；颜色在 QSS）
    setStatusDot(ui.statusDot, "idle");

    // —— 上下文使用情况（状态栏，位于系统监控左侧） ——
    m_contextLabel = new QLabel;
    m_contextLabel->setObjectName("contextLabel");
    statusBar()->addPermanentWidget(m_contextLabel);
    updateContextLabel(0, 0);

    // 手动压缩上下文（把较早对话总结成摘要，继续长对话）
    m_compressBtn = new QPushButton(I18n::t("压缩上下文"));
    m_compressBtn->setObjectName("compressBtn");
    m_compressBtn->setToolTip(I18n::t("将较早的对话压缩成摘要，继续长对话"));
    m_compressBtn->setEnabled(false);
    connect(m_compressBtn, &QPushButton::clicked, this, &MainWindow::onCompressContext);
    statusBar()->addPermanentWidget(m_compressBtn);

    // —— 系统监控：CPU / 内存 / GPU（2 秒刷新，状态栏右侧常驻） ——
    m_monitorLabel = new QLabel;
    m_monitorLabel->setObjectName("monitorLabel");
    statusBar()->addPermanentWidget(m_monitorLabel); // addPermanentWidget 默认靠右排列
    updateMonitorLabel();
    m_monitor = new SysMonitor(this);
    connect(m_monitor, &SysMonitor::statsUpdated, this, [this](double cpu, double mem, int gpu) {
        if (qEnvironmentVariableIsSet("QLLAMA_DUMP_GEOMETRY")) {
            static int dumpCount = 0;
            if (dumpCount++ < 3) {
                qInfo().noquote() << "MONITOR: cpu=" << cpu << " mem=" << mem << " gpu=" << gpu;
            }
        }
        m_lastCpu = cpu;
        m_lastMem = mem;
        m_lastGpu = gpu;
        updateMonitorLabel();
    });
    m_monitor->start(2000);

    statusBar()->showMessage(I18n::t("未加载模型"));

    prefillModelPath();
    I18n::retranslateTree(this); // .ui 中的静态文字按当前语言翻译

    // —— 自测模式：加载模型 → 两轮对话 → 截图 → 退出 ——
    if (m_selfTest) {
        auto * timeout = new QTimer(this);
        timeout->setSingleShot(true);
        connect(timeout, &QTimer::timeout, this, [this] { selfTestFinish(false, "超时"); });
        timeout->start(180000);
        QTimer::singleShot(500, this, &MainWindow::onLoadModel);
    }

    // —— 调试：QLLAMA_OPEN_MODELS=1 时启动即打开模型管理页面 ——
    if (qEnvironmentVariableIsSet("QLLAMA_OPEN_MODELS")) {
        QTimer::singleShot(300, this, &MainWindow::onManageModels);
    }

    // —— 配置页面截图模式：打开配置对话框截图后退出 ——
    if (configScreenshot) {
        QTimer::singleShot(300, this, [this] {
            auto * dlg = new ConfigDialog(m_config, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
            QTimer::singleShot(400, this, [this, dlg] {
                const QString shot = QDir::temp().absoluteFilePath("qllama_config.png");
                dlg->grab().save(shot);
                qInfo().noquote() << "CONFIG SCREENSHOT:" << shot;
                // 输出对话框结构（标签 + 控件值），便于无界面验证
                for (const QLabel * lab : dlg->findChildren<QLabel *>()) {
                    if (!lab->text().isEmpty()) {
                        qInfo().noquote() << "  [标签]" << lab->text();
                    }
                }
                for (const QLineEdit * le : dlg->findChildren<QLineEdit *>()) {
                    qInfo().noquote() << "  [文本]" << (le->text().isEmpty() ? "(空)" : le->text());
                }
                for (const QSpinBox * sb : dlg->findChildren<QSpinBox *>()) {
                    qInfo().noquote() << "  [整数]" << sb->value() << "宽" << sb->width();
                }
                for (const QDoubleSpinBox * ds : dlg->findChildren<QDoubleSpinBox *>()) {
                    qInfo().noquote() << "  [小数]" << ds->value() << "宽" << ds->width();
                }
                for (const QLineEdit * le : dlg->findChildren<QLineEdit *>()) {
                    qInfo().noquote() << "  [文本宽]" << le->width() << (le->objectName().isEmpty() ? "" : "(" + le->objectName() + ")");
                }
                for (const QCheckBox * cb : dlg->findChildren<QCheckBox *>()) {
                    qInfo().noquote() << "  [勾选]" << cb->text() << "=" << cb->isChecked()
                                      << "y=" << cb->mapTo(dlg, QPoint(0, 0)).y();
                }
                qInfo().noquote() << "  [对话框尺寸]" << dlg->size();
                dlg->close();
                close(); // 走正常关闭流程（closeEvent 中回收工作线程）
                QCoreApplication::exit(0);
            });
        });
    }
}

// —— 模型路径预填 ——
void MainWindow::prefillModelPath() {
    const QStringList candidates = {
        QDir::home().filePath("Documents/agents/models/gemma-3-4b-it-abliterated.q4_k_m.gguf"),
        QDir::home().filePath("Documents/agents/models/Qwen3.8-4B-Distill-Heretic-Abliterated-GGUF/model-Q4_K_M.gguf"),
    };
    // 自测带图片时优先选支持多模态（目录内有 mmproj）的模型
    if (!m_selfTestImage.isEmpty()) {
        for (const QString & p : candidates) {
            if (!QFileInfo::exists(p)) {
                continue;
            }
            const QDir dir = QFileInfo(p).absoluteDir();
            if (!dir.entryList({"mmproj*.gguf"}, QDir::Files).isEmpty()) {
                ui.modelPath->setText(p);
                prefillMmprojForModel();
                return;
            }
        }
    }
    for (const QString & p : candidates) {
        if (QFileInfo::exists(p)) {
            ui.modelPath->setText(p);
            prefillMmprojForModel();
            return;
        }
    }
    // 兜底：扫描常用模型目录（含子目录）
    const QDir modelsDir(QDir::home().filePath("Documents/agents/models"));
    if (modelsDir.exists()) {
        QDirIterator it(modelsDir.absolutePath(), {"*.gguf"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString p = it.next();
            if (p.contains("mmproj")) {
                continue;
            }
            ui.modelPath->setText(p);
            prefillMmprojForModel();
            return;
        }
    }
}

// —— 用户操作 ——
void MainWindow::browseModel() {
    const QString path = QFileDialog::getOpenFileName(this, I18n::t("选择 GGUF 模型"),
        QFileInfo(ui.modelPath->text()).absolutePath(),
        I18n::t("GGUF 模型 (*.gguf);;所有文件 (*)"));
    if (!path.isEmpty()) {
        ui.modelPath->setText(path);
        prefillMmprojForModel();
    }
}

// 打开配置页面（精简配置 / 全量配置）
void MainWindow::onConfig() {
    const QString oldTheme = m_config.theme;
    const QString oldLanguage = m_config.language;
    ConfigDialog dlg(m_config, this);
    // 语言按钮实时预览：立即重译主窗口
    connect(&dlg, &ConfigDialog::languageChanged, this, [this](const QString & code) {
        applyLanguage(code);
    });
    if (dlg.exec() == QDialog::Accepted) {
        m_config = dlg.config();
        if (m_config.theme != oldTheme) {
            // 主题已在配置页实时预览；这里确认应用并重建聊天记录（HTML 颜色随主题）
            QSettings().setValue("ui/theme", m_config.theme);
            Theme::apply(m_config.theme);
            refreshIcons();
            rebuildChatView();
        }
        if (m_config.language != oldLanguage) {
            QSettings().setValue("ui/language", m_config.language);
            applyLanguage(m_config.language);
        }
        statusBar()->showMessage(I18n::t("配置已更新"), 3000);
    } else {
        // 取消：恢复到打开配置页前的主题与语言（期间可能被实时预览改过）
        Theme::apply(m_config.theme);
        applyLanguage(oldLanguage);
    }
}

// 打开模型管理页面（Hugging Face 最新模型列表）
void MainWindow::onManageModels() {
    ModelManageDialog dlg(m_config, this);
    connect(&dlg, &ModelManageDialog::modelDownloaded, this, [this](const QString & localPath) {
        ui.modelPath->setText(localPath); // 下载完成即设为当前模型
        prefillMmprojForModel();
        statusBar()->showMessage(I18n::t("已设为当前模型：%1（点击「加载模型」开始使用）").arg(localPath), 8000);
        if (qEnvironmentVariableIsSet("QLLAMA_DUMP_GEOMETRY")) {
            qInfo().noquote() << "MODELPATH SET:" << ui.modelPath->text();
        }
    });
    dlg.exec();
}

// 若模型所在目录下有 mmproj*.gguf，自动填入投影模型路径
void MainWindow::prefillMmprojForModel() {
    if (!m_config.chat.mmprojPath.empty()) {
        return; // 用户已手动指定
    }
    const QDir dir = QFileInfo(ui.modelPath->text()).absoluteDir();
    const QStringList mmprojs = dir.entryList({"mmproj*.gguf"}, QDir::Files);
    if (!mmprojs.isEmpty()) {
        m_config.chat.mmprojPath = dir.filePath(mmprojs.first()).toStdString();
    }
}

void MainWindow::onLoadModel() {
    const QString path = ui.modelPath->text().trimmed();
    if (path.isEmpty()) {
        return;
    }
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, I18n::t("错误"), I18n::t("模型文件不存在：\n%1").arg(path));
        return;
    }
    prefillMmprojForModel();
    ui.loadBtn->setEnabled(false);
    setStatusDot(ui.statusDot, "loading"); // 加载中
    statusBar()->showMessage(I18n::t("正在加载模型，请稍候……"));
    emit loadRequested(path, m_config.chat);
}

void MainWindow::onUnloadModel() {
    statusBar()->showMessage(I18n::t("正在卸载模型……"));
    emit unloadRequested();
}

void MainWindow::onSend() {
    const QString text = ui.input->toPlainText().trimmed();
    const bool hasImages = !m_attachedImages.isEmpty();
    if ((text.isEmpty() && !hasImages) || m_generating || !m_modelLoaded) {
        return;
    }
    if (hasImages && !m_modelHasVision) {
        const QString msg = I18n::t("当前模型未加载 mmproj 投影模型，无法处理图片。\n请选择投影模型后重新加载。");
        if (m_selfTest) {
            selfTestFinish(false, msg);
        } else {
            QMessageBox::warning(this, I18n::t("无法发送图片"), msg);
        }
        return;
    }

    // 图片消息：content = 每张图片一个 <__media__> 标记 + 文本
    QString content = text;
    for (int i = 0; i < m_attachedImages.size(); i++) {
        content = QStringLiteral("<__media__>") + content;
    }

    // 组装完整历史：系统提示词 + 对话历史 + 本轮提问
    QVector<ChatMessage> msgs;
    QString systemPrompt = m_config.systemPrompt;
    if (!m_contextSummary.isEmpty()) {
        const QString block = I18n::t("[此前对话摘要]\n%1").arg(m_contextSummary);
        systemPrompt = systemPrompt.isEmpty() ? block : systemPrompt + "\n\n" + block;
    }
    if (!systemPrompt.isEmpty()) {
        msgs.append(ChatMessage{"system", systemPrompt.toStdString()});
    }
    for (const ChatMessage & m : m_history) {
        msgs.append(m);
    }
    ChatMessage userMsg{"user", content.toStdString()};
    for (const QString & p : m_attachedImages) {
        userMsg.imagePaths.push_back(p.toStdString());
    }
    msgs.append(userMsg);

    m_history.append(userMsg);
    appendUserMessage(text, m_attachedImages);
    m_attachedImages.clear();
    refreshImageStrip();
    appendAssistantHeader(); // 内部记录助手正文起点（流式原文，完成后用 Markdown 替换）
    ui.input->clear();
    setGenerating(true);
    statusBar()->showMessage(I18n::t("生成中……"));
    scrollChatToBottom(); // 让用户消息与「正在生成…」立即进入视野

    emit generateRequested(msgs, m_config.chat);
}

void MainWindow::onStop() {
    emit stopRequested(); // DirectConnection，生成循环中即刻生效
    statusBar()->showMessage(I18n::t("正在停止……"));
}

void MainWindow::onClearChat() {
    m_history.clear();
    m_contextSummary.clear();
    m_pendingCompressCount = 0;
    ui.chatView->clear();
    ui.chatView->append(welcomeHtml());
    updateContextLabel(0, m_ctxCapacity); // 对话已清空，下次生成会全量重算
    updateCompressButton();
}

void MainWindow::onAddImages() {
    if (m_attachedImages.size() >= 4) {
        QMessageBox::information(this, I18n::t("图片数量限制"), I18n::t("一次最多发送 4 张图片。"));
        return;
    }
    const QStringList paths = QFileDialog::getOpenFileNames(this, I18n::t("选择图片"), QDir::homePath(),
        I18n::t("图片 (*.png *.jpg *.jpeg *.bmp *.gif);;所有文件 (*)"));
    for (const QString & p : paths) {
        if (m_attachedImages.size() >= 4) {
            break;
        }
        if (!m_attachedImages.contains(p)) {
            m_attachedImages.append(p);
        }
    }
    refreshImageStrip();
}

// 重建图片缩略图条（点击缩略图移除）
void MainWindow::refreshImageStrip() {
    while (QLayoutItem * item = ui.imageStrip->takeAt(0)) {
        if (QWidget * w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }
    for (int i = 0; i < m_attachedImages.size(); i++) {
        const QString & path = m_attachedImages[i];
        auto * btn = new QPushButton;
        const QPixmap pm(path);
        btn->setIcon(QIcon(pm.isNull() ? QPixmap()
                                       : pm.scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        btn->setIconSize(QSize(56, 56));
        btn->setFixedSize(72, 72);
        btn->setObjectName("thumbBtn");
        btn->setToolTip(I18n::t("点击移除：%1").arg(path));
        const int idx = i;
        connect(btn, &QPushButton::clicked, this, [this, idx] {
            m_attachedImages.removeAt(idx);
            refreshImageStrip();
        });
        ui.imageStrip->addWidget(btn);
    }
    ui.imageStrip->addStretch(1);
}

// —— 工作线程回调 ——
void MainWindow::onModelLoaded(const QString & info, bool hasVision) {
    m_modelLoaded = true;
    m_modelHasVision = hasVision;
    ui.loadBtn->setEnabled(false);
    ui.unloadBtn->setEnabled(true);
    ui.sendBtn->setEnabled(!ui.input->toPlainText().trimmed().isEmpty()); // 输入为空时保持禁用
    setStatusDot(ui.statusDot, "ok"); // 已加载
    QString summary = I18n::t("已加载 %1%2 · 上下文 %3 · 温度 %4")
                          .arg(info, hasVision ? I18n::t(" · 多模态") : QString())
                          .arg(m_config.chat.n_ctx)
                          .arg(m_config.chat.temperature, 0, 'f', 2);
    statusBar()->showMessage(summary);
    updateCompressButton();
    if (m_selfTest) {
        qInfo().noquote() << "SELFTEST 模型已加载:" << info << "多模态:" << hasVision;
        QTimer::singleShot(500, this, [this] {
            const QString prompt = m_selfTestPrompt.isEmpty()
                                       ? QStringLiteral("请用 Markdown 回复：一个标题、一个列表和一段代码")
                                       : m_selfTestPrompt;
            if (!m_selfTestImage.isEmpty() && !m_attachedImages.contains(m_selfTestImage)) {
                m_attachedImages.append(m_selfTestImage); // 首轮附带图片
                refreshImageStrip();
            }
            selfTestAsk(prompt);
        });
    }
}

void MainWindow::onModelLoadFailed(const QString & err) {
    ui.loadBtn->setEnabled(true);
    setStatusDot(ui.statusDot, "error"); // 加载失败
    statusBar()->showMessage(I18n::t("模型加载失败"));
    QMessageBox::critical(this, I18n::t("加载失败"), err);
    if (m_selfTest) {
        selfTestFinish(false, "模型加载失败: " + err);
    }
}

void MainWindow::onModelUnloaded() {
    m_modelLoaded = false;
    m_modelHasVision = false;
    ui.loadBtn->setEnabled(true);
    ui.unloadBtn->setEnabled(false);
    ui.sendBtn->setEnabled(false);
    setStatusDot(ui.statusDot, "idle"); // 未加载
    statusBar()->showMessage(I18n::t("未加载模型"));
    updateCompressButton();
}

void MainWindow::onTokenReady(const QString & piece) {
    // 流式输出：直接追加到文档末尾（助手消息始终是最后一段）
    QTextCursor cur(ui.chatView->document());
    cur.movePosition(QTextCursor::End);
    cur.insertText(piece);
    ui.chatView->setTextCursor(cur); // 自动滚动到底部
}

void MainWindow::onGenerationFinished(const QString & text, const ChatResult & result) {
    finishAssistantHeader(); // 移除“正在生成”标记

    // 用 Markdown 渲染替换流式原文（起点到文档末尾之间的所有内容），
    // 助手消息左对齐：气泡贴合内容宽度
    QTextCursor cur(ui.chatView->document());
    const int docEnd = ui.chatView->document()->characterCount() - 1;
    if (m_assistantStartPos >= 0 && m_assistantStartPos <= docEnd) {
        cur.setPosition(m_assistantStartPos);
        cur.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        cur.removeSelectedText();
        if (!text.isEmpty()) {
            cur.insertHtml(assistantBubbleHtml(Markdown::toHtml(text)));
        }
    } else if (!text.isEmpty()) {
        // 兜底：起点失效（如生成期间清空了对话），直接追加
        cur.movePosition(QTextCursor::End);
        cur.insertText(text);
    }
    m_assistantStartPos = -1;

    if (!text.isEmpty()) {
        m_history.append(ChatMessage{"assistant", text.toStdString()});
    }
    // 统计行（替代分隔线）：生成 token 数、速度、本轮解码量
    // 统计行（替代分隔线）：生成 token 数、速度、本轮解码量
    const QString statLine = I18n::t("生成 %1 token · %2 tok/s · 本轮解码 %3 token")
                                 .arg(result.generated_tokens)
                                 .arg(result.tokens_per_sec, 0, 'f', 1)
                                 .arg(result.prompt_tokens);
    ui.chatView->append(QStringLiteral(
        "<div style=\"color:%1;font-size:11px;margin:4px 0 14px 0;\">%2</div>")
        .arg(Theme::css(Theme::palette().statText), statLine));
    setGenerating(false);
    const QString stats = I18n::t("解码 %1 token · 生成 %2 token · %3 tok/s")
                              .arg(result.prompt_tokens)
                              .arg(result.generated_tokens)
                              .arg(result.tokens_per_sec, 0, 'f', 1);
    statusBar()->showMessage(stats);
    // 上下文裁剪 / 截断提示
    if (result.omitted_messages > 0) {
        ui.chatView->append(QStringLiteral(
            "<div style=\"color:%1;font-size:11px;margin:-6px 0 14px 0;\">%2</div>")
            .arg(Theme::css(Theme::palette().monitorWarn),
                 I18n::t("已省略较早的 %1 条消息以适配上下文（界面仍保留显示）")
                     .arg(result.omitted_messages)));
        statusBar()->showMessage(
            I18n::t("上下文已满：本次发送省略了 %1 条较早消息").arg(result.omitted_messages), 8000);
    }
    if (result.context_limit_reached) {
        ui.chatView->append(QStringLiteral(
            "<div style=\"color:%1;font-size:11px;margin:-6px 0 14px 0;\">%2</div>")
            .arg(Theme::css(Theme::palette().monitorWarn),
                 I18n::t("已达上下文上限，回复被截断")));
    }
    ui.input->setFocus();
    scrollChatToBottom(); // Markdown 替换 + 统计行追加后滚动到底

    if (m_selfTest) {
        m_selfTestTurn++;
        qInfo().noquote() << QString("SELFTEST 第 %1 轮: 解码 %2 token · 生成 %3 token · %4 tok/s")
                                 .arg(m_selfTestTurn)
                                 .arg(result.prompt_tokens)
                                 .arg(result.generated_tokens)
                                 .arg(result.tokens_per_sec, 0, 'f', 1);
        qInfo().noquote() << "SELFTEST 回复:" << text;
        if (m_selfTestTurn == 1) {
            // 调试：QLLAMA_TEST_LANG_SWITCH=en|zh 时在两轮之间切换语言
            const QByteArray switchLang = qgetenv("QLLAMA_TEST_LANG_SWITCH");
            if (!switchLang.isEmpty()) {
                applyLanguage(QString::fromUtf8(switchLang));
                qInfo().noquote() << "LANGSWITCH:" << m_config.language;
            }
            // 调试：QLLAMA_TEST_THEME_SWITCH=<light|dark|pink> 时在两轮之间切换主题，
            // 验证「实时切换 + 聊天记录按新配色重建」全链路
            const QByteArray switchTo = qgetenv("QLLAMA_TEST_THEME_SWITCH");
            if (!switchTo.isEmpty()) {
                Theme::apply(QString::fromUtf8(switchTo));
                m_config.theme = Theme::currentName();
                refreshIcons();
                rebuildChatView();
                qInfo().noquote() << "THEMESWITCH:" << m_config.theme;
            }
            // 第二轮验证长对话 KV 增量复用
            QTimer::singleShot(500, this, [this] { selfTestAsk("你刚才说了什么？"); });
        } else {
            if (qEnvironmentVariableIsSet("QLLAMA_TEST_COMPRESS")) {
                m_selfTestCompressPending = true;
                QTimer::singleShot(300, this, &MainWindow::onCompressContext);
            } else {
                QTimer::singleShot(500, this, &MainWindow::selfTestCaptureAndFinish);
            }
        }
    }
}

void MainWindow::onGenerationFailed(const QString & err) {
    finishAssistantHeader(); // 移除“正在生成”标记
    m_assistantStartPos = -1;
    ui.chatView->append(QStringLiteral("<div style=\"color:%1;margin:6px 0;\">%2</div>")
                            .arg(Theme::css(Theme::palette().errorText),
                                 I18n::t("生成失败：%1").arg(err.toHtmlEscaped())));
    setGenerating(false);
    statusBar()->showMessage(I18n::t("生成失败"));
    scrollChatToBottom();
    if (m_selfTest) {
        selfTestFinish(false, "生成失败: " + err);
    }
}

// —— 内部工具 ——
void MainWindow::setGenerating(bool on) {
    m_generating = on;
    ui.sendBtn->setEnabled(!on && m_modelLoaded && !ui.input->toPlainText().trimmed().isEmpty());
    ui.stopBtn->setEnabled(on);
    ui.loadBtn->setEnabled(!on);
    ui.unloadBtn->setEnabled(!on && m_modelLoaded);
    ui.configBtn->setEnabled(!on);
    ui.addImageBtn->setEnabled(!on);
    // 生成期间锁定模型路径，避免状态不一致
    ui.modelPath->setEnabled(!on);
    updateCompressButton();
}

// 自测结束：截图后走正常退出流程
void MainWindow::selfTestCaptureAndFinish() {
    QTimer::singleShot(300, this, [this] {
        const QString shotDir = QDir::temp().absoluteFilePath("qllama_selftest_chat.png");
        const QString winDir  = QDir::temp().absoluteFilePath("qllama_selftest_win.png");
        ui.chatView->grab().save(shotDir);
        grab().save(winDir);
        selfTestFinish(true, QString("截图已保存: %1 / %2").arg(shotDir, winDir));
    });
}

// —— 自测模式 ——
void MainWindow::selfTestAsk(const QString & prompt) {
    ui.input->setPlainText(prompt);
    onSend();
}

void MainWindow::selfTestFinish(bool ok, const QString & reason) {
    qInfo().noquote() << (ok ? "SELFTEST PASS:" : "SELFTEST FAIL:") << reason;
    const int code = ok ? 0 : 1;
    QTimer::singleShot(200, this, [this, code] {
        close();                    // 走正常关闭流程（closeEvent 中回收工作线程）
        QCoreApplication::exit(code);
    });
}

void MainWindow::appendUserMessage(const QString & text, const QStringList & imagePaths) {
    // 用户消息右对齐：名字 + 头像在右侧，气泡贴合内容宽度。
    // 结构：一个 100% 宽的表格，头部行与气泡行都是右对齐单元格——
    // div 的 text-align 在 QTextDocument 里不可靠（与表格同段时会被
    // 塞进共享单元格结构，对齐退化为相对气泡内容宽度）。
    const ThemePalette & t = Theme::palette();
    const QString avatar = avatarDataUri(":/assets/icons/user-white.svg",
                                         t.avatarUser1, t.avatarUser2, 128);
    const QString timeStr = QTime::currentTime().toString("HH:mm");
    const QString headerRow = QStringLiteral(
        "<tr><td align=\"right\" style=\"padding:14px 0 6px 0;\">"
        "<span style=\"color:%1;font-size:11px;margin-right:8px;\">%2</span>"
        "<b style=\"color:%3;\">%5</b>"
        "<img src=\"%4\" width=\"26\" height=\"26\" style=\"vertical-align:middle;margin-left:8px;\">"
        "</td></tr>")
        .arg(Theme::css(t.textMuted), timeStr, Theme::css(t.userName), avatar,
             I18n::t("你"));

    QString bodyRows;
    // 图片行（右对齐）
    if (!imagePaths.isEmpty()) {
        QString imgs;
        for (const QString & p : imagePaths) {
            const QString uri = imageDataUri(p);
            if (uri.isEmpty()) {
                continue;
            }
            imgs += QStringLiteral(
                        "<div style=\"text-align:right;\"><img src=\"%1\" "
                        "style=\"max-width:320px;border-radius:10px;margin:2px 0;\"></div>")
                        .arg(uri);
        }
        bodyRows += QStringLiteral("<tr><td align=\"right\" style=\"padding:0;\">%1</td></tr>").arg(imgs);
    }
    // 文字气泡行：嵌套表格让气泡贴合内容宽度
    const QString esc = text.toHtmlEscaped().replace("\n", "<br>");
    if (!esc.isEmpty()) {
        bodyRows += QStringLiteral(
            "<tr><td align=\"right\" style=\"padding:0;\">"
            "<table cellspacing=\"0\" cellpadding=\"0\"><tr><td style=\"padding:0;\">"
            "<div style=\"background:%1;border-radius:16px 16px 4px 16px;"
            "padding:10px 14px;color:%2;\">%3</div>"
            "</td></tr></table></td></tr>")
            .arg(Theme::css(t.userBubble), Theme::css(t.userBubbleText), esc);
    }

    ui.chatView->append(QStringLiteral(
        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">%1%2</table>")
        .arg(headerRow, bodyRows));
}

// 滚到聊天记录末尾。
// 插入 Markdown HTML / 统计行后文档是异步重新布局的，立即设滚动条会停在旧的最大值，
// 所以延后到事件循环末尾再滚，并调用两次以覆盖布局完成后的高度变化。
void MainWindow::scrollChatToBottom() {
    auto scrollNow = [this] {
        QScrollBar * sb = ui.chatView->verticalScrollBar();
        sb->setValue(sb->maximum());
    };
    QTimer::singleShot(0, this, scrollNow);
    QTimer::singleShot(50, this, scrollNow);
}

// 按当前语言刷新状态栏的 CPU/内存/GPU 文字
void MainWindow::updateMonitorLabel() {
    if (!m_monitorLabel) {
        return;
    }
    const QColor na = Theme::palette().monitorNA;
    const QString dash = QStringLiteral("<span style=\"color:%1;\">—</span>").arg(Theme::css(na));
    auto fmt = [&dash](double pct) {
        return pct < 0 ? dash : coloredPct(pct);
    };
    const QString gpu = m_lastGpu >= 0 ? coloredPct(m_lastGpu) : dash;
    m_monitorLabel->setText(I18n::t("CPU %1 · 内存 %2 · GPU %3")
                                .arg(fmt(m_lastCpu), fmt(m_lastMem), gpu));
    m_monitorLabel->setToolTip(I18n::t("系统级：CPU 利用率 · 内存占用率（活动监视器口径）· GPU 利用率"));
}

// 状态栏上下文使用情况：已解码 token / n_ctx 容量（按占用率着色）
void MainWindow::updateContextLabel(int used, int capacity) {
    m_ctxUsed = used;
    m_ctxCapacity = capacity;
    if (!m_contextLabel) {
        return;
    }
    if (capacity <= 0) {
        m_contextLabel->setText(I18n::t("上下文 —"));
        m_contextLabel->setToolTip(I18n::t("加载模型后显示上下文使用情况"));
        return;
    }
    const double pct = 100.0 * used / capacity;
    const ThemePalette & t = Theme::palette();
    const QColor color = pct < 50.0 ? t.monitorOk : pct < 80.0 ? t.monitorWarn : t.monitorBad;
    auto fmt = [](int n) {
        return n >= 1000 ? QStringLiteral("%1K").arg(n / 1000.0, 0, 'f', 1)
                         : QString::number(n);
    };
    m_contextLabel->setText(
        QStringLiteral("<span style=\"color:%1;\">●</span> %2")
            .arg(Theme::css(color),
                 I18n::t("上下文 %1 / %2 (%3%)")
                     .arg(fmt(used), fmt(capacity), QString::number(pct, 'f', 0))));
    m_contextLabel->setToolTip(
        I18n::t("上下文使用：已解码 %1 token / 容量 %2 token（%3%）。接近上限时请调大「上下文」或清空对话。")
            .arg(used)
            .arg(capacity)
            .arg(pct, 0, 'f', 0));
}

// —— 上下文摘要压缩 ——

// 选择最早的一部分消息交给工作线程总结，至少保留最近一轮完整对话
void MainWindow::onCompressContext() {
    if (!m_modelLoaded || m_compressing || m_history.size() < 3) {
        return;
    }
    const int keep = 2; // 保留最近的 user + assistant
    const int maxTake = m_history.size() - keep;
    if (maxTake <= 0) {
        statusBar()->showMessage(I18n::t("没有可压缩的较早消息"), 5000);
        return;
    }
    // 按字符预算选择（约 n_ctx/3，保证摘要请求本身能放进上下文）
    const int budget = std::max(400, (int) m_config.chat.n_ctx / 3);
    int take = 0;
    int chars = m_contextSummary.size();
    while (take < maxTake) {
        const int len = (int) m_history[take].content.size() + 12;
        if (take > 0 && chars + len > budget) {
            break;
        }
        chars += len;
        take++;
    }
    if (take <= 0) {
        statusBar()->showMessage(I18n::t("没有可压缩的较早消息"), 5000);
        return;
    }

    QVector<ChatMessage> batch;
    if (!m_contextSummary.isEmpty()) {
        // 已有摘要时一并重新压缩，避免摘要无限叠加
        batch.append(ChatMessage{"system", m_contextSummary.toStdString()});
    }
    for (int i = 0; i < take; i++) {
        batch.append(m_history[i]);
    }
    m_pendingCompressCount = take;
    setCompressing(true);
    statusBar()->showMessage(I18n::t("正在压缩上下文……"));
    emit compressRequested(batch, m_config.chat);
}

void MainWindow::setCompressing(bool on) {
    m_compressing = on;
    ui.sendBtn->setEnabled(!on && m_modelLoaded && !m_generating
                          && !ui.input->toPlainText().trimmed().isEmpty());
    ui.configBtn->setEnabled(!on && !m_generating);
    ui.addImageBtn->setEnabled(!on && !m_generating);
    ui.loadBtn->setEnabled(!on && !m_generating);
    ui.unloadBtn->setEnabled(!on && !m_generating && m_modelLoaded);
    ui.clearBtn->setEnabled(!on && !m_generating);
    ui.modelPath->setEnabled(!on && !m_generating);
    updateCompressButton();
}

void MainWindow::updateCompressButton() {
    if (!m_compressBtn) {
        return;
    }
    m_compressBtn->setEnabled(m_modelLoaded && !m_generating && !m_compressing
                              && m_history.size() >= 3);
}

void MainWindow::onCompressionFinished(const QString & summary) {
    const int count = m_pendingCompressCount;
    m_pendingCompressCount = 0;
    m_contextSummary = summary;
    qInfo().noquote() << "COMPRESS SUMMARY:" << summary.left(200);
    if (count > 0 && count <= m_history.size()) {
        m_history.remove(0, count);
    }
    setCompressing(false);
    rebuildChatView();
    statusBar()->showMessage(I18n::t("上下文已压缩：%1 条消息已并入摘要").arg(count), 8000);
    if (m_selfTestCompressPending) {
        m_selfTestCompressPending = false;
        selfTestCaptureAndFinish();
    }
}

void MainWindow::onCompressionFailed(const QString & err) {
    m_pendingCompressCount = 0;
    setCompressing(false);
    if (m_selfTestCompressPending) {
        m_selfTestCompressPending = false;
        selfTestFinish(false, err);
        return;
    }
    QMessageBox::warning(this, I18n::t("上下文压缩失败"), err);
}

// 切换界面语言：重译静态控件 + 动态文字 + 重建聊天记录
void MainWindow::applyLanguage(const QString & code) {
    I18n::setLanguage(code);
    m_config.language = I18n::normalized(code);
    I18n::retranslateTree(this);
    if (m_compressBtn) {
        m_compressBtn->setText(I18n::t("压缩上下文"));
        m_compressBtn->setToolTip(I18n::t("将较早的对话压缩成摘要，继续长对话"));
    }
    updateMonitorLabel();
    updateContextLabel(m_ctxUsed, m_ctxCapacity);
    if (m_generating) {
        statusBar()->showMessage(I18n::t("生成中……"));
    } else if (!m_modelLoaded) {
        statusBar()->showMessage(I18n::t("未加载模型"));
    } else {
        statusBar()->showMessage(I18n::t("已加载模型"));
    }
    rebuildChatView();
}

// 按当前主题重建整个聊天记录（切换主题后调用）
void MainWindow::rebuildChatView() {
    ui.chatView->clear();
    if (!m_contextSummary.isEmpty()) {
        ui.chatView->append(summaryHtml(m_contextSummary)); // 保留压缩后的历史摘要
    }
    if (m_history.isEmpty()) {
        if (m_contextSummary.isEmpty()) {
            ui.chatView->append(welcomeHtml());
        }
        return;
    }
    for (const ChatMessage & m : m_history) {
        QString content = QString::fromStdString(m.content);
        if (m.role == "user") {
            content.remove(QStringLiteral("<__media__>")); // 图片标记不显示
            QStringList images;
            for (const auto & p : m.imagePaths) {
                images << QString::fromStdString(p);
            }
            appendUserMessage(content.trimmed(), images);
        } else if (m.role == "assistant") {
            ui.chatView->append(assistantHeaderHtml(false));
            if (!content.isEmpty()) {
                ui.chatView->append(assistantBubbleHtml(Markdown::toHtml(content)));
            }
        }
    }
    scrollChatToBottom();
}

// 主窗口按钮图标：按当前主题重新着色
void MainWindow::refreshIcons() {
    ui.browseModelBtn->setIcon(Theme::icon("folder"));
    ui.loadBtn->setIcon(Theme::icon("play-circle-white"));
    ui.unloadBtn->setIcon(Theme::icon("x-circle"));
    ui.manageModelsBtn->setIcon(Theme::icon("download")); // 管理模型：从 Hugging Face 下载
    ui.clearBtn->setIcon(Theme::icon("trash-2"));
    ui.configBtn->setIcon(Theme::icon("sliders"));
    ui.addImageBtn->setIcon(Theme::icon("image"));
    ui.sendBtn->setIcon(Theme::icon("send-white"));
    ui.stopBtn->setIcon(Theme::icon("square-white"));
}

void MainWindow::appendAssistantHeader() {
    QTextCursor cur(ui.chatView->document());
    cur.movePosition(QTextCursor::End);
    m_assistantHeaderStart = cur.position();
    ui.chatView->append(assistantHeaderHtml(true)); // 含“正在生成”标记
    cur.movePosition(QTextCursor::End);
    m_assistantHeaderEnd = cur.position();
    // 新建一个空段落：回复正文从下一行开始，
    // 且不继承“助手”标签的颜色/加粗等字符格式
    cur.insertBlock(QTextBlockFormat(), QTextCharFormat());
    m_assistantStartPos = cur.position();
}

// 生成结束（成功/失败/停止）后：移除头部的“正在生成”标记，
// 并按头部长度变化修正正文起点的文档位置
void MainWindow::finishAssistantHeader() {
    const int docEnd = ui.chatView->document()->characterCount() - 1;
    if (m_assistantHeaderStart < 0 || m_assistantHeaderEnd < m_assistantHeaderStart
        || m_assistantHeaderEnd > docEnd) {
        m_assistantHeaderStart = -1;
        m_assistantHeaderEnd = -1;
        return;
    }
    QTextCursor hc(ui.chatView->document());
    hc.setPosition(m_assistantHeaderStart);
    hc.setPosition(m_assistantHeaderEnd, QTextCursor::KeepAnchor);
    hc.removeSelectedText();
    hc.insertHtml(assistantHeaderHtml(false));
    const int newEnd = hc.position();
    m_assistantStartPos += newEnd - m_assistantHeaderEnd;
    m_assistantHeaderEnd = newEnd;
    m_assistantHeaderStart = -1;
}

// —— 事件 ——
void MainWindow::showEvent(QShowEvent * event) {
    QMainWindow::showEvent(event);
    static bool geometryDumped = false;
    if (!geometryDumped) {
        geometryDumped = true;
        // 调试：QLLAMA_DUMP_GEOMETRY=1 时打印关键控件几何信息
        if (qEnvironmentVariableIsSet("QLLAMA_DUMP_GEOMETRY")) {
            qInfo().noquote() << "GEOM input:" << ui.input->geometry()
                              << "| sendBtn:" << ui.sendBtn->geometry()
                              << "| stopBtn:" << ui.stopBtn->geometry()
                              << "| chatView:" << ui.chatView->geometry();
            qInfo().noquote() << "POLICY input:" << ui.input->sizePolicy()
                              << "| chatView:" << ui.chatView->sizePolicy()
                              << "| imageStrip hint:" << ui.imageStrip->sizeHint();
            qInfo().noquote() << "MONLABEL geom:" << m_monitorLabel->geometry()
                              << "text:" << m_monitorLabel->text()
                              << "| statusbar:" << statusBar()->size()
                              << "| modelPath:" << ui.modelPath->text();
        }
    }
}

void MainWindow::closeEvent(QCloseEvent * event) {
    emit stopRequested(); // 直接置位停止标志
    m_thread->quit();
    if (!m_thread->wait(10000)) {
        // 极端情况下解码尚未返回（如正在解码超长提示词），强制结束
        m_thread->terminate();
        m_thread->wait();
    }
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject * watched, QEvent * event) {
    if (watched == ui.input && event->type() == QEvent::KeyPress) {
        auto * ke = static_cast<QKeyEvent *>(event);
        const bool isEnter = ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter;
        if (isEnter && !(ke->modifiers() & Qt::ShiftModifier)) {
            onSend(); // Enter 发送，Shift+Enter 换行
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}
