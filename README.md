# qllama · Qt + llama.cpp 本地大模型对话客户端

[English](README.en.md) | 中文

基于 **Qt 6 Widgets** 的本地大模型聊天客户端（macOS / Apple Silicon），直接调用
**llama.cpp 0.9.0** 的 C API（`llama.h`）、common 聊天模板 API 与 mtmd 多模态 API，
Metal 加速、流式输出、完全离线。

## 特性

- **流式 Markdown 对话**：逐 token 刷新；标题分级、代码块卡片（语言标签）、斑马纹表格、
  引用、任务列表、行内代码、链接均按主题配色渲染
- **长对话 KV 增量复用**：每轮只解码新增内容，避免重复处理历史
- **上下文管理**：状态栏实时显示 KV 用量 / 容量；上下文满时自动省略较早消息继续对话
  （可关闭），也可一键把早期对话压缩成摘要保留在系统提示中
- **多模态图片问答**：加载 mmproj 投影模型后可发送图片（最多 4 张）
- **模型管理**：从 Hugging Face 拉取最新模型、搜索、下载 GGUF 并自动设为当前模型；
  悬停行显示 README 简介；官方站不可达时自动切换 `hf-mirror.com`，单次最多 1000 条
- **三套主题**：浅色 / 深色 / 粉色，实时预览并持久化；控件外观全部集中在 QSS
- **中英文界面**：中文 / English 实时切换，界面、提示、状态栏与聊天记录同步翻译
- **系统监控**：状态栏常驻 CPU / 内存 / GPU 占用率（2 秒刷新，按占用率着色）
- **文件日志**：应用与 llama.cpp 日志写入 `~/Library/Logs/qllama/qllama.log`（自动轮转）
- **一键打包 DMG**：`./pack.sh` 完成构建、依赖部署、签名与安装包生成

## 环境与依赖

| 依赖 | 版本 | 说明 |
|---|---|---|
| macOS | Apple Silicon | Metal 加速依赖（当前仅支持 macOS） |
| Qt 6（Widgets / Network） | ≥ 6.5（开发用 6.12） | Homebrew `qt`；`pack.sh` 另需 `qtsvg` |
| qtsvg | 与 Qt 一致 | SVG 图标渲染（`iconengines/libqsvgicon`） |
| llama.cpp | 0.9.0（源码 + 已构建动态库） | 需先构建出 `build/bin/*.dylib` |
| CMake / Ninja | ≥ 3.21 | |

> 本项目按 llama.cpp 0.9.0 的 API 编写（含 `tools/mtmd`），其他版本可能存在 API 差异。

## 构建与运行

```bash
# 1. 先构建 llama.cpp（任意位置）：
cd <llama.cpp 源码目录> && cmake -B build && cmake --build build

# 2. 构建 qllama：
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DLLAMA_CPP_DIR=<llama.cpp 源码目录>
cmake --build build

# 3. 运行：
open build/qllama.app
```

产物：

- `build/qllama.app` —— GUI 客户端
- `build/qllama_smoke` —— 无 GUI 冒烟测试，可先验证 llama.cpp 链路

## 打包为 DMG

```bash
./pack.sh               # 构建 + 打包 + 运行时验证（窗口会短暂出现）
./pack.sh --no-build    # 跳过构建
./pack.sh --no-verify   # 跳过启动验证
```

脚本会部署 Qt/SVG 依赖与 `qt.conf`、精简无用插件、清理本机绝对路径、ad-hoc 重签并生成
`dist/qllama-<版本>-<架构>.dmg`（拖入 Applications 即可）。安装包为 ad-hoc 签名，
首次启动若被 Gatekeeper 拦截，请在「系统设置 → 隐私与安全性」中允许。

## 使用

1. 选择 GGUF 模型并点击「加载模型」；也可在「管理模型」中从 Hugging Face 下载；
2. 「配置…」中调整参数：

**精简配置**

| 参数 | 默认 | 说明 |
|---|---|---|
| 温度 | 0.70 | 越高越发散；0 = 确定性输出 |
| 上下文 | 4096 | KV 缓存长度（token），加载模型时生效 |
| 最大回复长度 | 2048 | 单次回复最大 token 数 |
| 系统提示词 | 空 | 每轮开头注入的 system 消息 |

**全量配置**

| 参数 | 默认 | 说明 |
|---|---|---|
| 主题 | 浅色 | 浅色 / 深色 / 粉色，实时预览并持久化 |
| 语言 | 中文 | 中文 / English，实时预览并持久化 |
| 投影模型 | 空 | mmproj*.gguf，加载后可发送图片（选择模型时自动探测） |
| Top-K | 40 | 候选 token 截断；0 = 禁用 |
| Top-P | 0.95 | 核采样累积概率阈值 |
| Min-P | 0.05 | Minimum-P 采样过滤比例 |
| GPU 层数 | -1（全部） | -1 = 全部层 Metal；0 = 纯 CPU；加载时生效 |
| 批大小 | 512 | 单次 llama_decode 批大小 |
| 线程数 | 0（自动） | CPU 线程数 |
| 随机种子 | 随机 | 固定种子可复现输出 |
| 上下文满时自动省略较早消息 | 开 | 保留系统提示词与最近消息；关闭后上下文满会报错 |
| 思考模式 | 关 | Qwen3 等先输出推理再回答；切换后下一轮全量重算 |

3. 输入框 `Enter` 发送、`Shift+Enter` 换行；生成中可停止；「添加图片」最多 4 张；
4. 日志：`~/Library/Logs/qllama/qllama.log`（超过 5MB 轮转），配置页可一键打开。

## 架构与实现要点

```
UI 线程：MainWindow ──信号(QueuedConnection)──► ChatWorker（工作线程）
                                                     └── LlamaChat（纯 C++，无 Qt 依赖）
                                                         ├─ common_chat_templates_apply
                                                         ├─ llama_tokenize / llama_decode
                                                         └─ llama_sampler_chain_*
```

- **推理在工作线程**：`llama_decode` 为阻塞调用，UI 永不卡顿；逐 token 经信号流式上屏
- **停止生成**：`stopGeneration` 以 `DirectConnection` 直连并仅置位原子标志
  （生成循环阻塞工作线程事件队列，队列信号无法及时到达）
- **KV 增量复用**：每轮结束用渲染器自身重建状态文本（完整历史 + 空 user 占位渲染后
  减去占位），使最后一条 assistant 以“历史消息”形式渲染，下一轮完整渲染必以其为前缀；
  命中文案则只解码新增后缀，任何失配自动全量重算
- **上下文管理**：按 token 预算（`n_ctx − 回复预留`）裁剪最旧的一轮对话；生成到 KV
  上限前主动停止；摘要压缩保留最近两轮，其余由本地模型总结后注入 system 提示
- **多模态**：content 中的 `<__media__>` 标记由 `mtmd_tokenize_from_parts` 交织分词，
  `mtmd_helper_eval_chunks` 完成文本解码 + 图片嵌入（M-RoPE 由 helper 处理）
- **Markdown**：生成时显示原文，完成后用 `Markdown::toHtml` 替换为渲染结果，
  对中途停止的半成品有容错
- **BOS / 停止**：KV 为空时手动补 BOS（否则 Gemma/Qwen 会回声循环）；
  以 `llama_vocab_is_eog` 判定结束

## 源码结构

```
src/
  app/      入口（main.cpp、smoke.cpp）
  chat/     推理与聊天渲染（LlamaChat、ChatWorker、Markdown）
  ui/       窗口与对话框（MainWindow、ConfigDialog、ModelManageDialog、ModelListModel）
  common/   主题、国际化、日志（Theme、I18n、Logging）
  system/   系统监控（SysMonitor）
```

界面布局在 `ui/*.ui`，可用 **Qt Creator 双击打开**进入 Designer；CMake AUTOUIC 自动生成
`ui_*.h`。跨目录 include 使用 src 根限定路径（如 `ui/MainWindow.h`）。

| .ui 文件 | 生成的类 | 对应源码 |
|---|---|---|
| ui/mainwindow.ui | `Ui::MainWindow` | src/ui/MainWindow.h/.cpp |
| ui/configdialog.ui | `Ui::ConfigDialog` | src/ui/ConfigDialog.h/.cpp |
| ui/modelmanagedialog.ui | `Ui::ModelManageDialog` | src/ui/ModelManageDialog.h/.cpp |

## 自测与调试

```bash
# GUI 全链路自测（加载模型 → 两轮对话 → 截图退出，退出码 0 = 通过）
./build/qllama.app/Contents/MacOS/qllama --self-test "请用 Markdown 回复：一个标题、一个列表"
# 带图片自测（自动选择支持多模态的模型并预填 mmproj）
./build/qllama.app/Contents/MacOS/qllama --self-test "这张图片里有什么？" --image <图片路径>
# 配置页截图 / Markdown 排版预览（无需模型）
./build/qllama.app/Contents/MacOS/qllama --config-screenshot
./build/qllama.app/Contents/MacOS/qllama --markdown-screenshot

# 无 GUI 冒烟测试（支持多轮与图片）
./build/qllama_smoke <模型路径> "你好" "你刚才说了什么？"
./build/qllama_smoke <模型路径> "这张图片里有什么？" --mmproj <mmproj> --image <图片>
```

常用环境变量：`QLLAMA_THEME=dark`（强制主题）、`QLLAMA_LANG=en`（强制语言）、
`QLLAMA_DEBUG_PROMPT=1`（打印提示词与 KV 复用失配点）。

## 上下文长度参考

8GB 内存 Apple Silicon + gemma-3-4b（Q4_K_M）实测：

| n_ctx | 结果 |
|---|---|
| 4096 / 8192 / 16384 | ✓ 稳定（16384 下填充 8192 token + 生成正常，约 12–14 tok/s） |
| 20480 及以上 | ✗ decode 时 Metal 显存不足（kIOGPU OutOfMemory） |
| 131072（训练上限） | ✗ 仅能初始化，实际 decode 必 OOM |

原因：gemma-3-4b 的 SWA 架构下 llama.cpp 仍按全量 n_ctx 分配 KV（131072 时约 17.4GB），
物理内存无法支撑 decode 计算图。UI 中「上下文」上限据此设为 16384。

## 已知限制

- Markdown 为自研轻量子集：图片语法与代码高亮尚未支持；
- 自动裁剪会丢弃早期消息的模型上下文（界面仍保留显示），可用摘要压缩缓解；
- 仅支持 macOS（Metal 依赖），跨平台需适配其他后端；
- 安装包为 ad-hoc 签名，首次启动可能需要手动允许。
