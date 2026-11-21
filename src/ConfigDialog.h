#pragma once

#include "LlamaChat.h"

#include <QDialog>
#include <QString>

#include "ui_configdialog.h" // 由 uic 从 ui/configdialog.ui 生成

// 应用完整配置：UI 层配置 + llama.cpp 参数
struct AppConfig {
    QString     systemPrompt; // 系统提示词（空 = 不注入）
    ChatOptions chat;         // 采样 / 性能 / 多模态参数
};

// 配置页面：精简配置（常用参数）+ 全量配置（高级参数）两个标签页
// 界面布局见 ui/configdialog.ui（Qt Designer 编辑）
class ConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConfigDialog(const AppConfig & config, QWidget * parent = nullptr);
    AppConfig config() const;

private slots:
    void browseMmproj();

private:
    Ui::ConfigDialog ui;
};
