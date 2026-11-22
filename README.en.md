# qllama · Qt + llama.cpp Local LLM Chat Client

English | [中文](README.md)

A local LLM chat client built with **Qt 6 Widgets** (macOS / Apple Silicon) that talks directly
to the **llama.cpp 0.9.0** C API (`llama.h`), the common chat-template API and the mtmd
multimodal API. Metal accelerated, streaming, fully offline.

## Features

- **Streaming Markdown chat**: token-by-token output; tiered headings, code cards with language
  labels, zebra-striped tables, quotes, task lists, inline code and links, all theme-aware
- **KV reuse for long chats**: each turn decodes only the new content instead of reprocessing history
- **Context management**: live KV usage / capacity in the status bar; when the context fills up,
  older messages are omitted automatically (configurable), or compress early messages into a
  summary with one click
- **Multimodal image Q&A**: send up to 4 images after loading an mmproj projector
- **Model manager**: fetch the latest models from Hugging Face, search, download GGUF and set it as
  current automatically; hover a row for its README summary; falls back to `hf-mirror.com`, up to
  1000 models per fetch
- **Three themes**: light / dark / pink, live preview and persisted; all widget styling lives in QSS
- **Chinese & English UI**: live switching; widgets, hints, status bar and chat records retranslated
- **System monitor**: persistent CPU / memory / GPU usage in the status bar (2 s, color-coded)
- **File logging**: app and llama.cpp logs in `~/Library/Logs/qllama/qllama.log` (auto-rotated)
- **One-command DMG**: `./pack.sh` builds, deploys dependencies, signs and packages the app

## Requirements

| Dependency | Version | Notes |
|---|---|---|
| macOS | Apple Silicon | Metal acceleration (macOS only for now) |
| Qt 6 (Widgets / Network) | ≥ 6.5 (developed with 6.12) | Homebrew `qt`; `pack.sh` also needs `qtsvg` |
| qtsvg | matching Qt | SVG icon rendering (`iconengines/libqsvgicon`) |
| llama.cpp | 0.9.0 (source + built shared libraries) | build `build/bin/*.dylib` first |
| CMake / Ninja | ≥ 3.21 | |

> Written against the llama.cpp 0.9.0 API (including `tools/mtmd`); other versions may differ.

## Build & Run

```bash
# 1. Build llama.cpp first (anywhere):
cd <llama.cpp source dir> && cmake -B build && cmake --build build

# 2. Build qllama:
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DLLAMA_CPP_DIR=<llama.cpp source dir>
cmake --build build

# 3. Run:
open build/qllama.app
```

Artifacts:

- `build/qllama.app` — GUI client
- `build/qllama_smoke` — headless smoke test for the llama.cpp pipeline

## Package as DMG

```bash
./pack.sh               # build + package + runtime verification (window appears briefly)
./pack.sh --no-build    # skip the build
./pack.sh --no-verify   # skip the launch check
```

The script deploys Qt/SVG dependencies and `qt.conf`, strips unused plugins and machine-specific
paths, re-signs ad-hoc and produces `dist/qllama-<version>-<arch>.dmg` (drag into Applications).
The bundle is ad-hoc signed; if Gatekeeper blocks the first launch, allow it under
System Settings → Privacy & Security.

## Usage

1. Pick a GGUF model and click 「加载模型」(Load Model); or download one from Hugging Face via
   「管理模型」(Manage Models);
2. Adjust parameters in 「配置…」(Settings):

**Basic**

| Parameter | Default | Description |
|---|---|---|
| Temperature | 0.70 | higher = more diverse; 0 = deterministic |
| Context | 4096 | KV cache length (tokens), applied on model load |
| Max reply length | 2048 | max tokens per reply |
| System prompt | empty | injected as a system message every turn |

**Advanced**

| Parameter | Default | Description |
|---|---|---|
| Theme | light | light / dark / pink, live preview and persisted |
| Language | 中文 | 中文 / English, live preview and persisted |
| Projector | empty | mmproj*.gguf; enables images (auto-detected when choosing a model) |
| Top-K | 40 | candidate cut-off; 0 = disabled |
| Top-P | 0.95 | nucleus sampling cumulative probability |
| Min-P | 0.05 | Minimum-P sampling filter ratio |
| GPU layers | -1 (all) | -1 = all on Metal; 0 = CPU only; applied on load |
| Batch size | 512 | batch per `llama_decode` |
| Threads | 0 (auto) | CPU threads |
| Random seed | random | fixed seed makes output reproducible |
| Auto-omit older messages | on | keep system prompt + recent messages; when off, a context-full error is shown |
| Thinking mode | off | Qwen3 and similar output reasoning first; toggling forces full recompute |

3. `Enter` sends, `Shift+Enter` inserts a newline; stop while generating; add up to 4 images;
4. Logs: `~/Library/Logs/qllama/qllama.log` (rotated beyond 5 MB), openable from Settings.

## Architecture

```
UI thread: MainWindow ──signals (QueuedConnection)──► ChatWorker (worker thread)
                                                          └── LlamaChat (pure C++, no Qt)
                                                              ├─ common_chat_templates_apply
                                                              ├─ llama_tokenize / llama_decode
                                                              └─ llama_sampler_chain_*
```

- **Inference on a worker thread**: `llama_decode` blocks; the UI never stalls, tokens stream
  back through signals
- **Stop generation**: `stopGeneration` uses `DirectConnection` and only sets an atomic flag
  (the generation loop blocks the worker's event queue, so queued signals would arrive too late)
- **KV reuse**: after each turn the state text is rebuilt by the renderer itself (full history +
  an empty user placeholder, minus the placeholder), so the last assistant message is rendered as
  a history message and the next full render always starts with it as a prefix. On a match only
  the new suffix is decoded; any mismatch falls back to a full recompute
- **Context management**: oldest turns are trimmed against a token budget (`n_ctx − reply reserve`);
  generation stops before the KV limit; summary compression keeps the last two turns and injects a
  local-model summary as a system message
- **Multimodal**: `<__media__>` markers are tokenized by `mtmd_tokenize_from_parts` and evaluated
  by `mtmd_helper_eval_chunks` (text decoding + image embedding, M-RoPE handled by the helper)
- **Markdown**: raw text while streaming, replaced by `Markdown::toHtml` when finished, tolerant
  of half-finished Markdown after an early stop
- **BOS / stop**: manually add BOS when the KV is empty (otherwise Gemma/Qwen echo-loop);
  `llama_vocab_is_eog` determines the end of generation

## Source layout

```
src/
  app/      entry points (main.cpp, smoke.cpp)
  chat/     inference & chat rendering (LlamaChat, ChatWorker, Markdown)
  ui/       windows & dialogs (MainWindow, ConfigDialog, ModelManageDialog, ModelListModel)
  common/   theme, i18n, logging (Theme, I18n, Logging)
  system/   system monitor (SysMonitor)
```

UI layouts live in `ui/*.ui` and open directly in **Qt Creator** Designer; CMake AUTOUIC generates
`ui_*.h`. Cross-directory includes use the `src/` root prefix (e.g. `ui/MainWindow.h`).

| .ui file | Generated class | Source code |
|---|---|---|
| ui/mainwindow.ui | `Ui::MainWindow` | src/ui/MainWindow.h/.cpp |
| ui/configdialog.ui | `Ui::ConfigDialog` | src/ui/ConfigDialog.h/.cpp |
| ui/modelmanagedialog.ui | `Ui::ModelManageDialog` | src/ui/ModelManageDialog.h/.cpp |

## Self-test & debugging

```bash
# Full GUI self-test (load model → two turns → screenshots → exit; 0 = passed)
./build/qllama.app/Contents/MacOS/qllama --self-test "请用 Markdown 回复：一个标题、一个列表"
# With an image (auto-selects a multimodal model and pre-fills mmproj)
./build/qllama.app/Contents/MacOS/qllama --self-test "这张图片里有什么？" --image <image path>
# Config dialog screenshot / Markdown preview (no model required)
./build/qllama.app/Contents/MacOS/qllama --config-screenshot
./build/qllama.app/Contents/MacOS/qllama --markdown-screenshot

# Headless smoke test (multi-turn, optional image)
./build/qllama_smoke <model path> "你好" "你刚才说了什么？"
./build/qllama_smoke <model path> "这张图片里有什么？" --mmproj <mmproj> --image <image>
```

Environment variables: `QLLAMA_THEME=dark` (force theme), `QLLAMA_LANG=en` (force language),
`QLLAMA_DEBUG_PROMPT=1` (print prompts and KV-reuse mismatch points).

## Context length reference

Measured on an 8 GB Apple Silicon device with gemma-3-4b (Q4_K_M):

| n_ctx | Result |
|---|---|
| 4096 / 8192 / 16384 | ✓ stable (16384: 8192-token fill + generation OK, ~12–14 tok/s) |
| 20480 and above | ✗ Metal OOM during decode (kIOGPU OutOfMemory) |
| 131072 (training max) | ✗ initializes only; decode always OOMs |

Reason: with gemma-3-4b's SWA architecture llama.cpp still allocates KV for every layer at the full
n_ctx (~17.4 GB at 131072), which exceeds physical memory. The UI's Context cap is 16384 accordingly.

## Known limitations

- Markdown is a lightweight in-house subset: image syntax and syntax highlighting are not supported;
- Auto-trim drops older messages from the model context (still shown in the UI); use summary
  compression to retain their gist;
- macOS only (Metal dependency); other platforms require another backend;
- The DMG is ad-hoc signed; the first launch may need manual approval.
