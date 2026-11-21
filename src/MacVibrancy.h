#pragma once

class QWidget;

// macOS 原生磨砂玻璃背景，分两步：
//   1. macSetupVibrancy(widget, enabled) —— 在窗口【显示前】调用（构造函数中），
//      设置 WA_TranslucentBackground；必须在原生窗口创建前设置，否则窗口不可见
//   2. macEnableVibrancy(widget)          —— 在窗口首次显示后调用（showEvent 中），
//      插入 NSVisualEffectView 磨砂层
// enabled = false 时不做任何事（窗口保持不透明）；默认关闭。
// 设置环境变量 QLLAMA_NO_VIBRANCY=1 可强制关闭原生磨砂（逃生门）。
void macSetupVibrancy(QWidget * widget, bool enabled);
void macEnableVibrancy(QWidget * widget);
