@echo off
REM  Build HLASMLexer.dll - HLASM as a real Scintilla/Lexilla ILexer5 lexer.
REM  Needs MinGW-w64 g++ and windres in PATH. 64-bit Notepad++ only.

where g++ >nul 2>&1 || (echo g++ not found - install MinGW-w64 & goto :end)

if not exist obj mkdir obj
if not exist bin mkdir bin

echo Generating config XML header...
copy /b src\config_xml_head.inc + config\HLASMLexer.xml + src\config_xml_tail.inc obj\hlasm_config_xml.h > nul
if errorlevel 1 goto :fail
echo Compiling lexer...
g++ -c -Wall -Wextra -O2 -std=c++11 -DUNICODE -D_UNICODE -Isrc -Iobj -o obj\hlasm_ilexer.o src\hlasm_ilexer.cpp
if errorlevel 1 goto :fail

echo Resource...
windres --output-format=coff src\HLASMLexer.rc -o obj\HLASMLexer.res
if errorlevel 1 goto :fail

echo Linking...
g++ -shared -static -o bin\HLASMLexer.dll obj\hlasm_ilexer.o obj\HLASMLexer.res exports.def -s -luser32
if errorlevel 1 goto :fail

echo.
echo Done: bin\HLASMLexer.dll
echo.
echo Install:
echo   1. Copy bin\HLASMLexer.dll        to  Notepad++\plugins\HLASMLexer\HLASMLexer.dll
echo   2. Restart Notepad++. The plugin writes plugins\Config\HLASMLexer.xml itself on first launch.
echo   3. "HLASM" appears in the Language menu.
goto :end

:fail
echo BUILD FAILED
:end
