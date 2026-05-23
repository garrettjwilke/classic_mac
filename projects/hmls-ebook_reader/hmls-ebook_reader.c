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

#include <stdio.h>
#include <stdlib.h>
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

static Rect nextWindowRect = {60, 40, 280, 470};

void p2cstrcpy(char* dst, ConstStr255Param src) {
    int len = src[0];
    memcpy(dst, &src[1], len);
    dst[len] = '\0';
}

size_t normalize_line_endings(char* buf, size_t len) {
    size_t write_idx = 0;
    for (size_t read_idx = 0; read_idx < len; ) {
        if (buf[read_idx] == '\r' && read_idx + 1 < len && buf[read_idx + 1] == '\n') {
            buf[write_idx++] = '\r';
            read_idx += 2;
        } else if (buf[read_idx] == '\n') {
            buf[write_idx++] = '\r';
            read_idx++;
        } else {
            buf[write_idx++] = buf[read_idx++];
        }
    }
    return write_idx;
}

void DoCloseWindow(WindowRef w) {
    if (w) {
        if (GetWindowKind(w) < 0) {
            CloseDeskAcc(GetWindowKind(w));
        } else {
            TEHandle te = (TEHandle)GetWRefCon(w);
            if (te) {
                TEDispose(te);
            }
            DisposeWindow(w);
        }
    }
}

void DoOpenFile(void) {
    SFReply reply;
    Point where = {80, 50};
    
    SFGetFile(where, "\p", NULL, -1, NULL, NULL, &reply);
    
    if (reply.good) {
        SetVol(NULL, reply.vRefNum);
        
        char filename[64];
        p2cstrcpy(filename, reply.fName);
        
        FILE* f = fopen(filename, "rb");
        if (!f) {
            return;
        }
        
        char* buf = malloc(32768);
        if (!buf) {
            fclose(f);
            return;
        }
        
        size_t bytesRead = fread(buf, 1, 32767, f);
        fclose(f);
        buf[bytesRead] = '\0';
        
        bytesRead = normalize_line_endings(buf, bytesRead);
        
        WindowRef w = GetNewWindow(128, NULL, (WindowPtr)-1);
        if (w) {
            MoveWindow(w, nextWindowRect.left, nextWindowRect.top, false);
            nextWindowRect.left += 15;
            nextWindowRect.top += 15;
            if (nextWindowRect.left > 150) {
                nextWindowRect.left = 60;
                nextWindowRect.top = 40;
            }
            
            SetWTitle(w, reply.fName);
            SetPort(w);
            
            Rect viewRect = w->portRect;
            InsetRect(&viewRect, 6, 6);
            Rect destRect = viewRect;
            
            TEHandle te = TENew(&destRect, &viewRect);
            if (te) {
                TESetText(buf, bytesRead, te);
                SetWRefCon(w, (long)te);
            }
            
            ShowWindow(w);
        }
        
        free(buf);
    }
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
            // Edit command not handled by Desk Accessory
        }
    }
    
    HiliteMenu(0);
}

void DoUpdate(WindowRef w) {
    if (GetWindowKind(w) < 0) return;
    
    SetPort(w);
    BeginUpdate(w);
    EraseRect(&w->portRect);
    
    TEHandle te = (TEHandle)GetWRefCon(w);
    if (te) {
        TEUpdate(&w->portRect, te);
    }
    
    EndUpdate(w);
}

void DoActivate(WindowRef w, Boolean active) {
    if (GetWindowKind(w) < 0) return;
    
    SetPort(w);
    TEHandle te = (TEHandle)GetWRefCon(w);
    if (te) {
        if (active) {
            TEActivate(te);
        } else {
            TEDeactivate(te);
        }
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
    
    {
        WindowRef w = GetNewWindow(128, NULL, (WindowPtr)-1);
        if (w) {
            SetPort(w);
            
            Rect viewRect = w->portRect;
            InsetRect(&viewRect, 6, 6);
            Rect destRect = viewRect;
            
            TEHandle te = TENew(&destRect, &viewRect);
            if (te) {
                const char* welcome = "hmls ebook reader\r\rselect 'open...' from the File menu to view an book file.";
                TESetText((Ptr)welcome, strlen(welcome), te);
                SetWRefCon(w, (long)te);
            }
            
            ShowWindow(w);
        }
    }
    
    for (;;) {
        EventRecord e;
        WindowRef win;
        
        SystemTask();
        
        WindowRef activeWin = FrontWindow();
        if (activeWin && GetWindowKind(activeWin) >= 0) {
            TEHandle te = (TEHandle)GetWRefCon(activeWin);
            if (te) {
                TEIdle(te);
            }
        }
        
        if (GetNextEvent(everyEvent, &e)) {
            switch (e.what) {
                case keyDown:
                    if (e.modifiers & cmdKey) {
                        AdjustMenus();
                        DoMenuCommand(MenuKey(e.message & charCodeMask));
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
                        case inContent:
                            if (win != FrontWindow()) {
                                SelectWindow(win);
                            } else {
                                SetPort(win);
                                Point localPt = e.where;
                                GlobalToLocal(&localPt);
                                TEHandle te = (TEHandle)GetWRefCon(win);
                                if (te) {
                                    TEClick(localPt, (e.modifiers & shiftKey) != 0, te);
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
                case activateEvt:
                    DoActivate((WindowRef)e.message, (e.modifiers & activeFlag) != 0);
                    break;
            }
        }
    }
    
    return 0;
}
