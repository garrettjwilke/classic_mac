#include "Processes.r"
#include "Menus.r"
#include "Windows.r"
#include "MacTypes.r"
#include "Dialogs.r"

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
        {15, 20, 75, 320}, StaticText { disabled, "hmls-epub_converter\r" };
    }
};

resource 'STR#' (128) {
    {
        "hmls-epub_converter"
    }
};
