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

resource 'MENU' (131) {
    131, textMenuProc;
    allEnabled, enabled;
    "Bookmarks";
    {
        "Add Bookmark", noIcon, noKey, noMark, plain;
        "Delete Bookmark", noIcon, noKey, noMark, plain;
        "-", noIcon, noKey, noMark, plain;
    }
};

resource 'MENU' (132) {
    132, textMenuProc;
    allEnabled, enabled;
    "Options";
    {
        "Regenerate .book Index", noIcon, noKey, noMark, plain;
        "Delete Bookmarks and Reading Progress", noIcon, noKey, noMark, plain;
    }
};

resource 'MBAR' (128) {
    { 128, 129, 130, 131, 132 };
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
    {50, 50, 240, 400},
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
        {12, 16, 72, 334}, StaticText { disabled,
            "A .book index file is being created\r"
            "for this book. Please wait.\r\r"
            "You can read the book when this\r"
            "dialog closes."
        };
        {88, 16, 108, 334}, StaticText { disabled, "Starting..." };
    }
};

resource 'ALRT' (130) {
    {70, 60, 210, 380},
    130,
    {
        OK, visible, silent,
        OK, visible, silent,
        OK, visible, silent,
        OK, visible, silent
    },
    centerMainScreen
};

resource 'DITL' (130) {
    {
        {100, 220, 120, 280}, Button { enabled, "Yes" };
        {100, 140, 120, 200}, Button { enabled, "No" };
        {15, 20, 90, 300}, StaticText { disabled,
            "Regenerate the .book index file for this book?\r\r"
            "The existing index will be deleted and rebuilt."
        };
    }
};

resource 'ALRT' (131) {
    {70, 60, 210, 380},
    131,
    {
        OK, visible, silent,
        OK, visible, silent,
        OK, visible, silent,
        OK, visible, silent
    },
    centerMainScreen
};

resource 'DITL' (131) {
    {
        {100, 220, 120, 280}, Button { enabled, "Yes" };
        {100, 140, 120, 200}, Button { enabled, "No" };
        {15, 20, 90, 300}, StaticText { disabled,
            "Delete bookmarks and reading progress?\r\r"
            "The .read file for this book will be removed."
        };
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
