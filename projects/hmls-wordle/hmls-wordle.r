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
        "About Wordle...", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain;
    }
};

resource 'MENU' (129) {
    129, textMenuProc;
    allEnabled, enabled;
    "Game";
    {
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
    visible;
    goAway;
    0, "Wordle";
    noAutoCenter;
};

data 'TEXT' (128) {
    "Wordle for Classic Mac\r\r"
    "Guess the 5-letter word in 6 tries.\r\r"
    "Black tile = correct letter and spot.\r"
    "Pattern tile = letter is in the word.\r"
    "Dotted tile = letter not in the word.\r\r"
    "Type letters, Return to submit,\r"
    "Delete to erase.\r\r"
    "Edit 8x8 PNGs in data/:\r"
    "missrows / wrongplacerows / correctrows.\r\r"
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
    512 * 1024,
    512 * 1024
#else
    384 * 1024,
    256 * 1024
#endif
};
