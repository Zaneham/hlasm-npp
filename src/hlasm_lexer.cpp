/*
 * hlasm_lexer.cpp -- Column-aware HLASM highlighter for Notepad++
 *
 * Direct Scintilla styling via SendMessage. No ILexer5, no vtable,
 * no cross-compiler ABI roulette. Just column parsing and
 * SCI_SETSTYLING, like civilised folk.
 * Also yeah I did basically copy other Plugins I dont know c++ OOP scares me.
 *
 * 80-column card layout (0-indexed):
 *   Col 0-70    Content (label, operation, operand, remarks)
 *   Col 71      Continuation marker (non-blank = next line continues)
 *   Col 72-79   Sequence number
 */

#include <windows.h>
#include <cstring>
#include <cctype>

/* ---- Scintilla messages ---- */
#define SCI_STARTSTYLING        2032
#define SCI_SETSTYLING          2033
#define SCI_STYLESETFORE        2051
#define SCI_STYLESETBACK        2052
#define SCI_STYLESETBOLD        2053
#define SCI_STYLESETITALIC      2054
#define SCI_GETLINECOUNT        2154
#define SCI_GETLINE             2153
#define SCI_LINELENGTH          2350
#define SCI_POSITIONFROMLINE    2167
#define SCI_SETEDGEMODE         2363
#define SCI_MULTIEDGEADDLINE    2694
#define SCI_MULTIEDGECLEARALL   2695
#define EDGE_MULTILINE          3
#define SCN_MODIFIED            2008

/* ---- Notepad++ plugin interface ---- */
#define NPPMSG                    (WM_USER + 1000)
#define NPPM_GETCURRENTSCINTILLA  (NPPMSG + 4)

#define NPPN_FIRST              1000
#define NPPN_READY              (NPPN_FIRST + 1)
#define NPPN_FILEOPENED         (NPPN_FIRST + 4)
#define NPPN_SHUTDOWN           (NPPN_FIRST + 9)
#define NPPN_BUFFERACTIVATED    (NPPN_FIRST + 10)

struct NppData {
    HWND _nppHandle;
    HWND _scintillaMainHandle;
    HWND _scintillaSecondHandle;
};

typedef void (*PFUNCPLUGINCMD)();

struct ShortcutKey {
    bool _isCtrl;
    bool _isAlt;
    bool _isShift;
    UCHAR _key;
};

struct FuncItem {
    TCHAR _itemName[64];
    PFUNCPLUGINCMD _pFunc;
    int _cmdID;
    bool _init2Check;
    ShortcutKey *_pShKey;
};

struct SCNotification {
    NMHDR nmhdr;
    char _padding[256];
};

/* ---- Style IDs ---- */
enum {
    S_DEFAULT = 0, S_LABEL, S_OPERATION, S_OPERAND, S_COMMENT,
    S_STRING, S_NUMBER, S_CONTINUATION, S_SEQUENCE, S_REGISTER
};

#define MAX_LINE 256
#define COL72    71

/* ---- Helpers ---- */

static bool IsNameChar(char ch) {
    unsigned char c = (unsigned char)ch;
    return isalnum(c) || c == '@' || c == '#' || c == '$' || c == '_';
}

static int IsRegister(const char *buf, int pos, int limit) {
    if (pos >= limit) return 0;
    if (buf[pos] != 'R' && buf[pos] != 'r') return 0;
    int p = pos + 1;
    if (p >= limit || !isdigit((unsigned char)buf[p])) return 0;
    int d1 = buf[p] - '0';
    p++;
    if (p >= limit || !isdigit((unsigned char)buf[p])) {
        if (p < limit && IsNameChar(buf[p])) return 0;
        return 2;
    }
    int regnum = d1 * 10 + (buf[p] - '0');
    p++;
    if (regnum < 10 || regnum > 15) return 0;
    if (p < limit && IsNameChar(buf[p])) return 0;
    return 3;
}

static bool IsTypePrefix(char ch) {
    char u = (char)toupper((unsigned char)ch);
    return u == 'C' || u == 'X' || u == 'B' || u == 'F' || u == 'H' ||
           u == 'P' || u == 'Z' || u == 'A' || u == 'E' || u == 'D' ||
           u == 'Y' || u == 'V' || u == 'S' || u == 'G' || u == 'J';
}

/* ---- Operand sub-parser (fills sty[] from col to endCol) ---- */

static bool LexOperands(const char *buf, char *sty, int col, int endCol, bool inStr) {
    int parenDepth = 0;

    while (col < endCol) {
        if (inStr) {
            int start = col;
            bool closed = false;
            while (col < endCol) {
                if (buf[col] == '\'') {
                    col++;
                    if (col < endCol && buf[col] == '\'') col++;
                    else { closed = true; break; }
                } else col++;
            }
            memset(sty + start, S_STRING, col - start);
            if (!closed) return true;
            inStr = false;
            continue;
        }

        char ch = buf[col];

        if (ch == ' ' && parenDepth > 0) {
            sty[col] = S_OPERAND;
            col++;
            continue;
        }

        if (ch == ' ') {
            while (col < endCol && buf[col] == ' ') col++;
            if (col < endCol)
                memset(sty + col, S_COMMENT, endCol - col);
            col = endCol;
            return false;
        }

        if (ch == '(') parenDepth++;
        else if (ch == ')' && parenDepth > 0) parenDepth--;

        if (isalpha((unsigned char)ch) && IsTypePrefix(ch)) {
            int probe = col + 1;
            if (probe < endCol && toupper((unsigned char)buf[probe]) == 'L') {
                int saved = probe;
                probe++;
                while (probe < endCol && isdigit((unsigned char)buf[probe])) probe++;
                if (probe == saved + 1) probe = saved;
            }
            if (probe < endCol && buf[probe] == '\'') {
                if (col == 0 || !IsNameChar(buf[col - 1])) {
                    memset(sty + col, S_STRING, probe - col + 1);
                    col = probe + 1;
                    inStr = true;
                    continue;
                }
            }
        }

        if (isalpha((unsigned char)ch) || ch == '&' || ch == '.') {
            int rlen = IsRegister(buf, col, endCol);
            if (rlen > 0) {
                memset(sty + col, S_REGISTER, rlen);
                col += rlen;
                continue;
            }
            int sym = col;
            while (col < endCol && (IsNameChar(buf[col]) || buf[col] == '&' || buf[col] == '.')) col++;
            memset(sty + sym, S_OPERAND, col - sym);
            continue;
        }

        if (ch == '\'') {
            sty[col] = S_STRING;
            col++;
            inStr = true;
            continue;
        }

        if (isdigit((unsigned char)ch)) {
            int start = col;
            while (col < endCol && isdigit((unsigned char)buf[col])) col++;
            memset(sty + start, S_NUMBER, col - start);
            continue;
        }

        sty[col] = S_OPERAND;
        col++;
    }
    return inStr;
}

/* ---- Line lexer (fills sty[0..n-1], returns continuation state) ---- */

static int LexLine(const char *buf, char *sty, int n, int prevState) {
    if (n <= 0) return 0;

    int lim = (n > COL72) ? COL72 : n;
    int col = 0;
    bool inStr = false;

    /* Default everything first */
    memset(sty, S_DEFAULT, n);

    /* Full-line comment */
    if (buf[0] == '*' || (n > 1 && buf[0] == '.' && buf[1] == '*')) {
        memset(sty, S_COMMENT, lim);
        goto tail;
    }

    /* Continuation from previous line */
    if (prevState >= 1) {
        while (col < lim && buf[col] == ' ') col++;
        inStr = (prevState == 2);
        goto operands;
    }

    /* Label field */
    if (buf[0] != ' ') {
        while (col < lim && buf[col] != ' ') col++;
        memset(sty, S_LABEL, col);
    }

    /* Skip blanks to operation */
    while (col < lim && buf[col] == ' ') col++;

    /* Operation field */
    if (col < lim && buf[col] != ' ') {
        int op = col;
        while (col < lim && buf[col] != ' ') col++;
        memset(sty + op, S_OPERATION, col - op);
    }

    /* Skip blanks to operand */
    while (col < lim && buf[col] == ' ') col++;

operands:
    if (col < lim)
        inStr = LexOperands(buf, sty, col, lim, inStr);

tail:
    if (n > COL72) {
        bool cont = (buf[COL72] != ' ');
        sty[COL72] = cont ? S_CONTINUATION : S_DEFAULT;
        if (n > COL72 + 1) {
            int seq = n - (COL72 + 1);
            if (seq > 8) seq = 8;
            memset(sty + COL72 + 1, S_SEQUENCE, seq);
        }
        if (cont) return inStr ? 2 : 1;
    }
    return 0;
}

/* ---- Plugin State ---- */

static NppData nppData;
static FuncItem funcItems[1];
static bool pluginReady = false;
static UINT_PTR restyleTimer = 0;

static HWND getCurrentScintilla() {
    int which = -1;
    SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

/* ---- Style colours (BGR) ---- */

static void configureStyles(HWND sci) {
    SendMessage(sci, SCI_STYLESETFORE, S_DEFAULT,      0x000000);
    SendMessage(sci, SCI_STYLESETFORE, S_LABEL,         0x8B0000);
    SendMessage(sci, SCI_STYLESETBOLD, S_LABEL,         1);
    SendMessage(sci, SCI_STYLESETFORE, S_OPERATION,     0xFF0000);
    SendMessage(sci, SCI_STYLESETFORE, S_OPERAND,       0x000000);
    SendMessage(sci, SCI_STYLESETFORE, S_COMMENT,       0x008000);
    SendMessage(sci, SCI_STYLESETITALIC, S_COMMENT,     1);
    SendMessage(sci, SCI_STYLESETFORE, S_STRING,        0x0000CC);
    SendMessage(sci, SCI_STYLESETFORE, S_NUMBER,        0x008CFF);
    SendMessage(sci, SCI_STYLESETFORE, S_CONTINUATION,  0x808080);
    SendMessage(sci, SCI_STYLESETBACK, S_CONTINUATION,  0xCCFFFF);
    SendMessage(sci, SCI_STYLESETFORE, S_SEQUENCE,      0xC0C0C0);
    SendMessage(sci, SCI_STYLESETFORE, S_REGISTER,      0x800080);
}

static void configureEdges(HWND sci) {
    SendMessage(sci, SCI_SETEDGEMODE, EDGE_MULTILINE, 0);
    SendMessage(sci, SCI_MULTIEDGECLEARALL, 0, 0);
    SendMessage(sci, SCI_MULTIEDGEADDLINE, 71, 0xFF9050);
    SendMessage(sci, SCI_MULTIEDGEADDLINE, 72, 0xFF9050);
    SendMessage(sci, SCI_MULTIEDGEADDLINE, 80, 0x8080FF);
}

/* ---- The Main Event: style the entire document ---- */

static void styleDocument(HWND sci) {
    int lineCount = (int)SendMessage(sci, SCI_GETLINECOUNT, 0, 0);
    int prevState = 0;

    for (int line = 0; line < lineCount; line++) {
        int rawLen = (int)SendMessage(sci, SCI_LINELENGTH, line, 0);
        if (rawLen <= 0) { prevState = 0; continue; }
        if (rawLen > MAX_LINE - 1) rawLen = MAX_LINE - 1;

        char buf[MAX_LINE];
        char styles[MAX_LINE];
        SendMessage(sci, SCI_GETLINE, line, (LPARAM)buf);
        buf[rawLen] = '\0';

        /* Strip line ending to get content length */
        int n = rawLen;
        while (n > 0 && (buf[n - 1] == '\r' || buf[n - 1] == '\n')) n--;

        /* Parse content, fill styles */
        memset(styles, S_DEFAULT, rawLen);
        prevState = LexLine(buf, styles, n, prevState);

        /* Apply to Scintilla — run-length batched */
        int lineStart = (int)SendMessage(sci, SCI_POSITIONFROMLINE, line, 0);
        SendMessage(sci, SCI_STARTSTYLING, lineStart, 0);
        int i = 0;
        while (i < rawLen) {
            int s = styles[i];
            int run = 1;
            while (i + run < rawLen && styles[i + run] == s) run++;
            SendMessage(sci, SCI_SETSTYLING, run, s);
            i += run;
        }
    }
}

static void applyHLASM() {
    HWND sci = getCurrentScintilla();
    configureStyles(sci);
    configureEdges(sci);
    styleDocument(sci);
}

/* ---- Timer for debounced re-style on edits ---- */

static VOID CALLBACK restyleTimerProc(HWND, UINT, UINT_PTR id, DWORD) {
    KillTimer(NULL, id);
    restyleTimer = 0;
    HWND sci = getCurrentScintilla();
    configureStyles(sci);
    styleDocument(sci);
}

/* ---- Notepad++ Plugin Exports ---- */

extern "C" {

__declspec(dllexport) BOOL isUnicode() { return TRUE; }

__declspec(dllexport) void setInfo(NppData nd) { nppData = nd; }

__declspec(dllexport) const TCHAR * getName() { return TEXT("HLASM Lexer"); }

__declspec(dllexport) FuncItem * getFuncsArray(int *pNbFuncItems) {
    lstrcpy(funcItems[0]._itemName, TEXT("Apply HLASM Highlighting"));
    funcItems[0]._pFunc = applyHLASM;
    funcItems[0]._cmdID = 0;
    funcItems[0]._init2Check = false;
    funcItems[0]._pShKey = NULL;
    *pNbFuncItems = 1;
    return funcItems;
}

__declspec(dllexport) void beNotified(SCNotification *notify) {
    UINT code = notify->nmhdr.code;

    if (code == NPPN_READY) {
        pluginReady = true;
        applyHLASM();
        return;
    }

    if (code == NPPN_SHUTDOWN) {
        pluginReady = false;
        if (restyleTimer) { KillTimer(NULL, restyleTimer); restyleTimer = 0; }
        return;
    }

    if (!pluginReady) return;

    if (code == NPPN_BUFFERACTIVATED || code == NPPN_FILEOPENED) {
        applyHLASM();
    }

    if (code == SCN_MODIFIED) {
        if (restyleTimer) KillTimer(NULL, restyleTimer);
        restyleTimer = SetTimer(NULL, 0, 50, restyleTimerProc);
    }
}

__declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) { return TRUE; }

BOOL APIENTRY DllMain(HANDLE, DWORD, LPVOID) { return TRUE; }

} /* extern "C" */
