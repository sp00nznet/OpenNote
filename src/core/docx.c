// .docx -- ECMA-376 WordprocessingML, read and written through Windows' own
// packaging and XML APIs.
//
// A .docx is an Open Packaging Conventions container. Windows ships an API for
// that exact shape (msopc.dll, IOpcFactory) and a pull XML reader (xmllite.dll,
// IXmlReader), both documented and both already on every machine this runs on.
// Neither the zip container nor the XML parser is code this project owns.
//
// Reading converts WordprocessingML to RTF and hands it to the rich text view
// that already exists. That is deliberate -- see docx.h.

#define COBJMACROS

#include "supernote.h"
#include "core/docx.h"
#include "ui/editor_rich.h"

#include <msopc.h>
#include <xmllite.h>
#include <objbase.h>

#pragma comment(lib, "xmllite.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

// Relationship type identifying the package's main document part.
#define REL_OFFICE_DOCUMENT \
    L"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument"

#define CT_MAIN_DOCUMENT \
    L"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"

static WCHAR g_lastError[512] = {0};

static void SetError(const WCHAR* msg) {
    wcsncpy_s(g_lastError, 512, msg ? msg : L"", _TRUNCATE);
}

const WCHAR* Docx_GetLastError(void) {
    return g_lastError[0] ? g_lastError : L"No error.";
}

BOOL Docx_IsDocxPath(const WCHAR* path) {
    if (!path) return FALSE;
    const WCHAR* ext = wcsrchr(path, L'.');
    return ext && _wcsicmp(ext, L".docx") == 0;
}

// ---------------------------------------------------------------------------
// A growable byte string, used to build RTF and XML
// ---------------------------------------------------------------------------

typedef struct {
    char*  buf;
    size_t len;
    size_t cap;
    BOOL   failed;
} Str;

static void StrFree(Str* s) {
    free(s->buf);
    s->buf = NULL;
    s->len = s->cap = 0;
}

static BOOL StrReserve(Str* s, size_t extra) {
    if (s->failed) return FALSE;
    if (s->len + extra + 1 <= s->cap) return TRUE;

    size_t want = s->cap ? s->cap * 2 : 8192;
    while (want < s->len + extra + 1) want *= 2;

    char* grown = (char*)realloc(s->buf, want);
    if (!grown) {
        s->failed = TRUE;
        return FALSE;
    }
    s->buf = grown;
    s->cap = want;
    return TRUE;
}

static void StrAdd(Str* s, const char* text) {
    if (!text) return;
    size_t n = strlen(text);
    if (!StrReserve(s, n)) return;
    memcpy(s->buf + s->len, text, n);
    s->len += n;
    s->buf[s->len] = '\0';
}

static void StrAddF(Str* s, const char* fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n > 0) StrAdd(s, tmp);
}

// Text into RTF. Braces and backslashes are control characters; anything above
// ASCII goes out as \uN with a '?' fallback for readers that do not do Unicode.
static void StrAddRtfText(Str* s, const WCHAR* text, int len) {
    if (!text) return;
    if (len < 0) len = (int)wcslen(text);

    for (int i = 0; i < len; i++) {
        WCHAR c = text[i];
        switch (c) {
            case L'\\': StrAdd(s, "\\\\"); break;
            case L'{':  StrAdd(s, "\\{");  break;
            case L'}':  StrAdd(s, "\\}");  break;
            case L'\t': StrAdd(s, "\\tab "); break;
            case L'\r': case L'\n': break;   // paragraph breaks are structural
            default:
                if (c >= 0x20 && c < 0x80) {
                    if (!StrReserve(s, 1)) return;
                    s->buf[s->len++] = (char)c;
                    s->buf[s->len] = '\0';
                } else if (c >= 0x80) {
                    // RTF wants a signed 16-bit value here.
                    StrAddF(s, "\\u%d?", (int)(short)c);
                }
                break;
        }
    }
}

// XML text escaping, for the writer.
static void StrAddXmlText(Str* s, const WCHAR* text, int len) {
    if (!text) return;
    if (len < 0) len = (int)wcslen(text);

    // UTF-8 is what the part declares, so convert then escape.
    int u8len = WideCharToMultiByte(CP_UTF8, 0, text, len, NULL, 0, NULL, NULL);
    if (u8len <= 0) return;

    char* u8 = (char*)malloc((size_t)u8len + 1);
    if (!u8) return;
    WideCharToMultiByte(CP_UTF8, 0, text, len, u8, u8len, NULL, NULL);
    u8[u8len] = '\0';

    for (int i = 0; i < u8len; i++) {
        char c = u8[i];
        switch (c) {
            case '&':  StrAdd(s, "&amp;");  break;
            case '<':  StrAdd(s, "&lt;");   break;
            case '>':  StrAdd(s, "&gt;");   break;
            case '"':  StrAdd(s, "&quot;"); break;
            case '\'': StrAdd(s, "&apos;"); break;
            default:
                if ((unsigned char)c < 0x20 && c != '\t') break;  // illegal in XML 1.0
                if (!StrReserve(s, 1)) { free(u8); return; }
                s->buf[s->len++] = c;
                s->buf[s->len] = '\0';
                break;
        }
    }
    free(u8);
}

// ---------------------------------------------------------------------------
// Font and colour tables
// ---------------------------------------------------------------------------

#define MAX_FONTS  64
#define MAX_COLORS 64

typedef struct {
    WCHAR    fonts[MAX_FONTS][LF_FACESIZE];
    int      fontCount;
    COLORREF colors[MAX_COLORS];
    int      colorCount;
} Tables;

static int TableFont(Tables* t, const WCHAR* name) {
    if (!name || !name[0]) return 0;
    for (int i = 0; i < t->fontCount; i++) {
        if (_wcsicmp(t->fonts[i], name) == 0) return i;
    }
    if (t->fontCount >= MAX_FONTS) return 0;
    wcsncpy_s(t->fonts[t->fontCount], LF_FACESIZE, name, _TRUNCATE);
    return t->fontCount++;
}

// Colour 0 in an RTF colour table is "default", so real colours start at 1.
static int TableColor(Tables* t, COLORREF c) {
    for (int i = 0; i < t->colorCount; i++) {
        if (t->colors[i] == c) return i + 1;
    }
    if (t->colorCount >= MAX_COLORS) return 0;
    t->colors[t->colorCount] = c;
    return ++t->colorCount;
}

// ---------------------------------------------------------------------------
// Formatting carried while walking the document
// ---------------------------------------------------------------------------

typedef struct {
    BOOL bold, italic, underline, strike;
    BOOL superscript, subscript;
    int  halfPoints;     // w:sz is in half-points; 0 means unspecified
    int  colorIndex;     // into the RTF colour table; 0 = default
    int  fontIndex;
    BOOL hasFont;
} RunFmt;

typedef struct {
    int  align;          // 0 left, 1 centre, 2 right, 3 justify
    int  indentTwips;
    int  spaceBeforeTw;
    int  spaceAfterTw;
    BOOL isList;
    BOOL bullet;
    int  listIndent;
    BOOL inTable;
} ParaFmt;

// ---------------------------------------------------------------------------
// XmlLite helpers
// ---------------------------------------------------------------------------

static BOOL NameIs(const WCHAR* local, UINT len, const WCHAR* want) {
    size_t wl = wcslen(want);
    return len == wl && wcsncmp(local, want, wl) == 0;
}

// Read one attribute of the current element by local name. Returns FALSE when
// the attribute is absent.
static BOOL GetAttr(IXmlReader* r, const WCHAR* name, WCHAR* out, size_t outChars) {
    out[0] = L'\0';

    HRESULT hr = IXmlReader_MoveToFirstAttribute(r);
    while (hr == S_OK) {
        const WCHAR* local = NULL;
        UINT len = 0;
        if (SUCCEEDED(IXmlReader_GetLocalName(r, &local, &len)) &&
            NameIs(local, len, name)) {
            const WCHAR* val = NULL;
            UINT vlen = 0;
            if (SUCCEEDED(IXmlReader_GetValue(r, &val, &vlen))) {
                size_t copy = vlen < outChars - 1 ? vlen : outChars - 1;
                wcsncpy_s(out, outChars, val, copy);
                IXmlReader_MoveToElement(r);
                return TRUE;
            }
        }
        hr = IXmlReader_MoveToNextAttribute(r);
    }

    IXmlReader_MoveToElement(r);
    return FALSE;
}

// WordprocessingML writes booleans as w:val="0"/"false" to turn a property off;
// an absent w:val means on.
static BOOL AttrIsOn(IXmlReader* r) {
    WCHAR val[32];
    if (!GetAttr(r, L"val", val, 32)) return TRUE;
    return !(wcscmp(val, L"0") == 0 || _wcsicmp(val, L"false") == 0 ||
             _wcsicmp(val, L"off") == 0);
}

static int AttrInt(IXmlReader* r, const WCHAR* name, int fallback) {
    WCHAR val[32];
    if (!GetAttr(r, name, val, 32)) return fallback;
    return _wtoi(val);
}

static COLORREF ParseHexColor(const WCHAR* hex, BOOL* ok) {
    *ok = FALSE;
    if (!hex || wcslen(hex) < 6) return 0;
    if (_wcsicmp(hex, L"auto") == 0) return 0;

    unsigned int v = 0;
    for (int i = 0; i < 6; i++) {
        WCHAR c = hex[i];
        unsigned d;
        if (c >= L'0' && c <= L'9') d = (unsigned)(c - L'0');
        else if (c >= L'a' && c <= L'f') d = (unsigned)(c - L'a' + 10);
        else if (c >= L'A' && c <= L'F') d = (unsigned)(c - L'A' + 10);
        else return 0;
        v = (v << 4) | d;
    }
    *ok = TRUE;
    // OOXML writes RRGGBB; COLORREF is 0x00BBGGRR.
    return RGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

// ---------------------------------------------------------------------------
// Opening the package and finding the main document part
// ---------------------------------------------------------------------------

static IStream* OpenMainDocumentPart(const WCHAR* path, IOpcPackage** packageOut) {
    *packageOut = NULL;

    IOpcFactory* factory = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_OpcFactory, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IOpcFactory, (void**)&factory);
    if (FAILED(hr)) {
        SetError(L"The Windows packaging component could not be created.");
        return NULL;
    }

    IStream* fileStream = NULL;
    hr = IOpcFactory_CreateStreamOnFile(factory, path, OPC_STREAM_IO_READ,
                                        NULL, 0, &fileStream);
    if (FAILED(hr)) {
        SetError(L"The file could not be opened.");
        IOpcFactory_Release(factory);
        return NULL;
    }

    IOpcPackage* package = NULL;
    hr = IOpcFactory_ReadPackageFromStream(factory, fileStream,
                                           OPC_CACHE_ON_ACCESS, &package);
    IStream_Release(fileStream);

    if (FAILED(hr)) {
        SetError(L"This is not a valid Office package. A .doc renamed to .docx "
                 L"is a different format -- that one arrives in a later version.");
        IOpcFactory_Release(factory);
        return NULL;
    }

    IOpcPartSet* parts = NULL;
    if (FAILED(IOpcPackage_GetPartSet(package, &parts))) {
        SetError(L"The package has no parts.");
        IOpcPackage_Release(package);
        IOpcFactory_Release(factory);
        return NULL;
    }

    IOpcPartUri* docUri = NULL;

    // The main document is whatever the package-level relationship of type
    // officeDocument points at. It is /word/document.xml in practice, but
    // editing history can move it, so the relationship is the authority.
    IOpcRelationshipSet* rels = NULL;
    if (SUCCEEDED(IOpcPackage_GetRelationshipSet(package, &rels))) {
        IOpcRelationshipEnumerator* en = NULL;
        if (SUCCEEDED(IOpcRelationshipSet_GetEnumeratorForType(
                rels, REL_OFFICE_DOCUMENT, &en))) {
            BOOL has = FALSE;
            if (SUCCEEDED(IOpcRelationshipEnumerator_MoveNext(en, &has)) && has) {
                IOpcRelationship* rel = NULL;
                if (SUCCEEDED(IOpcRelationshipEnumerator_GetCurrent(en, &rel))) {
                    IUri* target = NULL;
                    if (SUCCEEDED(IOpcRelationship_GetTargetUri(rel, &target))) {
                        IOpcUri* root = NULL;
                        if (SUCCEEDED(IOpcFactory_CreatePackageRootUri(factory, &root))) {
                            IOpcUri_CombinePartUri(root, target, &docUri);
                            IOpcUri_Release(root);
                        }
                        IUri_Release(target);
                    }
                    IOpcRelationship_Release(rel);
                }
            }
            IOpcRelationshipEnumerator_Release(en);
        }
        IOpcRelationshipSet_Release(rels);
    }

    // Fall back to the conventional location when the relationship is missing
    // or unreadable, rather than refusing a file every other reader opens.
    if (!docUri) {
        IOpcFactory_CreatePartUri(factory, L"/word/document.xml", &docUri);
    }

    IStream* content = NULL;
    if (docUri) {
        IOpcPart* part = NULL;
        if (SUCCEEDED(IOpcPartSet_GetPart(parts, docUri, &part))) {
            IOpcPart_GetContentStream(part, &content);
            IOpcPart_Release(part);
        }
        IOpcPartUri_Release(docUri);
    }

    IOpcPartSet_Release(parts);
    IOpcFactory_Release(factory);

    if (!content) {
        SetError(L"The package contains no main document part.");
        IOpcPackage_Release(package);
        return NULL;
    }

    *packageOut = package;
    return content;
}

// ---------------------------------------------------------------------------
// WordprocessingML -> RTF
// ---------------------------------------------------------------------------

static void EmitRunProps(Str* body, const RunFmt* f) {
    StrAdd(body, "\\plain");
    if (f->bold)        StrAdd(body, "\\b");
    if (f->italic)      StrAdd(body, "\\i");
    if (f->underline)   StrAdd(body, "\\ul");
    if (f->strike)      StrAdd(body, "\\strike");
    if (f->superscript) StrAdd(body, "\\super");
    if (f->subscript)   StrAdd(body, "\\sub");
    if (f->hasFont)     StrAddF(body, "\\f%d", f->fontIndex);
    if (f->halfPoints)  StrAddF(body, "\\fs%d", f->halfPoints);
    if (f->colorIndex)  StrAddF(body, "\\cf%d", f->colorIndex);
    StrAdd(body, " ");
}

static void EmitParaProps(Str* body, const ParaFmt* p) {
    StrAdd(body, "\\pard");
    if (p->inTable) StrAdd(body, "\\intbl");

    switch (p->align) {
        case 1: StrAdd(body, "\\qc"); break;
        case 2: StrAdd(body, "\\qr"); break;
        case 3: StrAdd(body, "\\qj"); break;
        default: StrAdd(body, "\\ql"); break;
    }

    if (p->isList) {
        // A hanging indent, so the marker sits left of the text rather than on
        // top of it. 360 twips is a quarter inch, which is what Word uses.
        int li = p->listIndent > 0 ? p->listIndent : 720;
        StrAddF(body, "\\fi-360\\li%d", li);
        if (p->bullet) {
            StrAdd(body, "{\\pntext\\f0 \\'B7\\tab}"
                         "{\\*\\pn\\pnlvlblt\\pnf0\\pnindent0{\\pntxtb\\'B7}}");
        } else {
            StrAdd(body, "{\\pntext\\f0 1.\\tab}"
                         "{\\*\\pn\\pnlvlbody\\pnf0\\pnindent0\\pnstart1\\pndec{\\pntxta.}}");
        }
    } else if (p->indentTwips > 0) {
        StrAddF(body, "\\li%d", p->indentTwips);
    }

    if (p->spaceBeforeTw > 0) StrAddF(body, "\\sb%d", p->spaceBeforeTw);
    if (p->spaceAfterTw > 0)  StrAddF(body, "\\sa%d", p->spaceAfterTw);
}

#define MAX_GRID_COLS 32

typedef struct {
    Str     body;
    Tables  tables;
    RunFmt  run;
    // Formatting a paragraph style implies for every run inside it. Runs reset
    // to this rather than to nothing, or a heading would lose its weight the
    // moment its first run opened.
    RunFmt  paraDefaultRun;
    ParaFmt para;
    BOOL    inRunProps;
    BOOL    inParaProps;
    BOOL    inPreserveText;
    BOOL    paraOpen;
    int     tableDepth;
    int     cellsThisRow;
    BOOL    rowOpen;
    BOOL    sawDeleted;      // inside w:del -- text that is not in the document
    int     skipDepth;       // >0 while inside content to ignore entirely

    // Column edges for the table being read, taken from w:tblGrid so that
    // \cellx can be emitted before the row's content as RTF expects.
    int     gridEdges[MAX_GRID_COLS];
    int     gridCount;
    int     gridAccum;
    BOOL    inTblGrid;

    // A paragraph inside a cell is terminated by \cell, not \par. The mark is
    // therefore held back and only emitted if another paragraph follows in the
    // same cell.
    BOOL    pendingCellPara;
    BOOL    inCell;
} Walk;

static void StartParagraph(Walk* w) {
    if (w->paraOpen) return;
    EmitParaProps(&w->body, &w->para);
    w->paraOpen = TRUE;
}

static void EndParagraph(Walk* w) {
    if (!w->paraOpen) {
        // An empty w:p is a blank line and still needs its mark.
        EmitParaProps(&w->body, &w->para);
    }

    if (w->inCell) {
        // Held back: \cell ends the cell's final paragraph. If another
        // paragraph follows in this cell, the mark is emitted then.
        w->pendingCellPara = TRUE;
    } else {
        StrAdd(&w->body, "\\par\n");
    }
    w->paraOpen = FALSE;
}

static BOOL ConvertDocument(IStream* stream, Str* out) {
    IXmlReader* reader = NULL;
    if (FAILED(CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL))) {
        SetError(L"The XML reader could not be created.");
        return FALSE;
    }
    if (FAILED(IXmlReader_SetInput(reader, (IUnknown*)stream))) {
        SetError(L"The main document part could not be read.");
        IXmlReader_Release(reader);
        return FALSE;
    }

    // XmlLite stops at 256 levels by default; deeply nested tables exceed it.
    IXmlReader_SetProperty(reader, XmlReaderProperty_MaxElementDepth, 0);

    Walk w = {0};
    w.para.align = 0;

    // Font 0 is the document default; everything else is added as encountered.
    TableFont(&w.tables, L"Calibri");

    XmlNodeType nt;
    while (S_OK == IXmlReader_Read(reader, &nt)) {
        const WCHAR* local = NULL;
        UINT len = 0;

        if (nt == XmlNodeType_Element || nt == XmlNodeType_EndElement) {
            if (FAILED(IXmlReader_GetLocalName(reader, &local, &len))) continue;
        }

        if (nt == XmlNodeType_Element) {
            BOOL empty = IXmlReader_IsEmptyElement(reader);

            // Content inside a deletion is revision history, not the document.
            if (NameIs(local, len, L"del")) {
                if (!empty) w.skipDepth++;
                continue;
            }
            if (w.skipDepth > 0) continue;

            if (NameIs(local, len, L"p")) {
                memset(&w.para, 0, sizeof(w.para));
                memset(&w.run, 0, sizeof(w.run));
                memset(&w.paraDefaultRun, 0, sizeof(w.paraDefaultRun));
                w.para.inTable = (w.tableDepth > 0);
                w.paraOpen = FALSE;

                // A held-back cell paragraph mark means this is the second or
                // later paragraph in the cell, so the previous one ends here.
                if (w.pendingCellPara) {
                    StrAdd(&w.body, "\\par ");
                    w.pendingCellPara = FALSE;
                }
            } else if (NameIs(local, len, L"pPr")) {
                w.inParaProps = TRUE;
            } else if (NameIs(local, len, L"rPr")) {
                w.inRunProps = TRUE;
            } else if (NameIs(local, len, L"r")) {
                w.run = w.paraDefaultRun;
            }

            // --- paragraph properties ---
            else if (w.inParaProps && NameIs(local, len, L"jc")) {
                WCHAR val[32];
                if (GetAttr(reader, L"val", val, 32)) {
                    if (_wcsicmp(val, L"center") == 0)       w.para.align = 1;
                    else if (_wcsicmp(val, L"right") == 0)   w.para.align = 2;
                    else if (_wcsicmp(val, L"both") == 0 ||
                             _wcsicmp(val, L"justify") == 0) w.para.align = 3;
                    else                                     w.para.align = 0;
                }
            } else if (w.inParaProps && NameIs(local, len, L"ind")) {
                int left = AttrInt(reader, L"left", -1);
                if (left < 0) left = AttrInt(reader, L"start", -1);
                if (left > 0) w.para.indentTwips = left;
            } else if (w.inParaProps && NameIs(local, len, L"spacing")) {
                int before = AttrInt(reader, L"before", 0);
                int after  = AttrInt(reader, L"after", 0);
                if (before > 0) w.para.spaceBeforeTw = before;
                if (after > 0)  w.para.spaceAfterTw = after;
            } else if (w.inParaProps && NameIs(local, len, L"numPr")) {
                w.para.isList = TRUE;
                // Which marker a list uses lives in numbering.xml behind two
                // levels of indirection. Bulleted is overwhelmingly the common
                // case, so that is the default until numbering.xml is read.
                w.para.bullet = TRUE;
            } else if (w.inParaProps && NameIs(local, len, L"ilvl")) {
                int lvl = AttrInt(reader, L"val", 0);
                w.para.listIndent = 720 + lvl * 360;
            } else if (w.inParaProps && NameIs(local, len, L"pStyle")) {
                WCHAR val[64];
                if (GetAttr(reader, L"val", val, 64)) {
                    // Heading styles carry their weight in styles.xml. Rather
                    // than resolving the style graph, give headings the shape
                    // readers expect: bold and larger, scaled by level.
                    if (_wcsnicmp(val, L"Heading", 7) == 0) {
                        int level = _wtoi(val + 7);
                        if (level < 1) level = 1;
                        if (level > 6) level = 6;
                        w.paraDefaultRun.bold = TRUE;
                        w.paraDefaultRun.halfPoints = 36 - (level - 1) * 4;
                        if (w.paraDefaultRun.halfPoints < 22) w.paraDefaultRun.halfPoints = 22;
                        w.run = w.paraDefaultRun;
                        w.para.spaceBeforeTw = 240;
                        w.para.spaceAfterTw = 120;
                    }
                }
            }

            // --- run properties ---
            else if (w.inRunProps && NameIs(local, len, L"b")) {
                w.run.bold = AttrIsOn(reader);
            } else if (w.inRunProps && NameIs(local, len, L"i")) {
                w.run.italic = AttrIsOn(reader);
            } else if (w.inRunProps && NameIs(local, len, L"u")) {
                WCHAR val[32];
                w.run.underline = !GetAttr(reader, L"val", val, 32) ||
                                  _wcsicmp(val, L"none") != 0;
            } else if (w.inRunProps && NameIs(local, len, L"strike")) {
                w.run.strike = AttrIsOn(reader);
            } else if (w.inRunProps && NameIs(local, len, L"sz")) {
                int sz = AttrInt(reader, L"val", 0);
                if (sz > 0) w.run.halfPoints = sz;
            } else if (w.inRunProps && NameIs(local, len, L"color")) {
                WCHAR val[32];
                if (GetAttr(reader, L"val", val, 32)) {
                    BOOL ok = FALSE;
                    COLORREF c = ParseHexColor(val, &ok);
                    if (ok) w.run.colorIndex = TableColor(&w.tables, c);
                }
            } else if (w.inRunProps && NameIs(local, len, L"vertAlign")) {
                WCHAR val[32];
                if (GetAttr(reader, L"val", val, 32)) {
                    w.run.superscript = (_wcsicmp(val, L"superscript") == 0);
                    w.run.subscript   = (_wcsicmp(val, L"subscript") == 0);
                }
            } else if (w.inRunProps && NameIs(local, len, L"rFonts")) {
                WCHAR val[LF_FACESIZE];
                if (GetAttr(reader, L"ascii", val, LF_FACESIZE) && val[0]) {
                    w.run.fontIndex = TableFont(&w.tables, val);
                    w.run.hasFont = TRUE;
                }
            }

            // --- content ---
            else if (NameIs(local, len, L"t")) {
                StartParagraph(&w);
                EmitRunProps(&w.body, &w.run);
                w.inPreserveText = TRUE;
            } else if (NameIs(local, len, L"br")) {
                StartParagraph(&w);
                StrAdd(&w.body, "\\line ");
            } else if (NameIs(local, len, L"tab")) {
                StartParagraph(&w);
                StrAdd(&w.body, "\\tab ");
            } else if (NameIs(local, len, L"tbl")) {
                w.tableDepth++;
                w.gridCount = 0;
                w.gridAccum = 0;
            } else if (NameIs(local, len, L"tblGrid")) {
                w.inTblGrid = TRUE;
                w.gridCount = 0;
                w.gridAccum = 0;
            } else if (w.inTblGrid && NameIs(local, len, L"gridCol")) {
                int cw = AttrInt(reader, L"w", 0);
                if (cw <= 0) cw = 2000;
                if (w.gridCount < MAX_GRID_COLS) {
                    w.gridAccum += cw;
                    w.gridEdges[w.gridCount++] = w.gridAccum;
                }
            } else if (NameIs(local, len, L"tr")) {
                w.cellsThisRow = 0;
                w.rowOpen = TRUE;

                // RTF wants the row definition before the row's content. The
                // grid read above supplies it; without one, fall back to equal
                // columns once the cell count is known at \row.
                StrAdd(&w.body, "\\trowd\\trgaph108");
                for (int i = 0; i < w.gridCount; i++) {
                    StrAddF(&w.body, "\\cellx%d", w.gridEdges[i]);
                }
                StrAdd(&w.body, "\n");
            } else if (NameIs(local, len, L"tc")) {
                w.cellsThisRow++;
                w.inCell = TRUE;
                w.pendingCellPara = FALSE;
            }

        } else if (nt == XmlNodeType_Text || nt == XmlNodeType_Whitespace) {
            if (w.skipDepth > 0 || !w.inPreserveText) continue;

            const WCHAR* val = NULL;
            UINT vlen = 0;
            if (SUCCEEDED(IXmlReader_GetValue(reader, &val, &vlen))) {
                StrAddRtfText(&w.body, val, (int)vlen);
            }

        } else if (nt == XmlNodeType_EndElement) {
            if (NameIs(local, len, L"del")) {
                if (w.skipDepth > 0) w.skipDepth--;
                continue;
            }
            if (w.skipDepth > 0) continue;

            if (NameIs(local, len, L"t"))          w.inPreserveText = FALSE;
            else if (NameIs(local, len, L"pPr"))   w.inParaProps = FALSE;
            else if (NameIs(local, len, L"rPr"))   w.inRunProps = FALSE;
            else if (NameIs(local, len, L"tblGrid")) w.inTblGrid = FALSE;
            else if (NameIs(local, len, L"p"))     EndParagraph(&w);
            else if (NameIs(local, len, L"tc")) {
                // \cell terminates the cell's last paragraph; a \par before it
                // would leave an empty line in every cell.
                w.pendingCellPara = FALSE;
                w.inCell = FALSE;
                StrAdd(&w.body, "\\cell ");
            }
            else if (NameIs(local, len, L"tr")) {
                // Only needed when the table had no w:tblGrid to read.
                if (w.gridCount == 0 && w.cellsThisRow > 0) {
                    StrAdd(&w.body, "\\trowd\\trgaph108");
                    for (int i = 1; i <= w.cellsThisRow; i++) {
                        StrAddF(&w.body, "\\cellx%d", (9000 * i) / w.cellsThisRow);
                    }
                }
                StrAdd(&w.body, "\\row\n");
                w.rowOpen = FALSE;
            }
            else if (NameIs(local, len, L"tbl")) {
                if (w.tableDepth > 0) w.tableDepth--;
                w.gridCount = 0;
                StrAdd(&w.body, "\\pard\n");
            }
        }
    }

    IXmlReader_Release(reader);

    if (w.body.failed) {
        StrFree(&w.body);
        SetError(L"Ran out of memory building the document.");
        return FALSE;
    }

    // Assemble: header, font table, colour table, then the body.
    StrAdd(out, "{\\rtf1\\ansi\\ansicpg1252\\deff0{\\fonttbl");
    for (int i = 0; i < w.tables.fontCount; i++) {
        StrAddF(out, "{\\f%d\\fnil ", i);
        char name[LF_FACESIZE * 2];
        WideCharToMultiByte(CP_UTF8, 0, w.tables.fonts[i], -1,
                            name, sizeof(name), NULL, NULL);
        StrAdd(out, name);
        StrAdd(out, ";}");
    }
    StrAdd(out, "}\n");

    StrAdd(out, "{\\colortbl;");
    for (int i = 0; i < w.tables.colorCount; i++) {
        StrAddF(out, "\\red%d\\green%d\\blue%d;",
                GetRValue(w.tables.colors[i]),
                GetGValue(w.tables.colors[i]),
                GetBValue(w.tables.colors[i]));
    }
    StrAdd(out, "}\n\\viewkind4\\uc1\n");

    if (w.body.buf) StrAdd(out, w.body.buf);
    StrAdd(out, "}\n");

    StrFree(&w.body);
    return !out->failed;
}

char* Docx_ReadToRtf(const WCHAR* path) {
    if (!path) return NULL;
    SetError(NULL);

    // The packaging API is COM. Initialising per call keeps this independent of
    // whatever the calling thread has already done.
    HRESULT init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    BOOL needUninit = SUCCEEDED(init);

    IOpcPackage* package = NULL;
    IStream* docStream = OpenMainDocumentPart(path, &package);
    if (!docStream) {
        if (needUninit) CoUninitialize();
        return NULL;
    }

    Str rtf = {0};
    BOOL ok = ConvertDocument(docStream, &rtf);

    IStream_Release(docStream);
    IOpcPackage_Release(package);
    if (needUninit) CoUninitialize();

    if (!ok) {
        StrFree(&rtf);
        return NULL;
    }
    return rtf.buf;
}

// ---------------------------------------------------------------------------
// Rich text view -> WordprocessingML
//
// The document's structure is read back out of the control: paragraphs are
// separated by a single carriage return in its raw text, and within a
// paragraph a run is a maximal span whose character formatting does not
// change. Nothing here parses RTF -- the control is the model.
// ---------------------------------------------------------------------------

// Raw text, meaning paragraph marks stay as a single \r so that offsets match
// the ones the control itself uses. GT_DEFAULT would expand them to \r\n and
// every offset after the first paragraph would be wrong.
static WCHAR* GetRawText(HWND h, int* lenOut) {
    GETTEXTLENGTHEX gtl = { GTL_NUMCHARS | GTL_PRECISE, 1200 };
    int len = (int)SendMessageW(h, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    if (len < 0) len = 0;

    WCHAR* buf = (WCHAR*)malloc(((size_t)len + 2) * sizeof(WCHAR));
    if (!buf) return NULL;

    GETTEXTEX gt = {
        .cb = (DWORD)(((size_t)len + 1) * sizeof(WCHAR)),
        .flags = GT_RAWTEXT,
        .codepage = 1200,
        .lpDefaultChar = NULL,
        .lpUsedDefChar = NULL
    };
    int got = (int)SendMessageW(h, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)buf);
    if (got < 0) got = 0;
    buf[got] = L'\0';

    if (lenOut) *lenOut = got;
    return buf;
}

// RichEdit marks table structure with characters in the U+FFF9..U+FFFC range
// and separates cells with BEL. They are structure, not content, and writing
// them into a .docx puts unreadable characters in the document -- which is
// exactly what happened before this existed.
//
// ponytail: table structure is not rebuilt on write in this version. A table
// flattens to its text, cells separated by tabs and rows by paragraphs, which
// is what Word's own "convert table to text" does. Losing the grid is a
// documented limitation; losing the words would be a bug. Emitting real
// <w:tbl> needs the table model the DirectWrite engine brings in v0.7.
typedef enum {
    CHAR_NORMAL,
    CHAR_DROP,        // structure with no textual meaning
    CHAR_CELL_BREAK,  // becomes a tab
    CHAR_ROW_BREAK    // becomes a paragraph break
} CharKind;

static CharKind ClassifyChar(WCHAR c) {
    switch (c) {
        case 0x0007: return CHAR_CELL_BREAK;   // BEL, cell separator
        case 0xFFF9: return CHAR_DROP;         // start of a table row
        case 0xFFFA: return CHAR_DROP;
        case 0xFFFB: return CHAR_ROW_BREAK;    // end of a table row
        case 0xFFFC: return CHAR_DROP;         // embedded object placeholder
        default: break;
    }
    // Any other C0 control that is not a tab or a paragraph mark is not text.
    if (c < 0x20 && c != L'\t' && c != L'\r' && c != L'\n') return CHAR_DROP;
    return CHAR_NORMAL;
}

static void GetFormatAt(HWND h, int pos, CHARFORMAT2W* cf) {
    CHARRANGE cr = { pos, pos + 1 };
    SendMessageW(h, EM_EXSETSEL, 0, (LPARAM)&cr);

    memset(cf, 0, sizeof(*cf));
    cf->cbSize = sizeof(*cf);
    SendMessageW(h, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)cf);
}

static BOOL SameFormat(const CHARFORMAT2W* a, const CHARFORMAT2W* b) {
    DWORD mask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_STRIKEOUT |
                 CFM_SUBSCRIPT | CFM_SUPERSCRIPT;
    if ((a->dwEffects & mask) != (b->dwEffects & mask)) return FALSE;
    if (a->yHeight != b->yHeight) return FALSE;
    if (a->crTextColor != b->crTextColor) return FALSE;
    if ((a->dwEffects & CFE_AUTOCOLOR) != (b->dwEffects & CFE_AUTOCOLOR)) return FALSE;
    return wcscmp(a->szFaceName, b->szFaceName) == 0;
}

static void EmitRunPropsXml(Str* x, const CHARFORMAT2W* cf) {
    BOOL any = (cf->dwEffects & (CFE_BOLD | CFE_ITALIC | CFE_UNDERLINE |
                                 CFE_STRIKEOUT | CFE_SUBSCRIPT | CFE_SUPERSCRIPT)) ||
               cf->yHeight || cf->szFaceName[0] ||
               !(cf->dwEffects & CFE_AUTOCOLOR);
    if (!any) return;

    StrAdd(x, "<w:rPr>");

    if (cf->szFaceName[0]) {
        StrAdd(x, "<w:rFonts w:ascii=\"");
        StrAddXmlText(x, cf->szFaceName, -1);
        StrAdd(x, "\" w:hAnsi=\"");
        StrAddXmlText(x, cf->szFaceName, -1);
        StrAdd(x, "\"/>");
    }
    if (cf->dwEffects & CFE_BOLD)      StrAdd(x, "<w:b/>");
    if (cf->dwEffects & CFE_ITALIC)    StrAdd(x, "<w:i/>");
    if (cf->dwEffects & CFE_STRIKEOUT) StrAdd(x, "<w:strike/>");
    if (cf->dwEffects & CFE_UNDERLINE) StrAdd(x, "<w:u w:val=\"single\"/>");

    if (!(cf->dwEffects & CFE_AUTOCOLOR)) {
        StrAddF(x, "<w:color w:val=\"%02X%02X%02X\"/>",
                GetRValue(cf->crTextColor),
                GetGValue(cf->crTextColor),
                GetBValue(cf->crTextColor));
    }
    if (cf->yHeight > 0) {
        // yHeight is twips, w:sz is half-points: 20 twips per point.
        int halfPoints = cf->yHeight / 10;
        if (halfPoints < 2) halfPoints = 2;
        StrAddF(x, "<w:sz w:val=\"%d\"/><w:szCs w:val=\"%d\"/>", halfPoints, halfPoints);
    }
    if (cf->dwEffects & CFE_SUPERSCRIPT) StrAdd(x, "<w:vertAlign w:val=\"superscript\"/>");
    if (cf->dwEffects & CFE_SUBSCRIPT)   StrAdd(x, "<w:vertAlign w:val=\"subscript\"/>");

    StrAdd(x, "</w:rPr>");
}

static void EmitParaPropsXml(Str* x, const PARAFORMAT2* pf) {
    Str inner = {0};

    if (pf->dwMask & PFM_ALIGNMENT) {
        switch (pf->wAlignment) {
            case PFA_CENTER:  StrAdd(&inner, "<w:jc w:val=\"center\"/>"); break;
            case PFA_RIGHT:   StrAdd(&inner, "<w:jc w:val=\"right\"/>");  break;
            case PFA_JUSTIFY: StrAdd(&inner, "<w:jc w:val=\"both\"/>");   break;
            default: break;
        }
    }

    // A numbered or bulleted paragraph needs a numbering definition to point
    // at. Rather than emit a numbering part, the marker is kept as an indent so
    // the text still lands where the author put it; Word shows it as an
    // indented paragraph rather than losing it.
    LONG indent = (pf->dwMask & PFM_STARTINDENT) ? pf->dxStartIndent : 0;
    if ((pf->dwMask & PFM_NUMBERING) && pf->wNumbering) {
        if (indent < 720) indent = 720;
    }
    if (indent > 0) StrAddF(&inner, "<w:ind w:left=\"%ld\"/>", indent);

    if (inner.len) {
        StrAdd(x, "<w:pPr>");
        StrAdd(x, inner.buf);
        StrAdd(x, "</w:pPr>");
    }
    StrFree(&inner);
}

static BOOL BuildDocumentXml(HWND h, Str* x) {
    int textLen = 0;
    WCHAR* text = GetRawText(h, &textLen);
    if (!text) {
        SetError(L"The document text could not be read.");
        return FALSE;
    }

    // Walking the document moves the selection about; put it back afterwards
    // and keep the control from repainting while it happens.
    CHARRANGE saved = {0};
    SendMessageW(h, EM_EXGETSEL, 0, (LPARAM)&saved);
    SendMessageW(h, WM_SETREDRAW, FALSE, 0);

    StrAdd(x, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
              "<w:document xmlns:w=\"http://schemas.openxmlformats.org/"
              "wordprocessingml/2006/main\"><w:body>");

    int pos = 0;
    while (pos <= textLen) {
        int paraEnd = pos;
        while (paraEnd < textLen && text[paraEnd] != L'\r' && text[paraEnd] != L'\n') {
            paraEnd++;
        }

        StrAdd(x, "<w:p>");

        CHARRANGE pr = { pos, pos };
        SendMessageW(h, EM_EXSETSEL, 0, (LPARAM)&pr);

        PARAFORMAT2 pf = {0};
        pf.cbSize = sizeof(pf);
        SendMessageW(h, EM_GETPARAFORMAT, 0, (LPARAM)&pf);
        EmitParaPropsXml(x, &pf);

        // Split the paragraph into runs of unchanging character formatting.
        int runStart = pos;
        while (runStart < paraEnd) {
            CHARFORMAT2W base;
            GetFormatAt(h, runStart, &base);

            int runEnd = runStart + 1;
            while (runEnd < paraEnd) {
                CHARFORMAT2W next;
                GetFormatAt(h, runEnd, &next);
                if (!SameFormat(&base, &next)) break;
                runEnd++;
            }

            StrAdd(x, "<w:r>");
            EmitRunPropsXml(x, &base);
            StrAdd(x, "<w:t xml:space=\"preserve\">");

            // Tabs are their own element in WordprocessingML, so a run has to
            // be broken around them rather than carrying them as text. The
            // same goes for the structural characters classified above.
            int seg = runStart;
            for (int i = runStart; i < runEnd; i++) {
                CharKind kind = ClassifyChar(text[i]);
                if (text[i] != L'\t' && kind == CHAR_NORMAL) continue;

                if (i > seg) StrAddXmlText(x, text + seg, i - seg);
                seg = i + 1;

                if (text[i] == L'\t' || kind == CHAR_CELL_BREAK) {
                    StrAdd(x, "</w:t><w:tab/><w:t xml:space=\"preserve\">");
                } else if (kind == CHAR_ROW_BREAK) {
                    StrAdd(x, "</w:t><w:br/><w:t xml:space=\"preserve\">");
                }
                // CHAR_DROP contributes nothing.
            }
            if (runEnd > seg) StrAddXmlText(x, text + seg, runEnd - seg);

            StrAdd(x, "</w:t></w:r>");
            runStart = runEnd;
        }

        StrAdd(x, "</w:p>");

        if (paraEnd >= textLen) break;
        // Step past the paragraph mark, allowing for a CRLF pair.
        pos = paraEnd + 1;
        if (pos < textLen && text[paraEnd] == L'\r' && text[pos] == L'\n') pos++;
    }

    // A section is required for Word to consider the document well formed.
    // Letter paper, one inch margins.
    StrAdd(x, "<w:sectPr><w:pgSz w:w=\"12240\" w:h=\"15840\"/>"
              "<w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" "
              "w:left=\"1440\" w:header=\"720\" w:footer=\"720\" w:gutter=\"0\"/>"
              "</w:sectPr></w:body></w:document>");

    SendMessageW(h, EM_EXSETSEL, 0, (LPARAM)&saved);
    SendMessageW(h, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(h, NULL, TRUE);

    free(text);

    if (x->failed) {
        SetError(L"Ran out of memory building the document.");
        return FALSE;
    }
    return TRUE;
}

BOOL Docx_WriteFromEditor(HWND hRichEdit, const WCHAR* path) {
    if (!hRichEdit || !path) return FALSE;
    SetError(NULL);

    Str xml = {0};
    if (!BuildDocumentXml(hRichEdit, &xml)) {
        StrFree(&xml);
        return FALSE;
    }

    HRESULT init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    BOOL needUninit = SUCCEEDED(init);

    IOpcFactory* factory = NULL;
    IOpcPackage* package = NULL;
    IOpcPartSet* parts = NULL;
    IOpcPartUri* docUri = NULL;
    IOpcPart* part = NULL;
    IStream* content = NULL;
    IStream* file = NULL;
    IOpcRelationshipSet* rels = NULL;
    IOpcRelationship* rel = NULL;
    BOOL ok = FALSE;

    if (FAILED(CoCreateInstance(&CLSID_OpcFactory, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IOpcFactory, (void**)&factory))) {
        SetError(L"The Windows packaging component could not be created.");
        goto done;
    }

    if (FAILED(IOpcFactory_CreatePackage(factory, &package)) ||
        FAILED(IOpcPackage_GetPartSet(package, &parts)) ||
        FAILED(IOpcFactory_CreatePartUri(factory, L"/word/document.xml", &docUri))) {
        SetError(L"The package could not be created.");
        goto done;
    }

    if (FAILED(IOpcPartSet_CreatePart(parts, docUri, CT_MAIN_DOCUMENT,
                                      OPC_COMPRESSION_NORMAL, &part)) ||
        FAILED(IOpcPart_GetContentStream(part, &content))) {
        SetError(L"The main document part could not be created.");
        goto done;
    }

    // Through the vtable rather than the COBJMACROS name: shlwapi.h declares an
    // IStream_Write helper of its own with a different signature, and it wins.
    ULONG written = 0;
    if (FAILED(content->lpVtbl->Write(content, xml.buf, (ULONG)xml.len, &written)) ||
        written != (ULONG)xml.len) {
        SetError(L"The document content could not be written.");
        goto done;
    }

    // Without this relationship the package is a zip of XML that no reader
    // knows how to start from.
    if (FAILED(IOpcPackage_GetRelationshipSet(package, &rels)) ||
        FAILED(IOpcRelationshipSet_CreateRelationship(
            rels, NULL, REL_OFFICE_DOCUMENT, (IUri*)docUri,
            OPC_URI_TARGET_MODE_INTERNAL, &rel))) {
        SetError(L"The package relationship could not be created.");
        goto done;
    }

    if (FAILED(IOpcFactory_CreateStreamOnFile(factory, path, OPC_STREAM_IO_WRITE,
                                              NULL, 0, &file))) {
        SetError(L"The file could not be created for writing.");
        goto done;
    }

    if (FAILED(IOpcFactory_WritePackageToStream(factory, package,
                                                OPC_WRITE_DEFAULT, file))) {
        SetError(L"The package could not be written.");
        goto done;
    }

    ok = TRUE;

done:
    if (rel)     IOpcRelationship_Release(rel);
    if (rels)    IOpcRelationshipSet_Release(rels);
    if (file)    IStream_Release(file);
    if (content) IStream_Release(content);
    if (part)    IOpcPart_Release(part);
    if (docUri)  IOpcPartUri_Release(docUri);
    if (parts)   IOpcPartSet_Release(parts);
    if (package) IOpcPackage_Release(package);
    if (factory) IOpcFactory_Release(factory);
    if (needUninit) CoUninitialize();

    StrFree(&xml);

    if (ok) SendMessageW(hRichEdit, EM_SETMODIFY, FALSE, 0);
    return ok;
}

// ---------------------------------------------------------------------------
// Self-check. Run with: OpenNote.exe --selftest
//
// The reader is covered against externally produced documents by
// --docx-check; what is checked here is the writer, and the only honest way to
// check a writer is to read back what it wrote.
// ---------------------------------------------------------------------------

BOOL Docx_SelfTest(char* failure, size_t failureSize) {
#define FAIL(msg) do { \
        strncpy_s(failure, failureSize, (msg), _TRUNCATE); \
        if (h) DestroyWindow(h); \
        if (tmpFile[0]) DeleteFileW(tmpFile); \
        free(rtf); \
        return FALSE; \
    } while (0)

    HWND   h = NULL;
    char*  rtf = NULL;
    WCHAR  tmpFile[MAX_PATH] = {0};
    WCHAR  tmpDir[MAX_PATH];

    if (GetTempPathW(MAX_PATH, tmpDir) == 0 ||
        GetTempFileNameW(tmpDir, L"onx", 0, tmpFile) == 0) {
        strncpy_s(failure, failureSize, "could not make a temporary file", _TRUNCATE);
        return FALSE;
    }

    // GetTempFileName makes a .tmp; the writer does not care, but keeping the
    // extension honest keeps the file recognisable if a run leaves one behind.
    WCHAR docxPath[MAX_PATH];
    swprintf_s(docxPath, MAX_PATH, L"%s.docx", tmpFile);
    DeleteFileW(tmpFile);
    wcscpy_s(tmpFile, MAX_PATH, docxPath);

    // Not inherited from whichever check ran before this one.
    Rich_EnsureLoaded();

    h = CreateWindowExW(0, MSFTEDIT_CLASS, NULL,
                        WS_POPUP | ES_MULTILINE | ES_NOHIDESEL,
                        0, 0, 100, 100, HWND_MESSAGE, NULL,
                        GetModuleHandleW(NULL), NULL);
    if (!h) {
        strncpy_s(failure, failureSize, "could not create a RichEdit control", _TRUNCATE);
        DeleteFileW(tmpFile);
        return FALSE;
    }
    SendMessageW(h, EM_SETTEXTMODE, TM_RICHTEXT | TM_MULTILEVELUNDO, 0);
    SendMessageW(h, EM_EXLIMITTEXT, 0, 0x7FFFFFFF);

    // Three paragraphs: a bold+coloured run beside a plain one, a centred
    // paragraph, and text with characters that have to survive XML escaping.
    Rich_SetText(h, L"Alpha bravo\rCentred line\rAmpersand & angle < and \x2014 dash");

    Rich_SetSelection(h, 0, 5);
    Rich_ToggleEffect(h, CFE_BOLD);
    Rich_SetTextColor(h, RGB(200, 30, 30));
    Rich_SetFontSize(h, 18);

    Rich_SetSelection(h, 12, 24);
    Rich_SetAlignment(h, PFA_CENTER);

    if (!Docx_WriteFromEditor(h, tmpFile)) FAIL("Docx_WriteFromEditor failed");

    DWORD attrs = GetFileAttributesW(tmpFile);
    if (attrs == INVALID_FILE_ATTRIBUTES) FAIL("the written .docx does not exist");

    // Read it back through the same path a user opening the file would take.
    rtf = Docx_ReadToRtf(tmpFile);
    if (!rtf) FAIL("the written .docx could not be read back");

    if (strncmp(rtf, "{\\rtf", 5) != 0) FAIL("reading back did not produce RTF");

    if (!strstr(rtf, "Alpha")) FAIL("text did not survive the .docx round trip");
    if (!strstr(rtf, "bravo")) FAIL("later text in a paragraph was lost");
    if (!strstr(rtf, "Centred line")) FAIL("a later paragraph was lost");

    // Formatting must come back, not just the characters.
    if (!strstr(rtf, "\\b"))  FAIL("bold did not survive the .docx round trip");
    if (!strstr(rtf, "\\cf")) FAIL("colour did not survive the .docx round trip");
    if (!strstr(rtf, "\\qc")) FAIL("centred alignment did not survive the round trip");

    // XML-significant characters must have been escaped on the way out and
    // decoded on the way back, arriving as themselves.
    if (!strstr(rtf, "Ampersand & angle < and")) {
        FAIL("XML escaping did not round trip");
    }
    // The em dash is non-ASCII and must come back as an RTF unicode escape.
    if (!strstr(rtf, "\\u8212?")) FAIL("a non-ASCII character was lost");

    // Writing must leave the document unmodified, or saving would immediately
    // mark it dirty again.
    if (Rich_GetModified(h)) FAIL("writing a .docx left the document modified");

    free(rtf);
    rtf = NULL;

    // An empty document must still produce a package a reader accepts, rather
    // than a zero-paragraph body that fails to open.
    Rich_SetText(h, L"");
    if (!Docx_WriteFromEditor(h, tmpFile)) FAIL("writing an empty document failed");
    rtf = Docx_ReadToRtf(tmpFile);
    if (!rtf) FAIL("an empty .docx could not be read back");

    free(rtf);
    rtf = NULL;

    DeleteFileW(tmpFile);
    DestroyWindow(h);

    failure[0] = '\0';
    return TRUE;

#undef FAIL
}
