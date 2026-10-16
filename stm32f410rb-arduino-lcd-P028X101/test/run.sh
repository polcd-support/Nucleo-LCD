#!/bin/sh
# 主机端行为验证：编译 screen/ + app/ 并用虚拟面板跑完全流程。
set -e
cd "$(dirname "$0")/.."

CXX=${CXX:-g++}
BIN=test/out/host_test
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) BIN="$BIN.exe" ;;
esac

mkdir -p test/out

$CXX -std=c++17 -Wall -Wextra -O1 -Itest/host \
    test/host/main.cpp \
    test/host/stubs.cpp \
    test/host/sim.cpp \
    src/screen/screen.cpp \
    src/screen/touch/touch_ft6336u.cpp \
    src/screen/touch/touch_cst816.cpp \
    src/screen/panels/panel_p028x101.cpp \
    src/screen/gfx/gfx.cpp \
    src/screen/gfx/fonts.cpp \
    src/app/demo_app.cpp \
    -o "$BIN"

"$BIN"
