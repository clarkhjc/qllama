#pragma once

#include <QString>

// 将 Markdown 转为 QTextDocument 支持的 HTML 子集
// （标题 / 加粗 / 斜体 / 行内代码 / 围栏代码块 / 列表 / 引用 / 链接 / 分隔线）。
// 对未闭合的结构（如流式生成被中途停止）有容错。
namespace Markdown {
QString toHtml(const QString & md);
}
