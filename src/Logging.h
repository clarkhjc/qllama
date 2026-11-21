#pragma once

#include <QString>

// 安装文件日志：所有 Qt 日志（qInfo/qWarning/qCritical）写入
// ~/Library/Logs/qllama/qllama.log（超过 5MB 轮转为 .1），同时仍输出到 stderr。
void setupFileLogging();

// 当前日志文件路径（setupFileLogging 之后有效）
QString logFilePath();
