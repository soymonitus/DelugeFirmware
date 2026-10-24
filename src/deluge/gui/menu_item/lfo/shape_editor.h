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
#include "gui/l10n/l10n.h"
#include "gui/menu_item/menu_item.h"
#include "gui/ui/lfo_shape_editor.h"
#include "gui/ui/sound_editor.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "processing/sound/sound.h"

namespace deluge::gui::menu_item::lfo {

/// "Draw": opens the shape editor of a CUSTOM LFO. The horizontal menu shows the shape itself.
class ShapeEditor final : public MenuItem {
public:
	ShapeEditor(l10n::String name, uint8_t lfoId) : MenuItem(name), lfoId_(lfoId) {}

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		const auto* sound = static_cast<Sound*>(modControllable);
		return sound->lfoConfig[lfoId_].waveType == LFOType::CUSTOM;
	}

	void beginSession(MenuItem* navigatedBackwardFrom) override {
		soundEditor.shouldGoUpOneLevelOnBegin = true; // Once the editor closes
		Sound* sound = soundEditor.currentSound;
		if (!sound->lfoConfig[lfoId_].ensureShape() || !lfoShapeEditor.openFor(sound, lfoId_)) {
			display->displayPopup(l10n::get(l10n::String::STRING_FOR_ERROR_INSUFFICIENT_RAM));
			// No editor to come back from, so back out of this item now
			soundEditor.shouldGoUpOneLevelOnBegin = false;
			if (getCurrentUI() == &soundEditor) {
				soundEditor.goUpOneLevel();
			}
		}
	}

	MenuItem* selectButtonPress() override {
		beginSession(nullptr);
		return NO_NAVIGATION;
	}

	[[nodiscard]] bool allowToBeginSessionFromHorizontalMenu() override { return true; }

	void renderInHorizontalMenu(const SlotPosition& slot) override {
		const LFOConfig& config = soundEditor.currentSound->lfoConfig[lfoId_];
		if (config.shape == nullptr) {
			return;
		}
		LFOShapeEditor::drawShape(hid::display::OLED::main, *config.shape, slot.start_x + 4,
		                          slot.start_y + kHorizontalMenuSlotYOffset, slot.start_x + slot.width - 5,
		                          slot.start_y + slot.height - 5);
	}

private:
	uint8_t lfoId_;
};

} // namespace deluge::gui::menu_item::lfo
