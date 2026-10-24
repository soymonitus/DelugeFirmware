/*
 * Copyright (c) 2014-2023 Synthstrom Audible Limited
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
#include "gui/menu_item/sync_level.h"
#include "gui/ui/sound_editor.h"
#include "hid/display/oled.h"
#include "model/drum/drum.h"
#include "model/instrument/kit.h"
#include "model/song/song.h"
#include "processing/sound/sound.h"
#include "processing/sound/sound_drum.h"

namespace deluge::gui::menu_item::lfo {

/// LFO sync rate. On top of the shared sync values there's one more, longest one: twice the longest regular level -
/// 8 bars where that one is 4 (at the default song resolution). It's menu value 1; the regular values follow it.
class Sync final : public SyncLevel, FormattedTitle {
public:
	Sync(l10n::String name, l10n::String title, uint8_t lfoId)
	    : SyncLevel(name, title), FormattedTitle(title, lfoId + 1), lfoId_(lfoId) {}

	[[nodiscard]] std::string_view getTitle() const override { return FormattedTitle::title(); }

	size_t size() override { return NUM_SYNC_VALUES + 1; }

	void readCurrentValue() override {
		const LFOConfig& config = soundEditor.currentSound->lfoConfig[lfoId_];
		if (config.syncDoubled && config.syncLevel == SYNC_LEVEL_WHOLE && config.syncType == SYNC_TYPE_EVEN) {
			this->setValue(kDoubledWhole);
			return;
		}
		const int32_t regular = syncTypeAndLevelToMenuOption(config.syncType, config.syncLevel);
		this->setValue(regular == 0 ? 0 : regular + 1);
	}
	bool usesAffectEntire() override { return true; }
	void writeCurrentValue() override {
		int32_t current_value = this->getValue();
		// If affect-entire button held, do whole kit
		if (currentUIMode == UI_MODE_HOLDING_AFFECT_ENTIRE_IN_SOUND_EDITOR && soundEditor.editingKitRow()) {

			Kit* kit = getCurrentKit();

			for (Drum* thisDrum = kit->firstDrum; thisDrum != nullptr; thisDrum = thisDrum->next) {
				if (thisDrum->type == DrumType::SOUND) {
					auto* soundDrum = static_cast<SoundDrum*>(thisDrum);
					apply(current_value, soundDrum->lfoConfig[lfoId_]);
					// This fires unnecessarily for LFO2 assignments as well, but that's ok. It's not
					// entirely clear if we really need this for the LFO1, even: maybe the clock-driven resyncs
					// would be enough?
					soundDrum->resyncGlobalLFOs();
					soundDrum->setupPatchingForAllParamManagers(currentSong);
				}
			}
		}
		// Or, the normal case of just one sound
		else {
			apply(current_value, soundEditor.currentSound->lfoConfig[lfoId_]);
			// This fires unnecessarily for LFO2 assignments as well, but that's ok. It's not
			// entirely clear if we really need this for the LFO1, even: maybe the clock-driven resyncs
			// would be enough?
			soundEditor.currentSound->resyncGlobalLFOs();
			soundEditor.currentSound->setupPatchingForAllParamManagers(currentSong);
		}
	}

protected:
	void getNoteLengthName(StringBuf& buffer) override {
		const int32_t value = this->getValue();
		if (value == kDoubledWhole) {
			// One magnitude coarser shows the doubled length (8 bars instead of 4)
			syncValueToString(SYNC_LEVEL_WHOLE, buffer, currentSong->getInputTickMagnitude() - 1);
			return;
		}
		syncValueToString(toRegular(value), buffer, currentSong->getInputTickMagnitude());
	}

	void getColumnLabel(StringBuf& label) override {
		const int32_t value = this->getValue();
		if (value == 0) {
			return SyncLevel::getColumnLabel(label); // OFF
		}
		if (value == kDoubledWhole) {
			syncValueToStringForHorzMenuLabel(SYNC_TYPE_EVEN, SYNC_LEVEL_WHOLE, label,
			                                  currentSong->getInputTickMagnitude() - 1);
			return;
		}
		const int32_t regular = toRegular(value);
		syncValueToStringForHorzMenuLabel(syncValueToSyncType(regular), syncValueToSyncLevel(regular), label,
		                                  currentSong->getInputTickMagnitude());
	}

	void renderInHorizontalMenu(const SlotPosition& slot) override {
		using namespace deluge::hid::display;
		oled_canvas::Canvas& image = OLED::main;
		const int32_t value = this->getValue();
		if (value == 0) {
			const auto off_string = l10n::get(l10n::String::STRING_FOR_OFF);
			return image.drawStringCentered(off_string, slot.start_x, slot.start_y + kHorizontalMenuSlotYOffset,
			                                kTextSpacingX, kTextSpacingY, slot.width);
		}
		const SyncType type = (value == kDoubledWhole) ? SYNC_TYPE_EVEN : syncValueToSyncType(toRegular(value));
		const Icon& type_icon = (type == SYNC_TYPE_DOTTED)    ? OLED::syncTypeDottedIcon
		                        : (type == SYNC_TYPE_TRIPLET) ? OLED::syncTypeTripletsIcon
		                                                      : OLED::syncTypeEvenIcon;
		image.drawIconCentered(type_icon, slot.start_x, slot.width, slot.start_y + kHorizontalMenuSlotYOffset - 3);
	}

private:
	static constexpr int32_t kDoubledWhole = 1;

	/// The shared sync value for a menu value other than kDoubledWhole
	static int32_t toRegular(int32_t value) { return value <= kDoubledWhole ? 0 : value - 1; }

	static void apply(int32_t value, LFOConfig& config) {
		if (value == kDoubledWhole) {
			config.syncType = SYNC_TYPE_EVEN;
			config.syncLevel = SYNC_LEVEL_WHOLE;
			config.syncDoubled = true;
			return;
		}
		const int32_t regular = toRegular(value);
		config.syncType = syncValueToSyncType(regular);
		config.syncLevel = syncValueToSyncLevel(regular);
		config.syncDoubled = false;
	}

	uint8_t lfoId_;
};

} // namespace deluge::gui::menu_item::lfo
