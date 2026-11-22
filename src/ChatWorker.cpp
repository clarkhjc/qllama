#include "ChatWorker.h"

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

void ChatWorker::stopGeneration() {
    m_stop.store(true, std::memory_order_relaxed);
}
