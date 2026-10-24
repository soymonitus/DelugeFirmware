#include "modulation/lfo.h"
#include "util/exceptions.h"

LFOConfig& LFOConfig::operator=(const LFOConfig& other) {
	if (this == &other) {
		return *this;
	}
	waveType = other.waveType;
	syncType = other.syncType;
	syncLevel = other.syncLevel;
	syncDoubled = other.syncDoubled;
	if (other.shape == nullptr) {
		delete shape;
		shape = nullptr;
	}
	else if (shape != nullptr || ensureShape()) {
		*shape = *other.shape;
	}
	return *this;
}

bool LFOConfig::ensureShape() {
	if (shape == nullptr) {
		try {
			shape = new deluge::modulation::LFOShape();
		} catch (deluge::exception e) { // Out of memory - the LFO renders a triangle meanwhile
			return false;
		}
	}
	return true;
}

uint32_t LFOConfig::syncedTicksPerPeriod() const {
	uint32_t ticks = uint32_t{3} << (SYNC_LEVEL_256TH - syncLevel);
	if (syncDoubled) {
		ticks <<= 1;
	}
	switch (syncType) {
	case SYNC_TYPE_EVEN:
		break;
	case SYNC_TYPE_TRIPLET:
		ticks = ticks * 2 / 3;
		break;
	case SYNC_TYPE_DOTTED:
		ticks = ticks * 3 / 2;
		break;
	}
	return ticks;
}

uint32_t getLFOInitialPhaseForNegativeExtreme(LFOType waveType) {
	switch (waveType) {
	case LFOType::SAW:
		return 2147483648u;

	case LFOType::SINE:
		return 3221225472u;

	default:
		// This is actually positive extreme for square wave.
		// It is not entirely clear if that should be changed.
		return 0;
	}
}

uint32_t getLFOInitialPhaseForZero(LFOType waveType) {
	switch (waveType) {
	case LFOType::TRIANGLE:
		return 1073741824;

	default:
		return 0;
	}
}

// LFO2 always starts from the negative extreme (it triggers per note-on), but LFO1
// has historically waveform-specific behaviour when synced.

void LFO::setLocalInitialPhase(const LFOConfig& config) {
	phase = getLFOInitialPhaseForNegativeExtreme(config.waveType);
	holdValue = 0;
	speed = 0;
	target = 0;
}

void LFO::setGlobalInitialPhase(const LFOConfig& config, bool reset_state) {
	if (config.waveType == LFOType::SINE || config.waveType == LFOType::TRIANGLE) {
		phase = getLFOInitialPhaseForZero(config.waveType);
	}
	else {
		phase = getLFOInitialPhaseForNegativeExtreme(config.waveType);
	}
	// Clock resynchronization calls this repeatedly during playback. Only align
	// the phase: random waveforms must retain their held value and filter state.
	// Playback start explicitly resets state; construction also initializes it.
	if (reset_state) {
		holdValue = 0;
		speed = 0;
		target = 0;
	}
}
