#include "chat/Markdown.h"
#include "common/Theme.h"

#include <QRegularExpression>
#include <QStringList>

#include <functional>

namespace {

QString escape(const QString & s) {
    return s.toHtmlEscaped();
}

// 正则全局替换（QString::replace 没有正则 + 回调重载，手写匹配循环）
QString replaceAll(QString s, const QRegularExpression & re,
                   const std::function<QString(const QRegularExpressionMatch &)> & repl) {
    QString out;
    int last = 0;
    QRegularExpressionMatchIterator it = re.globalMatch(s);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        out += s.mid(last, m.capturedStart() - last);
        out += repl(m);
        last = m.capturedEnd();
    }
    out += s.mid(last);
    return out;
}

// 行内格式：`code`、[text](url)、**bold**、*italic*。
// 所有替换对捕获内容先做 HTML 转义，防止模型输出注入标签。
QString inlineFormat(QString s) {
    // 1. 行内代码先用占位符保护，避免其中内容被后续加粗/斜体规则改写
    static const QRegularExpression reCode("`([^`]*)`");
    QStringList codePieces;
    s = replaceAll(s, reCode, [&codePieces](const QRegularExpressionMatch & m) {
        codePieces << m.captured(1);
        return QStringLiteral("\x01%1\x02").arg(codePieces.size() - 1);
    });

    // 2. 链接（主题色）
    static const QRegularExpression reLink(R"(\[([^\]]+)\]\(([^)]+)\))");
    const QString linkColor = Theme::css(Theme::palette().link);
    s = replaceAll(s, reLink, [&linkColor](const QRegularExpressionMatch & m) {
        return QStringLiteral("<a href=\"%1\" style=\"color:%2;text-decoration:underline;\">%3</a>")
            .arg(escape(m.captured(2)), linkColor, escape(m.captured(1)));
    });

    // 3. 加粗
    static const QRegularExpression reBold(R"(\*\*([^*]+)\*\*)");
    s = replaceAll(s, reBold, [](const QRegularExpressionMatch & m) {
        return QStringLiteral("<b>%1</b>").arg(escape(m.captured(1)));
    });

    // 4. 斜体
    static const QRegularExpression reItalic(R"(\*([^*]+)\*)");
    s = replaceAll(s, reItalic, [](const QRegularExpressionMatch & m) {
        return QStringLiteral("<i>%1</i>").arg(escape(m.captured(1)));
    });

    // 5. 恢复行内代码（主题色，胶囊样式）
    const ThemePalette & t = Theme::palette();
    for (int i = 0; i < codePieces.size(); i++) {
        s.replace(QStringLiteral("\x01%1\x02").arg(i),
                  QStringLiteral("<code style=\"background:%1;color:%2;"
                                 "padding:2px 6px;border-radius:5px;font-size:13px;"
                                 "font-family:Menlo,monospace;\">%3</code>")
                      .arg(Theme::css(t.inlineCodeBg), Theme::css(t.inlineCodeText),
                           escape(codePieces[i])));
    }
    return s;
}

// —— 表格辅助 ——
// 分隔行如 |---|---| 或 | :--- | ---: |
static bool isTableSeparator(const QString & line) {
    const QString s = line.trimmed();
    if (!s.startsWith('|')) {
        return false;
    }
    QString inner = s.mid(1);
    if (inner.endsWith('|')) {
        inner.chop(1);
    }
    const QStringList cells = inner.split('|');
    if (cells.isEmpty()) {
        return false;
    }
    for (const QString & c : cells) {
        const QString t = c.trimmed();
        if (t.isEmpty()) {
            return false;
        }
        for (const QChar ch : t) {
            if (ch != '-' && ch != ':' && ch != ' ') {
                return false;
            }
        }
    }
    return true;
}

static QStringList tableCells(const QString & line) {
    QString s = line.trimmed();
    if (s.startsWith('|')) {
        s = s.mid(1);
    }
    if (s.endsWith('|')) {
        s.chop(1);
    }
    QStringList cells;
    const QStringList parts = s.split('|');
    for (const QString & p : parts) {
        cells << p.trimmed();
    }
    return cells;
}

} // namespace

QString Markdown::toHtml(const QString & md) {
    const ThemePalette & t = Theme::palette();
    const QStringList lines = md.split('\n');

    static const QRegularExpression reH   (R"(^(#{1,4})\s+(.*)$)");
    static const QRegularExpression reHr  (R"(^\s*(-{3,}|\*{3,})\s*$)");
    static const QRegularExpression reUl  (R"(^\s*[-*+]\s+(.*)$)");
    static const QRegularExpression reOl  (R"(^\s*\d+[.)]\s+(.*)$)");
    static const QRegularExpression reTask(R"(^\s*[-*+]\s+\[([ xX])\]\s+(.*)$)");

    QStringList out;
    int i = 0;
    while (i < lines.size()) {
        const QString line = lines[i];

        // 围栏代码块（未闭合则一直收集到文末），深色底 + 语言标签条
        if (line.trimmed().startsWith("```")) {
            const QString lang = line.trimmed().mid(3).trimmed();
            QString code;
            i++;
            while (i < lines.size() && !lines[i].trimmed().startsWith("```")) {
                if (!code.isEmpty()) {
                    code += '\n';
                }
                code += lines[i];
                i++;
            }
            i++; // 跳过闭合 ```
            const QString labelBar = lang.isEmpty() ? QString()
                : QStringLiteral(
                      "<div style=\"background-color:%1;color:%2;font-family:Menlo,monospace;"
                      "font-size:11px;padding:6px 14px;border-radius:12px 12px 0 0;\">"
                      "<span style=\"color:%3;\">●</span> %4</div>")
                      .arg(Theme::css(t.codeHeaderBg), Theme::css(t.codeHeaderText),
                           Theme::css(t.heading), escape(lang));
            const QString preRadius = lang.isEmpty()
                                          ? QStringLiteral("12px")
                                          : QStringLiteral("0 0 12px 12px");
            out << QStringLiteral(
                       "<div style=\"margin:12px 0;\">%1"
                       "<pre style=\"background-color:%2;color:%3;font-family:Menlo,monospace;"
                       "font-size:14px;font-weight:500;padding:12px 14px;margin:0;"
                       "border-radius:%4;\">%5</pre></div>")
                       .arg(labelBar, Theme::css(t.codeBg), Theme::css(t.codeText),
                            preRadius, escape(code));
            continue;
        }

        // 表格：| 表头 | + 分隔行 + 数据行（表头着色 + 斑马纹）
        if (line.trimmed().startsWith('|') && i + 1 < lines.size() && isTableSeparator(lines[i + 1])) {
            const QStringList header = tableCells(line);
            i += 2; // 跳过表头与分隔行
            QStringList rows;
            int rowIndex = 0;
            while (i < lines.size() && lines[i].trimmed().startsWith('|')) {
                const QString rowBg = (rowIndex % 2 == 1)
                                          ? QStringLiteral(" style=\"background-color:%1;\"")
                                                .arg(Theme::css(t.tableAltBg))
                                          : QString();
                QString row = QStringLiteral("<tr>");
                const QStringList cells = tableCells(lines[i]);
                for (const QString & c : cells) {
                    row += QStringLiteral("<td%1>%2</td>").arg(rowBg, inlineFormat(c));
                }
                row += QStringLiteral("</tr>");
                rows << row;
                rowIndex++;
                i++;
            }
            QString head = QStringLiteral("<tr>");
            for (const QString & c : header) {
                head += QStringLiteral(
                            "<th align=\"left\" style=\"background-color:%1;padding:8px 10px;"
                            "font-weight:600;\">%2</th>")
                            .arg(Theme::css(t.tableHeaderBg), inlineFormat(c));
            }
            head += QStringLiteral("</tr>");
            out << QStringLiteral(
                       "<table border=\"1\" cellspacing=\"0\" cellpadding=\"8\" width=\"100%\" "
                       "style=\"border-color:%1;border-radius:10px;margin:10px 0;\">%2%3</table>")
                       .arg(Theme::css(t.tableBorder), head, rows.join(""));
            continue;
        }

        // 标题：分级字号 + 前两级使用强调色
        const auto mH = reH.match(line);
        if (mH.hasMatch()) {
            const int level = mH.captured(1).size();
            static const int sizes[] = {0, 24, 20, 17, 15};
            const int size = sizes[qBound(1, level, 4)];
            const QColor color = level <= 2 ? t.heading : t.textPrimary;
            out << QStringLiteral(
                       "<h%1 style=\"font-size:%2px;font-weight:700;color:%3;"
                       "margin:%4px 0 %5px 0;\">%6</h%1>")
                       .arg(level)
                       .arg(size)
                       .arg(Theme::css(color))
                       .arg(level == 1 ? 16 : 12)
                       .arg(level == 1 ? 10 : 6)
                       .arg(inlineFormat(mH.captured(2)));
            i++;
            continue;
        }

        // 分隔线：细线表格实现（QTextDocument 对 hr 样式支持有限）
        if (reHr.match(line).hasMatch()) {
            out << QStringLiteral(
                       "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" "
                       "style=\"margin:14px 0;\"><tr><td style=\"border-top:1px solid %1;\">"
                       "</td></tr></table>")
                       .arg(Theme::css(t.tableBorder));
            i++;
            continue;
        }

        // 引用：表格单元格承载背景与左侧强调条（QTextDocument 对 blockquote 样式支持有限）
        if (line.trimmed().startsWith('>')) {
            QStringList quote;
            while (i < lines.size() && lines[i].trimmed().startsWith('>')) {
                quote << lines[i].trimmed().mid(1).trimmed();
                i++;
            }
            out << QStringLiteral(
                       "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" "
                       "style=\"margin:10px 0;\"><tr><td style=\"background-color:%1;"
                       "border-left:3px solid %2;padding:8px 14px;color:%3;\">%4</td></tr></table>")
                       .arg(Theme::css(t.quoteBg), Theme::css(t.quoteBorder),
                            Theme::css(t.quoteText), inlineFormat(quote.join("<br>")));
            continue;
        }

        // 任务列表：- [ ] / - [x]（主题色勾选标记）
        if (reTask.match(line).hasMatch()) {
            QStringList items;
            while (i < lines.size()) {
                const auto m = reTask.match(lines[i]);
                if (!m.hasMatch()) {
                    break;
                }
                const bool done = m.captured(1) != " ";
                QString content = inlineFormat(m.captured(2));
                if (done) {
                    content = QStringLiteral("<s style=\"color:%1;\">%2</s>")
                                  .arg(Theme::css(t.textMuted), content);
                }
                const QString marker = done
                    ? QStringLiteral("<span style=\"color:%1;\">✔</span>").arg(Theme::css(t.heading))
                    : QStringLiteral("<span style=\"color:%1;\">☐</span>").arg(Theme::css(t.textMuted));
                items << QStringLiteral("<li style=\"list-style-type:none;margin:3px 0;\">%1&nbsp;&nbsp;%2</li>")
                             .arg(marker, content);
                i++;
            }
            out << QStringLiteral("<ul style=\"margin:6px 0;padding-left:8px;\">%1</ul>").arg(items.join(""));
            continue;
        }

        // 列表（相邻同类条目合并为一个 <ul>/<ol>）
        const bool isUl = reUl.match(line).hasMatch();
        const bool isOl = reOl.match(line).hasMatch();
        if (isUl || isOl) {
            QStringList items;
            while (i < lines.size()) {
                const auto m = isOl ? reOl.match(lines[i]) : reUl.match(lines[i]);
                if (!m.hasMatch()) {
                    break;
                }
                items << QStringLiteral("<li style=\"margin:3px 0;\">%1</li>")
                             .arg(inlineFormat(m.captured(1)));
                i++;
            }
            out << (isOl ? QStringLiteral("<ol style=\"margin:6px 0;padding-left:22px;\">")
                         : QStringLiteral("<ul style=\"margin:6px 0;padding-left:22px;\">"))
                   + items.join("")
                   + (isOl ? QStringLiteral("</ol>") : QStringLiteral("</ul>"));
            continue;
        }

        // 空行
        if (line.trimmed().isEmpty()) {
            i++;
            continue;
        }

        // 普通段落：合并连续文本行
        QStringList para;
        while (i < lines.size()
               && !lines[i].trimmed().isEmpty()
               && !lines[i].trimmed().startsWith("```")
               && !lines[i].trimmed().startsWith('>')
               && !lines[i].trimmed().startsWith('|')
               && !reH.match(lines[i]).hasMatch()
               && !reHr.match(lines[i]).hasMatch()
               && !reUl.match(lines[i]).hasMatch()
               && !reOl.match(lines[i]).hasMatch()
               && !reTask.match(lines[i]).hasMatch()) {
            para << lines[i].trimmed();
            i++;
        }
        out << QStringLiteral("<p style=\"margin:6px 0;\">%1</p>").arg(inlineFormat(para.join("<br>")));
    }
    return out.join("");
}
