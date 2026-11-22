#include "I18n.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QWidget>

namespace {

QString s_lang = QStringLiteral("zh");

// 中文 → English
const QHash<QString, QString> & zhToEn() {
    static const QHash<QString, QString> dict = {
        // —— 主窗口 ——
        {QStringLiteral("qllama · 本地大模型对话"), QStringLiteral("qllama · Local LLM Chat")},
        {QStringLiteral("模型"), QStringLiteral("Model")},
        {QStringLiteral("选择 GGUF 模型文件（.gguf）"), QStringLiteral("Choose a GGUF model file (.gguf)")},
        {QStringLiteral("浏览…"), QStringLiteral("Browse…")},
        {QStringLiteral("加载模型"), QStringLiteral("Load Model")},
        {QStringLiteral("卸载模型"), QStringLiteral("Unload Model")},
        {QStringLiteral("管理模型"), QStringLiteral("Manage Models")},
        {QStringLiteral("从 Hugging Face 拉取最新模型列表"), QStringLiteral("Fetch the latest model list from Hugging Face")},
        {QStringLiteral("清空对话"), QStringLiteral("Clear Chat")},
        {QStringLiteral("配置…"), QStringLiteral("Settings…")},
        {QStringLiteral("打开配置页面（精简配置 / 全量配置）"), QStringLiteral("Open settings (Basic / Advanced)")},
        {QStringLiteral("输入消息，Enter 发送，Shift+Enter 换行"), QStringLiteral("Type a message. Enter to send, Shift+Enter for a new line")},
        {QStringLiteral("添加图片"), QStringLiteral("Add Image")},
        {QStringLiteral("随下一条消息发送图片（需要模型加载了 mmproj 投影模型）"), QStringLiteral("Attach images to the next message (requires an mmproj projector)")},
        {QStringLiteral("发送"), QStringLiteral("Send")},
        {QStringLiteral("停止"), QStringLiteral("Stop")},
        {QStringLiteral("选择模型并点击「加载模型」，然后开始对话"), QStringLiteral("Choose a model and click “Load Model” to start chatting")},
        {QStringLiteral("支持 Markdown 渲染 · 图片问答 · 长对话上下文复用"), QStringLiteral("Markdown rendering · Image Q&A · KV-cache reuse for long chats")},
        {QStringLiteral("助手"), QStringLiteral("Assistant")},
        {QStringLiteral("你"), QStringLiteral("You")},
        {QStringLiteral("正在生成…"), QStringLiteral("Generating…")},
        {QStringLiteral("生成 %1 token · %2 tok/s · 本轮解码 %3 token"), QStringLiteral("Generated %1 tokens · %2 tok/s · Decoded %3 tokens")},
        {QStringLiteral("未加载模型"), QStringLiteral("No model loaded")},
        {QStringLiteral("已加载模型"), QStringLiteral("Model loaded")},
        {QStringLiteral("正在加载模型，请稍候……"), QStringLiteral("Loading model, please wait…")},
        {QStringLiteral("正在卸载模型……"), QStringLiteral("Unloading model…")},
        {QStringLiteral("配置已更新"), QStringLiteral("Settings updated")},
        {QStringLiteral("生成中……"), QStringLiteral("Generating…")},
        {QStringLiteral("正在停止……"), QStringLiteral("Stopping…")},
        {QStringLiteral("模型加载失败"), QStringLiteral("Failed to load model")},
        {QStringLiteral("生成失败"), QStringLiteral("Generation failed")},
        {QStringLiteral("生成失败：%1"), QStringLiteral("Generation failed: %1")},
        {QStringLiteral("已加载 %1%2 · 上下文 %3 · 温度 %4"), QStringLiteral("Loaded %1%2 · Context %3 · Temperature %4")},
        {QStringLiteral(" · 多模态"), QStringLiteral(" · multimodal")},
        {QStringLiteral("解码 %1 token · 生成 %2 token · %3 tok/s"), QStringLiteral("Decoded %1 tokens · Generated %2 tokens · %3 tok/s")},
        {QStringLiteral("已设为当前模型：%1（点击「加载模型」开始使用）"), QStringLiteral("Set as current model: %1 (click “Load Model” to start)")},
        {QStringLiteral("无法发送图片"), QStringLiteral("Cannot Send Images")},
        {QStringLiteral("当前模型未加载 mmproj 投影模型，无法处理图片。\n请选择投影模型后重新加载。"), QStringLiteral("The current model has no mmproj projector, so images cannot be processed.\nPlease choose a projector and reload the model.")},
        {QStringLiteral("错误"), QStringLiteral("Error")},
        {QStringLiteral("模型文件不存在：\n%1"), QStringLiteral("Model file not found:\n%1")},
        {QStringLiteral("图片数量限制"), QStringLiteral("Image Limit")},
        {QStringLiteral("一次最多发送 4 张图片。"), QStringLiteral("You can attach up to 4 images at a time.")},
        {QStringLiteral("选择图片"), QStringLiteral("Choose Images")},
        {QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.gif);;所有文件 (*)"), QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.gif);;All Files (*)")},
        {QStringLiteral("点击移除：%1"), QStringLiteral("Click to remove: %1")},
        {QStringLiteral("选择 GGUF 模型"), QStringLiteral("Choose GGUF Model")},
        {QStringLiteral("GGUF 模型 (*.gguf);;所有文件 (*)"), QStringLiteral("GGUF Models (*.gguf);;All Files (*)")},
        {QStringLiteral("加载失败"), QStringLiteral("Load Failed")},
        {QStringLiteral("系统级：CPU 利用率 · 内存占用率（活动监视器口径）· GPU 利用率"), QStringLiteral("System-wide: CPU usage · Memory usage (Activity Monitor) · GPU usage")},
        {QStringLiteral("CPU %1 · 内存 %2 · GPU %3"), QStringLiteral("CPU %1 · Memory %2 · GPU %3")},
        {QStringLiteral("CPU — · 内存 — · GPU —"), QStringLiteral("CPU — · Memory — · GPU —")},
        {QStringLiteral("上下文 —"), QStringLiteral("Context —")},
        {QStringLiteral("加载模型后显示上下文使用情况"), QStringLiteral("Context usage appears after a model is loaded")},
        {QStringLiteral("上下文 %1 / %2 (%3%)"), QStringLiteral("Context %1 / %2 (%3%)")},
        {QStringLiteral("上下文使用：已解码 %1 token / 容量 %2 token（%3%）。接近上限时请调大「上下文」或清空对话。"), QStringLiteral("Context usage: %1 decoded tokens / %2 capacity (%3%). Increase “Context” or clear the chat when nearing the limit.")},

        // —— 配置页 ——
        {QStringLiteral("配置"), QStringLiteral("Settings")},
        {QStringLiteral("精简配置"), QStringLiteral("Basic")},
        {QStringLiteral("全量配置"), QStringLiteral("Advanced")},
        {QStringLiteral("常用参数，调整后立即生效（标记了「加载后生效」的除外）"), QStringLiteral("Common settings. Changes apply immediately (except those marked “on load”).")},
        {QStringLiteral("温度"), QStringLiteral("Temperature")},
        {QStringLiteral("上下文"), QStringLiteral("Context")},
        {QStringLiteral("最大回复长度"), QStringLiteral("Max Reply Length")},
        {QStringLiteral("系统提示词"), QStringLiteral("System Prompt")},
        {QStringLiteral("可选，例如：你是一个乐于助人的助手，请用中文回答"), QStringLiteral("Optional, e.g. You are a helpful assistant. Reply in English.")},
        {QStringLiteral("越高越发散，0 = 确定性输出"), QStringLiteral("Higher = more diverse; 0 = deterministic")},
        {QStringLiteral("上下文长度（token）。本机 8GB 内存实测最大稳定值 16384，超过会 Metal 显存不足；加载模型后需重新加载才生效"), QStringLiteral("Context length in tokens. On an 8 GB machine 16384 is the stable maximum; larger values exhaust Metal memory. Takes effect on model reload.")},
        {QStringLiteral("单次回复最多生成的 token 数"), QStringLiteral("Maximum tokens generated per reply")},
        {QStringLiteral("多模态"), QStringLiteral("Multimodal")},
        {QStringLiteral("投影模型"), QStringLiteral("Projector")},
        {QStringLiteral("可选：选择模型时自动探测同目录 mmproj*.gguf"), QStringLiteral("Optional: mmproj*.gguf in the same folder is detected automatically")},
        {QStringLiteral("采样参数"), QStringLiteral("Sampling")},
        {QStringLiteral("候选 token 截断，0 = 禁用"), QStringLiteral("Candidate token cutoff; 0 = disabled")},
        {QStringLiteral("核采样累积概率阈值"), QStringLiteral("Nucleus sampling cumulative probability")},
        {QStringLiteral("Minimum-P 采样：过滤概率低于最大概率该比例的 token"), QStringLiteral("Minimum-P sampling: filter tokens below this ratio of the top probability")},
        {QStringLiteral("随机种子"), QStringLiteral("Seed")},
        {QStringLiteral("随机（推荐）"), QStringLiteral("Random (recommended)")},
        {QStringLiteral("思考模式"), QStringLiteral("Thinking mode")},
        {QStringLiteral("允许模型先输出推理过程再回答（Qwen3 等支持思考的模型生效）；切换后下一轮对话会全量重算上下文"), QStringLiteral("Let the model output its reasoning before the answer (Qwen3 and similar). Switching forces a full context recompute on the next turn.")},
        {QStringLiteral("性能与界面"), QStringLiteral("Performance and UI")},
        {QStringLiteral("主题"), QStringLiteral("Theme")},
        {QStringLiteral("浅色"), QStringLiteral("Light")},
        {QStringLiteral("深色"), QStringLiteral("Dark")},
        {QStringLiteral("粉色"), QStringLiteral("Pink")},
        {QStringLiteral("浅色主题：浅色渐变底 + 白玻璃卡片"), QStringLiteral("Light theme: light gradient with white glass cards")},
        {QStringLiteral("深色主题：深色渐变底 + 半透明亮色卡片"), QStringLiteral("Dark theme: dark gradient with translucent light cards")},
        {QStringLiteral("粉色主题：粉色渐变底 + 玫粉主色"), QStringLiteral("Pink theme: pink gradient with rose accents")},
        {QStringLiteral("语言"), QStringLiteral("Language")},
        {QStringLiteral("中文"), QStringLiteral("Chinese")},
        {QStringLiteral("GPU 层数"), QStringLiteral("GPU Layers")},
        {QStringLiteral("-1 = 全部层 Metal 加速；0 = 纯 CPU；加载模型后需重新加载才生效"), QStringLiteral("-1 = all layers on Metal; 0 = CPU only. Takes effect on model reload.")},
        {QStringLiteral("全部(GPU)"), QStringLiteral("All (GPU)")},
        {QStringLiteral("批大小"), QStringLiteral("Batch Size")},
        {QStringLiteral("单次 llama_decode 的批大小，越大提示词处理越快但占用内存越多"), QStringLiteral("Batch size per llama_decode; larger processes prompts faster but uses more memory")},
        {QStringLiteral("线程数"), QStringLiteral("Threads")},
        {QStringLiteral("CPU 线程数，0 = 自动（Metal 卸载后影响较小）"), QStringLiteral("CPU threads; 0 = auto (less relevant with Metal offload)")},
        {QStringLiteral("自动"), QStringLiteral("Auto")},
        {QStringLiteral("打开日志"), QStringLiteral("Open Logs")},
        {QStringLiteral("确定"), QStringLiteral("OK")},
        {QStringLiteral("取消"), QStringLiteral("Cancel")},
        {QStringLiteral("选择多模态投影模型"), QStringLiteral("Choose Multimodal Projector")},
        {QStringLiteral("mmproj 模型 (*.gguf);;GGUF 模型 (*.gguf);;所有文件 (*)"), QStringLiteral("mmproj Models (*.gguf);;GGUF Models (*.gguf);;All Files (*)")},
        {QStringLiteral("日志文件：%1"), QStringLiteral("Log file: %1")},

        // —— 模型管理 ——
        {QStringLiteral("管理模型 · Hugging Face"), QStringLiteral("Manage Models · Hugging Face")},
        {QStringLiteral("搜索模型（如 gguf、gemma、qwen，留空 = 全部最新）"), QStringLiteral("Search models (e.g. gguf, gemma, qwen; empty = latest)")},
        {QStringLiteral("数量"), QStringLiteral("Count")},
        {QStringLiteral("拉取的模型条数（Hugging Face 单次最多返回 1000 条）"), QStringLiteral("Number of models to fetch (Hugging Face returns at most 1000 per request)")},
        {QStringLiteral("刷新"), QStringLiteral("Refresh")},
        {QStringLiteral("保存到"), QStringLiteral("Save to")},
        {QStringLiteral("模型保存目录（下载文件放在 目录/仓库名/ 下）"), QStringLiteral("Model directory (files go to <dir>/<repo>/)")},
        {QStringLiteral("模型 ID"), QStringLiteral("Model ID")},
        {QStringLiteral("类型"), QStringLiteral("Type")},
        {QStringLiteral("下载量"), QStringLiteral("Downloads")},
        {QStringLiteral("点赞"), QStringLiteral("Likes")},
        {QStringLiteral("更新时间"), QStringLiteral("Updated")},
        {QStringLiteral("点击「刷新」拉取最新模型"), QStringLiteral("Click “Refresh” to fetch the latest models")},
        {QStringLiteral("复制 ID"), QStringLiteral("Copy ID")},
        {QStringLiteral("下载 GGUF"), QStringLiteral("Download GGUF")},
        {QStringLiteral("查询所选仓库的 GGUF 文件并下载到本地模型目录，完成后自动设为当前模型"), QStringLiteral("Find GGUF files in the selected repo, download to the model directory, and set it as current")},
        {QStringLiteral("打开网页"), QStringLiteral("Open Page")},
        {QStringLiteral("关闭"), QStringLiteral("Close")},
        {QStringLiteral("正在从 Hugging Face 拉取最新模型…"), QStringLiteral("Fetching the latest models from Hugging Face…")},
        {QStringLiteral("官方站不可达，正在通过镜像 hf-mirror.com 拉取…"), QStringLiteral("huggingface.co unreachable, fetching via hf-mirror.com…")},
        {QStringLiteral("拉取失败：%1"), QStringLiteral("Fetch failed: %1")},
        {QStringLiteral("数据解析失败"), QStringLiteral("Failed to parse response")},
        {QStringLiteral("已拉取 %1 条模型（按最近更新排序）%2"), QStringLiteral("Fetched %1 models (sorted by last updated)%2")},
        {QStringLiteral(" · 经镜像 hf-mirror.com"), QStringLiteral(" · via hf-mirror.com")},
        {QStringLiteral("正在查询仓库文件列表…"), QStringLiteral("Listing repository files…")},
        {QStringLiteral("查询文件列表失败：%1"), QStringLiteral("Failed to list files: %1")},
        {QStringLiteral("文件列表解析失败"), QStringLiteral("Failed to parse file list")},
        {QStringLiteral("该仓库没有 GGUF 文件（试试搜索 gguf 找量化仓库）"), QStringLiteral("No GGUF files in this repository (try searching for gguf)")},
        {QStringLiteral("选择要下载的 GGUF 文件"), QStringLiteral("Choose a GGUF File to Download")},
        {QStringLiteral("选择模型保存目录"), QStringLiteral("Choose Model Directory")},
        {QStringLiteral("无法创建目录：%1"), QStringLiteral("Cannot create directory: %1")},
        {QStringLiteral("文件已存在"), QStringLiteral("File Exists")},
        {QStringLiteral("模型文件已存在：\n%1\n\n是否覆盖？"), QStringLiteral("File already exists:\n%1\n\nOverwrite?")},
        {QStringLiteral("无法写入文件：%1"), QStringLiteral("Cannot write file: %1")},
        {QStringLiteral("开始下载：%1%2"), QStringLiteral("Downloading: %1%2")},
        {QStringLiteral("（经镜像）"), QStringLiteral(" (via mirror)")},
        {QStringLiteral("下载中 %1 / %2 · %3"), QStringLiteral("Downloading %1 / %2 · %3")},
        {QStringLiteral("官方站不可达，改用镜像 hf-mirror.com 重试…"), QStringLiteral("huggingface.co unreachable, retrying via hf-mirror.com…")},
        {QStringLiteral("下载失败：%1（已删除不完整文件）"), QStringLiteral("Download failed: %1 (partial file removed)")},
        {QStringLiteral("下载完成：%1"), QStringLiteral("Downloaded: %1")},
        {QStringLiteral("正在下载，请等待完成或取消……"), QStringLiteral("Downloading, please wait or cancel…")},
        {QStringLiteral("已复制模型 ID：%1"), QStringLiteral("Copied model ID: %1")},
        {QStringLiteral("（暂无模型介绍）"), QStringLiteral("(No description available)")},
        {QStringLiteral("类型 %1 · 下载 %2 · 点赞 %3 · 更新 %4"), QStringLiteral("Type %1 · Downloads %2 · Likes %3 · Updated %4")},
    };
    return dict;
}

// English → 中文（切换回中文时使用）
const QHash<QString, QString> & enToZh() {
    static const QHash<QString, QString> reverse = [] {
        QHash<QString, QString> r;
        const auto & d = zhToEn();
        for (auto it = d.constBegin(); it != d.constEnd(); ++it) {
            if (it.value() != it.key() && !r.contains(it.value())) {
                r.insert(it.value(), it.key());
            }
        }
        return r;
    }();
    return reverse;
}

QString translateEither(const QString & text) {
    if (text.isEmpty()) {
        return text;
    }
    if (s_lang == QStringLiteral("en")) {
        return zhToEn().value(text, text);
    }
    return enToZh().value(text, text);
}

} // namespace

namespace I18n {

QString current() {
    return s_lang;
}

QString normalized(const QString & code) {
    return code == QStringLiteral("en") ? QStringLiteral("en") : QStringLiteral("zh");
}

bool isEnglish() {
    return s_lang == QStringLiteral("en");
}

void setLanguage(const QString & code) {
    s_lang = normalized(code);
}

QString t(const QString & zh) {
    return isEnglish() ? zhToEn().value(zh, zh) : zh;
}

QString t(const char * zh) {
    return t(QString::fromUtf8(zh));
}

void retranslateTree(QWidget * root) {
    if (!root) {
        return;
    }
    QList<QWidget *> widgets = root->findChildren<QWidget *>();
    widgets.prepend(root);

    for (QWidget * w : widgets) {
        const QString title = w->windowTitle();
        if (!title.isEmpty()) {
            w->setWindowTitle(translateEither(title));
        }
        const QString tip = w->toolTip();
        if (!tip.isEmpty()) {
            w->setToolTip(translateEither(tip));
        }

        if (auto * label = qobject_cast<QLabel *>(w)) {
            // 监控栏是运行时拼接的富文本，由 MainWindow 单独刷新
            if (label->objectName() != QStringLiteral("monitorLabel")) {
                const QString text = label->text();
                if (!text.isEmpty()) {
                    label->setText(translateEither(text));
                }
            }
        } else if (auto * button = qobject_cast<QAbstractButton *>(w)) {
            const QString text = button->text();
            if (!text.isEmpty()) {
                button->setText(translateEither(text));
            }
        } else if (auto * group = qobject_cast<QGroupBox *>(w)) {
            const QString title = group->title();
            if (!title.isEmpty()) {
                group->setTitle(translateEither(title));
            }
        }

        if (auto * tabs = qobject_cast<QTabWidget *>(w)) {
            for (int i = 0; i < tabs->count(); i++) {
                tabs->setTabText(i, translateEither(tabs->tabText(i)));
            }
        }
        if (auto * line = qobject_cast<QLineEdit *>(w)) {
            line->setPlaceholderText(translateEither(line->placeholderText()));
        }
        if (auto * text = qobject_cast<QTextEdit *>(w)) {
            text->setPlaceholderText(translateEither(text->placeholderText()));
        }
        if (auto * spin = qobject_cast<QSpinBox *>(w)) {
            spin->setSpecialValueText(translateEither(spin->specialValueText()));
        }
        if (auto * dspin = qobject_cast<QDoubleSpinBox *>(w)) {
            dspin->setSpecialValueText(translateEither(dspin->specialValueText()));
        }
        if (auto * combo = qobject_cast<QComboBox *>(w)) {
            for (int i = 0; i < combo->count(); i++) {
                combo->setItemText(i, translateEither(combo->itemText(i)));
            }
        }
    }
}

} // namespace I18n
