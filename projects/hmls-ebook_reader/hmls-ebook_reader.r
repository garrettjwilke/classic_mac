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
        "About Text Viewer...", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain;
    }
};

resource 'MENU' (129) {
    129, textMenuProc;
    allEnabled, enabled;
    "File";
    {
        "Open...", noIcon, "O", noMark, plain;
        "Close", noIcon, "W", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Quit", noIcon, "Q", noMark, plain;
    }
};

resource 'MENU' (130) {
    130, textMenuProc;
    allEnabled, enabled;
    "Edit";
    {
        "Undo", noIcon, "Z", noMark, plain;
        "-", noIcon, noKey, noMark, plain;
        "Cut", noIcon, "X", noMark, plain;
        "Copy", noIcon, "C", noMark, plain;
        "Paste", noIcon, "V", noMark, plain;
        "Clear", noIcon, noKey, noMark, plain;
    }
};

resource 'MBAR' (128) {
    { 128, 129, 130 };
};

resource 'WIND' (128) {
    {20, 0, 342, 512}, zoomDocProc;
    invisible;
    goAway;
    0, "ebook reader";
    noAutoCenter;
};

resource 'ALRT' (128) {
    {80, 80, 200, 420},
    128,
    {
        OK, visible, silent,
        OK, visible, silent,
        OK, visible, silent,
        OK, visible, silent
    },
    centerMainScreen
};

resource 'DITL' (128) {
    {
        {80, 240, 100, 320}, Button { enabled, "OK" };
        {15, 20, 75, 320}, StaticText { disabled, "hmls ebook reader v1.0\rFor Classic Macintosh System 6" };
    }
};

resource 'ALRT' (129) {
    {60, 60, 220, 380},
    129,
    {
        OK, visible, silent,
        OK, visible, silent,
        OK, visible, silent,
        OK, visible, silent
    },
    centerMainScreen
};

resource 'DITL' (129) {
    {
        {12, 16, 52, 304}, StaticText { disabled,
            "Creating .book index file...\r"
            "Please wait while page offsets are saved."
        };
        {64, 16, 84, 304}, StaticText { disabled, "Starting..." };
    }
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
