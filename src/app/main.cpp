#include "common/I18n.h"
#include "common/Logging.h"
#include "ui/MainWindow.h"
#include "chat/Markdown.h"
#include "common/Theme.h"

#include <QApplication>
#include <QDir>
#include <QSettings>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char * argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("qllama");
    app.setApplicationDisplayName("qllama 本地大模型对话");
    app.setOrganizationName("qllama"); // QSettings 持久化配置

    setupFileLogging(); // 日志文件：~/Library/Logs/qllama/qllama.log

    // 主题：QLLAMA_THEME 环境变量（调试/截图）> 上次选择（QSettings）> 浅色
    const QSettings settings;
    QString theme = settings.value("ui/theme", "light").toString();
    if (qEnvironmentVariableIsSet("QLLAMA_THEME")) {
        theme = qEnvironmentVariable("QLLAMA_THEME");
    }
    Theme::apply(theme); // 加载 assets/themes/<name>.qss + 调色板

    // 语言：QLLAMA_LANG 环境变量（调试/截图）> 上次选择（QSettings）> 中文
    QString language = settings.value("ui/language", "zh").toString();
    if (qEnvironmentVariableIsSet("QLLAMA_LANG")) {
        language = qEnvironmentVariable("QLLAMA_LANG");
    }
    I18n::setLanguage(language);

    // 跨线程 QueuedConnection 需要注册自定义类型的元类型
    qRegisterMetaType<ChatOptions>("ChatOptions");
    qRegisterMetaType<ChatMessage>("ChatMessage");
    qRegisterMetaType<ChatResult>("ChatResult");
    qRegisterMetaType<QVector<ChatMessage>>("QVector<ChatMessage>");

    // --self-test [提示词] [--image 图片路径]：自动加载模型、跑两轮对话、截图退出
    const QStringList args = app.arguments();
    const int selfTestIdx = args.indexOf("--self-test");
    const bool selfTest = selfTestIdx >= 0;
    QString selfTestPrompt;
    QString selfTestImage;
    if (selfTest) {
        // 提示词 = --self-test 后第一个非 -- 开头的参数
        for (int i = selfTestIdx + 1; i < args.size(); i++) {
            if (args[i] == "--image" && i + 1 < args.size()) {
                selfTestImage = args[++i];
            } else if (!args[i].startsWith("--") && selfTestPrompt.isEmpty()) {
                selfTestPrompt = args[i];
            }
        }
    }

    // --markdown-screenshot：用固定示例渲染 Markdown 并截图（无模型，便于视觉回归）
    if (args.contains("--markdown-screenshot")) {
        static const char * sample = R"MD(# Markdown 渲染示例

这是一段普通段落，包含 **加粗文字**、*斜体文字*、`inline_code()` 和 [链接](https://example.com)。

## 二级标题

### 三级标题

> 这是一段引用，用于强调重要信息。
> 引用可以有第二行。

- 无序列表项一
- 无序列表项二

1. 有序列表项一
2. 有序列表项二

| 功能 | 状态 | 说明 |
|---|---|---|
| 主题 | 完成 | 三套主题 |
| 语言 | 完成 | 中英文 |
| 表格 | 完成 | 斑马纹 |

- [x] 已完成任务
- [ ] 未完成任务

---

```python
def greet(name: str) -> str:
    """返回问候语"""
    return f"Hello, {name}!"

print(greet("qllama"))
```
)MD";
        auto * win = new QWidget;
        win->setObjectName("markdownPreview");
        win->setWindowTitle("Markdown Preview");
        auto * layout = new QVBoxLayout(win);
        layout->setContentsMargins(16, 16, 16, 16);
        auto * view = new QTextBrowser;
        view->setObjectName("chatView");
        view->setOpenExternalLinks(true);
        const ThemePalette & p = Theme::palette();
        const QString body = Markdown::toHtml(QString::fromUtf8(sample));
        view->setHtml(QStringLiteral(
            "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\"><tr><td style=\"padding:0;\">"
            "<table cellspacing=\"0\" cellpadding=\"0\"><tr><td style=\"background:%1;"
            "border:1px solid %2;border-radius:4px 16px 16px 16px;padding:14px 18px;\">%3"
            "</td></tr></table></td></tr></table>")
            .arg(Theme::css(p.assistantBubble), Theme::css(p.assistantBubbleBorder), body));
        layout->addWidget(view);
        win->resize(860, 1200);
        win->show();
        QTimer::singleShot(400, win, [win] {
            const QString path = QDir::temp().absoluteFilePath("qllama_markdown.png");
            win->grab().save(path);
            qInfo().noquote() << "MARKDOWN SCREENSHOT:" << path;
            QCoreApplication::exit(0);
        });
        return app.exec();
    }

    MainWindow w(selfTest, selfTestPrompt, selfTestImage, args.contains("--config-screenshot"));
    w.show();
    return app.exec();
}
