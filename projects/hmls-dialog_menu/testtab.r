#include "Dialogs.r"

resource 'DLOG' (128) {
  { 50, 100, 240, 420 },
  dBoxProc,
  visible,
  noGoAway,
  0,
  128,
  "",
  centerMainScreen
};

resource 'DITL' (128) {
  {
    { 190-10-20, 320-10-80, 190-10, 320-10 },
    Button { enabled, "exit" };

    { 190-10-20-5, 320-10-80-5, 190-10+5, 320-10+5 },
    UserItem { enabled };

    { 10, 10, 30, 310 },
    StaticText { enabled, "input stuffs below:" };

    { 40, 10, 56, 310 },
    EditText { enabled, "type here" };

    { 70, 10, 86, 310 },
    CheckBox { enabled, "enable/disable" };

    { 90, 10, 106, 310 },
    RadioButton { enabled, "choose 1" };

    { 110, 10, 126, 310 },
    RadioButton { enabled, "choose 2" };
  }
};

#include "Processes.r"

resource 'SIZE' (-1) {
  reserved,
  acceptSuspendResumeEvents,
  reserved,
  canBackground,
  doesActivateOnFGSwitch,
  backgroundAndForeground,
  dontGetFrontClicks,
  ignoreChildDiedEvents,
  is32BitCompatible,
  #ifdef TARGET_API_MAC_CARBON
    isHighLevelEventAware,
  #else
    notHighLevelEventAware,
  #endif
  onlyLocalHLEvents,
  notStationeryAware,
  dontUseTextEditServices,
  reserved,
  reserved,
  reserved,
  #ifdef TARGET_API_MAC_CARBON
    500 * 1024,  // Carbon apparently needs additional memory.
    500 * 1024
  #else
    100 * 1024,
    100 * 1024
  #endif
};
