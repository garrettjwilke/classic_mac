#include "Retro68.r"

type 'INIT' {
	RETRO68_CODE_TYPE
};

resource 'INIT' (128, locked) {
	dontBreakAtEntry, $$read("SystemExtension.flt");
};
