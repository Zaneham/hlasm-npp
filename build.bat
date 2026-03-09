@echo off
REM  Build HLASMLexer.dll - needs MinGW-w64 g++ in PATH
REM  Get it from https://winlibs.com or: choco install mingw

where g++ >nul 2>&1 || (echo g++ not found - install MinGW-w64 & goto :end)

if not exist obj mkdir obj
if not exist bin mkdir bin

echo Compiling...
g++ -c -Wall -Wextra -O2 -std=c++11 -DUNICODE -D_UNICODE -o obj\hlasm_lexer.o src\hlasm_lexer.cpp
if errorlevel 1 goto :fail

echo Linking...
g++ -shared -static -o bin\HLASMLexer.dll obj\hlasm_lexer.o exports.def -s -luser32
if errorlevel 1 goto :fail

echo.
echo Done: bin\HLASMLexer.dll
echo Copy to: Notepad++\plugins\HLASMLexer\HLASMLexer.dll
goto :end

:fail
echo BUILD FAILED
:end
