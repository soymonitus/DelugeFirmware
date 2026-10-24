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

#include "gui/menu_item/integer.h"
#include "gui/menu_item/patched_param/integer.h"
#include "gui/menu_item/value_scaling.h"
#include "gui/ui/sound_editor.h"
#include "model/instrument/kit.h"
#include "model/mod_controllable/mod_controllable_audio.h"
#include "model/song/song.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "processing/sound/sound.h"
#include "processing/sound/sound_drum.h"
#include <cstdint>

namespace deluge::gui::menu_item::fx {

/// Wavefold Symmetry: -25..+25, 0 = symmetric. A regular patched param (LOCAL_FOLD_SYMMETRY), so it
/// can be modulated from the mod matrix, e.g. LFO -> Symmetry. Same value mapping as Pan.
class WavefoldSymmetry final : public patched_param::Integer {
public:
	using patched_param::Integer::Integer;

protected:
	[[nodiscard]] int32_t getMaxValue() const override { return kMaxMenuRelativeValue; }
	[[nodiscard]] int32_t getMinValue() const override { return kMinMenuRelativeValue; }
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return PAN; }
	int32_t getFinalValue() override { return computeFinalValueForPan(this->getValue()); }
	void readCurrentValue() override {
		this->setValue(
		    computeCurrentValueForPan(soundEditor.currentParamManager->getPatchedParamSet()->getValue(getP())));
	}
};

/// Wavefold Tone: clean-lowend split before the folder (same crossover as the saturation's Tone), so
/// only the upper part of the signal gets folded. 0 = off.
class WavefoldTone final : public IntegerWithOff {
public:
	using IntegerWithOff::IntegerWithOff;

	void readCurrentValue() override { this->setValue(soundEditor.currentModControllable->foldTone); }
	bool usesAffectEntire() override { return true; }
	void writeCurrentValue() override {
		int32_t current_value = this->getValue();

		if (currentUIMode == UI_MODE_HOLDING_AFFECT_ENTIRE_IN_SOUND_EDITOR && soundEditor.editingKitRow()) {
			Kit* kit = getCurrentKit();
			for (Drum* thisDrum = kit->firstDrum; thisDrum != nullptr; thisDrum = thisDrum->next) {
				if (thisDrum->type == DrumType::SOUND) {
					auto* soundDrum = static_cast<SoundDrum*>(thisDrum);
					soundDrum->foldTone = current_value;
				}
			}
		}
		else {
			soundEditor.currentModControllable->foldTone = current_value;
		}
	}
	[[nodiscard]] int32_t getMaxValue() const override { return kMaxMenuValue; }
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return BAR; }
};

} // namespace deluge::gui::menu_item::fx
