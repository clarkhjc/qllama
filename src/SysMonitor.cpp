#include "SysMonitor.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include <mach/mach_host.h>
#include <sys/sysctl.h>

#include <cstdio>

SysMonitor::SysMonitor(QObject * parent) : QObject(parent) {
    connect(&m_timer, &QTimer::timeout, this, &SysMonitor::sample);
}

void SysMonitor::start(int intervalMs) {
    sample(); // 立即采样一次（CPU 首次仅记录基线）
    m_timer.start(intervalMs);
}

void SysMonitor::stop() {
    m_timer.stop();
}

void SysMonitor::sample() {
    // —— CPU：host_statistics 累计 tick，两次采样差值算利用率 ——
    double cpu = 0.0;
    host_cpu_load_info_data_t cpuInfo;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                        (host_info_t) &cpuInfo, &count) == KERN_SUCCESS) {
        const uint64_t user   = cpuInfo.cpu_ticks[CPU_STATE_USER];
        const uint64_t system = cpuInfo.cpu_ticks[CPU_STATE_SYSTEM];
        const uint64_t idle   = cpuInfo.cpu_ticks[CPU_STATE_IDLE];
        const uint64_t nice   = cpuInfo.cpu_ticks[CPU_STATE_NICE];
        if (m_hasPrevCpu) {
            const uint64_t dUser   = user - m_prevCpuUser;
            const uint64_t dSystem = system - m_prevCpuSystem;
            const uint64_t dIdle   = idle - m_prevCpuIdle;
            const uint64_t dNice   = nice - m_prevCpuNice;
            const uint64_t total   = dUser + dSystem + dIdle + dNice;
            if (total > 0) {
                cpu = 100.0 * (double) (dUser + dSystem + dNice) / (double) total;
            }
        }
        m_hasPrevCpu   = true;
        m_prevCpuUser   = user;
        m_prevCpuSystem = system;
        m_prevCpuIdle   = idle;
        m_prevCpuNice   = nice;
    }

    // —— 内存：active + wired + compressed 占物理内存比例（与“活动监视器”口径一致） ——
    double mem = 0.0;
    vm_statistics64_data_t vm;
    count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                          (host_info64_t) &vm, &count) == KERN_SUCCESS) {
        uint64_t totalBytes = 0;
        size_t len = sizeof(totalBytes);
        if (sysctlbyname("hw.memsize", &totalBytes, &len, nullptr, 0) == 0 && totalBytes > 0) {
            const uint64_t pageSize = (uint64_t) vm_kernel_page_size;
            const uint64_t used = ((uint64_t) vm.active_count + (uint64_t) vm.wire_count
                                   + (uint64_t) vm.compressor_page_count) * pageSize;
            mem = 100.0 * (double) used / (double) totalBytes;
        }
    }

    // —— GPU：IOAccelerator 的 PerformanceStatistics["Device Utilization %"] ——
    int gpu = -1;
    io_registry_entry_t service = IOServiceGetMatchingService(
        kIOMainPortDefault, IOServiceMatching("IOAccelerator"));
    if (service) {
        CFMutableDictionaryRef props = nullptr;
        if (IORegistryEntryCreateCFProperties(service, &props, kCFAllocatorDefault,
                                              kNilOptions) == KERN_SUCCESS) {
            CFDictionaryRef perf = (CFDictionaryRef) CFDictionaryGetValue(
                props, CFSTR("PerformanceStatistics"));
            if (perf) {
                CFNumberRef util = (CFNumberRef) CFDictionaryGetValue(
                    perf, CFSTR("Device Utilization %"));
                int value = -1;
                if (util && CFNumberGetValue(util, kCFNumberIntType, &value)) {
                    gpu = value;
                }
            }
            CFRelease(props);
        }
        IOObjectRelease(service);
    }

    emit statsUpdated(cpu, mem, gpu);
}
