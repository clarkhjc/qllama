#include "chat/ChatWorker.h"

#include <algorithm>

ChatWorker::ChatWorker(QObject * parent) : QObject(parent) {}

void ChatWorker::loadModel(const QString & path, const ChatOptions & opts) {
    std::string err;
    if (!m_chat.load(path.toStdString(), opts, err)) {
        emit modelLoadFailed(QString::fromUtf8(err.c_str()));
        return;
    }
    emit modelLoaded(QString::fromUtf8(m_chat.modelDesc().c_str()), m_chat.hasVision());
    emit contextUpdated(0, (int) m_chat.contextCapacity());
}

void ChatWorker::unloadModel() {
    m_chat.unload();
    emit contextUpdated(0, 0);
    emit modelUnloaded();
}

void ChatWorker::generate(const QVector<ChatMessage> & history, const ChatOptions & opts) {
    std::vector<ChatMessage> msgs;
    msgs.reserve((size_t) history.size());
    for (const ChatMessage & m : history) {
        msgs.push_back(m);
    }

    m_stop.store(false, std::memory_order_relaxed);

    std::string fullText;
    ChatResult result;
    std::string err;

    const bool ok = m_chat.chat(msgs, opts,
        [this, &fullText](const std::string & piece) {
            fullText += piece;
            emit tokenReady(QString::fromUtf8(piece.data(), (int) piece.size()));
        },
        m_stop, result, err,
        [this](size_t used, size_t capacity) {
            emit contextUpdated((int) used, (int) capacity);
        });

    if (ok) {
        emit generationFinished(QString::fromUtf8(fullText.data(), (int) fullText.size()), result);
    } else {
        emit generationFailed(QString::fromUtf8(err.c_str()));
    }
}

void ChatWorker::compressContext(const QVector<ChatMessage> & history, const ChatOptions & opts) {
    // 拼成纯文本对话记录；超长时截断，保证摘要请求本身能放进上下文
    std::string transcript;
    const size_t maxChars = std::max<size_t>(400, (size_t) opts.n_ctx / 2);
    for (const ChatMessage & m : history) {
        const char * role = m.role == "user" ? "用户：" : m.role == "assistant" ? "助手：" : "系统：";
        transcript += role;
        transcript += m.content;
        transcript += "\n\n";
        if (transcript.size() >= maxChars) {
            break;
        }
    }
    if (transcript.size() > maxChars) {
        transcript = transcript.substr(transcript.size() - maxChars); // 保留最近的内容
    }

    std::vector<ChatMessage> msgs = {
        {"system",
         "你是一个严谨的对话摘要器。只依据用户提供的对话记录写摘要，禁止编造、扩写或续写新内容。"
         "输出 3-5 条要点，每条一行，总长不超过 200 字；"
         "直接陈述事实与结论，不要使用“用户问/助手答”的问答形式；"
         "使用对话的主要语言输出。"},
        {"user", "对话记录：\n" + transcript + "\n\n请写出摘要要点："},
    };

    ChatOptions sumOpts = opts;
    sumOpts.n_predict        = 260;
    sumOpts.temperature      = 0.2f;
    sumOpts.top_k            = 20;
    sumOpts.top_p            = 0.90f;
    sumOpts.enable_thinking  = false;
    sumOpts.auto_trim        = true;

    std::string summary;
    std::string err;
    ChatResult result;
    std::atomic_bool noStop{false};
    const bool ok = m_chat.chat(msgs, sumOpts,
        [&summary](const std::string & piece) { summary += piece; },
        noStop, result, err, {});

    if (!ok || summary.empty()) {
        emit compressionFailed(QString::fromUtf8(err.empty() ? "摘要生成为空" : err.c_str()));
        return;
    }
    // 摘要完成后 KV 里是摘要对话，下一次正式对话会自动全量重算
    emit contextUpdated(0, (int) m_chat.contextCapacity());
    emit compressionFinished(QString::fromUtf8(summary.data(), (int) summary.size()));
}

void ChatWorker::stopGeneration() {
    m_stop.store(true, std::memory_order_relaxed);
}
