#include "model/scale/preset_scales.h"
#include "model/scale/note_set.h"

std::array<char const*, NUM_PRESET_SCALES> presetScaleNames = {
#define DEF(id, name, notes) name,
    DEF_SCALES()
#undef DEF
};

const NoteSet presetScaleNotes[NUM_PRESET_SCALES] = {
#define DEF(id, name, notes) notes,
    DEF_SCALES()
#undef DEF
};

const char* getScaleName(uint8_t scale) {
	// Not using l10n since USER and ERR are <= 4 characters, and
	// keeing this disentangled from l10n makes testing easier.
	if (scale == NUM_PRESET_SCALES) {
		return "USER";
	}
	else if (scale < NUM_PRESET_SCALES) {
		return presetScaleNames[scale];
	}
	else {
		return "ERR";
	}
}