#include "Processes.r"
#include "Menus.r"
#include "Windows.r"
#include "MacTypes.r"

resource 'MENU' (128) {
    128, textMenuProc;
    allEnabled, enabled;
    apple;
    {
        "About Minesweeper...", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain;
    }
};

resource 'MENU' (129) {
    129, textMenuProc;
    allEnabled, enabled;
    "Game";
    {
        "Beginner", noIcon, noKey, noMark, plain;
        "Intermediate", noIcon, noKey, noMark, plain;
        "Expert", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "New Game", noIcon, "N", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Quit", noIcon, "Q", noMark, plain;
    }
};

resource 'MBAR' (128) {
    { 128, 129 };
};

resource 'WIND' (128) {
    {20, 0, 342, 512}, noGrowDocProc;
    invisible;
    goAway;
    0, "Minesweeper";
    noAutoCenter;
};

data 'TEXT' (128) {
    "Minesweeper for Classic Mac\r\r"
    "Left click to reveal.\r"
    "Option-click to flag.\r\r"
    "Built with Retro68."
};

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
    isHighLevelEventAware,
    onlyLocalHLEvents,
    notStationeryAware,
    dontUseTextEditServices,
    reserved,
    reserved,
    reserved,
#ifdef TARGET_API_MAC_CARBON
    500 * 1024,
    500 * 1024
#else
    256 * 1024,
    256 * 1024
#endif
};

#include "tiles_generated.r"
