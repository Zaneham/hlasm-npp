# HLASM Lexer — Column-Aware Syntax Highlighting for Notepad++

A Notepad++ plugin that provides **column-aware** syntax highlighting for IBM High Level Assembler (HLASM) source files. Unlike generic assembly highlighters, this plugin understands the 80-column punch card layout that HLASM requires.

## What It Does

HLASM source follows a rigid column layout inherited from 80-column punch cards:

```
Columns 1-71     Content area (label, operation, operand, remarks)
Column  72       Continuation marker (non-blank = line continues)
Columns 73-80    Sequence number
```

This plugin highlights each field by its **column position**, not just by keywords:

| Field        | Colour        | Description                          |
|-------------|---------------|--------------------------------------|
| Label       | Dark blue, bold | Symbols starting in column 1       |
| Operation   | Blue          | Instruction/directive after label    |
| Operand     | Black         | Arguments to the operation           |
| Remarks     | Green, italic | Comments after the operand field     |
| String      | Red           | Quoted constants (`C'HELLO'`, `X'FF'`) |
| Number      | Orange        | Numeric literals                     |
| Register    | Purple        | R0–R15                               |
| Continuation| Grey on yellow| Non-blank character in column 72     |
| Sequence    | Light grey    | Columns 73–80                        |
| Comment     | Green, italic | Lines starting with `*` or `.*`      |

Vertical **column ruler lines** are drawn at the content/continuation boundary (col 72), continuation/sequence boundary (col 73), and end of card (col 80).

## Supported Features

- Full-line comments (`*` in column 1, `.*` for macro comments)
- Continuation lines (non-blank in column 72, operand resumes at column 16)
- Multi-line string continuation (tracks open quotes across continuation lines)
- Type-prefixed string constants (`C'...'`, `X'...'`, `B'...'`, `F'...'`, etc.)
- Parenthesised sub-expressions in operands (e.g. `AIF ('&REG' EQ '').NOREG`)
- Macro variable references (`&VAR`) and sequence symbols (`.LABEL`)
- Register detection (R0–R15, case-insensitive)
- Debounced re-styling on edit (50ms delay so typing stays responsive)

## Installation

### Requirements

- **Notepad++ 8.x** (64-bit)
- The plugin DLL must match your Notepad++ architecture (64-bit NP++ needs 64-bit DLL)

### Steps

1. **Close Notepad++** completely.

2. **Copy the DLL** to the Notepad++ plugins directory:
   ```
   C:\Program Files\Notepad++\plugins\HLASMLexer\HLASMLexer.dll
   ```
   Create the `HLASMLexer` folder if it doesn't exist. The folder name **must** match the DLL name (minus the `.dll`).

3. **Restart Notepad++**. You should see **Plugins → HLASM Lexer → Apply HLASM Highlighting** in the menu.

4. **Open an HLASM source file** and the highlighting applies automatically.

### File Extensions

The plugin applies automatically to **all files** when a buffer is opened. For best results, use one of these extensions for your HLASM source:

| Extension | Use                        |
|-----------|----------------------------|
| `.mlc`    | Main assembler source (recommended) |
| `.mac`    | Macro definitions          |
| `.cpy`    | Copy members               |

> **!!!!!!! Important: Avoid `.asm` for HLASM files.**
>
> Notepad++ has a built-in Assembly language definition that claims `.asm` files. Its highlighter will fight with this plugin, producing garbled colours. Rename your files to `.mlc` or `.mac` instead. If you must use `.asm`, go to **Settings → Style Configurator → Language: Assembly** and remove `asm` from the file extensions list.

### Manual Apply

If auto-apply doesn't trigger for a particular file, use **Plugins → HLASM Lexer → Apply HLASM Highlighting** from the menu.

## Building from Source

### Requirements

- MinGW-w64 (64-bit GCC for Windows)
- No other dependencies 

### Build

```bat
g++ -c -Wall -Wextra -O2 -std=c++11 -DUNICODE -D_UNICODE -o obj\hlasm_lexer.o src\hlasm_lexer.cpp
g++ -shared -static -o bin\HLASMLexer.dll obj\hlasm_lexer.o exports.def -s -luser32
```

Or just run `build.bat`.

The resulting DLL is ~24KB.

## Limitations

- **No keyword colouring.** Operations like `MVC`, `LA`, `STM` are all the same blue. The plugin highlights by column position, not by matching instruction mnemonics against a dictionary.
- **No semantic analysis.** The parser doesn't know the difference between a DC operand and a macro parameter, it just follows the column rules.
- **Fixed-point and bit-level constructs** (e.g. `POS`, scaled fixed-point) are not specially handled.
- **`.asm` extension conflict.** See the installation note above.
- **Applies to all buffers.** The plugin styles whatever file is open. For non-HLASM files, Notepad++'s built-in lexer will typically override it. If a non-HLASM file looks wrong, just switch to its correct language via the Language menu.
- **Full re-style on edit.** Every keystroke triggers a full document re-style (debounced to 50ms). This is fine for typical HLASM files (< 5,000 lines) but may lag on very large files.

## Architecture

The plugin is a single C++ file (~280 lines) that:

1. Exports the six functions Notepad++ requires from a plugin DLL
2. On buffer activation, reads each line via `SCI_GETLINE`
3. Parses HLASM column layout into a style byte array
4. Applies styles via `SCI_STARTSTYLING` + `SCI_SETSTYLING`
5. Configures colours via `SCI_STYLESETFORE` / `SCI_STYLESETBOLD` / etc.
6. Draws column rulers via `SCI_MULTIEDGEADDLINE`

No custom Scintilla lexer (ILexer5) is used. All interaction with Scintilla is through `SendMessage`, which avoids cross-compiler ABI issues between MinGW and Notepad++'s MSVC-compiled Scintilla. Yes this was a joy to figure out originally lol!

## Licence

Apache 2.0 — see [LICENSE](LICENSE).
