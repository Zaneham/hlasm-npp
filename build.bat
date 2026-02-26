@echo off
if not exist obj mkdir obj
if not exist bin mkdir bin

echo Compiling...
g++ -c -Wall -Wextra -O2 -std=c++11 -DUNICODE -D_UNICODE -o obj\hlasm_lexer.o src\hlasm_lexer.cpp
if errorlevel 1 goto :fail

echo Linking...
g++ -shared -static -o bin\HLASMLexer.dll obj\hlasm_lexer.o exports.def -s -luser32
if errorlevel 1 goto :fail

echo.
echo OK: bin\HLASMLexer.dll (copy to Notepad++\plugins\HLASMLexer\)
goto :end

:fail
echo.
echo BUILD FAILED

:end
