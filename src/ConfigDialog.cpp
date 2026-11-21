#include "ConfigDialog.h"
#include "Logging.h"
#include "MacVibrancy.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QUrl>

ConfigDialog::ConfigDialog(const AppConfig & config, QWidget * parent) : QDialog(parent) {
    ui.setupUi(this); // 界面布局见 ui/configdialog.ui（Qt Designer 编辑）
    macSetupVibrancy(this, config.vibrancy); // 磨砂玻璃：半透明属性必须在显示前设置
    if (!config.vibrancy) {
        // 关闭磨砂时窗口不透明，铺浅色渐变底，玻璃卡片叠于其上
        setStyleSheet("QDialog { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                      "stop:0 #f2f4fa, stop:1 #e4e8f4); }");
    }

    // 输入框一键清除
    ui.mmprojEdit->setClearButtonEnabled(true);
    ui.systemPromptEdit->setClearButtonEnabled(true);

    // —— 用当前配置填充控件 ——
    ui.tempSpin->setValue(config.chat.temperature);
    ui.ctxSpin->setValue((int) config.chat.n_ctx);
    ui.nPredictSpin->setValue(config.chat.n_predict);
    ui.systemPromptEdit->setText(config.systemPrompt);

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
    ui.vibrancyCheck->setChecked(config.vibrancy);

    // —— 连接 ——
    connect(ui.browseMmprojBtn, &QPushButton::clicked, this, &ConfigDialog::browseMmproj);
    connect(ui.seedRandomCheck, &QCheckBox::toggled, ui.seedSpin, &QSpinBox::setDisabled);
    connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    ui.buttonBox->button(QDialogButtonBox::Ok)->setText("确定");
    ui.buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");

    // —— 图标 ——
    ui.browseMmprojBtn->setIcon(QIcon(":/assets/icons/folder.svg"));
    ui.buttonBox->button(QDialogButtonBox::Ok)->setIcon(QIcon(":/assets/icons/check.svg"));
    ui.buttonBox->button(QDialogButtonBox::Cancel)->setIcon(QIcon(":/assets/icons/x.svg"));
    ui.tabs->setTabIcon(0, QIcon(":/assets/icons/star.svg"));     // 精简配置
    ui.tabs->setTabIcon(1, QIcon(":/assets/icons/settings.svg")); // 全量配置
    ui.tabs->setIconSize(QSize(16, 16));

    // —— 打开日志：在 Finder 中定位日志文件 ——
    ui.openLogBtn->setIcon(QIcon(":/assets/icons/folder.svg"));
    ui.openLogBtn->setToolTip("日志文件：" + logFilePath());
    connect(ui.openLogBtn, &QPushButton::clicked, this, [] {
        const QString path = logFilePath();
        if (QFileInfo::exists(path)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        }
    });
}

void ConfigDialog::showEvent(QShowEvent * event) {
    QDialog::showEvent(event);
    static bool vibrancyDone = false;
    if (!vibrancyDone) {
        vibrancyDone = true;
        macEnableVibrancy(this); // 磨砂层：窗口显示后插入
    }
}

AppConfig ConfigDialog::config() const {
    AppConfig c;
    c.systemPrompt = ui.systemPromptEdit->text().trimmed();

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
    o.mmprojPath      = ui.mmprojEdit->text().trimmed().toStdString();
    c.vibrancy        = ui.vibrancyCheck->isChecked();
    return c;
}

void ConfigDialog::browseMmproj() {
    const QString path = QFileDialog::getOpenFileName(this, "选择多模态投影模型",
        QFileInfo(ui.mmprojEdit->text()).absolutePath(),
        "mmproj 模型 (*.gguf);;GGUF 模型 (*.gguf);;所有文件 (*)");
    if (!path.isEmpty()) {
        ui.mmprojEdit->setText(path);
    }
}
