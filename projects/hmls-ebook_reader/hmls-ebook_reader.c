#include <Quickdraw.h>
#include <Windows.h>
#include <Menus.h>
#include <Fonts.h>
#include <Resources.h>
#include <TextEdit.h>
#include <TextUtils.h>
#include <Dialogs.h>
#include <Devices.h>
#include <StandardFile.h>
#include <Files.h>
#include <OSUtils.h>
#include <Memory.h>

#include <string.h>

enum {
    kMenuApple = 128,
    kMenuFile = 129,
    kMenuEdit = 130
};

enum {
    kItemAbout = 1
};

enum {
    kItemOpen = 1,
    kItemClose = 2,
    kItemQuit = 4
};

enum {
    kMenuBarHeight = 20,
    kGrowBoxSize = 15,
    kContentMargin = 8,
    kNavBarHeight = 36,
    kButtonWidth = 90,
    kButtonHeight = 20,
    kTextInset = 6,
    kPageBufSize = 8192,
    kLineBufSize = 256,
    kPageHistoryMax = 64,
    kMinWindowWidth = 240,
    kMinWindowHeight = 180
};

typedef struct ReaderDoc {
    short fileRef;
    const char* memText;
    long memLen;
    long fileLen;
    long pageOffset;
    long nextPageOffset;
    char* pageText;
    size_t pageTextLen;
    Boolean hasFile;
    Boolean canGoBack;
    Boolean canGoForward;
    ControlHandle btnPrev;
    ControlHandle btnNext;
    Rect textBox;
    short lineHeight;
    short linesPerPage;
    short maxPixelWidth;
    long pageHistory[kPageHistoryMax];
    short pageHistoryCount;
} ReaderDoc;

static WindowRef gMainWindow;

static void FillScreenWindow(WindowRef w);
static void LayoutReaderWindow(WindowRef w);
static void DrawReaderPage(WindowRef w);
static void TurnPage(WindowRef w, short direction);
static void DoContentClick(WindowRef w, Point localPt);
static void DoKeyPage(WindowRef w, char key);

static void SetButtonTitle(ControlHandle c, const char* title) {
    Str255 ptitle;
    short len = (short)strlen(title);
    if (len > 255) {
        len = 255;
    }
    ptitle[0] = (unsigned char)len;
    memcpy(&ptitle[1], title, len);
    SetControlTitle(c, ptitle);
}

static ReaderDoc* GetDoc(WindowRef w) {
    return (ReaderDoc*)GetWRefCon(w);
}

static int ReadRawByte(ReaderDoc* doc, long* pos) {
    Byte byte;
    long count = 1;
    OSErr err;

    if (doc->fileRef <= 0 || *pos >= doc->fileLen) {
        return -1;
    }

    err = SetFPos(doc->fileRef, fsFromStart, *pos);
    if (err != noErr) {
        return -1;
    }

    err = FSRead(doc->fileRef, &count, (Ptr)&byte);
    if (err != noErr || count == 0) {
        return -1;
    }

    (*pos)++;
    return (int)byte;
}

static int ReadSanitizedChar(ReaderDoc* doc, long* pos) {
    int c = ReadRawByte(doc, pos);
    unsigned char uc;
    unsigned char u2;
    unsigned char u3;

    if (c < 0) {
        return -1;
    }

    uc = (unsigned char)c;
    if (uc < 0x80) {
        return (c == '\n') ? '\r' : c;
    }

    if (uc == 0xE2) {
        c = ReadRawByte(doc, pos);
        u2 = (c < 0) ? 0 : (unsigned char)c;
        c = ReadRawByte(doc, pos);
        u3 = (c < 0) ? 0 : (unsigned char)c;
        if (u2 == 0x80) {
            if (u3 == 0x9C || u3 == 0x9D) {
                return '"';
            }
            if (u3 == 0x98 || u3 == 0x99) {
                return '\'';
            }
            if (u3 == 0x94) {
                return '-';
            }
        }
        return ' ';
    }

    if (uc >= 0xC0) {
        for (;;) {
            c = ReadRawByte(doc, pos);
            if (c < 0) {
                break;
            }
            if (((unsigned char)c & 0xC0) != 0x80) {
                break;
            }
        }
    }
    return ' ';
}

static int ReadMemChar(ReaderDoc* doc, long* pos) {
    long i = *pos;
    unsigned char uc;
    unsigned char u2;
    unsigned char u3;

    if (i >= doc->memLen) {
        return -1;
    }

    (*pos)++;
    uc = (unsigned char)doc->memText[i];
    if (uc < 0x80) {
        return (uc == '\n') ? '\r' : (int)uc;
    }

    if (uc == 0xE2 && i + 2 < doc->memLen) {
        u2 = (unsigned char)doc->memText[i + 1];
        u3 = (unsigned char)doc->memText[i + 2];
        *pos += 2;
        if (u2 == 0x80) {
            if (u3 == 0x9C || u3 == 0x9D) {
                return '"';
            }
            if (u3 == 0x98 || u3 == 0x99) {
                return '\'';
            }
            if (u3 == 0x94) {
                return '-';
            }
        }
        return ' ';
    }

    return ' ';
}

static short LinePixelWidth(char* line, short len) {
    if (len <= 0) {
        return 0;
    }
    return TextWidth(line, 0, len);
}

static void AppendLineToPage(ReaderDoc* doc, const char* line, short len) {
    if (doc->pageTextLen + (size_t)len + 2 >= kPageBufSize) {
        return;
    }
    if (doc->pageTextLen > 0) {
        doc->pageText[doc->pageTextLen++] = '\r';
    }
    memcpy(doc->pageText + doc->pageTextLen, line, len);
    doc->pageTextLen += (size_t)len;
    doc->pageText[doc->pageTextLen] = '\0';
}

static Boolean BuildPageAtOffset(ReaderDoc* doc, long offset) {
    char lineBuf[kLineBufSize];
    short lineLen;
    short line;
    long pos;
    int ch;
    Boolean atEOF;

    if (!doc->pageText) {
        return false;
    }

    doc->pageTextLen = 0;
    doc->pageText[0] = '\0';
    doc->pageOffset = offset;
    pos = offset;
    atEOF = false;

    if (doc->fileRef > 0) {
        if (SetFPos(doc->fileRef, fsFromStart, offset) != noErr) {
            return false;
        }
    }

    for (line = 0; line < doc->linesPerPage; line++) {
        lineLen = 0;
        lineBuf[0] = '\0';

        while (lineLen < kLineBufSize - 1) {
            if (doc->fileRef > 0) {
                ch = ReadSanitizedChar(doc, &pos);
            } else {
                ch = ReadMemChar(doc, &pos);
            }
            if (ch < 0) {
                atEOF = true;
                break;
            }

            lineBuf[lineLen++] = (char)ch;
            lineBuf[lineLen] = '\0';

            if (LinePixelWidth(lineBuf, lineLen) > doc->maxPixelWidth) {
                short breakAt = lineLen - 1;

                while (breakAt > 0 && lineBuf[breakAt - 1] != ' ') {
                    breakAt--;
                }
                if (breakAt == 0) {
                    breakAt = lineLen - 1;
                }

                if (doc->fileRef > 0) {
                    long rewindCount = lineLen - breakAt;
                    pos -= rewindCount;
                    SetFPos(doc->fileRef, fsFromStart, pos);
                } else {
                    pos -= (lineLen - breakAt);
                }

                lineLen = breakAt;
                lineBuf[lineLen] = '\0';
                break;
            }
        }

        if (lineLen > 0) {
            while (lineLen > 0 && lineBuf[lineLen - 1] == ' ') {
                lineLen--;
            }
            if (lineLen > 0) {
                AppendLineToPage(doc, lineBuf, lineLen);
            }
        }

        if (atEOF) {
            break;
        }
    }

    doc->nextPageOffset = pos;
    doc->canGoBack = doc->pageHistoryCount > 0;
    doc->canGoForward = (doc->fileRef > 0 && pos < doc->fileLen) ||
        (doc->memText && pos < doc->memLen);
    return true;
}

static void UpdatePageButtons(ReaderDoc* doc) {
    if (doc->btnPrev) {
        HiliteControl(doc->btnPrev, doc->canGoBack ? 0 : 255);
    }
    if (doc->btnNext) {
        HiliteControl(doc->btnNext, doc->canGoForward ? 0 : 255);
    }
}

static void TextBoxRect(WindowRef w, Rect* box) {
    *box = w->portRect;
    InsetRect(box, kContentMargin, kContentMargin);
    box->bottom -= kNavBarHeight;
}

static void FillScreenWindow(WindowRef w) {
    Rect screen = qd.screenBits.bounds;
    Rect bounds;

    bounds.left = screen.left;
    bounds.top = screen.top + kMenuBarHeight;
    bounds.right = screen.right;
    bounds.bottom = screen.bottom;

    MoveWindow(w, bounds.left, bounds.top, false);
    SizeWindow(w, bounds.right - bounds.left, bounds.bottom - bounds.top, true);
}

static void LayoutReaderWindow(WindowRef w) {
    ReaderDoc* doc = GetDoc(w);
    Rect box;
    Rect navBar;
    Rect prevRect;
    Rect nextRect;
    short midX;
    FontInfo fontInfo;

    if (!doc) {
        return;
    }

    TextBoxRect(w, &box);
    doc->textBox = box;

    TextFont(3); /* Geneva */
    TextSize(12);
    GetFontInfo(&fontInfo);
    doc->lineHeight = fontInfo.ascent + fontInfo.descent + fontInfo.leading;
    if (doc->lineHeight < 10) {
        doc->lineHeight = 12;
    }
    doc->linesPerPage = (box.bottom - box.top - (kTextInset * 2)) / doc->lineHeight;
    if (doc->linesPerPage < 4) {
        doc->linesPerPage = 4;
    }
    doc->maxPixelWidth = (box.right - box.left) - (kTextInset * 2);

    navBar.left = box.left;
    navBar.right = box.right;
    navBar.top = box.bottom + 4;
    navBar.bottom = w->portRect.bottom - kContentMargin;

    midX = navBar.left + (navBar.right - navBar.left) / 2;
    SetRect(&prevRect,
        midX - kButtonWidth - 8,
        navBar.top + 6,
        midX - 8,
        navBar.top + 6 + kButtonHeight);
    SetRect(&nextRect,
        midX + 8,
        navBar.top + 6,
        midX + 8 + kButtonWidth,
        navBar.top + 6 + kButtonHeight);

    if (doc->btnPrev) {
        MoveControl(doc->btnPrev, prevRect.left, prevRect.top);
        SizeControl(doc->btnPrev, prevRect.right - prevRect.left, prevRect.bottom - prevRect.top);
    } else {
        doc->btnPrev = NewControl(w, &prevRect, "\p", true, 0, 0, 0, pushButProc, 0);
        SetButtonTitle(doc->btnPrev, "Page Left");
    }

    if (doc->btnNext) {
        MoveControl(doc->btnNext, nextRect.left, nextRect.top);
        SizeControl(doc->btnNext, nextRect.right - nextRect.left, nextRect.bottom - nextRect.top);
    } else {
        doc->btnNext = NewControl(w, &nextRect, "\p", true, 0, 0, 0, pushButProc, 0);
        SetButtonTitle(doc->btnNext, "Page Right");
    }

    BuildPageAtOffset(doc, doc->pageOffset);
    UpdatePageButtons(doc);
}

static void DrawReaderPage(WindowRef w) {
    ReaderDoc* doc = GetDoc(w);
    Rect inner;

    if (!doc || !doc->pageText) {
        return;
    }

    FrameRect(&doc->textBox);
    inner = doc->textBox;
    InsetRect(&inner, kTextInset, kTextInset);
    EraseRect(&inner);

    TextFont(3);
    TextSize(12);

    if (doc->pageTextLen > 0) {
        TETextBox(doc->pageText, (long)doc->pageTextLen, &inner, teJustLeft);
    }
}

static void InvalidateReader(WindowRef w) {
    ReaderDoc* doc = GetDoc(w);
    if (!doc) {
        return;
    }
    InvalRect(&w->portRect);
}

static void TurnPage(WindowRef w, short direction) {
    ReaderDoc* doc = GetDoc(w);
    long target;

    if (!doc) {
        return;
    }

    if (direction > 0) {
        if (!doc->canGoForward) {
            return;
        }
        if (doc->pageHistoryCount < kPageHistoryMax) {
            doc->pageHistory[doc->pageHistoryCount++] = doc->pageOffset;
        }
        target = doc->nextPageOffset;
    } else {
        if (!doc->canGoBack || doc->pageHistoryCount <= 0) {
            return;
        }
        target = doc->pageHistory[--doc->pageHistoryCount];
    }

    BuildPageAtOffset(doc, target);
    UpdatePageButtons(doc);
    InvalidateReader(w);
}

static WindowRef NewReaderWindow(ConstStr255Param title) {
    WindowRef w = GetNewWindow(128, NULL, (WindowPtr)-1);
    ReaderDoc* doc;

    if (!w) {
        return NULL;
    }

    doc = (ReaderDoc*)NewPtrClear(sizeof(ReaderDoc));
    if (!doc) {
        DisposeWindow(w);
        return NULL;
    }

    doc->pageText = (char*)NewPtrClear(kPageBufSize);
    if (!doc->pageText) {
        DisposePtr((Ptr)doc);
        DisposeWindow(w);
        return NULL;
    }

    SetWTitle(w, title);
    SetWRefCon(w, (long)doc);
    FillScreenWindow(w);
    SetPort(w);
    LayoutReaderWindow(w);
    ShowWindow(w);
    return w;
}

static void SetWelcomeText(WindowRef w) {
    ReaderDoc* doc = GetDoc(w);
    static const char welcome[] =
        "hmls ebook reader\r\r"
        "select Open from the File menu to read a book file.\r\r"
        "Each screen is one page. Use Page Left and Page Right "
        "below the text box, or the arrow keys.\r\r"
        "Long chapters are read from disk in sections; there is "
        "no 32K limit.";

    if (!doc) {
        return;
    }

    doc->hasFile = false;
    doc->memText = welcome;
    doc->memLen = (long)strlen(welcome);
    doc->fileRef = 0;
    doc->fileLen = doc->memLen;
    doc->pageHistoryCount = 0;
    doc->pageOffset = 0;
    BuildPageAtOffset(doc, 0);
    UpdatePageButtons(doc);
    InvalidateReader(w);
}

static OSErr OpenFromSFReply(const SFReply* reply, short* refNum, long* fileLen) {
    OSErr err;
    WDPBRec wd;

    *refNum = 0;
    *fileLen = 0;

    /*
     * Open immediately while SFGetFile still has the correct volume and
     * working directory. Do not call SetVol (resets dir to volume root).
     */
    err = OpenDF(reply->fName, reply->vRefNum, refNum);
    if (err == noErr) {
        return GetEOF(*refNum, fileLen);
    }

    err = FSOpen(reply->fName, reply->vRefNum, refNum);
    if (err == noErr) {
        return GetEOF(*refNum, fileLen);
    }

    memset(&wd, 0, sizeof(wd));
    wd.ioNamePtr = NULL;
    wd.ioWDIndex = 0;
    wd.ioVRefNum = reply->vRefNum;
    if (PBGetWDInfoSync(&wd) == noErr) {
        err = HOpenDF(wd.ioWDVRefNum, wd.ioWDDirID, reply->fName, fsRdPerm, refNum);
        if (err == noErr) {
            return GetEOF(*refNum, fileLen);
        }
        err = HOpenDF(reply->vRefNum, wd.ioWDDirID, reply->fName, fsRdPerm, refNum);
        if (err == noErr) {
            return GetEOF(*refNum, fileLen);
        }
    }

    return err;
}

static void AttachBookToWindow(WindowRef w, short refNum, long fileLen, ConstStr255Param title) {
    ReaderDoc* doc = GetDoc(w);

    if (!doc) {
        if (refNum > 0) {
            FSClose(refNum);
        }
        return;
    }

    if (doc->fileRef > 0) {
        FSClose(doc->fileRef);
    }

    SetWTitle(w, title);
    doc->fileRef = refNum;
    doc->memText = NULL;
    doc->memLen = 0;
    doc->hasFile = true;
    doc->fileLen = fileLen;
    doc->pageHistoryCount = 0;
    doc->pageOffset = 0;
    doc->nextPageOffset = 0;

    BuildPageAtOffset(doc, 0);
    UpdatePageButtons(doc);
    InvalidateReader(w);
}

void DoCloseWindow(WindowRef w) {
    if (!w) {
        return;
    }

    if (GetWindowKind(w) < 0) {
        CloseDeskAcc(GetWindowKind(w));
        return;
    }

    if (w == gMainWindow) {
        ReaderDoc* doc = GetDoc(w);
        if (doc && doc->fileRef > 0) {
            FSClose(doc->fileRef);
            doc->fileRef = 0;
        }
        SetWelcomeText(gMainWindow);
        return;
    }

    {
        ReaderDoc* doc = GetDoc(w);
        if (doc) {
            if (doc->fileRef > 0) {
                FSClose(doc->fileRef);
            }
            if (doc->pageText) {
                DisposePtr((Ptr)doc->pageText);
            }
            if (doc->btnPrev) {
                DisposeControl(doc->btnPrev);
            }
            if (doc->btnNext) {
                DisposeControl(doc->btnNext);
            }
            DisposePtr((Ptr)doc);
        }
        DisposeWindow(w);
    }
}

void DoOpenFile(void) {
    SFReply reply;
    Point where = {80, 50};
    short refNum;
    long fileLen;
    OSErr err;

    SFGetFile(where, "\p", NULL, -1, NULL, NULL, &reply);

    if (!reply.good || !gMainWindow) {
        return;
    }

    err = OpenFromSFReply(&reply, &refNum, &fileLen);
    if (err != noErr) {
        SysBeep(1);
        return;
    }

    SelectWindow(gMainWindow);
    AttachBookToWindow(gMainWindow, refNum, fileLen, reply.fName);
}

void AdjustMenus(void) {
    WindowRef w = FrontWindow();
    MenuRef fileMenu = GetMenu(kMenuFile);
    if (w) {
        EnableItem(fileMenu, kItemClose);
    } else {
        DisableItem(fileMenu, kItemClose);
    }

    MenuRef editMenu = GetMenu(kMenuEdit);
    if (w && GetWindowKind(w) < 0) {
        EnableItem(editMenu, 1);
        EnableItem(editMenu, 3);
        EnableItem(editMenu, 4);
        EnableItem(editMenu, 5);
        EnableItem(editMenu, 6);
    } else {
        DisableItem(editMenu, 1);
        DisableItem(editMenu, 3);
        DisableItem(editMenu, 4);
        DisableItem(editMenu, 5);
        DisableItem(editMenu, 6);
    }
}

void DoMenuCommand(long menuCommand) {
    short menuID = HiWord(menuCommand);
    short menuItem = LoWord(menuCommand);
    Str255 str;

    if (menuID == kMenuApple) {
        if (menuItem == kItemAbout) {
            NoteAlert(128, NULL);
        } else {
            GetMenuItemText(GetMenu(kMenuApple), menuItem, str);
            OpenDeskAcc(str);
        }
    } else if (menuID == kMenuFile) {
        switch (menuItem) {
            case kItemOpen:
                DoOpenFile();
                break;
            case kItemClose:
                DoCloseWindow(FrontWindow());
                break;
            case kItemQuit:
                ExitToShell();
                break;
        }
    } else if (menuID == kMenuEdit) {
        if (!SystemEdit(menuItem - 1)) {
            /* Edit command not handled by Desk Accessory */
        }
    }

    HiliteMenu(0);
}

void DoUpdate(WindowRef w) {
    ReaderDoc* doc;

    if (GetWindowKind(w) < 0) {
        return;
    }

    SetPort(w);
    BeginUpdate(w);
    EraseRect(&w->portRect);

    doc = GetDoc(w);
    if (doc) {
        DrawReaderPage(w);
        if (doc->btnPrev) {
            Draw1Control(doc->btnPrev);
        }
        if (doc->btnNext) {
            Draw1Control(doc->btnNext);
        }
    }

    EndUpdate(w);
}

static void DoContentClick(WindowRef w, Point localPt) {
    ReaderDoc* doc = GetDoc(w);
    ControlHandle control;
    short part;

    if (!doc) {
        return;
    }

    control = NULL;
    part = FindControl(localPt, w, &control);
    if (!control || part == 0) {
        return;
    }

    if (control == doc->btnPrev) {
        if (TrackControl(control, localPt, NULL)) {
            TurnPage(w, -1);
        }
        return;
    }

    if (control == doc->btnNext) {
        if (TrackControl(control, localPt, NULL)) {
            TurnPage(w, 1);
        }
    }
}

static void DoKeyPage(WindowRef w, char key) {
    switch (key) {
        case 0x0B: /* Page Up */
        case 0x1E: /* Left Arrow */
            TurnPage(w, -1);
            break;
        case 0x0C: /* Page Down */
        case 0x1F: /* Down Arrow */
        case ' ':
            TurnPage(w, 1);
            break;
        default:
            break;
    }
}

static void DoGrowWindow(WindowRef w, Point startPt) {
    Rect minBounds;
    Rect screen;
    long growResult;

    SetRect(&minBounds, kMinWindowWidth, kMinWindowHeight, kMinWindowWidth, kMinWindowHeight);
    screen = qd.screenBits.bounds;
    screen.top += kMenuBarHeight;

    growResult = GrowWindow(w, startPt, &minBounds);
    if (growResult != 0) {
        short width = LoWord(growResult);
        short height = HiWord(growResult);
        SizeWindow(w, width, height, true);
        LayoutReaderWindow(w);
        InvalRect(&w->portRect);
    }
}

int main(void) {
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(NULL);

    SetMenuBar(GetNewMBar(128));
    AppendResMenu(GetMenu(128), 'DRVR');
    DrawMenuBar();

    InitCursor();

    gMainWindow = NewReaderWindow("\pebook reader");
    if (gMainWindow) {
        SetWelcomeText(gMainWindow);
    }

    for (;;) {
        EventRecord e;
        WindowRef win;

        SystemTask();

        if (GetNextEvent(everyEvent, &e)) {
            switch (e.what) {
                case keyDown:
                case autoKey:
                    if ((e.modifiers & cmdKey) != 0) {
                        AdjustMenus();
                        DoMenuCommand(MenuKey(e.message & charCodeMask));
                    } else {
                        win = FrontWindow();
                        if (win && GetWindowKind(win) >= 0) {
                            DoKeyPage(win, (char)(e.message & charCodeMask));
                        }
                    }
                    break;
                case mouseDown:
                    switch (FindWindow(e.where, &win)) {
                        case inMenuBar:
                            AdjustMenus();
                            DoMenuCommand(MenuSelect(e.where));
                            break;
                        case inDrag:
                            DragWindow(win, e.where, &qd.screenBits.bounds);
                            break;
                        case inGoAway:
                            if (TrackGoAway(win, e.where)) {
                                DoCloseWindow(win);
                            }
                            break;
                        case inGrow:
                            DoGrowWindow(win, e.where);
                            break;
                        case inContent:
                            if (win != FrontWindow()) {
                                SelectWindow(win);
                            } else {
                                SetPort(win);
                                {
                                    Point localPt = e.where;
                                    GlobalToLocal(&localPt);
                                    DoContentClick(win, localPt);
                                }
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
            }
        }
    }

    return 0;
}
