#include "ui/ConfigDialog.h"
#include "common/I18n.h"
#include "common/Logging.h"
#include "common/Theme.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QUrl>

ConfigDialog::ConfigDialog(const AppConfig & config, QWidget * parent) : QDialog(parent) {
    ui.setupUi(this); // 界面布局见 ui/configdialog.ui（Qt Designer 编辑）

    // 标签页内容放入滚动区：内容高度超过屏幕时滚动，底部「取消/确定」始终可见
    ui.tabsScroll->viewport()->setAutoFillBackground(false);
    if (const QScreen * scr = screen()) {
        const int maxH = qMax(420, scr->availableGeometry().height() - 140); // 预留标题栏/菜单栏/Dock
        setMaximumHeight(maxH);
        resize(width(), qMin(maxH, 700)); // 内容超出时由滚动区滚动
    }

    // 输入框一键清除
    ui.mmprojEdit->setClearButtonEnabled(true);
    ui.systemPromptEdit->setClearButtonEnabled(true);

    // —— 用当前配置填充控件 ——
    ui.tempSpin->setValue(config.chat.temperature);
    ui.ctxSpin->setValue((int) config.chat.n_ctx);
    ui.nPredictSpin->setValue(config.chat.n_predict);
    ui.systemPromptEdit->setText(config.systemPrompt);

    // 主题（当前生效值；按钮点击立即预览）
    m_theme = Theme::normalized(config.theme);
    ui.themeLightBtn->setChecked(m_theme == "light");
    ui.themeDarkBtn->setChecked(m_theme == "dark");
    ui.themePinkBtn->setChecked(m_theme == "pink");

    // 语言（按钮点击立即预览）
    m_language = I18n::normalized(config.language);
    ui.langZhBtn->setChecked(m_language == "zh");
    ui.langEnBtn->setChecked(m_language == "en");

    ui.mmprojEdit->setText(QString::fromStdString(config.chat.mmprojPath));
    ui.topKSpin->setValue(config.chat.top_k);
    ui.topPSpin->setValue(config.chat.top_p);
    ui.minPSpin->setValue(config.chat.min_p);
    ui.gpuLayersSpin->setValue(config.chat.n_gpu_layers);
    ui.batchSpin->setValue((int) config.chat.n_batch);
    ui.threadsSpin->setValue(config.chat.n_threads);
    const bool seedRandom = config.chat.seed == LLAMA_DEFAULT_SEED;
    ui.seedRandomCheck->setChecked(seedRandom);
    ui.seedSpin->setEnabled(!seedRandom);
    if (!seedRandom) {
        ui.seedSpin->setValue((int) config.chat.seed);
    }
    ui.thinkCheck->setChecked(config.chat.enable_thinking);
    ui.autoTrimCheck->setChecked(config.chat.auto_trim);

    // —— 连接 ——
    connect(ui.browseMmprojBtn, &QPushButton::clicked, this, &ConfigDialog::browseMmproj);
    connect(ui.seedRandomCheck, &QCheckBox::toggled, ui.seedSpin, &QSpinBox::setDisabled);
    connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 主题切换按钮：点击立即应用（实时预览；取消时主窗口会恢复原主题）
    connect(ui.themeLightBtn, &QPushButton::clicked, this, [this] { setTheme("light"); });
    connect(ui.themeDarkBtn, &QPushButton::clicked, this, [this] { setTheme("dark"); });
    connect(ui.themePinkBtn, &QPushButton::clicked, this, [this] { setTheme("pink"); });

    // 语言切换按钮：点击立即预览（取消时主窗口会恢复原语言）
    connect(ui.langZhBtn, &QPushButton::clicked, this, [this] { setLanguage("zh"); });
    connect(ui.langEnBtn, &QPushButton::clicked, this, [this] { setLanguage("en"); });

    // —— 图标（单色 SVG 按主题着色，见 Theme::icon） ——
    refreshIcons();
    ui.tabs->setIconSize(QSize(16, 16));

    // 静态控件文字（.ui 中文 → 当前语言）+ 动态文字
    I18n::retranslateTree(this);
    refreshTexts();

    // —— 打开日志：在 Finder 中定位日志文件 ——
    connect(ui.openLogBtn, &QPushButton::clicked, this, [] {
        const QString path = logFilePath();
        if (QFileInfo::exists(path)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        }
    });
}

// 主题按钮：应用并重设图标（QSS 由 Theme::apply 全局替换）
void ConfigDialog::setTheme(const QString & name) {
    m_theme = Theme::normalized(name);
    Theme::apply(m_theme);
    refreshIcons();
}

// 语言按钮：应用并重译界面
void ConfigDialog::setLanguage(const QString & code) {
    m_language = I18n::normalized(code);
    I18n::setLanguage(m_language);
    I18n::retranslateTree(this);
    refreshTexts();
    emit languageChanged(m_language);
}

// 语言切换后刷新代码中设置的文字
void ConfigDialog::refreshTexts() {
    ui.buttonBox->button(QDialogButtonBox::Ok)->setText(I18n::t("确定"));
    ui.buttonBox->button(QDialogButtonBox::Cancel)->setText(I18n::t("取消"));
    ui.openLogBtn->setToolTip(I18n::t("日志文件：%1").arg(logFilePath()));
}

void ConfigDialog::refreshIcons() {
    ui.browseMmprojBtn->setIcon(Theme::icon("folder"));
    ui.buttonBox->button(QDialogButtonBox::Ok)->setIcon(Theme::icon("check"));
    ui.buttonBox->button(QDialogButtonBox::Cancel)->setIcon(Theme::icon("x"));
    ui.openLogBtn->setIcon(Theme::icon("folder"));
    ui.tabs->setTabIcon(0, Theme::icon("star"));     // 精简配置
    ui.tabs->setTabIcon(1, Theme::icon("settings")); // 全量配置
}

AppConfig ConfigDialog::config() const {
    AppConfig c;
    c.systemPrompt = ui.systemPromptEdit->text().trimmed();
    c.theme        = m_theme;
    c.language     = m_language;

    ChatOptions & o = c.chat;
    o.n_ctx           = (uint32_t) ui.ctxSpin->value();
    o.n_batch         = (uint32_t) ui.batchSpin->value();
    o.n_gpu_layers    = ui.gpuLayersSpin->value();
    o.n_threads       = ui.threadsSpin->value();
    o.n_predict       = ui.nPredictSpin->value();
    o.temperature     = (float) ui.tempSpin->value();
    o.top_k           = ui.topKSpin->value();
    o.top_p           = (float) ui.topPSpin->value();
    o.min_p           = (float) ui.minPSpin->value();
    o.seed            = ui.seedRandomCheck->isChecked() ? LLAMA_DEFAULT_SEED : (uint32_t) ui.seedSpin->value();
    o.enable_thinking = ui.thinkCheck->isChecked();
    o.auto_trim       = ui.autoTrimCheck->isChecked();
    o.mmprojPath      = ui.mmprojEdit->text().trimmed().toStdString();
    return c;
}

void ConfigDialog::browseMmproj() {
    const QString path = QFileDialog::getOpenFileName(this, I18n::t("选择多模态投影模型"),
        QFileInfo(ui.mmprojEdit->text()).absolutePath(),
        I18n::t("mmproj 模型 (*.gguf);;GGUF 模型 (*.gguf);;所有文件 (*)"));
    if (!path.isEmpty()) {
        ui.mmprojEdit->setText(path);
    }
}
