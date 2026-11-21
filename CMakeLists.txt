cmake_minimum_required(VERSION 3.21)
project(qllama VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON) # ui/*.ui → ui_*.h，供 Qt Designer / Qt Creator 可视化编辑
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui")
set(CMAKE_AUTORCC ON) # resources.qrc → qrc_resources.cpp

# ---------- 依赖检查 ----------
# llama.cpp 源码目录（需要已用 CMake 构建出 build/bin 下的动态库，
# 版本需与源码一致，本项目按 llama.cpp 0.9.0 的 API 编写）。
# 未指定 -DLLAMA_CPP_DIR 时，尝试探测开发机常见位置（~/Documents/agents/llama_cpp）。
if (APPLE AND DEFINED ENV{HOME} AND EXISTS "$ENV{HOME}/Documents/agents/llama_cpp")
    set(LLAMA_CPP_DEFAULT "$ENV{HOME}/Documents/agents/llama_cpp")
else()
    set(LLAMA_CPP_DEFAULT "")
endif()
set(LLAMA_CPP_DIR "${LLAMA_CPP_DEFAULT}" CACHE PATH "llama.cpp 源码目录（含已构建的 build/bin/*.dylib）")

set(LLAMA_INCLUDE_DIR      "${LLAMA_CPP_DIR}/include")
set(LLAMA_GGML_INCLUDE_DIR "${LLAMA_CPP_DIR}/ggml/include")
set(LLAMA_COMMON_DIR       "${LLAMA_CPP_DIR}/common")
set(LLAMA_MTMD_DIR         "${LLAMA_CPP_DIR}/tools/mtmd")
set(LLAMA_BIN_DIR          "${LLAMA_CPP_DIR}/build/bin")

foreach(p ${LLAMA_INCLUDE_DIR}/llama.h
          ${LLAMA_GGML_INCLUDE_DIR}/ggml.h
          ${LLAMA_COMMON_DIR}/chat.h
          ${LLAMA_MTMD_DIR}/mtmd.h
          ${LLAMA_BIN_DIR}/libllama.dylib
          ${LLAMA_BIN_DIR}/libllama-common.dylib
          ${LLAMA_BIN_DIR}/libmtmd.dylib)
    if (NOT EXISTS "${p}")
        message(FATAL_ERROR "缺少依赖: ${p}\n"
            "请先构建 llama.cpp（cmake -B build && cmake --build build），"
            "或使用 -DLLAMA_CPP_DIR=<路径> 指向正确的源码目录")
    endif()
endforeach()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# ---------- llama.cpp 统一接口目标 ----------
add_library(llamacpp_deps INTERFACE)
target_include_directories(llamacpp_deps INTERFACE
    ${LLAMA_INCLUDE_DIR}
    ${LLAMA_GGML_INCLUDE_DIR}
    ${LLAMA_COMMON_DIR}
    ${LLAMA_MTMD_DIR})
target_link_libraries(llamacpp_deps INTERFACE
    ${LLAMA_BIN_DIR}/libllama.dylib
    ${LLAMA_BIN_DIR}/libllama-common.dylib
    ${LLAMA_BIN_DIR}/libmtmd.dylib)
# 运行时通过 @rpath 找到 libllama / libggml-* / libllama-common
target_link_options(llamacpp_deps INTERFACE "-Wl,-rpath,${LLAMA_BIN_DIR}")

set(LLAMA_SRCS src/LlamaChat.cpp)

# ---------- GUI 客户端 ----------
add_executable(qllama MACOSX_BUNDLE
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/ConfigDialog.cpp
    src/ConfigDialog.h
    src/ChatWorker.cpp
    src/ChatWorker.h
    src/LlamaChat.h
    src/Logging.cpp
    src/Logging.h
    src/Markdown.cpp
    src/Markdown.h
    src/ModelManageDialog.cpp
    src/ModelManageDialog.h
    src/SysMonitor.cpp
    src/SysMonitor.h
    ui/mainwindow.ui
    ui/configdialog.ui
    ui/modelmanagedialog.ui
    resources.qrc
    assets/qllama.icns
    ${LLAMA_SRCS}
    README.md
    README.en.md
    pack.sh)
set_target_properties(qllama PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "qllama"
    MACOSX_BUNDLE_GUI_IDENTIFIER "local.qllama"
    MACOSX_BUNDLE_ICON_FILE "qllama.icns")
set_source_files_properties(assets/qllama.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources")
target_link_libraries(qllama PRIVATE Qt6::Widgets Qt6::Network llamacpp_deps)
# Homebrew Qt 的 framework 目录（若 Qt 的 install_name 使用 @rpath 时需要）
if (APPLE AND EXISTS "/opt/homebrew/opt/qt/lib")
    target_link_options(qllama PRIVATE "-Wl,-rpath,/opt/homebrew/opt/qt/lib")
endif()

# ---------- 冒烟测试（无 GUI，验证 llama.cpp 集成是否正常） ----------
add_executable(qllama_smoke src/smoke.cpp ${LLAMA_SRCS})
target_link_libraries(qllama_smoke PRIVATE llamacpp_deps)
