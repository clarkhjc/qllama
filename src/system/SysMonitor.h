#pragma once

#include <QObject>
#include <QTimer>

// 系统资源监控：CPU 利用率 / 内存占用率 / GPU 利用率（系统级）。
// 数据采集开销极小（微秒级），直接在 UI 线程定时采样。
class SysMonitor : public QObject {
    Q_OBJECT
public:
    explicit SysMonitor(QObject * parent = nullptr);
    void start(int intervalMs = 2000);
    void stop();

signals:
    // gpuPercent 为 -1 表示取不到（非 Apple GPU / 驱动不支持）
    void statsUpdated(double cpuPercent, double memPercent, int gpuPercent);

private:
    void sample();

    QTimer m_timer;
    bool    m_hasPrevCpu = false;
    uint64_t m_prevCpuUser = 0;
    uint64_t m_prevCpuSystem = 0;
    uint64_t m_prevCpuIdle = 0;
    uint64_t m_prevCpuNice = 0;
};
