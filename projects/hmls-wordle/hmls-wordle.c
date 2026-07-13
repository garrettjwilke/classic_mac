#include <Quickdraw.h>
#include <Windows.h>
#include <Menus.h>
#include <Fonts.h>
#include <Resources.h>
#include <TextEdit.h>
#include <TextUtils.h>
#include <Dialogs.h>
#include <Devices.h>
#include <OSUtils.h>
#include <ToolUtils.h>
#include <Events.h>

#include "game.h"
#include "letters.h"

/*
 * Uncomment to enable clickable on-screen QWERTY (Enter/Backspace keys + mouse input).
 * Leave commented for keyboard-only play and a narrower window.
 */
//#define CLICKABLE_LETTERS

enum {
    kMenuApple = 128,
    kMenuGame = 129
};

enum {
    kItemAbout = 1
};

enum {
    kItemNewGame = 1,
    kItemQuit = 3
};

enum {
    kMenuBarHeight = 28,
    kTileSize = 36,
    kTileGap = 2,
    kMessageHeight = 16,
    kContentPad = 2,
    kAlphabetTopPad = 8,
    kAlphabetKeyWidth = 18,
    kAlphabetSpecialWidth = 28,
    kAlphabetKeyHeight = 12,
    kAlphabetKeyGap = 2,
    kAlphabetRowGap = 3,
    kAlphabetRows = 3
};

/*
 * Tile fill patterns come from 8x8 PNGs in data/:
 *   missrows.png, wrongplacerows.png, correctrows.png, empty.png
 * Edit those images, then rebuild (tools/generate_patterns.py).
 * Dark pixels -> black in the pattern; light pixels -> white.
 */
extern Pattern gPatMiss;
extern Pattern gPatWrongPlace;
extern Pattern gPatCorrect;
extern Pattern gPatEmpty;
extern short gInvertMiss;
extern short gInvertWrongPlace;
extern short gInvertCorrect;
extern short gInvertEmpty;

static WindowRef gMainWindow;
static GameState gGame;
static short gDone;

static const char* kAlphabetLayout[3] = {
#ifdef CLICKABLE_LETTERS
    "QWERTYUIOP\b",
    "ASDFGHJKL",
    "ZXCVBNM\r"
#else
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM"
#endif
};

static void SizeToContent(WindowRef w);
static void GetGridOrigin(WindowRef w, short* originX, short* originY);
static void GetCellRect(short originX, short originY, short row, short col, Rect* r);
static void DrawCell(const TileCell* cell, const Rect* r, short isCurrent);
static void DrawGrid(WindowRef w);
static void DrawMessageBar(WindowRef w);
static void DrawAlphabet(WindowRef w);
static short AlphabetHeight(void);
static void DoUpdate(WindowRef w);
static void RedrawFullWindow(WindowRef w);
static void RedrawOneCell(WindowRef w, short row, short col);
static void RedrawRow(WindowRef w, short row);
static void RedrawMessageBar(WindowRef w);
static void RedrawAlphabet(WindowRef w);
static void HandleLetter(char code);
static void HandleSubmit(void);
static void HandleBackspace(void);
static void HandleKey(long message, short modifiers);
static short AlphabetKeyWidthFor(char code);
static short AlphabetRowWidth(const char* row);
#ifdef CLICKABLE_LETTERS
static void DrawEnterGlyph(const Rect* key);
static void DrawBackspaceGlyph(const Rect* key);
static char HitTestAlphabet(WindowRef w, Point localPt);
static void HandleContentClick(WindowRef w, Point globalPt);
#endif
static void ShowAboutBox(void);
static void DoMenuCommand(long menuCommand);
static void RequestNewGame(void);
static void RequestQuit(void);
static void CleanupApplication(void);
static void CStringToPascal(const char* src, Str255 dst);

static void RequestQuit(void)
{
    gDone = 1;
}

static void CleanupApplication(void)
{
    if (gMainWindow) {
        DisposeWindow(gMainWindow);
        gMainWindow = NULL;
    }
    FlushEvents(everyEvent, 0);
}

static short AlphabetHeight(void)
{
    return (short)(kAlphabetTopPad
        + kAlphabetRows * kAlphabetKeyHeight
        + (kAlphabetRows - 1) * kAlphabetRowGap);
}

static void SizeToContent(WindowRef w)
{
    Rect screen = qd.screenBits.bounds;
    short gridWidth = (short)(kWordLength * kTileSize + (kWordLength - 1) * kTileGap);
    short gridHeight = (short)(kMaxGuesses * kTileSize + (kMaxGuesses - 1) * kTileGap);
    short alphabetWidth = 0;
    short row;
    short width = (short)(gridWidth + 2 * kContentPad);
    short height;
    short left;
    short top;
    short availTop = (short)(screen.top + kMenuBarHeight + 8);
    short availBottom = screen.bottom;

    for (row = 0; row < kAlphabetRows; ++row) {
        short rowWidth = AlphabetRowWidth(kAlphabetLayout[row]);
        if (rowWidth > alphabetWidth) {
            alphabetWidth = rowWidth;
        }
    }
    if (width < alphabetWidth + 2 * kContentPad) {
        width = (short)(alphabetWidth + 2 * kContentPad);
    }

    height = (short)(kMessageHeight + gridHeight + AlphabetHeight() + 2 * kContentPad);

    left = (short)(screen.left + (screen.right - screen.left - width) / 2);
    top = (short)(availTop + (availBottom - availTop - height) / 3);
    if (top < availTop) {
        top = availTop;
    }

    SizeWindow(w, width, height, true);
    MoveWindow(w, left, top, false);
}

static void GetGridOrigin(WindowRef w, short* originX, short* originY)
{
    Rect port = w->portRect;
    short gridWidth = (short)(kWordLength * kTileSize + (kWordLength - 1) * kTileGap);

    *originX = (short)(port.left + (port.right - port.left - gridWidth) / 2);
    *originY = (short)(port.top + kMessageHeight + kContentPad);
}

static void GetCellRect(short originX, short originY, short row, short col, Rect* r)
{
    short left = (short)(originX + col * (kTileSize + kTileGap));
    short top = (short)(originY + row * (kTileSize + kTileGap));
    SetRect(r, left, top, (short)(left + kTileSize), (short)(top + kTileSize));
}

static void DrawCell(const TileCell* cell, const Rect* r, short isCurrent)
{
    Rect box = *r;

    PenNormal();
    PenSize(isCurrent ? 2 : 1, isCurrent ? 2 : 1);

    switch (cell->state) {
        case kCellCorrect:
            FillRect(&box, &gPatCorrect);
            FrameRect(&box);
            DrawLetterGlyph(cell->letter, &box, gInvertCorrect);
            break;
        case kCellPresent:
            FillRect(&box, &gPatWrongPlace);
            FrameRect(&box);
            DrawLetterGlyph(cell->letter, &box, gInvertWrongPlace);
            break;
        case kCellAbsent:
            FillRect(&box, &gPatMiss);
            FrameRect(&box);
            DrawLetterGlyph(cell->letter, &box, gInvertMiss);
            break;
        case kCellFilled:
            FillRect(&box, &gPatEmpty);
            FrameRect(&box);
            DrawLetterGlyph(cell->letter, &box, gInvertEmpty);
            break;
        case kCellEmpty:
        default:
            FillRect(&box, &gPatEmpty);
            FrameRect(&box);
            break;
    }

    PenNormal();
}

static void DrawGrid(WindowRef w)
{
    short originX;
    short originY;
    short row;
    short col;
    short currentRow = GameGetCurrentRow(&gGame);
    short currentCol = GameGetCurrentCol(&gGame);

    GetGridOrigin(w, &originX, &originY);

    for (row = 0; row < kMaxGuesses; ++row) {
        for (col = 0; col < kWordLength; ++col) {
            Rect cellRect;
            const TileCell* cell = GameGetCell(&gGame, row, col);
            short isCurrent = (GameGetStatus(&gGame) == kGamePlaying
                && row == currentRow
                && col == currentCol);

            GetCellRect(originX, originY, row, col, &cellRect);
            DrawCell(cell, &cellRect, isCurrent);
        }
    }
}

static void CStringToPascal(const char* src, Str255 dst)
{
    short i = 0;
    while (src[i] != '\0' && i < 255) {
        dst[i + 1] = (unsigned char)src[i];
        ++i;
    }
    dst[0] = (unsigned char)i;
}

static void DrawMessageBar(WindowRef w)
{
    Rect bar;
    Str255 text;
    const char* msg = GameGetMessage(&gGame);
    short width;

    SetRect(&bar,
        w->portRect.left,
        w->portRect.top,
        w->portRect.right,
        (short)(w->portRect.top + kMessageHeight));

    PenNormal();
    FillRect(&bar, &qd.white);

    if (msg == NULL || msg[0] == '\0') {
        return;
    }

    CStringToPascal(msg, text);
    TextFont(systemFont);
    TextSize(12);
    TextFace(bold);
    TextMode(srcOr);
    width = StringWidth(text);
    MoveTo((short)(bar.left + (bar.right - bar.left - width) / 2),
        (short)(bar.bottom - 4));
    DrawString(text);
    TextFace(0);
}

static short AlphabetKeyWidthFor(char code)
{
    if (code == '\r' || code == '\b') {
        return (short)kAlphabetSpecialWidth;
    }
    return (short)kAlphabetKeyWidth;
}

static short AlphabetRowWidth(const char* row)
{
    short i;
    short width = 0;
    short count = 0;

    for (i = 0; row[i] != '\0'; ++i) {
        if (count > 0) {
            width = (short)(width + kAlphabetKeyGap);
        }
        width = (short)(width + AlphabetKeyWidthFor(row[i]));
        ++count;
    }
    return width;
}

#ifdef CLICKABLE_LETTERS
static void DrawEnterGlyph(const Rect* key)
{
    short right = (short)(key->right - 6);
    short left = (short)(key->left + 6);
    short top = (short)(key->top + 2);
    short midY = (short)((key->top + key->bottom) / 2 + 1);

    PenNormal();
    MoveTo(right, top);
    LineTo(right, midY);
    LineTo(left, midY);
    MoveTo(left, midY);
    LineTo((short)(left + 3), (short)(midY - 3));
    MoveTo(left, midY);
    LineTo((short)(left + 3), (short)(midY + 3));
}

static void DrawBackspaceGlyph(const Rect* key)
{
    short midY = (short)((key->top + key->bottom) / 2);
    short left = (short)(key->left + 5);
    short right = (short)(key->right - 5);

    PenNormal();
    MoveTo(right, midY);
    LineTo(left, midY);
    MoveTo(left, midY);
    LineTo((short)(left + 4), (short)(midY - 3));
    MoveTo(left, midY);
    LineTo((short)(left + 4), (short)(midY + 3));
}
#endif

static void DrawAlphabet(WindowRef w)
{
    Rect port = w->portRect;
    short gridHeight = (short)(kMaxGuesses * kTileSize + (kMaxGuesses - 1) * kTileGap);
    short originY = (short)(port.top + kMessageHeight + kContentPad + gridHeight + kAlphabetTopPad);
    short row;
    short i;

    TextFont(systemFont);
    TextSize(12);
    TextFace(0);
    TextMode(srcOr);

    for (row = 0; row < kAlphabetRows; ++row) {
        const char* keys = kAlphabetLayout[row];
        short rowWidth = AlphabetRowWidth(keys);
        short left = (short)(port.left + (port.right - port.left - rowWidth) / 2);
        short top = (short)(originY + row * (kAlphabetKeyHeight + kAlphabetRowGap));
        short x = left;

        for (i = 0; keys[i] != '\0'; ++i) {
            char ch = keys[i];
            short keyWidth = AlphabetKeyWidthFor(ch);
            Rect key;
            short used = 0;

            SetRect(&key, x, top, (short)(x + keyWidth), (short)(top + kAlphabetKeyHeight));

            PenNormal();
            if (ch >= 'A' && ch <= 'Z') {
                used = GameIsLetterUsed(&gGame, ch);
            }

            if (used) {
                Rect box = key;
                OffsetRect(&box, 0, -2); /* raise fill only; keep glyph baseline */
                InsetRect(&box, 2, 0); /* 4px narrower */
                box.top -= 1; /* 1px taller */
                FillRect(&box, &qd.black);
                TextMode(srcBic);
            } else {
                FillRect(&key, &qd.white);
                TextMode(srcOr);
            }

#ifdef CLICKABLE_LETTERS
            if (ch == '\r') {
                DrawEnterGlyph(&key);
            } else if (ch == '\b') {
                DrawBackspaceGlyph(&key);
            } else
#endif
            {
                Str255 text;
                short textWidth;

                text[0] = 1;
                text[1] = (unsigned char)ch;
                textWidth = StringWidth(text);
                MoveTo((short)(key.left + (key.right - key.left - textWidth) / 2),
                    (short)(key.bottom - 4));
                DrawString(text);
            }
            TextMode(srcOr);

            x = (short)(x + keyWidth + kAlphabetKeyGap);
        }
    }
}

#ifdef CLICKABLE_LETTERS
static char HitTestAlphabet(WindowRef w, Point localPt)
{
    Rect port = w->portRect;
    short gridHeight = (short)(kMaxGuesses * kTileSize + (kMaxGuesses - 1) * kTileGap);
    short originY = (short)(port.top + kMessageHeight + kContentPad + gridHeight + kAlphabetTopPad);
    short row;
    short i;

    for (row = 0; row < kAlphabetRows; ++row) {
        const char* keys = kAlphabetLayout[row];
        short rowWidth = AlphabetRowWidth(keys);
        short left = (short)(port.left + (port.right - port.left - rowWidth) / 2);
        short top = (short)(originY + row * (kAlphabetKeyHeight + kAlphabetRowGap));
        short x = left;

        for (i = 0; keys[i] != '\0'; ++i) {
            char ch = keys[i];
            short keyWidth = AlphabetKeyWidthFor(ch);
            Rect key;

            SetRect(&key, x, top, (short)(x + keyWidth), (short)(top + kAlphabetKeyHeight));
            if (PtInRect(localPt, &key)) {
                return ch;
            }
            x = (short)(x + keyWidth + kAlphabetKeyGap);
        }
    }

    return 0;
}
#endif

static void DoUpdate(WindowRef w)
{
    BeginUpdate(w);
    SetPort(w);
    EraseRect(&w->portRect);
    DrawMessageBar(w);
    DrawGrid(w);
    DrawAlphabet(w);
    EndUpdate(w);
}

static void RedrawFullWindow(WindowRef w)
{
    SetPort(w);
    InvalRect(&w->portRect);
}

static void RedrawOneCell(WindowRef w, short row, short col)
{
    short originX;
    short originY;
    Rect cellRect;
    Rect clean;
    short currentRow;
    short currentCol;
    const TileCell* cell;
    short isCurrent;

    if (row < 0 || row >= kMaxGuesses || col < 0 || col >= kWordLength) {
        return;
    }

    SetPort(w);
    GetGridOrigin(w, &originX, &originY);
    GetCellRect(originX, originY, row, col, &cellRect);

    /* Clear a 1px halo so thick (current) borders do not leave residue. */
    clean = cellRect;
    InsetRect(&clean, -1, -1);
    EraseRect(&clean);

    currentRow = GameGetCurrentRow(&gGame);
    currentCol = GameGetCurrentCol(&gGame);
    cell = GameGetCell(&gGame, row, col);
    isCurrent = (GameGetStatus(&gGame) == kGamePlaying
        && row == currentRow
        && col == currentCol);
    DrawCell(cell, &cellRect, isCurrent);
}

static void RedrawRow(WindowRef w, short row)
{
    short col;

    for (col = 0; col < kWordLength; ++col) {
        RedrawOneCell(w, row, col);
    }
}

static void RedrawMessageBar(WindowRef w)
{
    SetPort(w);
    DrawMessageBar(w);
}

static void RedrawAlphabet(WindowRef w)
{
    Rect port = w->portRect;
    short gridHeight = (short)(kMaxGuesses * kTileSize + (kMaxGuesses - 1) * kTileGap);
    Rect area;

    SetPort(w);
    SetRect(&area,
        port.left,
        (short)(port.top + kMessageHeight + kContentPad + gridHeight),
        port.right,
        port.bottom);
    EraseRect(&area);
    DrawAlphabet(w);
}

static void RequestNewGame(void)
{
    GameNew(&gGame);
    if (gMainWindow) {
        RedrawFullWindow(gMainWindow);
    }
}

static void HandleLetter(char code)
{
    short row;
    short col;
    short newCol;
    short hadMessage;

    if (gMainWindow == NULL) {
        return;
    }
    if (code >= 'a' && code <= 'z') {
        code = (char)(code - 'a' + 'A');
    }
    if (code < 'A' || code > 'Z') {
        return;
    }

    hadMessage = (GameGetMessage(&gGame)[0] != '\0');
    row = GameGetCurrentRow(&gGame);
    col = GameGetCurrentCol(&gGame);
    if (!GameTypeLetter(&gGame, code)) {
        return;
    }

    RedrawOneCell(gMainWindow, row, col);
    newCol = GameGetCurrentCol(&gGame);
    if (newCol < kWordLength) {
        RedrawOneCell(gMainWindow, row, newCol);
    }
    if (hadMessage) {
        RedrawMessageBar(gMainWindow);
    }
}

static void HandleSubmit(void)
{
    short row;
    SubmitResult submit;

    if (gMainWindow == NULL) {
        return;
    }

    row = GameGetCurrentRow(&gGame);
    submit = GameSubmit(&gGame);
    if (submit == kSubmitOk) {
        RedrawRow(gMainWindow, row);
        if (GameGetStatus(&gGame) == kGamePlaying) {
            RedrawOneCell(gMainWindow, GameGetCurrentRow(&gGame), 0);
        }
        RedrawAlphabet(gMainWindow);
    }
    RedrawMessageBar(gMainWindow);
}

static void HandleBackspace(void)
{
    short row;
    short col;
    short newCol;
    short hadMessage;

    if (gMainWindow == NULL) {
        return;
    }

    hadMessage = (GameGetMessage(&gGame)[0] != '\0');
    row = GameGetCurrentRow(&gGame);
    col = GameGetCurrentCol(&gGame);
    if (!GameBackspace(&gGame)) {
        return;
    }
    newCol = GameGetCurrentCol(&gGame);
    RedrawOneCell(gMainWindow, row, newCol);
    if (col != newCol && col < kWordLength) {
        RedrawOneCell(gMainWindow, row, col);
    }
    if (hadMessage) {
        RedrawMessageBar(gMainWindow);
    }
}

static void HandleKey(long message, short modifiers)
{
    char code = (char)(message & charCodeMask);

    if (modifiers & cmdKey || gMainWindow == NULL) {
        return;
    }

    if (code == '\r' || code == '\n' || code == 3) {
        HandleSubmit();
        return;
    }

    if (code == 8 || code == 127) {
        HandleBackspace();
        return;
    }

    HandleLetter(code);
}

#ifdef CLICKABLE_LETTERS
static void HandleContentClick(WindowRef w, Point globalPt)
{
    Point localPt = globalPt;
    char ch;

    if (w != gMainWindow) {
        return;
    }

    SetPort(w);
    GlobalToLocal(&localPt);
    ch = HitTestAlphabet(w, localPt);
    if (ch == '\r') {
        HandleSubmit();
    } else if (ch == '\b') {
        HandleBackspace();
    } else if (ch != 0) {
        HandleLetter(ch);
    }
}
#endif

static void ShowAboutBox(void)
{
    WindowRef w = GetNewWindow(128, NULL, (WindowPtr)-1);
    Handle h;

    if (w == NULL) {
        return;
    }

    SizeWindow(w, 280, 180, true);
    MoveWindow(w,
        (short)(qd.screenBits.bounds.right / 2 - 140),
        (short)(qd.screenBits.bounds.bottom / 2 - 90),
        false);
    ShowWindow(w);
    SetPort(w);
    EraseRect(&w->portRect);

    h = GetResource('TEXT', 128);
    if (h) {
        HLock(h);
        {
            Rect r = w->portRect;
            InsetRect(&r, 10, 10);
            TETextBox(*h, GetHandleSize(h), &r, teJustLeft);
        }
        HUnlock(h);
        ReleaseResource(h);
    }

    while (!Button()) {
        ;
    }
    while (Button()) {
        ;
    }
    FlushEvents(everyEvent, 0);
    DisposeWindow(w);
    if (gMainWindow) {
        SetPort(gMainWindow);
        RedrawFullWindow(gMainWindow);
    }
}

static void DoMenuCommand(long menuCommand)
{
    short menuID = HiWord(menuCommand);
    short menuItem = LoWord(menuCommand);
    Str255 str;

    if (menuID == kMenuApple) {
        if (menuItem == kItemAbout) {
            ShowAboutBox();
        } else {
            GetMenuItemText(GetMenu(kMenuApple), menuItem, str);
            OpenDeskAcc(str);
        }
    } else if (menuID == kMenuGame) {
        switch (menuItem) {
            case kItemNewGame:
                RequestNewGame();
                break;
            case kItemQuit:
                RequestQuit();
                break;
        }
    }

    HiliteMenu(0);
}

static WindowRef NewMainWindow(void)
{
    WindowRef w = GetNewWindow(128, NULL, (WindowPtr)-1);
    SizeToContent(w);
    SetPort(w);
    return w;
}

int main(void)
{
    EventRecord e;
    WindowRef win;

    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);

    SetMenuBar(GetNewMBar(128));
    AppendResMenu(GetMenu(kMenuApple), 'DRVR');
    DrawMenuBar();
    InitCursor();

    GameInit(&gGame);
    gMainWindow = NewMainWindow();
    ShowWindow(gMainWindow);
    SelectWindow(gMainWindow);

    for (;;) {
        if (gDone) {
            break;
        }

        SystemTask();

        if (GetNextEvent(everyEvent, &e)) {
            switch (e.what) {
                case keyDown:
                case autoKey:
                    if (e.modifiers & cmdKey) {
                        DoMenuCommand(MenuKey(e.message & charCodeMask));
                    } else {
                        HandleKey(e.message, e.modifiers);
                    }
                    break;
                case mouseDown:
                    switch (FindWindow(e.where, &win)) {
                        case inMenuBar:
                            DoMenuCommand(MenuSelect(e.where));
                            break;
                        case inGoAway:
                            if (TrackGoAway(win, e.where)) {
                                RequestQuit();
                            }
                            break;
                        case inDrag:
                            DragWindow(win, e.where, &qd.screenBits.bounds);
                            break;
                        case inContent:
                            if (win != FrontWindow()) {
                                SelectWindow(win);
#ifdef CLICKABLE_LETTERS
                            } else {
                                HandleContentClick(win, e.where);
#endif
                            }
                            break;
                        case inSysWindow:
                            SystemClick(&e, win);
                            break;
                    }
                    break;
                case updateEvt:
                    DoUpdate((WindowRef)e.message);
                    break;
                case nullEvent:
                    break;
            }
        }
    }

    CleanupApplication();
    ExitToShell();
    return 0;
}
