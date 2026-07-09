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
    kStatusPadding = 4
};

enum {
    kDifficultyDialog = 129,
    kDialogItemBeginner = 2,
    kDialogItemIntermediate = 3,
    kDialogItemExpert = 4,
    kDialogItemQuit = 5
};

static WindowRef gMainWindow;
static GameState gGame;
static short gMouseDownInContent;
static short gFacePressed;
static short gDone;

static void FillScreenWindow(WindowRef w);
static void GetBoardLayout(WindowRef w, Rect* boardRect, Rect* statusRect, Rect* faceRect);
static void GetCounterRects(WindowRef w, Rect* leftCounter, Rect* rightCounter, Rect* faceRect);
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
static void DoContentClick(WindowRef w, Point localPt, short optionKey);
static void ShowAboutBox(void);
static void DoMenuCommand(long menuCommand);
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
    short totalWidth;
    short totalHeight;
    short originX;
    short originY;

    SetRect(statusRect, port.left, port.top, port.right, port.top + kStatusBarHeight);

    boardWidth = (short)(GameGetWidth(&gGame) * kTileSize);
    boardHeight = (short)(GameGetHeight(&gGame) * kTileSize);
    totalWidth = boardWidth;
    totalHeight = (short)(kStatusBarHeight + boardHeight);

    originX = (short)(port.left + (port.right - port.left - totalWidth) / 2);
    originY = (short)(port.top + (port.bottom - port.top - totalHeight) / 2);
    if (originY < port.top) {
        originY = port.top;
    }

    SetRect(boardRect,
        originX,
        (short)(originY + kStatusBarHeight),
        (short)(originX + boardWidth),
        (short)(originY + kStatusBarHeight + boardHeight));

    SetRect(faceRect,
        (short)(originX + totalWidth / 2 - kTileSize / 2),
        (short)(originY + (kStatusBarHeight - kTileSize) / 2),
        (short)(originX + totalWidth / 2 + kTileSize / 2),
        (short)(originY + (kStatusBarHeight - kTileSize) / 2 + kTileSize));
}

static void DrawCounter(short value, const Rect* area)
{
    Str255 text;
    Rect box = *area;

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

    PenNormal();
    FillRect(&box, &qd.white);
    FrameRect(&box);
    InsetRect(&box, 2, 1);
    TextFont(1);
    TextSize(12);
    TextFace(bold);
    MoveTo((short)(box.left + 4), (short)(box.bottom - 3));
    DrawString(text);
    TextFace(0);
}

static void GetCounterRects(WindowRef w, Rect* leftCounter, Rect* rightCounter, Rect* faceRect)
{
    Rect boardRect;
    Rect statusRect;
    short originX;
    short originY;
    short totalWidth;

    GetBoardLayout(w, &boardRect, &statusRect, faceRect);
    totalWidth = boardRect.right - boardRect.left;
    originX = boardRect.left;
    originY = statusRect.top;

    SetRect(leftCounter,
        (short)(originX + kStatusPadding),
        (short)(originY + 1),
        (short)(originX + kStatusPadding + 39),
        (short)(originY + kStatusBarHeight - 1));

    SetRect(rightCounter,
        (short)(originX + totalWidth - kStatusPadding - 39),
        (short)(originY + 1),
        (short)(originX + totalWidth - kStatusPadding),
        (short)(originY + kStatusBarHeight - 1));
}

static void RedrawTimer(WindowRef w)
{
    Rect leftCounter;
    Rect rightCounter;
    Rect faceRect;

    SetPort(w);
    GetCounterRects(w, &leftCounter, &rightCounter, &faceRect);
    DrawCounter(GameGetElapsedSeconds(&gGame), &rightCounter);
}

static void RedrawMineCounter(WindowRef w)
{
    Rect leftCounter;
    Rect rightCounter;
    Rect faceRect;

    SetPort(w);
    GetCounterRects(w, &leftCounter, &rightCounter, &faceRect);
    DrawCounter(GameGetRemainingMines(&gGame), &leftCounter);
}

static void RedrawFace(WindowRef w)
{
    Rect leftCounter;
    Rect rightCounter;
    Rect faceRect;

    SetPort(w);
    GetCounterRects(w, &leftCounter, &rightCounter, &faceRect);
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
    Rect leftCounter;
    Rect rightCounter;
    short originX;
    short originY;
    short totalWidth;

    GetBoardLayout(w, &boardRect, &statusRect, &faceRect);
    totalWidth = boardRect.right - boardRect.left;
    originX = boardRect.left;
    originY = statusRect.top;

    PenNormal();
    FillRect(&statusRect, &qd.ltGray);
    FrameRect(&statusRect);

    SetRect(&leftCounter,
        (short)(originX + kStatusPadding),
        (short)(originY + 1),
        (short)(originX + kStatusPadding + 39),
        (short)(originY + kStatusBarHeight - 1));

    SetRect(&rightCounter,
        (short)(originX + totalWidth - kStatusPadding - 39),
        (short)(originY + 1),
        (short)(originX + totalWidth - kStatusPadding),
        (short)(originY + kStatusBarHeight - 1));

    DrawCounter(GameGetRemainingMines(&gGame), &leftCounter);
    DrawCounter(GameGetElapsedSeconds(&gGame), &rightCounter);
    DrawTile(FaceTileForGame(&gGame, gFacePressed), &faceRect);
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
        DrawBoard(w);
    }

    EndUpdate(w);
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

static void DoContentClick(WindowRef w, Point localPt, short optionKey)
{
    short x;
    short y;

    if (PointInFace(localPt, w)) {
        StartNewGame(GameGetDifficulty(&gGame));
        return;
    }

    if (GameGetStatus(&gGame) != kGamePlaying) {
        return;
    }

    PointToCell(localPt, w, &x, &y);
    if (x < 0 || y < 0) {
        return;
    }

    if (optionKey) {
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

static void StartNewGame(GameDifficulty difficulty)
{
    gFacePressed = 0;
    GameNewDifficulty(&gGame, difficulty);
    if (gMainWindow) {
        RedrawFullWindow(gMainWindow);
    }
}

static short ShowDifficultyDialog(GameDifficulty* difficulty)
{
    DialogPtr dlg;
    short item;
    short selected = 0;

    dlg = GetNewDialog(kDifficultyDialog, NULL, (WindowPtr)-1);
    if (!dlg) {
        *difficulty = kDifficultyBeginner;
        return 1;
    }

    for (;;) {
        ModalDialog(NULL, &item);
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
                StartNewGame(kDifficultyBeginner);
                break;
            case kItemIntermediate:
                StartNewGame(kDifficultyIntermediate);
                break;
            case kItemExpert:
                StartNewGame(kDifficultyExpert);
                break;
            case kItemNewGame: {
                GameDifficulty difficulty;
                if (ShowDifficultyDialog(&difficulty)) {
                    StartNewGame(difficulty);
                }
                break;
            }
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
    ShowWindow(w);
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

    for (;;) {
        if (gDone) {
            break;
        }

        SystemTask();
        GameUpdateTimer(&gGame);
        if (gMainWindow && GameGetElapsedSeconds(&gGame) != oldSeconds
            && GameGetStatus(&gGame) == kGamePlaying && gGame.firstClickDone) {
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
                        DoContentClick(gMainWindow, localPt, (e.modifiers & optionKey) != 0);
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
