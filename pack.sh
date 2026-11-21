#!/usr/bin/env bash
# ============================================================
# qllama 打包脚本：构建 → 部署依赖 → 修复路径 → 签名 → 生成 DMG
#
# 用法:
#   ./pack.sh               # 增量构建 + 打包 + 运行时验证（窗口会短暂出现）
#   ./pack.sh --no-build    # 跳过构建，直接打包现有 build/qllama.app
#   ./pack.sh --no-verify   # 跳过运行时验证（启动部署版确认插件加载）
#
# 产物: dist/qllama-<版本>-<架构>.dmg（拖拽 qllama.app 到 Applications 安装）
#
# 踩坑备忘（脚本里的修复步骤依赖这些事实，勿删）:
#   1. Homebrew 版 macdeployqt 不生成 qt.conf → 部署后 Qt 找不到 bundle 内插件目录
#   2. macdeployqt 漏打 SVG 渲染链（QtSvg.framework + libqsvg.dylib）→ 按钮图标全部消失
#   3. macdeployqt 误带入 QML 框架簇与 Qt 虚拟键盘插件（纯 Widgets 应用用不到）
#   4. install_name_tool 改动后必须 ad-hoc 重签，arm64 缺签名无法运行
# ============================================================
set -euo pipefail
cd "$(dirname "$0")"

APP_DIR=dist/qllama.app
NO_BUILD=0
NO_VERIFY=0
for arg in "$@"; do
    case "$arg" in
        --no-build)  NO_BUILD=1 ;;
        --no-verify) NO_VERIFY=1 ;;
        *) echo "未知参数: $arg"; exit 2 ;;
    esac
done

MACDEPLOYQT=/opt/homebrew/opt/qt/bin/macdeployqt
QTSVG_FW=/opt/homebrew/opt/qtsvg/lib/QtSvg.framework
QT_PLUGINS=/opt/homebrew/opt/qt/share/qt/plugins

# ---------- 工具函数 ----------
# 把二进制里指向 Homebrew qtbase 框架的绝对路径依赖改写为 @rpath 或 @executable_path
fix_qt_framework_deps() { # $1=二进制  $2=rpath|exec
    local dep name repl
    while IFS= read -r dep; do
        name=$(basename "$dep")
        if [ "$2" = exec ]; then
            repl="@executable_path/../Frameworks/$name.framework/Versions/A/$name"
        else
            repl="@rpath/$name.framework/Versions/A/$name"
        fi
        install_name_tool -change "$dep" "$repl" "$1" 2>/dev/null  # "will invalidate signature" 警告属预期，稍后统一重签
    done < <(otool -L "$1" | awk '/\/opt\/homebrew\/opt\/qtbase\/lib\/.*\.framework/{print $1}')
}

# 删除二进制里所有本机绝对路径 rpath（/Users、/opt/homebrew），可选确保某 rpath 存在
clean_machine_rpaths() { # $1=二进制  [$2=需要确保存在的 rpath]
    local rp
    while IFS= read -r rp; do
        case "$rp" in
            /Users/*|/opt/homebrew/*) install_name_tool -delete_rpath "$rp" "$1" 2>/dev/null || true ;;
        esac
    done < <(otool -l "$1" | awk '/^ *path /{print $2}')
    if [ -n "${2:-}" ] && ! otool -l "$1" | grep -qF "path $2 "; then
        install_name_tool -add_rpath "$2" "$1" 2>/dev/null
    fi
}

# ---------- 0. 依赖检查 ----------
[ -x "$MACDEPLOYQT" ] || { echo "✗ 缺少 macdeployqt: $MACDEPLOYQT"; exit 1; }
[ -d "$QTSVG_FW" ] || { echo "✗ 缺少 QtSvg.framework（brew install qtsvg）"; exit 1; }
[ -f "$QT_PLUGINS/imageformats/libqsvg.dylib" ] || { echo "✗ 缺少 libqsvg.dylib 插件"; exit 1; }

# ---------- 1. 构建 ----------
if [ $NO_BUILD -eq 0 ]; then
    echo "== 构建 =="
    if [ ! -f build/CMakeCache.txt ]; then
        cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    fi
    cmake --build build
fi
[ -d build/qllama.app ] || { echo "✗ 找不到 build/qllama.app，请先构建"; exit 1; }

# ---------- 2. 复制并部署 Qt 依赖 ----------
echo "== 部署 Qt 依赖（macdeployqt） =="
rm -rf "$APP_DIR"
cp -R build/qllama.app "$APP_DIR"
"$MACDEPLOYQT" "$APP_DIR" >/dev/null 2>&1 || true  # QtVirtualKeyboard 等无关警告可忽略

CONTENTS="$APP_DIR/Contents"

# ---------- 3. 精简：删除用不到的 QML 框架簇与虚拟键盘插件 ----------
echo "== 精简 bundle =="
for fw in QtQml QtQmlMeta QtQmlModels QtQmlWorkerScript QtQuick; do
    rm -rf "$CONTENTS/Frameworks/$fw.framework"
done
rm -f "$CONTENTS/PlugIns/platforminputcontexts/libqtvirtualkeyboardplugin.dylib"
rmdir "$CONTENTS/PlugIns/platforminputcontexts" 2>/dev/null || true

# ---------- 4. 补齐 SVG 渲染链（按钮图标依赖，macdeployqt 会漏打） ----------
echo "== 补齐 SVG 渲染链 =="
mkdir -p "$CONTENTS/Frameworks" "$CONTENTS/PlugIns/imageformats" "$CONTENTS/Resources"
cp -R "$QTSVG_FW" "$CONTENTS/Frameworks/"
cp "$QT_PLUGINS/imageformats/libqsvg.dylib" "$CONTENTS/PlugIns/imageformats/"

# 框架自身 ID 若为绝对路径则改写为 @rpath（如 QtDBus）
for fwbin in "$CONTENTS"/Frameworks/*.framework/Versions/A/*; do
    [ -f "$fwbin" ] || continue
    id=$(otool -D "$fwbin" 2>/dev/null | tail -1)
    case "$id" in
        /opt/homebrew/*)
            name=$(basename "$fwbin")
            install_name_tool -id "@rpath/$name.framework/Versions/A/$name" "$fwbin" 2>/dev/null ;;
    esac
done
# 框架内部与插件的绝对 Qt 依赖全部改写
while IFS= read -r fwbin; do fix_qt_framework_deps "$fwbin" rpath; done \
    < <(find "$CONTENTS/Frameworks" -path '*/Versions/A/*' -type f ! -name '*.prl' ! -name '*.plist' ! -name '*.json')
while IFS= read -r p; do fix_qt_framework_deps "$p" exec; done \
    < <(find "$CONTENTS/PlugIns" -name '*.dylib')

# ---------- 5. 清理本机路径 ----------
echo "== 清理本机路径 =="
clean_machine_rpaths "$CONTENTS/MacOS/qllama" "@executable_path/../Frameworks"
while IFS= read -r dylib; do clean_machine_rpaths "$dylib" "@loader_path"; done \
    < <(find "$CONTENTS/Frameworks" -maxdepth 1 -name '*.dylib')
while IFS= read -r fwbin; do clean_machine_rpaths "$fwbin"; done \
    < <(find "$CONTENTS/Frameworks" -path '*/Versions/A/*' -type f ! -name '*.prl' ! -name '*.plist' ! -name '*.json')

# ---------- 6. 生成 qt.conf（指向 bundle 内插件目录） ----------
printf '[Paths]\nPlugins = PlugIns\n' > "$CONTENTS/Resources/qt.conf"

# ---------- 7. 全量扫描：不允许任何本机绝对路径残留 ----------
echo "== 扫描本机路径残留 =="
RESIDUAL=0
while IFS= read -r bin; do
    if { otool -L "$bin" 2>/dev/null; otool -l "$bin" 2>/dev/null | grep -A1 LC_RPATH; } \
            | grep -qE '/opt/homebrew|/Users/'; then
        echo "✗ 残留本机路径: $bin"
        RESIDUAL=1
    fi
done < <(find "$CONTENTS" -type f \( -name '*.dylib' -o -name 'qllama' -o -path '*/Versions/*/*' \) \
    ! -name '*.prl' ! -name '*.plist' ! -name '*.json' ! -name '*.qmltypes')
[ $RESIDUAL -eq 0 ] || { echo "✗ 存在本机绝对路径残留，打包中止"; exit 1; }
echo "✅ 无残留"

# ---------- 8. 签名 ----------
echo "== 签名 =="
find "$CONTENTS/Frameworks" -maxdepth 1 -name '*.dylib' -exec codesign --force --sign - {} \; 2>/dev/null || true
find "$CONTENTS/PlugIns" -name '*.dylib' -exec codesign --force --sign - {} \; 2>/dev/null || true
find "$CONTENTS/Frameworks" -maxdepth 1 -name '*.framework' -type d -exec codesign --force --sign - {} \; 2>/dev/null || true
codesign --force --sign - "$APP_DIR"
codesign --verify --deep --strict "$APP_DIR"
echo "✅ 签名验证通过"

# ---------- 9. 运行时验证：启动部署版，确认 SVG 插件从 bundle 内加载 ----------
if [ $NO_VERIFY -eq 0 ]; then
    echo "== 运行时验证（窗口会短暂出现） =="
    QT_DEBUG_PLUGINS=1 "$CONTENTS/MacOS/qllama" >/tmp/qllama_pack_verify.log 2>&1 &
    VPID=$!
    sleep 5
    if kill -0 $VPID 2>/dev/null; then
        echo "✅ 部署版启动正常（5 秒内未崩溃）"
        kill $VPID
        wait $VPID 2>/dev/null || true  # 吞掉 "Terminated" 杂音
    else
        echo "✗ 部署版启动失败，日志: /tmp/qllama_pack_verify.log"
        exit 1
    fi
    grep -q 'qsvgicon.*loaded library' /tmp/qllama_pack_verify.log \
        && echo "✅ SVG 图标插件加载正常" \
        || echo "⚠️ 未检测到 SVG 插件加载，请人工确认按钮图标"
fi

# ---------- 10. 生成 DMG ----------
echo "== 生成 DMG =="
VERSION=$(grep -m1 'project(qllama VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/' || echo 0.1.0)
ARCH=$(uname -m)
DMG="dist/qllama-${VERSION}-${ARCH}.dmg"
STAGE=dist/dmg
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp -R "$APP_DIR" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
hdiutil create -volname qllama -srcfolder "$STAGE" -ov -format UDZO "$DMG" 2>/dev/null
rm -rf "$STAGE"

echo ""
echo "✅ 打包完成: $DMG"
ls -lh "$DMG"
echo "安装: 双击挂载后把 qllama.app 拖入 Applications"
