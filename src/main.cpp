#include "Logging.h"
#include "MainWindow.h"

#include <QApplication>
#include <QFile>

int main(int argc, char * argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("qllama");
    app.setApplicationDisplayName("qllama 本地大模型对话");
    app.setOrganizationName("qllama"); // QSettings 持久化配置

    setupFileLogging(); // 日志文件：~/Library/Logs/qllama/qllama.log

    // 全局玻璃质感主题（assets/theme.qss）
    QFile theme(":/theme.qss");
    if (theme.open(QIODevice::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(theme.readAll()));
    }

    // 输入框占位文字颜色（统一浅灰）
    QPalette pal = app.palette();
    pal.setColor(QPalette::PlaceholderText, QColor(0x9a, 0x9a, 0xb0));
    app.setPalette(pal);

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
