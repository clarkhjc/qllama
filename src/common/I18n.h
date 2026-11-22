#pragma once

#include <QString>

class QWidget;

// 轻量国际化：中文为源语言，英文通过内置字典翻译。
// - 代码中的动态文本用 I18n::t("中文") 生成
// - .ui 中的静态文本通过 I18n::retranslateTree(widget) 在切换时原地替换
//   （中→英查字典，英→中查反向字典，因此无需重新 setupUi）
namespace I18n {

QString current();          // "zh" / "en"
QString normalized(const QString & code);
bool    isEnglish();
void    setLanguage(const QString & code);

QString t(const QString & zh);
QString t(const char * zh);

// 遍历控件树，翻译窗口标题、标签/按钮/分组框/标签页文字、
// 占位文字、tooltip、spinbox 特殊值与下拉项
void retranslateTree(QWidget * root);

} // namespace I18n
