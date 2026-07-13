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
static void HandleKey(long message, short modifiers);
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
    short alphabetWidth = (short)(10 * kAlphabetKeyWidth + 9 * kAlphabetKeyGap);
    short width = (short)(gridWidth + 2 * kContentPad);
    short height;
    short left;
    short top;
    short availTop = (short)(screen.top + kMenuBarHeight + 8);
    short availBottom = screen.bottom;

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

static void DrawAlphabet(WindowRef w)
{
    static const char* rows[3] = {
        "QWERTYUIOP",
        "ASDFGHJKL",
        "ZXCVBNM"
    };
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
        short len = 0;
        short rowWidth;
        short left;
        short top;

        while (rows[row][len] != '\0') {
            ++len;
        }

        rowWidth = (short)(len * kAlphabetKeyWidth + (len - 1) * kAlphabetKeyGap);
        left = (short)(port.left + (port.right - port.left - rowWidth) / 2);
        top = (short)(originY + row * (kAlphabetKeyHeight + kAlphabetRowGap));

        for (i = 0; i < len; ++i) {
            char ch = rows[row][i];
            Rect key;
            Str255 text;
            short textWidth;
            short used = GameIsLetterUsed(&gGame, ch);

            SetRect(&key,
                (short)(left + i * (kAlphabetKeyWidth + kAlphabetKeyGap)),
                top,
                (short)(left + i * (kAlphabetKeyWidth + kAlphabetKeyGap) + kAlphabetKeyWidth),
                (short)(top + kAlphabetKeyHeight));

            PenNormal();
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

            text[0] = 1;
            text[1] = (unsigned char)ch;
            textWidth = StringWidth(text);
            MoveTo((short)(key.left + (key.right - key.left - textWidth) / 2),
                (short)(key.bottom - 4));
            DrawString(text);
            TextMode(srcOr);
        }
    }
}

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

static void HandleKey(long message, short modifiers)
{
    char code = (char)(message & charCodeMask);
    short row;
    short col;
    short newCol;
    short hadMessage;
    SubmitResult submit;

    if (modifiers & cmdKey || gMainWindow == NULL) {
        return;
    }

    hadMessage = (GameGetMessage(&gGame)[0] != '\0');

    if (code == '\r' || code == '\n' || code == 3) {
        row = GameGetCurrentRow(&gGame);
        submit = GameSubmit(&gGame);
        if (submit == kSubmitOk) {
            RedrawRow(gMainWindow, row);
            if (GameGetStatus(&gGame) == kGamePlaying) {
                /* Cursor moved to the next row's first cell. */
                RedrawOneCell(gMainWindow, GameGetCurrentRow(&gGame), 0);
            }
            RedrawAlphabet(gMainWindow);
        }
        RedrawMessageBar(gMainWindow);
        return;
    }

    if (code == 8 || code == 127) {
        row = GameGetCurrentRow(&gGame);
        col = GameGetCurrentCol(&gGame);
        if (!GameBackspace(&gGame)) {
            return;
        }
        /* Cleared cell is the new currentCol; old cursor cell loses thick border. */
        newCol = GameGetCurrentCol(&gGame);
        RedrawOneCell(gMainWindow, row, newCol);
        if (col != newCol && col < kWordLength) {
            RedrawOneCell(gMainWindow, row, col);
        }
        if (hadMessage) {
            RedrawMessageBar(gMainWindow);
        }
        return;
    }

    if (code >= 'a' && code <= 'z') {
        code = (char)(code - 'a' + 'A');
    } else if (code < 'A' || code > 'Z') {
        return;
    }

    row = GameGetCurrentRow(&gGame);
    col = GameGetCurrentCol(&gGame);
    if (!GameTypeLetter(&gGame, code)) {
        return;
    }

    /* Letter landed in `col`; cursor advanced to currentCol. */
    RedrawOneCell(gMainWindow, row, col);
    newCol = GameGetCurrentCol(&gGame);
    if (newCol < kWordLength) {
        RedrawOneCell(gMainWindow, row, newCol);
    }
    if (hadMessage) {
        RedrawMessageBar(gMainWindow);
    }
}

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
