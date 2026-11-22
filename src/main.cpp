#include "Logging.h"
#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QSettings>

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

    MainWindow w(selfTest, selfTestPrompt, selfTestImage, args.contains("--config-screenshot"));
    w.show();
    return app.exec();
}
