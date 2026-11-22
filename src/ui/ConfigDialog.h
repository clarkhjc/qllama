#pragma once

#include "chat/LlamaChat.h"

#include <QDialog>
#include <QString>

#include "ui_configdialog.h" // 由 uic 从 ui/configdialog.ui 生成

// 应用完整配置：UI 层配置 + llama.cpp 参数
struct AppConfig {
    QString     systemPrompt; // 系统提示词（空 = 不注入）
    QString     theme = QStringLiteral("light"); // 界面主题：light / dark / pink
    QString     language = QStringLiteral("zh"); // 界面语言：zh / en
    ChatOptions chat;         // 采样 / 性能 / 多模态参数
};

// 配置页面：精简配置（常用参数）+ 全量配置（高级参数）两个标签页
// 界面布局见 ui/configdialog.ui（Qt Designer 编辑）
class ConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConfigDialog(const AppConfig & config, QWidget * parent = nullptr);
    AppConfig config() const;

signals:
    void languageChanged(const QString & code); // 语言按钮实时预览

private slots:
    void browseMmproj();

private:
    void setTheme(const QString & name);    // 主题按钮：立即预览
    void setLanguage(const QString & code); // 语言按钮：立即预览
    void refreshIcons();                    // 主题切换后按新主题重设图标
    void refreshTexts();                    // 语言切换后刷新动态文字

    Ui::ConfigDialog ui;
    QString m_theme = QStringLiteral("light");
    QString m_language = QStringLiteral("zh");
};
