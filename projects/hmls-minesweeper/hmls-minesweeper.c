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

#include "game.h"
#include "tiles.h"

enum {
    kMenuApple = 128,
    kMenuGame = 129
};

enum {
    kItemAbout = 1
};

enum {
    kItemBeginner = 1,
    kItemIntermediate,
    kItemExpert,
    kItemNewGame = 5,
    kItemQuit = 7
};

enum {
    kMenuBarHeight = 20,
    kStatusBarHeight = 16,
    kStatusPadding = 4,
    kStatusItemGap = 8,
    kCounterTileGap = 2,
    kCounterWidth = 39,
    kHelpButtonWidth = 48
};

enum {
    kDifficultyDialog = 129,
    kDialogItemBeginner = 2,
    kDialogItemIntermediate = 3,
    kDialogItemExpert = 4,
    kDialogItemQuit = 5
};

enum {
    kHelpDialog = 130,
    kDialogItemHelpClose = 2
};

enum {
    kDiscardDialog = 131,
    kDialogItemDiscardYes = 2,
    kDialogItemDiscardNo = 3
};

static WindowRef gMainWindow;
static GameState gGame;
static short gMouseDownInContent;
static short gFacePressed;
static short gHelpDialogOpen;
static short gMainWindowVisible;
static short gModalTimerLastSeconds;
static short gDone;

static void FillScreenWindow(WindowRef w);
static void GetBoardLayout(WindowRef w, Rect* boardRect, Rect* statusRect, Rect* faceRect);
static void GetStatusLayout(WindowRef w, Rect* mineTile, Rect* mineCounter, Rect* timerTile, Rect* timerCounter, Rect* faceRect, Rect* helpRect);
static void DrawStatusBar(WindowRef w);
static void DrawBoard(WindowRef w);
static void DoUpdate(WindowRef w);
static void DrawCounter(short value, const Rect* area);
static void RedrawTimer(WindowRef w);
static void RedrawMineCounter(WindowRef w);
static void RedrawFace(WindowRef w);
static void RedrawCell(WindowRef w, short x, short y);
static void RedrawChangedCells(WindowRef w);
static void RedrawFullWindow(WindowRef w);
static void ClearBoardArea(WindowRef w);
static void DoContentClick(WindowRef w, Point localPt, short markKey);
static void ShowAboutBox(void);
static void ShowHelpDialog(WindowRef w);
static short ShowDiscardConfirmDialog(WindowRef w);
static short ConfirmDiscardIfNeeded(WindowRef w);
static void ServiceActiveGameTimer(WindowRef w, short* lastSeconds);
static void HandleModalDialogUpdate(WindowRef mainWin, WindowPtr dlgWin, DialogPtr dlg, WindowPtr updateWin);
static void DoMenuCommand(long menuCommand);
static void RequestNewGame(GameDifficulty difficulty);
static void RequestNewGameFromMenu(void);
static void StartNewGame(GameDifficulty difficulty);
static short ShowDifficultyDialog(GameDifficulty* difficulty);
static short TileForCell(const GameState* game, short x, short y);
static short FaceTileForGame(const GameState* game, short pressed);
static void RequestQuit(void);
static void CleanupApplication(void);

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
    GameDispose(&gGame);
    FlushEvents(everyEvent, 0);
}

static void InvalidateWindow(WindowRef w)
{
    SetPort(w);
    InvalRect(&w->portRect);
}

static void FillScreenWindow(WindowRef w)
{
    Rect screen = qd.screenBits.bounds;
    Rect bounds;

    bounds.left = screen.left;
    bounds.top = screen.top + kMenuBarHeight;
    bounds.right = screen.right;
    bounds.bottom = screen.bottom;

    MoveWindow(w, bounds.left, bounds.top, false);
    SizeWindow(w, bounds.right - bounds.left, bounds.bottom - bounds.top, true);
}

static void GetBoardLayout(WindowRef w, Rect* boardRect, Rect* statusRect, Rect* faceRect)
{
    Rect port = w->portRect;
    short boardWidth;
    short boardHeight;
    short contentTop;
    short contentHeight;
    short originX;
    short originY;

    SetRect(statusRect, port.left, port.top, port.right, port.top + kStatusBarHeight);

    boardWidth = (short)(GameGetWidth(&gGame) * kTileSize);
    boardHeight = (short)(GameGetHeight(&gGame) * kTileSize);
    contentTop = (short)(port.top + kStatusBarHeight);
    contentHeight = (short)(port.bottom - contentTop);

    originX = (short)(port.left + (port.right - port.left - boardWidth) / 2);
    originY = (short)(contentTop + (contentHeight - boardHeight) / 2);
    if (originY < contentTop) {
        originY = contentTop;
    }

    SetRect(boardRect,
        originX,
        originY,
        (short)(originX + boardWidth),
        (short)(originY + boardHeight));

    SetRect(faceRect,
        (short)(originX + boardWidth / 2 - kTileSize / 2),
        (short)(port.top + (kStatusBarHeight - kTileSize) / 2),
        (short)(originX + boardWidth / 2 + kTileSize / 2),
        (short)(port.top + (kStatusBarHeight - kTileSize) / 2 + kTileSize));
}

static void GetStatusLayout(WindowRef w, Rect* mineTile, Rect* mineCounter, Rect* timerTile, Rect* timerCounter, Rect* faceRect, Rect* helpRect)
{
    Rect boardRect;
    Rect statusRect;
    short barTop = w->portRect.top;

    GetBoardLayout(w, &boardRect, &statusRect, faceRect);

    SetRect(mineTile,
        (short)(w->portRect.left + kStatusPadding),
        (short)(barTop + (kStatusBarHeight - kTileSize) / 2),
        (short)(w->portRect.left + kStatusPadding + kTileSize),
        (short)(barTop + (kStatusBarHeight - kTileSize) / 2 + kTileSize));

    SetRect(mineCounter,
        (short)(mineTile->right + kCounterTileGap),
        (short)(barTop + 1),
        (short)(mineTile->right + kCounterTileGap + kCounterWidth),
        (short)(barTop + kStatusBarHeight - 1));

    SetRect(timerTile,
        (short)(mineCounter->right + kStatusItemGap),
        mineTile->top,
        (short)(mineCounter->right + kStatusItemGap + kTileSize),
        mineTile->bottom);

    SetRect(timerCounter,
        (short)(timerTile->right + kCounterTileGap),
        mineCounter->top,
        (short)(timerTile->right + kCounterTileGap + kCounterWidth),
        mineCounter->bottom);

    SetRect(helpRect,
        (short)(w->portRect.right - kStatusPadding - kHelpButtonWidth),
        mineCounter->top,
        (short)(w->portRect.right - kStatusPadding),
        mineCounter->bottom);
}

static void DrawStatusCounterGroup(const Rect* tileRect, const Rect* counterRect, short tileID, short value)
{
    Rect group;

    SetRect(&group,
        tileRect->left,
        counterRect->top,
        counterRect->right,
        counterRect->bottom);
    PenNormal();
    FillRect(&group, &qd.ltGray);
    DrawTile(tileID, tileRect);
    DrawCounter(value, counterRect);
}

static void FormatCounterText(short value, Str255 text)
{
    if (value < 0) {
        value = 0;
    }
    if (value > 999) {
        value = 999;
    }

    text[0] = 3;
    text[1] = (unsigned char)('0' + (value / 100) % 10);
    text[2] = (unsigned char)('0' + (value / 10) % 10);
    text[3] = (unsigned char)('0' + value % 10);
}

static void DrawCounterValue(short value, const Rect* area, short drawFrame)
{
    Str255 text;
    Rect box = *area;

    FormatCounterText(value, text);

    PenNormal();
    if (drawFrame) {
        FillRect(&box, &qd.white);
        FrameRect(&box);
        InsetRect(&box, 2, 1);
    } else {
        /* Leave the existing border; only refresh the digits. */
        InsetRect(&box, 1, 1);
        FillRect(&box, &qd.white);
        InsetRect(&box, 1, 0);
    }
    TextFont(1);
    TextSize(12);
    TextFace(bold);
    MoveTo((short)(box.left + 4), (short)(box.bottom - 3));
    DrawString(text);
    TextFace(0);
}

static void DrawCounter(short value, const Rect* area)
{
    DrawCounterValue(value, area, 1);
}

static void DrawHelpButton(const Rect* area)
{
    Rect box = *area;

    PenNormal();
    FillRect(&box, &qd.white);
    FrameRect(&box);
    InsetRect(&box, 2, 1);
    TextFont(1);
    TextSize(12);
    TextFace(bold);
    MoveTo((short)(box.left + 6), (short)(box.bottom - 3));
    DrawString("\pHelp");
    TextFace(0);
}

static void RedrawTimer(WindowRef w)
{
    Rect mineTile;
    Rect mineCounter;
    Rect timerTile;
    Rect timerCounter;
    Rect faceRect;
    Rect helpRect;

    SetPort(w);
    GetStatusLayout(w, &mineTile, &mineCounter, &timerTile, &timerCounter, &faceRect, &helpRect);
    DrawCounterValue(GameGetElapsedSeconds(&gGame), &timerCounter, 0);
}

static void RedrawMineCounter(WindowRef w)
{
    Rect mineTile;
    Rect mineCounter;
    Rect timerTile;
    Rect timerCounter;
    Rect faceRect;
    Rect helpRect;

    SetPort(w);
    GetStatusLayout(w, &mineTile, &mineCounter, &timerTile, &timerCounter, &faceRect, &helpRect);
    DrawCounterValue(GameGetRemainingMines(&gGame), &mineCounter, 0);
}

static void RedrawFace(WindowRef w)
{
    Rect mineTile;
    Rect mineCounter;
    Rect timerTile;
    Rect timerCounter;
    Rect faceRect;
    Rect helpRect;

    SetPort(w);
    GetStatusLayout(w, &mineTile, &mineCounter, &timerTile, &timerCounter, &faceRect, &helpRect);
    DrawTile(FaceTileForGame(&gGame, gFacePressed), &faceRect);
}

static void CellRect(WindowRef w, short x, short y, Rect* tileRect)
{
    Rect boardRect;
    Rect statusRect;
    Rect faceRect;

    GetBoardLayout(w, &boardRect, &statusRect, &faceRect);
    SetRect(tileRect,
        (short)(boardRect.left + x * kTileSize),
        (short)(boardRect.top + y * kTileSize),
        (short)(boardRect.left + (x + 1) * kTileSize),
        (short)(boardRect.top + (y + 1) * kTileSize));
}

static void RedrawCell(WindowRef w, short x, short y)
{
    Rect tileRect;

    SetPort(w);
    CellRect(w, x, y, &tileRect);
    PenNormal();
    FillRect(&tileRect, &qd.white);
    DrawTile(TileForCell(&gGame, x, y), &tileRect);
}

static void RedrawChangedCells(WindowRef w)
{
    short i;
    short x;
    short y;

    for (i = 0; i < GameGetChangedCount(&gGame); ++i) {
        GameGetChangedCell(&gGame, i, &x, &y);
        RedrawCell(w, x, y);
    }
}

static short FaceTileForGame(const GameState* game, short pressed)
{
    if (pressed) {
        return kTileFaceSurprised;
    }

    switch (GameGetStatus(game)) {
        case kGameWon:
            return kTileFaceCool;
        case kGameLost:
            return kTileFaceDead;
        case kGamePlaying:
        default:
            return kTileFaceNormal;
    }
}

static void DrawStatusBar(WindowRef w)
{
    Rect statusRect;
    Rect boardRect;
    Rect faceRect;
    Rect mineTile;
    Rect mineCounter;
    Rect timerTile;
    Rect timerCounter;
    Rect helpRect;

    GetBoardLayout(w, &boardRect, &statusRect, &faceRect);
    GetStatusLayout(w, &mineTile, &mineCounter, &timerTile, &timerCounter, &faceRect, &helpRect);

    PenNormal();
    FillRect(&statusRect, &qd.ltGray);
    FrameRect(&statusRect);

    DrawStatusCounterGroup(&mineTile, &mineCounter, kTileFlag, GameGetRemainingMines(&gGame));
    DrawStatusCounterGroup(&timerTile, &timerCounter, kTileTimer, GameGetElapsedSeconds(&gGame));
    DrawTile(FaceTileForGame(&gGame, gFacePressed), &faceRect);
    DrawHelpButton(&helpRect);
}

static short TileForCell(const GameState* game, short x, short y)
{
    if (GameIsFlagged(game, x, y)) {
        return kTileFlag;
    }

    if (GameIsUnsure(game, x, y)) {
        return kTileUnsure;
    }

    if (GameGetStatus(game) == kGameLost && GameIsMine(game, x, y)) {
        if (GameIsRevealed(game, x, y)) {
            return kTileMineHit;
        }
        return kTileMine;
    }

    if (!GameIsRevealed(game, x, y)) {
        return kTileCovered;
    }

    if (GameIsMine(game, x, y)) {
        return kTileMine;
    }

    switch (GameGetCount(game, x, y)) {
        case 1: return kTile1;
        case 2: return kTile2;
        case 3: return kTile3;
        case 4: return kTile4;
        case 5: return kTile5;
        case 6: return kTile6;
        case 7: return kTile7;
        case 8: return kTile8;
        default: return kTileEmpty;
    }
}

static void DrawBoard(WindowRef w)
{
    Rect boardRect;
    Rect statusRect;
    Rect faceRect;
    Rect tileRect;
    short x;
    short y;

    GetBoardLayout(w, &boardRect, &statusRect, &faceRect);

    PenNormal();
    FillRect(&boardRect, &qd.white);
    FrameRect(&boardRect);

    for (y = 0; y < GameGetHeight(&gGame); ++y) {
        for (x = 0; x < GameGetWidth(&gGame); ++x) {
            SetRect(&tileRect,
                (short)(boardRect.left + x * kTileSize),
                (short)(boardRect.top + y * kTileSize),
                (short)(boardRect.left + (x + 1) * kTileSize),
                (short)(boardRect.top + (y + 1) * kTileSize));
            DrawTile(TileForCell(&gGame, x, y), &tileRect);
        }
    }
}

static void DoUpdate(WindowRef w)
{
    Rect boardRect;
    Rect statusRect;
    Rect faceRect;

    SetPort(w);
    BeginUpdate(w);
    EraseRgn(((GrafPtr)w)->visRgn);

    GetBoardLayout(w, &boardRect, &statusRect, &faceRect);
    if (RectInRgn(&statusRect, ((GrafPtr)w)->visRgn)) {
        DrawStatusBar(w);
    }
    if (RectInRgn(&boardRect, ((GrafPtr)w)->visRgn)) {
        if (gHelpDialogOpen) {
            PenNormal();
            FillRect(&boardRect, &qd.white);
        } else {
            DrawBoard(w);
        }
    }

    EndUpdate(w);
}

static void ClearBoardArea(WindowRef w)
{
    Rect boardRect;
    Rect statusRect;
    Rect faceRect;

    SetPort(w);
    GetBoardLayout(w, &boardRect, &statusRect, &faceRect);
    PenNormal();
    FillRect(&boardRect, &qd.white);
}

static void RedrawFullWindow(WindowRef w)
{
    SetPort(w);
    EraseRect(&w->portRect);
    DrawStatusBar(w);
    DrawBoard(w);
}

static void PointToCell(Point localPt, WindowRef w, short* outX, short* outY)
{
    Rect boardRect;
    Rect statusRect;
    Rect faceRect;

    *outX = -1;
    *outY = -1;
    GetBoardLayout(w, &boardRect, &statusRect, &faceRect);

    if (localPt.h < boardRect.left || localPt.v < boardRect.top
        || localPt.h >= boardRect.right || localPt.v >= boardRect.bottom) {
        return;
    }

    *outX = (short)((localPt.h - boardRect.left) / kTileSize);
    *outY = (short)((localPt.v - boardRect.top) / kTileSize);
}

static short PointInFace(Point localPt, WindowRef w)
{
    Rect boardRect;
    Rect statusRect;
    Rect faceRect;

    GetBoardLayout(w, &boardRect, &statusRect, &faceRect);
    return PtInRect(localPt, &faceRect);
}

static short PointInHelp(Point localPt, WindowRef w)
{
    Rect mineTile;
    Rect mineCounter;
    Rect timerTile;
    Rect timerCounter;
    Rect faceRect;
    Rect helpRect;

    GetStatusLayout(w, &mineTile, &mineCounter, &timerTile, &timerCounter, &faceRect, &helpRect);
    return PtInRect(localPt, &helpRect);
}

static void DoContentClick(WindowRef w, Point localPt, short markKey)
{
    short x;
    short y;

    if (PointInHelp(localPt, w)) {
        ShowHelpDialog(w);
        RedrawFullWindow(w);
        SetPort(w);
        ValidRect(&w->portRect);
        return;
    }

    if (PointInFace(localPt, w)) {
        RequestNewGame(GameGetDifficulty(&gGame));
        return;
    }

    if (GameGetStatus(&gGame) != kGamePlaying) {
        return;
    }

    PointToCell(localPt, w, &x, &y);
    if (x < 0 || y < 0) {
        return;
    }

    if (markKey) {
        GameCycleMark(&gGame, x, y);
        RedrawMineCounter(w);
    } else {
        GameReveal(&gGame, x, y);
        if (GameGetStatus(&gGame) != kGamePlaying) {
            RedrawFace(w);
        }
    }

    RedrawChangedCells(w);
}

static void ShowAboutBox(void)
{
    WindowRef w = GetNewWindow(128, NULL, (WindowPtr)-1);
    MoveWindow(w,
        qd.screenBits.bounds.right / 2 - w->portRect.right / 2,
        qd.screenBits.bounds.bottom / 2 - w->portRect.bottom / 2,
        false);
    ShowWindow(w);
    SetPort(w);

    Handle h = GetResource('TEXT', 128);
    if (h) {
        HLock(h);
        Rect r = w->portRect;
        InsetRect(&r, 10, 10);
        TETextBox(*h, GetHandleSize(h), &r, teJustLeft);
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
}

static void ServiceActiveGameTimer(WindowRef w, short* lastSeconds)
{
    GameUpdateTimer(&gGame);
    if (w && GameGetElapsedSeconds(&gGame) != *lastSeconds && GameIsActive(&gGame)) {
        *lastSeconds = GameGetElapsedSeconds(&gGame);
        RedrawTimer(w);
    }
}

static void HandleModalDialogUpdate(WindowRef mainWin, WindowPtr dlgWin, DialogPtr dlg, WindowPtr updateWin)
{
    if (updateWin == dlgWin) {
        SetPort(dlgWin);
        DrawDialog(dlg);
        BeginUpdate(updateWin);
        EndUpdate(updateWin);
        return;
    }

    BeginUpdate(updateWin);
    if (updateWin == mainWin && gMainWindowVisible) {
        SetPort(mainWin);
        if (gHelpDialogOpen) {
            ClearBoardArea(mainWin);
        }
        DrawStatusBar(mainWin);
    }
    EndUpdate(updateWin);
}

static WindowPtr PrepareModalDialog(DialogPtr dlg)
{
    WindowPtr dlgWin = (WindowPtr)dlg;

    ShowWindow(dlgWin);
    SelectWindow(dlgWin);
    SetPort(dlgWin);
    DrawDialog(dlg);
    return dlgWin;
}

static void FinishModalDialog(DialogPtr dlg, WindowRef mainWin)
{
    DisposeDialog(dlg);
    FlushEvents(everyEvent, 0);
    if (mainWin && gMainWindowVisible) {
        SelectWindow(mainWin);
    }
}

static pascal Boolean ModalTimerFilter(DialogPtr dlg, EventRecord* theEvent, short* itemHit)
{
    #pragma unused(dlg, theEvent, itemHit)
    if (gMainWindow && gMainWindowVisible) {
        ServiceActiveGameTimer(gMainWindow, &gModalTimerLastSeconds);
    }
    return false;
}

static short ShowDiscardConfirmDialog(WindowRef w)
{
    DialogPtr dlg;
    short item = 0;
    short confirmed = 0;

    #pragma unused(w)

    dlg = GetNewDialog(kDiscardDialog, NULL, (WindowPtr)-1);
    if (!dlg) {
        return 0;
    }

    gModalTimerLastSeconds = GameGetElapsedSeconds(&gGame);

    for (;;) {
        ModalDialog(ModalTimerFilter, &item);
        if (item == kDialogItemDiscardYes) {
            confirmed = 1;
            break;
        }
        if (item == kDialogItemDiscardNo) {
            break;
        }
    }

    DisposeDialog(dlg);
    FlushEvents(everyEvent, 0);
    return confirmed;
}

static short ConfirmDiscardIfNeeded(WindowRef w)
{
    if (!GameIsActive(&gGame)) {
        return 1;
    }
    return ShowDiscardConfirmDialog(w);
}

static void ShowHelpDialog(WindowRef w)
{
    DialogPtr dlg;
    WindowPtr dlgWin;
    short item = 0;
    EventRecord e;
    short lastSeconds = GameGetElapsedSeconds(&gGame);

    gHelpDialogOpen = 1;
    ClearBoardArea(w);

    dlg = GetNewDialog(kHelpDialog, NULL, (WindowPtr)-1);
    if (!dlg) {
        gHelpDialogOpen = 0;
        return;
    }

    dlgWin = PrepareModalDialog(dlg);

    for (;;) {
        ServiceActiveGameTimer(w, &lastSeconds);
        SystemTask();

        if (WaitNextEvent(everyEvent, &e, 15, NULL)) {
            if (e.what == updateEvt) {
                HandleModalDialogUpdate(w, dlgWin, dlg, (WindowPtr)e.message);
            } else if (IsDialogEvent(&e)) {
                DialogSelect(&e, &dlg, &item);
                if (item == kDialogItemHelpClose) {
                    break;
                }
            }
        }
    }

    FinishModalDialog(dlg, w);
    gHelpDialogOpen = 0;
}

static void RequestNewGame(GameDifficulty difficulty)
{
    if (!ConfirmDiscardIfNeeded(gMainWindow)) {
        if (gMainWindow) {
            RedrawFullWindow(gMainWindow);
            SetPort(gMainWindow);
            ValidRect(&gMainWindow->portRect);
        }
        return;
    }
    StartNewGame(difficulty);
}

static void RequestNewGameFromMenu(void)
{
    GameDifficulty difficulty;

    if (!ConfirmDiscardIfNeeded(gMainWindow)) {
        if (gMainWindow) {
            RedrawFullWindow(gMainWindow);
            SetPort(gMainWindow);
            ValidRect(&gMainWindow->portRect);
        }
        return;
    }
    if (ShowDifficultyDialog(&difficulty)) {
        StartNewGame(difficulty);
    } else if (gMainWindow) {
        RedrawFullWindow(gMainWindow);
        SetPort(gMainWindow);
        ValidRect(&gMainWindow->portRect);
    }
}

static void StartNewGame(GameDifficulty difficulty)
{
    gFacePressed = 0;
    GameNewDifficulty(&gGame, difficulty);
    if (gMainWindow) {
        RedrawFullWindow(gMainWindow);
        SetPort(gMainWindow);
        ValidRect(&gMainWindow->portRect);
    }
}

static short ShowDifficultyDialog(GameDifficulty* difficulty)
{
    DialogPtr dlg;
    short item;
    short selected = 0;
    ModalFilterUPP filter = NULL;

    dlg = GetNewDialog(kDifficultyDialog, NULL, (WindowPtr)-1);
    if (!dlg) {
        *difficulty = kDifficultyBeginner;
        return 1;
    }

    if (gMainWindowVisible && GameIsActive(&gGame)) {
        gModalTimerLastSeconds = GameGetElapsedSeconds(&gGame);
        filter = ModalTimerFilter;
    }

    for (;;) {
        ModalDialog(filter, &item);
        if (item == kDialogItemBeginner) {
            *difficulty = kDifficultyBeginner;
            selected = 1;
            break;
        }
        if (item == kDialogItemIntermediate) {
            *difficulty = kDifficultyIntermediate;
            selected = 1;
            break;
        }
        if (item == kDialogItemExpert) {
            *difficulty = kDifficultyExpert;
            selected = 1;
            break;
        }
        if (item == kDialogItemQuit) {
            RequestQuit();
            selected = 0;
            break;
        }
    }

    DisposeDialog(dlg);
    FlushEvents(everyEvent, 0);
    return selected;
}

static void DoMenuCommand(long menuCommand)
{
    short menuID = menuCommand >> 16;
    short menuItem = menuCommand & 0xFFFF;
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
            case kItemBeginner:
                RequestNewGame(kDifficultyBeginner);
                break;
            case kItemIntermediate:
                RequestNewGame(kDifficultyIntermediate);
                break;
            case kItemExpert:
                RequestNewGame(kDifficultyExpert);
                break;
            case kItemNewGame:
                RequestNewGameFromMenu();
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
    FillScreenWindow(w);
    SetPort(w);
    return w;
}

int main(void)
{
    EventRecord e;
    WindowRef win;
    short oldSeconds = -1;

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
    {
        GameDifficulty difficulty;
        if (ShowDifficultyDialog(&difficulty)) {
            StartNewGame(difficulty);
        }
    }
    if (!gDone) {
        ShowWindow(gMainWindow);
        SelectWindow(gMainWindow);
        gMainWindowVisible = 1;
    }

    for (;;) {
        if (gDone) {
            break;
        }

        SystemTask();
        GameUpdateTimer(&gGame);
        if (gMainWindow && GameGetElapsedSeconds(&gGame) != oldSeconds
            && GameIsActive(&gGame)) {
            oldSeconds = GameGetElapsedSeconds(&gGame);
            RedrawTimer(gMainWindow);
        }

        if (GetNextEvent(everyEvent, &e)) {
            switch (e.what) {
                case keyDown:
                case autoKey:
                    if (e.modifiers & cmdKey) {
                        DoMenuCommand(MenuKey(e.message & charCodeMask));
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
                            } else {
                                Point localPt = e.where;
                                SetPort(win);
                                GlobalToLocal(&localPt);
                                gMouseDownInContent = 1;
                                gFacePressed = PointInFace(localPt, win) ? 1 : 0;
                                if (gFacePressed) {
                                    RedrawFace(win);
                                }
                            }
                            break;
                        case inSysWindow:
                            SystemClick(&e, win);
                            break;
                    }
                    break;
                case mouseUp:
                    if (gMouseDownInContent && FrontWindow() == gMainWindow) {
                        Point localPt = e.where;
                        short wasFacePressed = gFacePressed;
                        SetPort(gMainWindow);
                        GlobalToLocal(&localPt);
                        DoContentClick(gMainWindow, localPt,
                            (e.modifiers & (optionKey | cmdKey)) != 0);
                        gFacePressed = 0;
                        if (wasFacePressed) {
                            RedrawFace(gMainWindow);
                        }
                        gMouseDownInContent = 0;
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
