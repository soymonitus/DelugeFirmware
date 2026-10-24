/*
 * Copyright © 2026 Synthstrom Audible Limited
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Firmware is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "dsp/filter/formant_core.hpp"
#include "gui/menu_item/integer.h"
#include "gui/ui/sound_editor.h"
#include "model/instrument/kit.h"
#include "model/mod_controllable/mod_controllable_audio.h"
#include "model/song/song.h"
#include "processing/sound/sound_drum.h"

namespace deluge::gui::menu_item::filter {

/// Vowel filter "Raw": how much of the source's own highs comes through unshaped on top of the formants - 0 is the
/// clean filter, 25 the buzzy character of its first version, 50 twice that. Only there in the Vowel LPF mode.
class VowelRaw final : public IntegerWithOff {
public:
	using IntegerWithOff::IntegerWithOff;

	void readCurrentValue() override { this->setValue(soundEditor.currentModControllable->vowelRaw); }
	bool usesAffectEntire() override { return true; }
	void writeCurrentValue() override {
		const auto value = static_cast<uint8_t>(this->getValue());
		if (currentUIMode == UI_MODE_HOLDING_AFFECT_ENTIRE_IN_SOUND_EDITOR && soundEditor.editingKitRow()) {
			Kit* kit = getCurrentKit();
			for (Drum* thisDrum = kit->firstDrum; thisDrum != nullptr; thisDrum = thisDrum->next) {
				if (thisDrum->type == DrumType::SOUND) {
					static_cast<SoundDrum*>(thisDrum)->vowelRaw = value;
				}
			}
		}
		else {
			soundEditor.currentModControllable->vowelRaw = value;
		}
	}
	[[nodiscard]] int32_t getMaxValue() const override { return dsp::filter::formant::kRawMax; }
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return BAR; }
	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return modControllable != nullptr && modControllable->lpfMode == ::FilterMode::VOWEL;
	}
};

} // namespace deluge::gui::menu_item::filter
