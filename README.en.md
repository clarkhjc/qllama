# qllama · Qt + llama.cpp Local LLM Chat Client

English | [中文](README.md)

A local LLM chat client built with **Qt 6 Widgets** (macOS / Apple Silicon) that directly calls
the **llama.cpp 0.9.0** C API (`llama.h`) and the common chat-template API
(`common_chat_templates`), fully Metal-accelerated, streaming output, no cloud services required.

## Features

- **Streaming chat**: real-time token-by-token refresh, Markdown rendering (headings / lists /
  tables / task lists / dark code blocks with language labels / blockquotes / links)
- **KV reuse for long conversations**: incremental KV reuse — each turn only decodes the new content
- **Multimodal**: after loading an mmproj projector model, image Q&A is supported (up to 4 images)
- **Model management**: fetch the latest models from Hugging Face, search, download GGUF files and
  set one as the current model in a single click (auto-falls back to the hf-mirror.com mirror when
  the official site is unreachable; customizable save directory, with progress bar)
- **Three themes**: light / dark / pink, switchable in one click from the config dialog
  (live preview, persisted), with all widget styles living in QSS
- **Chinese & English UI**: switch between 中文 and English (live preview, persisted);
  widgets, hints, status bar and chat records are retranslated together
- **Glassmorphism**: gradient background + glass cards + gradient avatars/icons
- **System monitor**: persistent CPU / memory / GPU usage in the status bar (color-coded,
  refreshed every 2 seconds)
- **Config & file logging**: all app and llama.cpp logs go to `~/Library/Logs/qllama/qllama.log`
  (auto-rotated), openable in one click from the config dialog
- **GUI self-test**: `--self-test` end-to-end verification with no human interaction
  (load model → two-turn conversation → screenshot → exit)

## Architecture

```
┌──────────────────────────── UI thread ───────────────────────────
│ MainWindow (chat UI, system monitor, model management)
│   │ signals (QueuedConnection, cross-thread)
│   ▼
│ ChatWorker (worker thread, QObject + QThread)
│   └── LlamaChat (pure C++ wrapper, no Qt dependency)
│         ├── chat-template rendering  common_chat_templates_apply()
│         ├── tokenize / decode        llama_tokenize / llama_decode
│         └── sampling                 llama_sampler_chain_*
└───────────────────────────────────────────────────────────────────
```

Key points:

- **Inference on a worker thread**: `llama_decode` is a blocking call, so it runs inside
  `ChatWorker` (`moveToThread`) — the UI thread never stalls; tokens stream to the UI
  one by one through the `tokenReady` signal.
- **Stop generation**: `stopGeneration` is connected with `Qt::DirectConnection` (the generation
  loop blocks the worker thread's event queue, so queued signals would not arrive in time) and
  only sets a `std::atomic_bool` — thread-safe.
- **Incremental KV reuse for long conversations**: after each turn, the state text is rebuilt by
  **the renderer itself** (full history including this turn's reply + an empty user placeholder,
  rendered and then the placeholder part subtracted), so the last assistant message is rendered
  in "history message" form — template behaviors such as thinking-block stripping and `| trim`
  are guaranteed consistent by the renderer itself. The next turn's full render is guaranteed to
  start with that text as a prefix; on a hit, only the new suffix is tokenized and decoded,
  continuing right after the existing KV. Any mismatch (changing the system prompt, clearing the
  conversation, toggling thinking mode) automatically falls back to full recomputation.
- **Markdown rendering**: raw text streams during generation; once finished it is replaced with
  the rendered output of `Markdown::toHtml` (a lightweight in-house converter with fully
  HTML-escaped output), tolerant of half-finished Markdown after an early stop.
- **Multimodal (images)**: in messages with images, the content carries `<__media__>` markers;
  `mtmd_tokenize_from_parts` interleaves text/image tokens into chunks and
  `mtmd_helper_eval_chunks` performs text decoding + image encoding/embedding in one step
  (vision tokens auto-expanded, M-RoPE positions handled by the helper). When a model is
  selected, `mmproj*.gguf` in the same directory is auto-detected and pre-filled.
- **BOS handling**: consistent with the official `llama-simple-chat` — a BOS token is added
  manually when the KV cache is empty (`llama_vocab_get_add_bos`), otherwise Gemma/Qwen degrade
  into echo loops.
- **Stop condition**: `llama_vocab_is_eog` (EOS / EOT and all other end-of-generation tokens).

## Dependencies

| Dependency | Version | Notes |
|---|---|---|
| macOS | Apple Silicon | required for Metal acceleration (macOS only for now) |
| Qt 6 (Widgets / Network) | ≥ 6.5 (developed with 6.12) | Homebrew `qt` |
| llama.cpp | 0.9.0 (source + built shared libraries) | must be built with CMake first, producing `build/bin/*.dylib` |
| CMake / Ninja | ≥ 3.21 | |

> Note: this project is written against the llama.cpp 0.9.0 API (including the `tools/mtmd`
> multimodal API). Other llama.cpp versions may have API differences; please stick to 0.9.0.

## Building

```bash
# 1. Build llama.cpp first (anywhere):
#    cd <llama.cpp source dir> && cmake -B build && cmake --build build

# 2. Build qllama, with -DLLAMA_CPP_DIR pointing at the llama.cpp source directory:
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLAMA_CPP_DIR=<llama.cpp source dir>
cmake --build build
```

Artifacts:

- `build/qllama.app` — GUI client (`open build/qllama.app`)
- `build/qllama_smoke` — headless smoke test; verify the llama.cpp pipeline before opening the GUI

## UI themes (light / dark / pink)

- **Switching**: three buttons under 「全量配置 → 性能与界面 → 主题」 in the config dialog;
  clicking previews the theme immediately, 「确定」(OK) saves it to QSettings (`ui/theme`) for
  the next launch, 「取消」(Cancel) restores the previous theme
- **Style files**: `assets/themes/light.qss`, `dark.qss`, `pink.qss` — every widget style
  (window gradient, glass cards, buttons, inputs, tabs, tables, scrollbars, progress bar,
  status dot, theme buttons, …) is defined there, compiled into the binary via `resources.qrc`
- **Palette**: what QSS cannot reach (chat HTML, Markdown code blocks/tables/quotes, avatar
  gradients, monochrome SVG icon tinting, system-monitor colors) comes from the `ThemePalette`
  in `src/Theme.h/.cpp`, kept in sync with the matching QSS
- **Icons**: `assets/icons/` (Feather icons, MIT license); in the dark theme non-white icons are
  tinted by `Theme::icon()`, while the `-white.svg` variants stay as-is for gradient buttons/avatars
- **Debug**: the `QLLAMA_THEME=dark` environment variable forces the startup theme
  (useful for screenshots/regression)

## Qt Creator / Designer development

UI layouts live in `ui/mainwindow.ui`, `ui/configdialog.ui` and `ui/modelmanagedialog.ui` —
double-click them in **Qt Creator** to open the visual Designer; CMake's AUTOUIC generates the
`ui_*.h` headers. Widget interaction, streaming rendering, threading and other dynamic logic live
in the corresponding classes under `src/`:

| .ui file | Generated class | Source code |
|---|---|---|
| ui/mainwindow.ui | `Ui::MainWindow` | src/MainWindow.h/.cpp |
| ui/configdialog.ui | `Ui::ConfigDialog` | src/ConfigDialog.h/.cpp |
| ui/modelmanagedialog.ui | `Ui::ModelManageDialog` | src/ModelManageDialog.h/.cpp |

## Usage

1. Launch the app, pick a GGUF model file and click 「加载模型」(Load Model); or use
   「管理模型」(Manage Models) to download a GGUF from Hugging Face directly and have it set as
   the current model automatically;
2. Click 「配置…」(Config...) to open the settings dialog. Parameters come in two tiers:

**Basic config** (common parameters):

| Parameter | Default | Description |
|---|---|---|
| 温度 Temperature | 0.70 | higher = more diverse; 0 = deterministic output |
| 上下文 Context | 4096 | KV cache size (tokens); takes effect when the model is loaded |
| 最大回复长度 Max reply length | 2048 | max tokens generated per reply |
| 系统提示词 System prompt | empty | optional; a system message injected at the start of every conversation |

**Full config** (advanced parameters):

| Parameter | Default | Description |
|---|---|---|
| 主题 Theme | light | light / dark / pink; clicking previews immediately, 「确定」(OK) persists it |
| 语言 Language | 中文 | 中文 / English; clicking previews immediately, 「确定」(OK) persists it |
| 投影模型 Projector model | empty | multimodal projector mmproj*.gguf; once loaded, 「添加图片」(Add Image) lets you send images (auto-detected when selecting a model) |
| Top-K | 40 | candidate token cutoff; 0 = disabled |
| Top-P | 0.95 | nucleus sampling cumulative probability threshold |
| Min-P | 0.05 | Minimum-P sampling filter ratio |
| GPU layers | -1 (all) | -1 = all layers Metal-accelerated; 0 = CPU only; takes effect when the model is loaded |
| Batch size | 512 | batch size per `llama_decode` call; larger = faster prompt processing but more memory |
| Threads | 0 (auto) | CPU thread count |
| Random seed | random | a fixed seed makes output reproducible |
| Thinking mode | off | Qwen3 and other thinking-capable models output their reasoning before answering; toggling forces a full context recomputation on the next turn |

3. In the input box, `Enter` sends and `Shift+Enter` inserts a newline; click 「停止」(Stop) while
   generating; assistant replies are rendered as Markdown; click 「添加图片」(Add Image) to attach
   images (up to 4; click a thumbnail to remove it) sent with the next message;
4. Logs: complete app + llama.cpp logs are written to `~/Library/Logs/qllama/qllama.log`
   (auto-rotated beyond 5 MB); 「打开日志」(Open Logs) in the config dialog reveals it in Finder;
5. Smoke test (multi-turn conversation, optionally with images):

```bash
./build/qllama_smoke <model path> "你好" "你刚才说了什么？"
# Multimodal (image sent with the first turn):
./build/qllama_smoke <model path> "这张图片里有什么？" \
    --mmproj <mmproj path> --image <image path>
```

6. GUI self-test (end-to-end verification with no human interaction: auto-load model →
   two-turn conversation → screenshot → exit):

```bash
./build/qllama.app/Contents/MacOS/qllama --self-test "请用 Markdown 回复：一个标题、一个列表和一段代码"
# With an image (auto-selects a multimodal model and pre-fills mmproj):
./build/qllama.app/Contents/MacOS/qllama --self-test "这张图片里有什么？" --image <image path>
# Exit code 0 = passed; screenshots go to the system temp dir (qllama_selftest_*.png)
# Markdown layout preview (fixed sample, no model):
./build/qllama.app/Contents/MacOS/qllama --markdown-screenshot
```

## Context length reference

Measured on an 8 GB Apple Silicon device with gemma-3-4b (Q4_K_M), as a reference when choosing
the 「上下文」(Context) parameter (other models / memory configs will differ):

| n_ctx | Result |
|---|---|
| 4096 / 8192 / 16384 | ✓ stable (deep-fill of 8192 tokens + generation OK at 16384, ~12–14 tok/s) |
| 20480 and above | ✗ Metal OOM during decode (kIOGPU OutOfMemory) |
| 131072 (training max) | ✗ initializes only (KV virtual allocation of 17.4 GB appears to succeed); decode inevitably OOMs |

Reason: under gemma-3-4b's SWA (sliding-window attention) architecture, llama.cpp still allocates
KV for all 34 layers at the full n_ctx (~17.4 GB at 131072), and physical memory cannot support
the decode compute graph. The UI's 「上下文」 cap is set to 16384 accordingly
(adjustable in `ui/configdialog.ui`).

## Known limitations / roadmap

- Long conversations exceeding n_ctx are not auto-windowed yet; a context-exceeded error is shown
  (raise the 「上下文」 parameter instead);
- Markdown rendering is a lightweight in-house subset (tables / task lists supported; image
  syntax and syntax highlighting not implemented);
- Distribution requires handling the `@rpath` dependencies (libllama, libggml-*, libllama-common,
  openssl) — use `install_name_tool` or copy the dylibs into the app bundle and rewrite the rpaths;
- macOS only (Metal acceleration dependency); porting to other platforms requires backend work.
