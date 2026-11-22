#pragma once

#include <QColor>
#include <QIcon>
#include <QString>
#include <QStringList>

// 主题调色板。
// 控件外观全部在 assets/themes/<name>.qss 中；这里只保留 QSS 覆盖不到的部分：
// 聊天记录 HTML（气泡/代码块/表格等）、头像渐变、状态灯与监控颜色、单色图标着色。
struct ThemePalette {
    QString name; // light / dark / pink

    // 基础
    QColor textPrimary;
    QColor textSecondary;
    QColor textMuted;
    QColor accent;
    QColor link;
    QColor icon;        // 单色 SVG 图标着色
    QColor placeholder; // 输入框占位文字
    QColor windowBg;    // 窗口底色（QPalette::Window）

    // 聊天消息
    QColor userName;
    QColor assistantName;
    QColor userBubble;
    QColor userBubbleText;
    QColor assistantBubble;
    QColor assistantBubbleBorder;
    QColor avatarUser1;
    QColor avatarUser2;
    QColor avatarAssistant1;
    QColor avatarAssistant2;
    QColor welcomeTitle;
    QColor welcomeSubtitle;
    QColor statText;
    QColor errorText;

    // Markdown 渲染
    QColor codeBg;
    QColor codeHeaderBg;
    QColor codeText;
    QColor codeHeaderText;
    QColor inlineCodeBg;
    QColor inlineCodeText;
    QColor tableHeaderBg;
    QColor tableBorder;
    QColor quoteBg;
    QColor quoteBorder;
    QColor quoteText;

    // 状态点与系统监控
    QColor dotIdle;
    QColor dotLoading;
    QColor dotOk;
    QColor dotError;
    QColor monitorOk;
    QColor monitorWarn;
    QColor monitorBad;
    QColor monitorNA;
};

namespace Theme {

// 当前主题（apply 后生效）
const ThemePalette & palette();
QString currentName();

// 支持的主题：light / dark / pink
QStringList names();
QString     displayName(const QString & name); // 浅色 / 深色 / 粉色
bool        isValid(const QString & name);
QString     normalized(const QString & name);  // 非法值回退 light

// 应用主题：加载 assets/themes/<name>.qss 并同步 QPalette（全局生效）
void apply(const QString & name);

// 单色 SVG 图标按主题着色（-white 变体保持原样，用于渐变按钮/头像）
QIcon icon(const QString & svgPathOrName);

// QColor → HTML/CSS 颜色字符串（带透明度时输出 rgba()）
QString css(const QColor & c);

} // namespace Theme
