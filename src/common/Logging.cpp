#include "common/Logging.h"

#include "llama.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>

#include <cstdio>

namespace {

QFile g_logFile;
QMutex g_logMutex;
QString g_logPath;

// llama.cpp 日志 → Qt 日志（进而写入客户端日志文件）
void llamaLogCallback(ggml_log_level level, const char * text, void *) {
    if (!text) {
        return;
    }
    switch (level) {
        case GGML_LOG_LEVEL_ERROR:
            qCritical().noquote() << "llama:" << text;
            break;
        case GGML_LOG_LEVEL_WARN:
            qWarning().noquote() << "llama:" << text;
            break;
        case GGML_LOG_LEVEL_DEBUG:
            qDebug().noquote() << "llama:" << text;
            break;
        default: // INFO / CONT
            qInfo().noquote() << "llama:" << text;
            break;
    }
}

// 超过 5MB 轮转：qllama.log → qllama.log.1
void rotateIfNeeded() {
    if (g_logFile.size() < 5 * 1024 * 1024) {
        return;
    }
    g_logFile.close();
    QFile::remove(g_logPath + ".1");
    QFile::rename(g_logPath, g_logPath + ".1");
    g_logFile.setFileName(g_logPath);
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void logHandler(QtMsgType type, const QMessageLogContext &, const QString & msg) {
    const char * level = "INFO";
    switch (type) {
        case QtDebugMsg:    level = "DEBUG"; break;
        case QtInfoMsg:     level = "INFO";  break;
        case QtWarningMsg:  level = "WARN";  break;
        case QtCriticalMsg: level = "ERROR"; break;
        case QtFatalMsg:    level = "FATAL"; break;
    }

    QMutexLocker lock(&g_logMutex);
    if (g_logFile.isOpen()) {
        QTextStream out(&g_logFile);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")
            << " [" << level << "] " << msg << '\n';
        out.flush();
        rotateIfNeeded();
    }
    // 同时保留 stderr 输出（终端启动时可见）
    fprintf(stderr, "[%s] %s\n", level, qPrintable(msg));
    fflush(stderr);
}

} // namespace

QString logFilePath() {
    return g_logPath;
}

void setupFileLogging() {
    const QDir logDir(QDir::home().filePath("Library/Logs/qllama"));
    logDir.mkpath(".");
    g_logPath = logDir.filePath("qllama.log");
    g_logFile.setFileName(g_logPath);
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler(logHandler);
    // llama.cpp 日志也写入客户端日志文件
    llama_log_set(llamaLogCallback, nullptr);
    qInfo().noquote() << "===== qllama 启动 " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
                      << " 日志文件:" << g_logPath << " =====";
}
