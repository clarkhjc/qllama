#pragma once

#include "ChatWorker.h"
#include "ConfigDialog.h"
#include "SysMonitor.h"

#include <QMainWindow>

#include "ui_mainwindow.h" // 由 uic 从 ui/mainwindow.ui 生成

class QCloseEvent;
class QShowEvent;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // selfTest：--self-test 模式，自动加载模型、发送两轮对话、
    // 截图保存到临时目录并退出（用于无人工操作的端到端验证）；
    // selfTestImage 非空时首轮对话附带该图片（需模型支持多模态）
    explicit MainWindow(bool selfTest = false, const QString & selfTestPrompt = {},
                        const QString & selfTestImage = {}, bool configScreenshot = false,
                        QWidget * parent = nullptr);

protected:
    void closeEvent(QCloseEvent * event) override;
    void showEvent(QShowEvent * event) override; // 仅用于 QLLAMA_DUMP_GEOMETRY 调试
    bool eventFilter(QObject * watched, QEvent * event) override;

signals:
    // 发往工作线程的请求（跨线程自动使用 QueuedConnection）
    void loadRequested(const QString & path, const ChatOptions & opts);
    void unloadRequested();
    void generateRequested(const QVector<ChatMessage> & history, const ChatOptions & opts);
    // 停止生成必须以 DirectConnection 直连（生成循环是阻塞的）
    void stopRequested();

private slots:
    void browseModel();
    void onConfig();
    void onManageModels();
    void onLoadModel();
    void onUnloadModel();
    void onSend();
    void onStop();
    void onClearChat();
    void onAddImages();

    void onModelLoaded(const QString & info, bool hasVision);
    void onModelLoadFailed(const QString & err);
    void onModelUnloaded();
    void onTokenReady(const QString & piece);
    void onGenerationFinished(const QString & text, const ChatResult & result);
    void onGenerationFailed(const QString & err);

private:
    void prefillModelPath();
    void prefillMmprojForModel();
    void setGenerating(bool on);
    void appendUserMessage(const QString & text, const QStringList & imagePaths);
    void appendAssistantHeader();
    void finishAssistantHeader(); // 移除“正在生成”标记并修正正文起点位置
    void refreshImageStrip();
    void refreshIcons();          // 按当前主题重设工具栏图标
    void rebuildChatView();       // 切换主题后按新配色重建聊天记录
    void scrollChatToBottom();    // 滚到聊天记录末尾（等文档布局完成后再滚）
    void applyLanguage(const QString & code); // 切换界面语言并重译界面
    void updateMonitorLabel();    // 按当前语言刷新状态栏监控文字
    void selfTestAsk(const QString & prompt);
    void selfTestFinish(bool ok, const QString & reason);

    // 界面（ui/mainwindow.ui，Qt Designer 编辑）
    Ui::MainWindow ui;

    // 工作线程：llama.cpp 的所有调用都在这里执行
    QThread *    m_thread = nullptr;
    ChatWorker * m_worker = nullptr;

    // 系统监控（CPU / 内存 / GPU）
    SysMonitor * m_monitor = nullptr;
    QLabel *     m_monitorLabel = nullptr; // 状态栏右侧常驻
    double       m_lastCpu = -1;
    double       m_lastMem = -1;
    int          m_lastGpu = -2;

    // 配置（配置页面「精简配置 / 全量配置」编辑）
    AppConfig m_config;

    // 状态
    QVector<ChatMessage> m_history;
    QStringList          m_attachedImages; // 待随下一条消息发送的图片
    bool m_generating  = false;
    bool m_modelLoaded = false;
    bool m_modelHasVision = false;

    // 流式输出的助手正文起点（用于完成后用 Markdown 渲染替换原文）
    int m_assistantStartPos = -1;
    // 助手消息头的起止位置（用于移除“正在生成”状态标记）
    int m_assistantHeaderStart = -1;
    int m_assistantHeaderEnd   = -1;

    // --self-test 模式
    bool     m_selfTest = false;
    QString  m_selfTestPrompt;
    QString  m_selfTestImage;
    int      m_selfTestTurn = 0;
};
