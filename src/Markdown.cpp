#include "Markdown.h"

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

    // 2. 链接（主题蓝）
    static const QRegularExpression reLink(R"(\[([^\]]+)\]\(([^)]+)\))");
    s = replaceAll(s, reLink, [](const QRegularExpressionMatch & m) {
        return QStringLiteral("<a href=\"%1\" style=\"color:#3a6df0;text-decoration:underline;\">%2</a>")
            .arg(escape(m.captured(2)), escape(m.captured(1)));
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

    // 5. 恢复行内代码
    for (int i = 0; i < codePieces.size(); i++) {
        s.replace(QStringLiteral("\x01%1\x02").arg(i),
                  QStringLiteral("<code style=\"background:rgba(91,124,255,0.10);color:#c04a7c;"
                                 "padding:1px 5px;border-radius:4px;font-family:Menlo,monospace;\">%1</code>")
                      .arg(escape(codePieces[i])));
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
                      "<div style=\"background:#2b2b40;color:#9a9ac0;font-family:Menlo,monospace;"
                      "font-size:12px;padding:5px 12px;border-radius:10px 10px 0 0;\">%1</div>")
                      .arg(escape(lang));
            out << QStringLiteral(
                       "<div style=\"background:#1e1e2e;border-radius:10px;margin:8px 0;\">%1"
                       "<pre style=\"background:transparent;color:#e8e8f0;font-family:Menlo,monospace;"
                       "font-size:13px;padding:8px 12px;margin:0;\">%2</pre></div>")
                       .arg(labelBar, escape(code));
            continue;
        }

        // 表格：| 表头 | + 分隔行 + 数据行
        if (line.trimmed().startsWith('|') && i + 1 < lines.size() && isTableSeparator(lines[i + 1])) {
            const QStringList header = tableCells(line);
            i += 2; // 跳过表头与分隔行
            QStringList rows;
            while (i < lines.size() && lines[i].trimmed().startsWith('|')) {
                QString row = QStringLiteral("<tr>");
                const QStringList cells = tableCells(lines[i]);
                for (const QString & c : cells) {
                    row += QStringLiteral("<td>%1</td>").arg(inlineFormat(c));
                }
                row += QStringLiteral("</tr>");
                rows << row;
                i++;
            }
            QString head = QStringLiteral("<tr>");
            for (const QString & c : header) {
                head += QStringLiteral("<th bgcolor=\"#e9ecf7\" align=\"left\">%1</th>").arg(inlineFormat(c));
            }
            head += QStringLiteral("</tr>");
            out << QStringLiteral(
                       "<table border=\"1\" cellspacing=\"0\" cellpadding=\"6\" width=\"100%\" "
                       "style=\"border-color:#d8dbe8;border-radius:8px;margin:6px 0;\">%1%2</table>")
                       .arg(head, rows.join(""));
            continue;
        }

        // 标题
        const auto mH = reH.match(line);
        if (mH.hasMatch()) {
            const int level = mH.captured(1).size();
            out << QStringLiteral("<h%1 style=\"margin:10px 0 4px 0;\">%2</h%1>")
                       .arg(level)
                       .arg(inlineFormat(mH.captured(2)));
            i++;
            continue;
        }

        // 分隔线
        if (reHr.match(line).hasMatch()) {
            out << QStringLiteral("<hr>");
            i++;
            continue;
        }

        // 引用（主题蓝左边框 + 浅蓝底）
        if (line.trimmed().startsWith('>')) {
            QStringList quote;
            while (i < lines.size() && lines[i].trimmed().startsWith('>')) {
                quote << lines[i].trimmed().mid(1).trimmed();
                i++;
            }
            out << QStringLiteral(
                       "<blockquote style=\"background:rgba(91,124,255,0.10);border-left:3px solid #5b7cff;"
                       "margin:6px 0;padding:6px 12px;color:#5a5a70;\">%1</blockquote>")
                       .arg(inlineFormat(quote.join("<br>")));
            continue;
        }

        // 任务列表：- [ ] / - [x]
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
                    content = QStringLiteral("<s style=\"color:#9a9ab0;\">%1</s>").arg(content);
                }
                items << QStringLiteral("<li style=\"list-style-type:none;\">%1 %2</li>")
                             .arg(done ? QStringLiteral("☑") : QStringLiteral("☐"), content);
                i++;
            }
            out << QStringLiteral("<ul style=\"margin:4px 0;padding-left:6px;\">%1</ul>").arg(items.join(""));
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
                items << QStringLiteral("<li>%1</li>").arg(inlineFormat(m.captured(1)));
                i++;
            }
            out << (isOl ? QStringLiteral("<ol>") : QStringLiteral("<ul>"))
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
        out << QStringLiteral("<p style=\"margin:4px 0;\">%1</p>").arg(inlineFormat(para.join("<br>")));
    }
    return out.join("");
}
