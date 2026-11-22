# qllama · Qt + llama.cpp 本地大模型对话客户端

[English](README.en.md) | 中文

基于 **Qt 6 Widgets** 的本地大模型聊天客户端（macOS / Apple Silicon），直接调用
**llama.cpp 0.9.0** 的 C API（`llama.h`）与 common 聊天模板 API（`common_chat_templates`），
全程 Metal 加速，无需任何云服务，流式输出。

## 特性一览

- **流式对话**：逐 token 实时刷新，Markdown 渲染（标题 / 列表 / 表格 / 任务列表 /
  深色代码块（含语言标签）/ 引用 / 链接）
- **长对话上下文复用**：增量 KV 复用，每轮只解码新增内容
- **上下文管理**：状态栏实时显示用量；上下文满时自动省略较早消息继续对话
  （可配置），也可一键把早期对话压缩成摘要保留在系统提示中
- **多模态**：加载 mmproj 投影模型后支持图片问答（最多 4 张）
- **模型管理**：从 Hugging Face 拉取最新模型列表、搜索、下载 GGUF 并一键设为当前模型
  （官方站不可达时自动切 hf-mirror.com 镜像，可自定义保存目录，带进度条）
- **三套主题**：浅色 / 深色 / 粉色，配置页一键切换（实时预览，选择持久化），
  控件外观全部集中在 QSS
- **中英文界面**：中文 / English 一键切换（实时预览，选择持久化），
  界面、提示、状态栏与聊天记录同步翻译
- **玻璃质感主题**：渐变底 + 玻璃卡片 + 渐变头像/图标
- **系统监控**：状态栏常驻 CPU / 内存 / GPU 占用率与上下文使用量
  （KV token / n_ctx 容量 + 百分比，按占用率着色，2 秒刷新）
- **配置文件日志**：所有应用与 llama.cpp 日志写入 `~/Library/Logs/qllama/qllama.log`
  （自动轮转），配置页一键打开
- **GUI 自测**：`--self-test` 无人工全链路验证（加载模型 → 两轮对话 → 截图退出）

## 架构

```
┌─────────────────────────── UI 线程 ───────────────────────────
│ MainWindow（聊天界面、系统监控、模型管理）                         
│   │ 信号（QueuedConnection，跨线程）                            
│   ▼                                                          
│ ChatWorker（工作线程，QObject + QThread）                       
│   └── LlamaChat（纯 C++ 封装，无 Qt 依赖）                       
│         ├── 聊天模板渲染  common_chat_templates_apply()         
│         ├── 分词/解码      llama_tokenize / llama_decode        
│         └── 采样          llama_sampler_chain_*                
└───────────────────────────────────────────────────────────────
```

要点：

- **推理在工作线程**：`llama_decode` 是阻塞调用，放在 `ChatWorker`（`moveToThread`），
  UI 线程永不卡顿；逐 token 通过 `tokenReady` 信号流式刷到界面。
- **停止生成**：`stopGeneration` 以 `Qt::DirectConnection` 直连（生成循环阻塞了工作线程
  的事件队列，队列信号无法及时到达），仅置位 `std::atomic_bool`，线程安全。
- **长对话 KV 增量复用**：每轮结束后用**渲染器自身**重建状态文本（完整历史含本轮
  回复 + 空 user 占位渲染，再减去占位部分），使最后一条 assistant 以"历史消息"形式
  渲染——思考块剥离、`| trim` 修剪等模板行为由渲染器自身保证一致；下一轮完整渲染
  必然以该文本为前缀，命中则只分词解码新增后缀，续接在现有 KV 之后。任何不匹配
  （改系统提示词、清空对话、切换思考模式）自动回退全量重算。
- **Markdown 渲染**：生成时流式显示原文，完成后用 `Markdown::toHtml`（自研轻量转换，
  输出全量 HTML 转义）替换为渲染结果，对中途停止的半成品 Markdown 有容错。
- **多模态（图片）**：带图消息的 content 中以 `<__media__>` 标记占位，由
  `mtmd_tokenize_from_parts` 把文本/图片交织分词成 chunks，`mtmd_helper_eval_chunks`
  一步完成文本解码 + 图片编码嵌入（vision token 自动展开，M-RoPE 位置由 helper 处理）。
  选择模型时自动探测同目录下的 `mmproj*.gguf` 并预填。
- **BOS 处理**：与官方 `llama-simple-chat` 一致——KV 为空时手动补 BOS token
  （`llama_vocab_get_add_bos`），否则 Gemma/Qwen 会退化为回声循环。
- **停止条件**：`llama_vocab_is_eog`（EOS / EOT 等所有结束标记）。

## 依赖

| 依赖 | 版本 | 说明 |
|---|---|---|
| macOS | Apple Silicon | Metal 加速依赖（当前仅支持 macOS） |
| Qt 6（Widgets / Network） | ≥ 6.5（开发用 6.12） | Homebrew `qt` |
| llama.cpp | 0.9.0（源码 + 已构建动态库） | 需先用 CMake 构建出 `build/bin/*.dylib` |
| CMake / Ninja | ≥ 3.21 | |

> 注意：本项目按 llama.cpp 0.9.0 的 API 编写（含 `tools/mtmd` 多模态 API），
> 其他版本的 llama.cpp 可能存在 API 差异，请以 0.9.0 为准。

## 构建

```bash
# 1. 先构建 llama.cpp（任意位置）：
#    cd <llama.cpp 源码目录> && cmake -B build && cmake --build build

# 2. 构建 qllama，-DLLAMA_CPP_DIR 指向 llama.cpp 源码目录：
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLAMA_CPP_DIR=<llama.cpp 源码目录>
cmake --build build
```

产物：

- `build/qllama.app` —— GUI 客户端（`open build/qllama.app`）
- `build/qllama_smoke` —— 无 GUI 冒烟测试，先验证 llama.cpp 链路再开 GUI

## 界面主题（浅色 / 深色 / 粉色）

- **切换方式**：配置页「全量配置 → 性能与界面 → 主题」三个按钮，点击立即预览；
  「确定」后保存到 QSettings（`ui/theme`），下次启动沿用；「取消」恢复原主题
- **样式文件**：`assets/themes/light.qss`、`dark.qss`、`pink.qss`，所有控件外观
  （窗口渐变底、玻璃卡片、按钮、输入框、标签页、表格、滚动条、进度条、状态灯、
  主题切换按钮等）都集中在这三个文件里，经 `resources.qrc` 编译进二进制
- **调色板**：QSS 无法覆盖的部分（聊天记录 HTML、Markdown 代码块/表格/引用、
  头像渐变、单色 SVG 图标着色、系统监控颜色）由 `src/common/Theme.h/.cpp` 的
  `ThemePalette` 提供，与同名 QSS 配色保持一致
- **图标**：`assets/icons/`（Feather 图标，MIT 协议）；深色主题下非白色图标
  由 `Theme::icon()` 自动着色，白色变体（`-white.svg`）保持原样用于渐变按钮/头像
- **调试**：`QLLAMA_THEME=dark` 环境变量可强制指定启动主题（便于截图/回归）

## Qt Creator / Designer 开发

界面布局在 `ui/mainwindow.ui`、`ui/configdialog.ui`、`ui/modelmanagedialog.ui` 中，
可直接用 **Qt Creator 双击打开**进入 Designer 可视化编辑；CMake 的 AUTOUIC 自动生成
`ui_*.h`。控件交互、流式渲染、线程等动态逻辑在 `src/` 对应类中：

```
src/
  app/      入口（main.cpp、smoke.cpp）
  chat/     推理与聊天渲染（LlamaChat、ChatWorker、Markdown）
  ui/       窗口与对话框（MainWindow、ConfigDialog、ModelManageDialog、ModelListModel）
  common/   主题、国际化、日志（Theme、I18n、Logging）
  system/   系统监控（SysMonitor）
```

| .ui 文件 | 生成的类 | 对应源码 |
|---|---|---|
| ui/mainwindow.ui | `Ui::MainWindow` | src/ui/MainWindow.h/.cpp |
| ui/configdialog.ui | `Ui::ConfigDialog` | src/ui/ConfigDialog.h/.cpp |
| ui/modelmanagedialog.ui | `Ui::ModelManageDialog` | src/ui/ModelManageDialog.h/.cpp |


## 使用

1. 打开程序后选择 GGUF 模型文件，点击「加载模型」；也可在「管理模型」中直接从
   Hugging Face 下载 GGUF 并自动设为当前模型；
2. 点击「配置…」打开配置页面，参数分为两级：

**精简配置**（常用参数）：

| 参数 | 默认 | 说明 |
|---|---|---|
| 温度 | 0.70 | 越高越发散，0 = 确定性输出 |
| 上下文 | 4096 | KV 缓存长度（token），加载模型时生效 |
| 最大回复长度 | 2048 | 单次回复最大 token 数 |
| 系统提示词 | 空 | 可选，每轮对话开头注入 system 消息 |

**全量配置**（高级参数）：

| 参数 | 默认 | 说明 |
|---|---|---|
| 主题 | 浅色 | 浅色 / 深色 / 粉色，点击立即预览，「确定」后持久化 |
| 语言 | 中文 | 中文 / English，点击立即预览，「确定」后持久化 |
| 投影模型 | 空 | 多模态投影 mmproj*.gguf，加载后「添加图片」可发送图片（选择模型时自动探测） |
| Top-K | 40 | 候选 token 截断，0 = 禁用 |
| Top-P | 0.95 | 核采样累积概率阈值 |
| Min-P | 0.05 | Minimum-P 采样过滤比例 |
| GPU 层数 | -1（全部） | -1 = 全部层 Metal 加速；0 = 纯 CPU；加载模型时生效 |
| 批大小 | 512 | 单次 llama_decode 批大小，越大提示词处理越快但占用内存越多 |
| 线程数 | 0（自动） | CPU 线程数 |
| 随机种子 | 随机 | 固定种子可复现输出 |
| 上下文满时自动省略较早消息 | 开 | 保留系统提示词与最近消息，自动丢弃最旧的一轮对话；关闭后上下文满会提示错误 |
| 思考模式 | 关 | Qwen3 等支持思考的模型先输出推理过程再回答；切换后下一轮自动全量重算上下文 |

3. 输入框 `Enter` 发送，`Shift+Enter` 换行；生成中可点「停止」；助手回复以 Markdown 渲染；
   点「添加图片」选择图片（最多 4 张，点击缩略图可移除）随下一条消息发送；
4. 日志：应用与 llama.cpp 的完整日志写入 `~/Library/Logs/qllama/qllama.log`
   （超过 5MB 自动轮转），配置页「打开日志」可在 Finder 中定位；
5. 冒烟测试（多轮对话，可带图片）：

```bash
./build/qllama_smoke <模型路径> "你好" "你刚才说了什么？"
# 多模态（图片随第一轮发送）：
./build/qllama_smoke <模型路径> "这张图片里有什么？" \
    --mmproj <mmproj 路径> --image <图片路径>
```

6. GUI 自测（无人工操作的全链路验证：自动加载模型 → 两轮对话 → 截图退出）：

```bash
./build/qllama.app/Contents/MacOS/qllama --self-test "请用 Markdown 回复：一个标题、一个列表和一段代码"
# 带图片自测（自动选择支持多模态的模型并预填 mmproj）：
./build/qllama.app/Contents/MacOS/qllama --self-test "这张图片里有什么？" --image <图片路径>
# 退出码 0 = 通过；截图输出到系统临时目录（qllama_selftest_*.png）
# Markdown 排版预览（固定示例，无需模型）：
./build/qllama.app/Contents/MacOS/qllama --markdown-screenshot
```

## 上下文长度参考

以下为 8GB 内存的 Apple Silicon 设备 + gemma-3-4b（Q4_K_M）的实测数据，供选择
「上下文」参数时参考（其他模型/内存配置会不同）：

| n_ctx | 结果 |
|---|---|
| 4096 / 8192 / 16384 | ✓ 稳定可用（16384 下深度填充 8192 token + 生成正常，约 12-14 tok/s） |
| 20480 及以上 | ✗ decode 时 Metal 显存不足（kIOGPU OutOfMemory） |
| 131072（训练上限） | ✗ 仅能初始化（KV 虚拟分配 17.4GB 看似成功），实际 decode 必 OOM |

原因：gemma-3-4b 的 SWA（滑动窗口注意力）架构下，llama.cpp 仍按全量 n_ctx
为全部 34 层分配 KV（131072 时约 17.4GB），物理内存无法支撑 decode 计算图。
UI 中「上下文」上限据此设为 16384（可在 `ui/configdialog.ui` 中调整）。

## 已知限制 / 后续方向

- 长对话超出 n_ctx 时暂不支持自动滑动窗口，会报上下文不足（可调大「上下文」参数）；
- Markdown 渲染为自研轻量子集（表格/任务列表已支持，图片语法、代码高亮未做）；
- 部署分发时需处理 `@rpath` 依赖（libllama、libggml-*、libllama-common、openssl），
  可用 `install_name_tool` 或把 dylib 拷贝进 app bundle 并改写 rpath；
- 仅支持 macOS（Metal 加速依赖），跨平台移植需处理后端。
