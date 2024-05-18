#include <Quickdraw.h>
#include <Dialogs.h>
#include <Fonts.h>

#ifndef TARGET_API_MAC_CARBON
  #define NewUserItemUPP NewUserItemProc
#endif

pascal void ButtonFrameProc(DialogRef dlg, DialogItemIndex itemNo)
{
  DialogItemType type;
  Handle itemH;
  Rect box;

  GetDialogItem(dlg, 1, &type, &itemH, &box);
  InsetRect(&box, -4, -4);
  PenSize(3,3);
  FrameRoundRect(&box,16,16);
}

int main(void)
{
#if !TARGET_API_MAC_CARBON
  InitGraf(&qd.thePort);
  InitFonts();
  InitWindows();
  InitMenus();
  TEInit();
  InitDialogs(NULL);
#endif
  DialogPtr dlg = GetNewDialog(128,0,(WindowPtr)-1);
  InitCursor();
  SelectDialogItemText(dlg,4,0,32767);

  DialogItemType type;
  Handle itemH;
  Rect box;

  GetDialogItem(dlg, 2, &type, &itemH, &box);
  SetDialogItem(dlg, 2, type, (Handle) NewUserItemUPP(&ButtonFrameProc), &box);

  ControlHandle cb, radio1, radio2;
  GetDialogItem(dlg, 5, &type, &itemH, &box);
  cb = (ControlHandle)itemH;
  GetDialogItem(dlg, 6, &type, &itemH, &box);
  radio1 = (ControlHandle)itemH;
  GetDialogItem(dlg, 7, &type, &itemH, &box);
  radio2 = (ControlHandle)itemH;
  SetControlValue(radio2, 1);

  short item;
  do {
    ModalDialog(NULL, &item);

    if(item >= 5 && item <= 7)
    {
      if(item == 5)
        SetControlValue(cb, !GetControlValue(cb));
      if(item == 6 || item == 7)
      {
        SetControlValue(radio1, item == 6);
        SetControlValue(radio2, item == 7);
      }
    }
  } while(item != 1);

  FlushEvents(everyEvent, -1);
  return 0;
}
