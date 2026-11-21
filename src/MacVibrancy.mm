#include "MacVibrancy.h"

#include <QWidget>

#ifdef Q_OS_MACOS

#import <Cocoa/Cocoa.h>

static bool vibrancyAllowed() {
    static const bool allowed = qgetenv("QLLAMA_NO_VIBRANCY") != "1";
    return allowed;
}

void macSetupVibrancy(QWidget * widget, bool enabled) {
    if (!enabled || !vibrancyAllowed()) {
        return;
    }
    // 半透明窗口背景。必须在显示之前设置（原生窗口创建前），
    // 否则 Qt 按不透明创建窗口，之后再切换会导致整窗不可见
    widget->setAttribute(Qt::WA_TranslucentBackground);
}

void macEnableVibrancy(QWidget * widget) {
    if (!widget->testAttribute(Qt::WA_TranslucentBackground) || !vibrancyAllowed()) {
        return;
    }
    NSView * view = reinterpret_cast<NSView *>(widget->winId());
    NSWindow * window = [view window];
    if (!window) {
        return;
    }
    window.opaque = NO;
    window.backgroundColor = [NSColor clearColor];

    // 磨砂层：位于所有 Qt 内容之下，对桌面背景做实时模糊
    NSVisualEffectView * effect = [[NSVisualEffectView alloc] initWithFrame:view.bounds];
    effect.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    effect.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    effect.material = NSVisualEffectMaterialUnderWindowBackground;
    effect.state = NSVisualEffectStateActive;
    [view addSubview:effect positioned:NSWindowBelow relativeTo:nil];
}

#else

void macSetupVibrancy(QWidget *, bool) {}
void macEnableVibrancy(QWidget *) {}

#endif
