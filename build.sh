#!/bin/bash
# Build HLASMLexer.dll — needs MinGW-w64 g++ in PATH

command -v g++ >/dev/null || { echo "g++ not found — install MinGW-w64"; exit 1; }

mkdir -p obj bin

echo "Compiling..."
g++ -c -Wall -Wextra -O2 -std=c++11 -DUNICODE -D_UNICODE -o obj/hlasm_lexer.o src/hlasm_lexer.cpp || exit 1

echo "Linking..."
g++ -shared -static -o bin/HLASMLexer.dll obj/hlasm_lexer.o exports.def -s -luser32 || exit 1

echo ""
echo "Done: bin/HLASMLexer.dll"
echo "Copy to: Notepad++/plugins/HLASMLexer/HLASMLexer.dll"
