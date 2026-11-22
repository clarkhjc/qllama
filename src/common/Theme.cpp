#include "common/Theme.h"

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QPainter>
#include <QPalette>
#include <QPixmap>

namespace {

QString s_current = QStringLiteral("light");

// —— 浅色（默认） ——
ThemePalette makeLight() {
    ThemePalette p;
    p.name             = "light";
    p.textPrimary      = QColor("#33334a");
    p.textSecondary    = QColor("#5a5a70");
    p.textMuted        = QColor("#9a9aa8");
    p.accent           = QColor("#5b7cff");
    p.link             = QColor("#3a6df0");
    p.icon             = QColor("#4a4a60");
    p.placeholder      = QColor("#9a9ab0");
    p.windowBg         = QColor("#f2f4fa");

    p.userName         = QColor("#3a6df0");
    p.assistantName    = QColor("#7c5bff");
    p.userBubble       = QColor(74, 125, 255, 235);
    p.userBubbleText   = QColor("#ffffff");
    p.assistantBubble  = QColor("#fbfcff");
    p.assistantBubbleBorder = QColor(255, 255, 255, 242);
    p.avatarUser1      = QColor("#4a7dff");
    p.avatarUser2      = QColor("#6a5bff");
    p.avatarAssistant1 = QColor("#8a5bff");
    p.avatarAssistant2 = QColor("#d06bff");
    p.welcomeTitle     = QColor("#3a3a4e");
    p.welcomeSubtitle  = QColor("#8a8a9e");
    p.statText         = QColor("#b0b0c0");
    p.errorText        = QColor("#c62828");

    p.codeBg           = QColor("#1e1e2e");
    p.codeHeaderBg     = QColor("#2b2b40");
    p.codeText         = QColor("#ffffff");
    p.codeHeaderText   = QColor("#c8c8da");
    p.codeBorder       = QColor(255, 255, 255, 26);
    p.inlineCodeBg     = QColor(91, 124, 255, 26);
    p.inlineCodeText   = QColor("#c04a7c");
    p.tableHeaderBg    = QColor("#e9ecf7");
    p.tableBorder      = QColor("#d8dbe8");
    p.tableAltBg       = QColor(91, 124, 255, 14);
    p.quoteBg          = QColor(91, 124, 255, 26);
    p.quoteBorder      = QColor("#5b7cff");
    p.quoteText        = QColor("#5a5a70");
    p.heading          = QColor("#5b7cff");

    p.dotIdle          = QColor("#9a9aa8");
    p.dotLoading       = QColor("#f5a623");
    p.dotOk            = QColor("#2ecc71");
    p.dotError         = QColor("#e5484d");
    p.monitorOk        = QColor("#2ecc71");
    p.monitorWarn      = QColor("#f5a623");
    p.monitorBad       = QColor("#e5484d");
    p.monitorNA        = QColor("#9a9aa8");
    return p;
}

// —— 深色 ——
ThemePalette makeDark() {
    ThemePalette p;
    p.name             = "dark";
    p.textPrimary      = QColor("#e4e4ee");
    p.textSecondary    = QColor("#a6a6bc");
    p.textMuted        = QColor("#7c7c94");
    p.accent           = QColor("#6f8cff");
    p.link             = QColor("#8fa8ff");
    p.icon             = QColor("#d0d0e0");
    p.placeholder      = QColor("#6e6e86");
    p.windowBg         = QColor("#20212e");

    p.userName         = QColor("#8fa8ff");
    p.assistantName    = QColor("#b799ff");
    p.userBubble       = QColor(96, 132, 255, 235);
    p.userBubbleText   = QColor("#ffffff");
    p.assistantBubble  = QColor("#30313f");
    p.assistantBubbleBorder = QColor(255, 255, 255, 36);
    p.avatarUser1      = QColor("#4a7dff");
    p.avatarUser2      = QColor("#6a5bff");
    p.avatarAssistant1 = QColor("#8a5bff");
    p.avatarAssistant2 = QColor("#d06bff");
    p.welcomeTitle     = QColor("#e4e4ee");
    p.welcomeSubtitle  = QColor("#a6a6bc");
    p.statText         = QColor("#6e6e86");
    p.errorText        = QColor("#ff7a7a");

    p.codeBg           = QColor("#12121c");
    p.codeHeaderBg     = QColor("#1e1e2c");
    p.codeText         = QColor("#e4e4ee");
    p.codeHeaderText   = QColor("#8a8aa6");
    p.codeBorder       = QColor(255, 255, 255, 22);
    p.inlineCodeBg     = QColor(143, 168, 255, 36);
    p.inlineCodeText   = QColor("#ff9ec4");
    p.tableHeaderBg    = QColor(255, 255, 255, 23);
    p.tableBorder      = QColor(255, 255, 255, 41);
    p.tableAltBg       = QColor(255, 255, 255, 10);
    p.quoteBg          = QColor(111, 140, 255, 33);
    p.quoteBorder      = QColor("#6f8cff");
    p.quoteText        = QColor("#a6a6bc");
    p.heading          = QColor("#8fa8ff");

    p.dotIdle          = QColor("#7c7c94");
    p.dotLoading       = QColor("#f5a623");
    p.dotOk            = QColor("#2ecc71");
    p.dotError         = QColor("#e5484d");
    p.monitorOk        = QColor("#3ddc84");
    p.monitorWarn      = QColor("#ffb84d");
    p.monitorBad       = QColor("#ff6b6b");
    p.monitorNA        = QColor("#7c7c94");
    return p;
}

// —— 粉色 ——
ThemePalette makePink() {
    ThemePalette p;
    p.name             = "pink";
    p.textPrimary      = QColor("#4a3340");
    p.textSecondary    = QColor("#7a5a6a");
    p.textMuted        = QColor("#a87e92");
    p.accent           = QColor("#ff6fa5");
    p.link             = QColor("#d94f8a");
    p.icon             = QColor("#7a4a60");
    p.placeholder      = QColor("#b08a9c");
    p.windowBg         = QColor("#fff6fa");

    p.userName         = QColor("#e84a86");
    p.assistantName    = QColor("#b25bd0");
    p.userBubble       = QColor(255, 111, 165, 235);
    p.userBubbleText   = QColor("#ffffff");
    p.assistantBubble  = QColor("#fff9fc");
    p.assistantBubbleBorder = QColor(255, 255, 255, 242);
    p.avatarUser1      = QColor("#ff6fa5");
    p.avatarUser2      = QColor("#c86bff");
    p.avatarAssistant1 = QColor("#b25bd0");
    p.avatarAssistant2 = QColor("#ff8ac2");
    p.welcomeTitle     = QColor("#4a3340");
    p.welcomeSubtitle  = QColor("#9a7284");
    p.statText         = QColor("#b08a9c");
    p.errorText        = QColor("#d64550");

    p.codeBg           = QColor("#2e1f2a");
    p.codeHeaderBg     = QColor("#3a2836");
    p.codeText         = QColor("#ffffff");
    p.codeHeaderText   = QColor("#e8c0d4");
    p.codeBorder       = QColor(255, 255, 255, 26);
    p.inlineCodeBg     = QColor(255, 111, 165, 36);
    p.inlineCodeText   = QColor("#c8447e");
    p.tableHeaderBg    = QColor("#ffe9f2");
    p.tableBorder      = QColor("#f3cfdd");
    p.tableAltBg       = QColor(255, 111, 165, 14);
    p.quoteBg          = QColor(255, 111, 165, 26);
    p.quoteBorder      = QColor("#ff6fa5");
    p.quoteText        = QColor("#7a5a6a");
    p.heading          = QColor("#e84a86");

    p.dotIdle          = QColor("#b08a9c");
    p.dotLoading       = QColor("#f5a623");
    p.dotOk            = QColor("#2ecc71");
    p.dotError         = QColor("#e5484d");
    p.monitorOk        = QColor("#2ecc71");
    p.monitorWarn      = QColor("#f5a623");
    p.monitorBad       = QColor("#e5484d");
    p.monitorNA        = QColor("#b08a9c");
    return p;
}

const ThemePalette & paletteFor(const QString & name) {
    static const ThemePalette light = makeLight();
    static const ThemePalette dark  = makeDark();
    static const ThemePalette pink  = makePink();
    if (name == "dark") {
        return dark;
    }
    if (name == "pink") {
        return pink;
    }
    return light;
}

} // namespace

namespace Theme {

const ThemePalette & palette() {
    return paletteFor(s_current);
}

QString currentName() {
    return s_current;
}

QStringList names() {
    return {QStringLiteral("light"), QStringLiteral("dark"), QStringLiteral("pink")};
}

QString displayName(const QString & name) {
    if (name == "dark") {
        return QStringLiteral("深色");
    }
    if (name == "pink") {
        return QStringLiteral("粉色");
    }
    return QStringLiteral("浅色");
}

bool isValid(const QString & name) {
    return names().contains(name);
}

QString normalized(const QString & name) {
    return isValid(name) ? name : QStringLiteral("light");
}

void apply(const QString & name) {
    s_current = normalized(name);
    // 控件外观：assets/themes/<name>.qss（全部控件样式集中于此）
    QFile qss(QStringLiteral(":/assets/themes/%1.qss").arg(s_current));
    if (qss.open(QIODevice::ReadOnly)) {
        qApp->setStyleSheet(QString::fromUtf8(qss.readAll()));
    }
    // 调色板：覆盖 QSS 管不到的原生绘制（占位文字、选中高亮、原生控件底纹等）
    const ThemePalette & p = palette();
    const bool dark = p.name == "dark";
    QPalette pal = qApp->palette();
    pal.setColor(QPalette::Window, p.windowBg);
    pal.setColor(QPalette::WindowText, p.textPrimary);
    pal.setColor(QPalette::Base, dark ? QColor("#181926") : QColor("#ffffff"));
    pal.setColor(QPalette::AlternateBase, dark ? QColor("#1e1f2c") : QColor("#eef1f8"));
    pal.setColor(QPalette::Text, p.textPrimary);
    pal.setColor(QPalette::PlaceholderText, p.placeholder);
    pal.setColor(QPalette::ButtonText, p.textPrimary);
    pal.setColor(QPalette::Highlight, p.accent);
    pal.setColor(QPalette::HighlightedText, Qt::white);
    qApp->setPalette(pal);
}

QIcon icon(const QString & svgPathOrName) {
    const QString path = svgPathOrName.startsWith(":/")
                             ? svgPathOrName
                             : QStringLiteral(":/assets/icons/%1.svg").arg(svgPathOrName);
    if (path.endsWith("-white.svg")) {
        return QIcon(path); // 白色变体画在渐变底上，不随主题改色
    }
    // 按“路径 + 主题”缓存着色结果
    static QHash<QString, QIcon> cache;
    const QString key = path + '|' + palette().name;
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }
    const QPixmap src = QIcon(path).pixmap(64, 64);
    if (src.isNull()) {
        return QIcon(path);
    }
    QPixmap pm = src;
    QPainter painter(&pm);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pm.rect(), palette().icon);
    painter.end();
    const QIcon result(pm);
    cache.insert(key, result);
    return result;
}

QString css(const QColor & c) {
    if (c.alpha() == 255) {
        return c.name();
    }
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(QString::number(c.alphaF(), 'f', 2));
}

} // namespace Theme
