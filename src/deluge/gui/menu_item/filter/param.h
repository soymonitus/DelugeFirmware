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

#include "dsp/filter/formant_core.hpp"
#include "gui/menu_item/filter/info.h"
#include "gui/menu_item/horizontal_menu.h"
#include "gui/menu_item/patched_param/integer_non_fm.h"
#include "gui/menu_item/unpatched_param.h"
#include "gui/menu_item/value_scaling.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "modulation/params/param.h"

#include <util/comparison.h>

namespace deluge::gui::menu_item::filter {

using namespace deluge::modulation;
using namespace hid::display;

/// The LPF cutoff in Vowel mode walks the vowels, so it shows the nearest vowel instead of a number
[[nodiscard]] inline bool showsVowel(const FilterInfo& info) {
	return info.getFilterParamType() == FilterParamType::FREQUENCY && info.getSlot() == FilterSlot::LPF
	       && info.getMode() == ::FilterMode::VOWEL;
}

[[nodiscard]] inline const char* vowelForKnob(int32_t knobValue) {
	return dsp::filter::formant::kVowelNames[dsp::filter::formant::nearestVowelForKnob(knobValue)];
}

inline void drawVowelInSlot(const SlotPosition& slot, int32_t knobValue) {
	OLED::main.drawStringCentered(vowelForKnob(knobValue), slot.start_x, slot.start_y + kHorizontalMenuSlotYOffset + 5,
	                              kTextTitleSpacingX, kTextTitleSizeY, slot.width);
}

class FilterParam : public patched_param::Integer {
public:
	FilterParam(l10n::String newName, int32_t newP, FilterSlot slot_, FilterParamType type_)
	    : Integer{newName, newP}, info{slot_, type_} {}
	FilterParam(l10n::String newName, l10n::String title, int32_t newP, FilterSlot slot_, FilterParamType type_)
	    : Integer{newName, title, newP}, info{slot_, type_} {}

	[[nodiscard]] std::string_view getName() const override { return info.getMorphNameOr(Integer::getName()); }
	[[nodiscard]] std::string_view getTitle() const override { return info.getMorphNameOr(Integer::getTitle()); }
	[[nodiscard]] bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return info.isOn();
	}
	[[nodiscard]] FilterInfo const& getFilterInfo() const { return info; }

	void getColumnLabel(StringBuf& label) override { label.append(info.getMorphNameOr(Integer::getName(), true)); }

	void renderInHorizontalMenu(const SlotPosition& slot) override {
		if (showsVowel(info)) {
			return drawVowelInSlot(slot, getValue());
		}
		if (info.getFilterParamType() == FilterParamType::MORPH && info.isMorphable()) {
			int32_t value = getValue();
			if (info.getSlot() == FilterSlot::HPF) {
				// Treat HPF as fully morphed LPF visually
				value = 50 - value;
			}
			drawSlider(slot, value);
		}
		else {
			drawBar(slot);
		}
	}

	void selectEncoderAction(int32_t offset) override {
		if (parent != nullptr && parent->renderingStyle() == Submenu::RenderingStyle::HORIZONTAL
		    && info.getFilterParamType() == FilterParamType::MORPH && info.isMorphable()
		    && info.getSlot() == FilterSlot::HPF) {
			// Treat HPF as fully morphed LPF visually, reverse direction
			offset *= -1;
		}
		Integer::selectEncoderAction(offset);
	}

	// 7SEG
	void drawValue() override {
		if (showsVowel(info)) {
			return display->setText(vowelForKnob(getValue()));
		}
		Integer::drawValue();
	}

protected:
	void drawInteger(int32_t textWidth, int32_t textHeight, int32_t yPixel) override {
		if (showsVowel(info)) {
			return OLED::main.drawStringCentred(vowelForKnob(getValue()), yPixel + OLED_MAIN_TOPMOST_PIXEL, textWidth,
			                                    textHeight);
		}
		Integer::drawInteger(textWidth, textHeight, yPixel);
	}

private:
	FilterInfo info;
};

class UnpatchedFilterParam final : public UnpatchedParam {
public:
	UnpatchedFilterParam(l10n::String newName, l10n::String title, int32_t newP, FilterSlot slot_,
	                     FilterParamType type_)
	    : UnpatchedParam{newName, title, newP}, info{slot_, type_} {}

	[[nodiscard]] std::string_view getName() const override { return info.getMorphNameOr(UnpatchedParam::getName()); }
	[[nodiscard]] std::string_view getTitle() const override { return info.getMorphNameOr(UnpatchedParam::getTitle()); }
	[[nodiscard]] bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return info.isOn();
	}
	void getColumnLabel(StringBuf& label) override { label.append(info.getMorphNameOr(Integer::getName(), true)); }
	[[nodiscard]] FilterInfo const& getFilterInfo() const { return info; }

	void renderInHorizontalMenu(const SlotPosition& slot) override {
		if (showsVowel(info)) {
			return drawVowelInSlot(slot, getValue());
		}
		if (info.getFilterParamType() == FilterParamType::MORPH && info.isMorphable()) {
			int32_t value = getValue();
			if (info.getSlot() == FilterSlot::HPF) {
				// Treat HPF as fully morphed LPF visually
				value = 50 - value;
			}
			drawSlider(slot, value);
		}
		else {
			drawBar(slot);
		}
	}
	void selectEncoderAction(int32_t offset) override {
		if (parent != nullptr && parent->renderingStyle() == Submenu::RenderingStyle::HORIZONTAL
		    && info.getFilterParamType() == FilterParamType::MORPH && info.isMorphable()
		    && info.getSlot() == FilterSlot::HPF) {
			// Treat HPF as fully morphed LPF visually, reverse direction
			offset *= -1;
		}
		Integer::selectEncoderAction(offset);
	}

	// 7SEG
	void drawValue() override {
		if (showsVowel(info)) {
			return display->setText(vowelForKnob(getValue()));
		}
		UnpatchedParam::drawValue();
	}

protected:
	void drawInteger(int32_t textWidth, int32_t textHeight, int32_t yPixel) override {
		if (showsVowel(info)) {
			return OLED::main.drawStringCentred(vowelForKnob(getValue()), yPixel + OLED_MAIN_TOPMOST_PIXEL, textWidth,
			                                    textHeight);
		}
		UnpatchedParam::drawInteger(textWidth, textHeight, yPixel);
	}

private:
	FilterInfo info;
};

} // namespace deluge::gui::menu_item::filter
