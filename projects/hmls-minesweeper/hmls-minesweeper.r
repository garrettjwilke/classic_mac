#include "Processes.r"
#include "Menus.r"
#include "Windows.r"
#include "MacTypes.r"
#include "Dialogs.r"

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
        "Beginner", noIcon, "1", noMark, plain;
        "Intermediate", noIcon, "2", noMark, plain;
        "Expert", noIcon, "3", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "New Game", noIcon, "N", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Quit", noIcon, "Q", noMark, plain;
    }
};

resource 'MBAR' (128) {
    { 128, 129 };
};

resource 'DLOG' (129) {
    { 80, 80, 230, 424 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    129,
    "New Game",
    centerMainScreen
};

resource 'DITL' (129) {
    {
        { 12, 16, 28, 328 },
        StaticText { disabled, "Choose a difficulty:" };

        { 44, 16, 64, 112 },
        Button { enabled, "Beginner" };

        { 44, 120, 64, 216 },
        Button { enabled, "Intermediate" };

        { 44, 224, 64, 320 },
        Button { enabled, "Expert" };

        { 80, 120, 100, 216 },
        Button { enabled, "Quit Game" };
    }
};

resource 'DLOG' (130) {
    { 60, 70, 255, 400 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    130,
    "Help",
    centerMainScreen
};

resource 'DITL' (130) {
    {
        { 12, 16, 138, 314 },
        StaticText { disabled,
            "Hold Option or Command and click a tile\r"
            "to place flags or question blocks.\r\r"
            "To start a new game with a specific mode:\r"
            "CMD + 1 = Beginner\r"
            "CMD + 2 = Intermediate\r"
            "CMD + 3 = Expert"
        };

        { 148, 130, 168, 230 },
        Button { enabled, "Close" };
    }
};

resource 'DLOG' (131) {
    { 80, 100, 180, 340 },
    dBoxProc,
    visible,
    noGoAway,
    0,
    131,
    "",
    centerMainScreen
};

resource 'DITL' (131) {
    {
        { 20, 16, 36, 224 },
        StaticText { disabled, "Discard Current Game?" };

        { 52, 40, 72, 100 },
        Button { enabled, "Yes" };

        { 52, 140, 72, 200 },
        Button { enabled, "No" };
    }
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
    "Option/Command-click to cycle flag/unsure.\r\r"
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
