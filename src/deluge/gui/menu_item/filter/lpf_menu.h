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

#include "gui/menu_item/horizontal_menu.h"
#include "gui/ui/sound_editor.h"
#include "model/mod_controllable/mod_controllable_audio.h"
#include <algorithm>

namespace deluge::gui::menu_item::filter {

/// The LPF's horizontal menu: Mode, Frequency, Resonance, Morph - and in the Vowel mode a fifth item, Raw, on a second
/// page. There, CV (the Morph / Formant shift slot) pressed again goes on to Raw, and from Raw back to Formant shift.
/// The items must come in that order.
class LpfMenu final : public HorizontalMenu {
public:
	using HorizontalMenu::HorizontalMenu;

protected:
	void handleInstrumentButtonPress(std::span<MenuItem*> visible_page_items, const MenuItem* previous,
	                                 int32_t pressed_button_position) override {
		constexpr int32_t kCvPosition = 3;
		if (pressed_button_position == kCvPosition && items.size() >= 5 && isVowel()) {
			MenuItem* morph = items[3];
			MenuItem* raw = items[4];
			if (previous == morph || previous == raw) {
				focus(previous == morph ? raw : morph);
				return;
			}
		}
		HorizontalMenu::handleInstrumentButtonPress(visible_page_items, previous, pressed_button_position);
	}

private:
	[[nodiscard]] static bool isVowel() {
		return soundEditor.currentModControllable != nullptr
		       && soundEditor.currentModControllable->lpfMode == ::FilterMode::VOWEL;
	}

	void focus(MenuItem* item) {
		current_item_ = std::ranges::find(items, item);
		initializeItem(item);
		updateDisplay();
		updatePadLights();
		displayNotification(item);
	}
};

} // namespace deluge::gui::menu_item::filter
