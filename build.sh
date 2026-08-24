#!/bin/bash
# Build HLASMLexer.dll. The output is a Windows DLL, so on Linux/macOS
# you need the MinGW-w64 *cross* toolchain, not the native g++ (native
# g++ builds Linux .so files, which Notepad++ can't load). We prefer
# the cross tools when present and fall back to plain g++ on Windows
# (MSYS2/MinGW shells), where g++ already targets Windows.

# Pick the toolchain. x86_64-w64-mingw32-* is the standard cross prefix
# on Debian/Ubuntu (mingw-w64), Fedora (mingw64-gcc), Arch (mingw-w64-gcc),
# and Homebrew (mingw-w64).
if command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
    CXX=x86_64-w64-mingw32-g++
    WINDRES=x86_64-w64-mingw32-windres
    command -v "$WINDRES" >/dev/null 2>&1 || WINDRES=windres
elif command -v g++ >/dev/null 2>&1; then
    CXX=g++
    WINDRES=windres
else
    echo "No compiler found."
    echo "Linux/macOS: install the MinGW-w64 cross toolchain, e.g."
    echo "  Debian/Ubuntu : sudo apt install mingw-w64"
    echo "  Fedora        : sudo dnf install mingw64-gcc-c++"
    echo "  Arch          : sudo pacman -S mingw-w64-gcc"
    echo "  macOS (brew)  : brew install mingw-w64"
    echo "Windows: install MSYS2 + MinGW-w64 and run from the MinGW shell."
    exit 1
fi

command -v "$WINDRES" >/dev/null 2>&1 || { echo "$WINDRES not found (part of the MinGW-w64 toolchain)"; exit 1; }

echo "Toolchain: $CXX / $WINDRES"
mkdir -p obj bin

echo "Generating config XML header..."
cat src/config_xml_head.inc config/HLASMLexer.xml src/config_xml_tail.inc > obj/hlasm_config_xml.h || exit 1

echo "Compiling..."
"$CXX" -c -Wall -Wextra -O2 -std=c++11 -DUNICODE -D_UNICODE -Isrc -Iobj -o obj/hlasm_ilexer.o src/hlasm_ilexer.cpp || exit 1

echo "Resource..."
"$WINDRES" --output-format=coff src/HLASMLexer.rc -o obj/HLASMLexer.res || exit 1

echo "Linking..."
"$CXX" -shared -static -o bin/HLASMLexer.dll obj/hlasm_ilexer.o obj/HLASMLexer.res exports.def -s -luser32 || exit 1

echo ""
echo "Done: bin/HLASMLexer.dll"
echo "Copy to: Notepad++/plugins/HLASMLexer/HLASMLexer.dll"
