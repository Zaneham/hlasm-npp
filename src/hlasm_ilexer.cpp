/*
 * hlasm_ilexer.cpp -- HLASM as a real Lexilla/Scintilla ILexer5 lexer.
 *
 * This is the "proper" version of the highlighter: instead of painting
 * styles over the top via SendMessage (which Notepad++'s own lexing
 * fights, hence the flicker), this registers as the buffer's actual
 * lexer. Notepad++ then owns nothing on top of it, so the highlighting
 * just sticks. Same brain as the plugin (hlasm_core.h); different socket.
 *
 * Built with MinGW-w64 for x64 Notepad++. On 64-bit Windows there is one
 * calling convention, so the GCC<->MSVC vtable ABI is compatible for a
 * single-inheritance pure-virtual interface like ILexer5.
 */

#include <stdint.h>   /* intptr_t, for Sci_Position.h (it only pulls stddef.h) */

#include "scintilla/ILexer.h"
#include "scintilla/Lexilla.h"
#include "hlasm_core.h"

#include <cstring>

using namespace Scintilla;

namespace {

/* Style names handed to Notepad++ so it can map styles to colours.
 * Order MUST match the S_* enum in hlasm_core.h. */
static const char *kStyleNames[HLASM_NUM_STYLES] = {
    "default", "label", "operation", "operand", "comment",
    "string", "number", "continuation", "sequence", "register"
};

/* Lexer identifier. Not in SciLexer.h (this is an external lexer), so use a
 * private high value. Notepad++ keys external lexers by name, not this id. */
#define HLASM_LEXER_ID 9001

class LexerHLASM : public ILexer5 {
public:
    LexerHLASM() {}
    virtual ~LexerHLASM() {}

    /* ---- ILexer4 ---- */
    int SCI_METHOD Version() const override { return lvRelease5; }
    void SCI_METHOD Release() override { delete this; }

    const char * SCI_METHOD PropertyNames() override { return ""; }
    int SCI_METHOD PropertyType(const char *) override { return 0; }
    const char * SCI_METHOD DescribeProperty(const char *) override { return ""; }
    Sci_Position SCI_METHOD PropertySet(const char *, const char *) override { return -1; }
    const char * SCI_METHOD DescribeWordListSets() override { return ""; }
    Sci_Position SCI_METHOD WordListSet(int, const char *) override { return -1; }

    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc,
                        int initStyle, IDocument *pAccess) override;
    void SCI_METHOD Fold(Sci_PositionU, Sci_Position, int, IDocument *) override {}

    void * SCI_METHOD PrivateCall(int, void *) override { return nullptr; }
    int SCI_METHOD LineEndTypesSupported() override { return 0; }

    int SCI_METHOD AllocateSubStyles(int, int) override { return -1; }
    int SCI_METHOD SubStylesStart(int) override { return -1; }
    int SCI_METHOD SubStylesLength(int) override { return 0; }
    int SCI_METHOD StyleFromSubStyle(int subStyle) override { return subStyle; }
    int SCI_METHOD PrimaryStyleFromStyle(int style) override { return style; }
    void SCI_METHOD FreeSubStyles() override {}
    void SCI_METHOD SetIdentifiers(int, const char *) override {}
    int SCI_METHOD DistanceToSecondaryStyles() override { return 0; }
    const char * SCI_METHOD GetSubStyleBases() override { return ""; }

    int SCI_METHOD NamedStyles() override { return HLASM_NUM_STYLES; }
    const char * SCI_METHOD NameOfStyle(int style) override {
        return (style >= 0 && style < HLASM_NUM_STYLES) ? kStyleNames[style] : "";
    }
    const char * SCI_METHOD TagsOfStyle(int) override { return ""; }
    const char * SCI_METHOD DescriptionOfStyle(int) override { return ""; }

    /* ---- ILexer5 ---- */
    const char * SCI_METHOD GetName() override { return "HLASM"; }
    int SCI_METHOD GetIdentifier() override { return HLASM_LEXER_ID; }
    const char * SCI_METHOD PropertyGet(const char *) override { return ""; }
};

void SCI_METHOD LexerHLASM::Lex(Sci_PositionU startPos, Sci_Position lengthDoc,
                                int /*initStyle*/, IDocument *pAccess) {
    const Sci_Position docLen = pAccess->Length();
    const Sci_Position startLine = pAccess->LineFromPosition((Sci_Position)startPos);

    Sci_Position endPos = (Sci_Position)startPos + lengthDoc;
    if (endPos > docLen) endPos = docLen;
    const Sci_Position endLine =
        pAccess->LineFromPosition(endPos > 0 ? endPos - 1 : 0);

    /* Continuation state carried in from the previous line (line-state),
     * so re-lexing a slice mid-document resumes correctly. */
    int prevState = (startLine > 0) ? (pAccess->GetLineState(startLine - 1) & 3) : 0;

    for (Sci_Position line = startLine; line <= endLine; line++) {
        const Sci_Position ls = pAccess->LineStart(line);
        const Sci_Position lnext = pAccess->LineStart(line + 1);
        int rawLen = (int)(lnext - ls);

        if (rawLen <= 0) {
            pAccess->StartStyling(ls);
            pAccess->SetLineState(line, 0);
            prevState = 0;
            continue;
        }
        if (rawLen > MAX_LINE - 1) rawLen = MAX_LINE - 1;

        char buf[MAX_LINE];
        char sty[MAX_LINE];
        pAccess->GetCharRange(buf, ls, rawLen);
        buf[rawLen] = '\0';

        /* Strip the line ending to get content length; the ending bytes
         * stay S_DEFAULT. */
        int n = rawLen;
        while (n > 0 && (buf[n - 1] == '\r' || buf[n - 1] == '\n')) n--;

        memset(sty, S_DEFAULT, rawLen);
        const int newState = LexLine(buf, sty, n, prevState);

        pAccess->StartStyling(ls);
        pAccess->SetStyles(rawLen, sty);

        pAccess->SetLineState(line, newState);
        prevState = newState;
    }
}

LexerHLASM *FactoryHLASM() { return new LexerHLASM(); }

} /* anonymous namespace */

/* ---- Lexilla exports (Notepad++ loads these) ---- */

extern "C" {

ILexer5 * LEXILLA_CALL CreateLexer(const char *name) {
    if (name && _stricmp(name, "HLASM") == 0) return FactoryHLASM();
    return nullptr;
}

int LEXILLA_CALL GetLexerCount(void) { return 1; }

void LEXILLA_CALL GetLexerName(unsigned int index, char *name, int buflength) {
    if (buflength <= 0) return;
    name[0] = '\0';
    if (index == 0) {
        strncpy(name, "HLASM", buflength - 1);
        name[buflength - 1] = '\0';
    }
}

LexerFactoryFunction LEXILLA_CALL GetLexerFactory(unsigned int index) {
    return (index == 0) ? reinterpret_cast<LexerFactoryFunction>(FactoryHLASM) : nullptr;
}

const char * LEXILLA_CALL GetNameSpace(void) { return ""; }

const char * LEXILLA_CALL GetLibraryPropertyNames(void) { return ""; }

void LEXILLA_CALL SetLibraryProperty(const char *, const char *) {}

} /* extern "C" */

/* ---- Minimal Notepad++ plugin glue ----
 * Since Scintilla 5 / NPP 8.4, custom lexers load via a *plugin* that also
 * exports the Lexilla functions above. These stubs make the DLL a valid
 * plugin with no menu commands; the lexer itself is delivered by CreateLexer.
 * Must be built with -DUNICODE so getName returns wchar_t*, matching NPP. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

struct NppData { HWND _nppHandle; HWND _scintillaMainHandle; HWND _scintillaSecondHandle; };
typedef void (*PFUNCPLUGINCMD)();
struct ShortcutKey { bool _isCtrl; bool _isAlt; bool _isShift; UCHAR _key; };
struct FuncItem { TCHAR _itemName[64]; PFUNCPLUGINCMD _pFunc; int _cmdID; bool _init2Check; ShortcutKey *_pShKey; };

/* ---- NPP / Scintilla bits for the card-boundary rulers ---- */
#define NPPMSG                   (WM_USER + 1000)
#define NPPM_GETCURRENTSCINTILLA (NPPMSG + 4)
#define NPPM_GETPLUGINSCONFIGDIR (NPPMSG + 46)
#define NPPN_FIRST               1000
#define NPPN_FILEOPENED          (NPPN_FIRST + 4)
#define NPPN_BUFFERACTIVATED     (NPPN_FIRST + 10)
#define SCI_SETEDGEMODE          2363
#define SCI_MULTIEDGEADDLINE     2694
#define SCI_MULTIEDGECLEARALL    2695
#define SCI_GETLEXERLANGUAGE     4012
#define SCI_STYLESETFORE         2051
#define SCI_STYLESETBACK         2052
#define SCI_STYLESETBOLD         2053
#define SCI_STYLESETITALIC       2054
#define EDGE_MULTI               3

struct SciNotify { NMHDR nmhdr; char _pad[256]; };

static NppData g_nppData;

static HWND CurrentSci() {
    int which = -1;
    SendMessage(g_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    return (which == 0) ? g_nppData._scintillaMainHandle
                        : g_nppData._scintillaSecondHandle;
}

static bool CurrentIsHLASM(HWND sci) {
    char name[32] = { 0 };
    SendMessageA(sci, SCI_GETLEXERLANGUAGE, 0, (LPARAM)name);
    return _stricmp(name, "HLASM") == 0;
}

/* The card-boundary rulers the old plugin drew, restored here and shown
 * only on HLASM buffers (cleared on everything else). */
static void ApplyEdges(HWND sci, bool on) {
    SendMessage(sci, SCI_MULTIEDGECLEARALL, 0, 0);
    if (on) {
        SendMessage(sci, SCI_SETEDGEMODE, EDGE_MULTI, 0);
        SendMessage(sci, SCI_MULTIEDGEADDLINE, 71, 0xFF9050);
        SendMessage(sci, SCI_MULTIEDGEADDLINE, 72, 0xFF9050);
        SendMessage(sci, SCI_MULTIEDGEADDLINE, 80, 0x8080FF);
    }
}

/* Default colours (BGR), set in code so the lexer works out of the box even
 * when installed via Plugins Admin with no styling XML present. Colours match
 * the original plugin; the shipped XML lets users override via Style Configurator. */
static void SetStyleColours(HWND sci) {
    SendMessage(sci, SCI_STYLESETFORE,   S_DEFAULT,      0x000000);
    SendMessage(sci, SCI_STYLESETFORE,   S_LABEL,        0x8B0000);
    SendMessage(sci, SCI_STYLESETBOLD,   S_LABEL,        1);
    SendMessage(sci, SCI_STYLESETFORE,   S_OPERATION,    0xFF0000);
    SendMessage(sci, SCI_STYLESETFORE,   S_OPERAND,      0x000000);
    SendMessage(sci, SCI_STYLESETFORE,   S_COMMENT,      0x008000);
    SendMessage(sci, SCI_STYLESETITALIC, S_COMMENT,      1);
    SendMessage(sci, SCI_STYLESETFORE,   S_STRING,       0x0000CC);
    SendMessage(sci, SCI_STYLESETFORE,   S_NUMBER,       0x008CFF);
    SendMessage(sci, SCI_STYLESETFORE,   S_CONTINUATION, 0x808080);
    SendMessage(sci, SCI_STYLESETBACK,   S_CONTINUATION, 0xCCFFFF);
    SendMessage(sci, SCI_STYLESETFORE,   S_SEQUENCE,     0xC0C0C0);
    SendMessage(sci, SCI_STYLESETFORE,   S_REGISTER,     0x800080);
}

/* NPP rejects a plugin whose getFuncsArray returns null or 0 items, so we
 * expose one harmless command. The lexer itself is used via the Language
 * menu, not this command. */
static FuncItem g_funcItems[1];
static void cmdAbout() {
    MessageBoxW(NULL,
        L"HLASM Lexer is loaded.\nChoose Language > HLASM to highlight a file.",
        L"HLASM Lexer", MB_OK | MB_ICONINFORMATION);
}

/* ---- Config XML ----
 * NPP refuses to load any external lexer without plugins\Config\<name>.xml
 * and reports the refusal as "not compatible with the current version",
 * which sends people hunting for a bad build. Plugins Admin unpacks into
 * plugins\<name>\ and never touches Config, so shipping the file in the zip
 * does not help. Write it ourselves instead. setInfo runs before NPP looks,
 * so this lands in time for the same launch. */
#include "hlasm_config_xml.h"

/* Only ever creates a missing file, so hand-edited colours survive. */
static void EnsureConfigXml() {
    wchar_t dir[MAX_PATH] = { 0 };
    SendMessage(g_nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR,
                MAX_PATH, (LPARAM)dir);
    if (!dir[0]) return;

    static const wchar_t leaf[] = L"\\HLASMLexer.xml";
    wchar_t path[MAX_PATH];
    if (wcslen(dir) + wcslen(leaf) + 1 > MAX_PATH) return;
    wcscpy(path, dir);
    wcscat(path, leaf);
    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) return;

    CreateDirectoryW(dir, NULL);   /* usually already there */
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, kConfigXml, (DWORD)(sizeof(kConfigXml) - 1), &written, NULL);
    CloseHandle(h);
}

extern "C" {
BOOL          isUnicode() { return TRUE; }
void          setInfo(NppData nd) { g_nppData = nd; EnsureConfigXml(); }
const TCHAR * getName() { return TEXT("HLASM Lexer"); }
FuncItem *    getFuncsArray(int *n) {
    lstrcpy(g_funcItems[0]._itemName, TEXT("About HLASM Lexer"));
    g_funcItems[0]._pFunc = cmdAbout;
    g_funcItems[0]._cmdID = 0;
    g_funcItems[0]._init2Check = false;
    g_funcItems[0]._pShKey = NULL;
    *n = 1;
    return g_funcItems;
}
void          beNotified(SciNotify *n) {
    if (!n) return;
    unsigned int code = n->nmhdr.code;
    if (code == NPPN_BUFFERACTIVATED || code == NPPN_FILEOPENED) {
        HWND sci = CurrentSci();
        if (!sci) return;
        bool hlasm = CurrentIsHLASM(sci);
        if (hlasm) SetStyleColours(sci);
        ApplyEdges(sci, hlasm);
    }
}
LRESULT       messageProc(UINT, WPARAM, LPARAM) { return TRUE; }
}
