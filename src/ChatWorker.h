#pragma once

#include "LlamaChat.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>

// 供跨线程信号槽（QueuedConnection）使用的元类型注册声明
Q_DECLARE_METATYPE(ChatOptions)
Q_DECLARE_METATYPE(ChatMessage)
Q_DECLARE_METATYPE(ChatResult)
Q_DECLARE_METATYPE(QVector<ChatMessage>)

// llama.cpp 的所有阻塞调用（加载模型、生成）都在工作线程中执行，
// 避免卡住 UI 线程。
class ChatWorker : public QObject {
    Q_OBJECT
public:
    explicit ChatWorker(QObject * parent = nullptr);

public slots:
    void loadModel(const QString & path, const ChatOptions & opts);
    void unloadModel();
    void generate(const QVector<ChatMessage> & history, const ChatOptions & opts);

    // 注意：生成是阻塞调用，工作线程的事件循环不会处理队列信号，
    // 因此本槽必须以 Qt::DirectConnection 调用（仅置位原子标志，线程安全）。
    void stopGeneration();

signals:
    void modelLoaded(const QString & info, bool hasVision);
    void modelLoadFailed(const QString & err);
    void modelUnloaded();
    void tokenReady(const QString & piece);
    void generationFinished(const QString & text, const ChatResult & result);
    void generationFailed(const QString & err);

private:
    LlamaChat m_chat;
    std::atomic_bool m_stop{false};
};
