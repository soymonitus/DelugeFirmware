/*
 * Copyright (c) 2025 Leonid Burygin
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

#include "gui/menu_item/horizontal_menu_container.h"
#include "hid/led/indicator_leds.h"
#include "param.h"
#include <complex>

namespace deluge::gui::menu_item::filter {

using namespace deluge::hid::display;

class FilterContainer final : public HorizontalMenuContainer {
public:
	FilterContainer(std::initializer_list<MenuItem*> items, FilterParam* morph_item)
	    : HorizontalMenuContainer(items), morph_item_{morph_item} {}
	FilterContainer(std::initializer_list<MenuItem*> items, UnpatchedFilterParam* morph_item)
	    : HorizontalMenuContainer(items), morph_item_unpatched_{morph_item} {}

	void render(const SlotPosition& slots, const MenuItem* selected_item, HorizontalMenu* parent) override {
		oled_canvas::Canvas& image = OLED::main;

		const auto [freq_raw, reso_raw, morph_raw, is_morphable, is_hpf] = getFilterValues();
		if (!is_hpf && isVowel()) {
			return renderVowel(slots, freq_raw, reso_raw, morph_raw);
		}
		const float freq_value = freq_raw / 50.f;
		const float reso_value = sigmoidLikeCurve(reso_raw, 50.f, 30.f);
		const float morph_value = [&] {
			float result = 0.f;
			if (is_morphable) {
				result = morph_raw / 50.f;
			}
			if (is_hpf) {
				result = 1.0f - result; // treat HPF as fully morphed LPF visually
			}
			return result;
		}();

		constexpr uint8_t reso_segment_width = 6;
		constexpr uint8_t freq_slope_width = 5;
		constexpr uint8_t padding_x = 5;
		const uint8_t total_width = slots.width - 2 - padding_x * 2;
		const uint8_t base_width = total_width - freq_slope_width - reso_segment_width;

		uint8_t min_x = slots.start_x + padding_x;
		uint8_t max_x = min_x + total_width;
		uint8_t reso_x0 = min_x - reso_segment_width + base_width * freq_value;
		uint8_t reso_x1 = reso_x0 + reso_segment_width;
		uint8_t reso_x2 = reso_x1 + reso_segment_width;
		int8_t left_slope_x0 = reso_x0 - base_width - freq_slope_width;
		int8_t left_slope_x1 = left_slope_x0 + freq_slope_width;
		uint8_t right_slope_x0 = reso_x2;
		uint8_t right_slope_x1 = right_slope_x0 + freq_slope_width;

		if (morph_value > 0) {
			const uint8_t slope_shift = std::lerp(0, total_width, morph_value);
			const uint8_t reso_shift = std::lerp(0, freq_slope_width + reso_segment_width, morph_value);
			left_slope_x0 += slope_shift;
			left_slope_x1 += slope_shift;
			right_slope_x0 += slope_shift;
			right_slope_x1 += slope_shift;
			reso_x0 += reso_shift;
			reso_x1 += reso_shift;
			reso_x2 += reso_shift;
		}

		constexpr uint8_t container_height = 23;
		const uint8_t start_y = slots.start_y + 1;
		const uint8_t end_y = start_y + container_height - 1;
		const uint8_t center_y = start_y + (end_y - start_y) / 2;
		const uint8_t reso_y = std::lerp(center_y, start_y, reso_value);

		// Outline
		image.drawRectangleRounded(min_x - 1, start_y - 1, max_x + 1, end_y + 1);

		auto draw_segment = [&](int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
			oled_canvas::Point last_point{-1, -1};
			auto draw_fill_pattern = [&](oled_canvas::Point point) {
				if (last_point.x != point.x && point.x % 3 == 0) {
					for (int32_t y = point.y; y <= end_y; y++) {
						if (y % 3 == 1) {
							image.drawPixel(point.x, y);
						}
					}
				}
				last_point = point;
			};
			image.drawLine(x0, y0, x1, y1, {.min_x = min_x, .max_x = max_x, .point_callback = draw_fill_pattern});
		};

		// Left slope
		draw_segment(left_slope_x0, end_y, left_slope_x1, center_y);

		// Body
		draw_segment(left_slope_x1, center_y, reso_x0, center_y);

		// Resonance
		draw_segment(reso_x0, center_y, reso_x1, reso_y);
		draw_segment(reso_x1, reso_y, reso_x2, center_y);
		draw_segment(reso_x2, center_y, right_slope_x0, center_y);

		// Right slope
		draw_segment(right_slope_x0, center_y, right_slope_x1, end_y);
	};

private:
	FilterParam* morph_item_{nullptr};
	UnpatchedFilterParam* morph_item_unpatched_{nullptr};

	[[nodiscard]] bool isVowel() const {
		const FilterInfo& info =
		    (morph_item_ != nullptr) ? morph_item_->getFilterInfo() : morph_item_unpatched_->getFilterInfo();
		return info.getSlot() == FilterSlot::LPF && info.getMode() == ::FilterMode::VOWEL;
	}

	/// Vowel mode: draws the filter's actual frequency response (100 Hz - 8 kHz, log scale), so the formants can be
	/// seen moving with the knobs
	void renderVowel(const SlotPosition& slots, int32_t freq_raw, int32_t reso_raw, int32_t morph_raw) const {
		namespace formant = dsp::filter::formant;
		oled_canvas::Canvas& image = OLED::main;

		const formant::Shape shape = formant::shapeAt(freq_raw / 50.f, morph_raw / 50.f, reso_raw / 50.f);
		const int32_t raw =
		    (soundEditor.currentModControllable != nullptr) ? soundEditor.currentModControllable->vowelRaw : 0;
		const formant::Coefficients co = formant::coefficientsFor(shape, shape.level, formant::rawGainFor(raw));

		constexpr uint8_t padding_x = 5;
		constexpr uint8_t container_height = 23;
		const int32_t min_x = slots.start_x + padding_x;
		const int32_t max_x = min_x + slots.width - 2 - padding_x * 2;
		const int32_t start_y = slots.start_y + 1;
		const int32_t end_y = start_y + container_height - 1;
		image.drawRectangleRounded(min_x - 1, start_y - 1, max_x + 1, end_y + 1);

		constexpr float kLowHz = 100.0f;
		constexpr float kHighHz = 8000.0f;
		constexpr float kTopDb = 24.0f;
		constexpr float kBottomDb = -30.0f;
		constexpr float kTwoPi = 6.28318530718f;
		int32_t prev_y = -1;
		for (int32_t x = min_x; x <= max_x; x++) {
			const float hz = kLowHz * std::pow(kHighHz / kLowHz, static_cast<float>(x - min_x) / (max_x - min_x));
			const std::complex<float> z1 = std::polar(1.0f, -kTwoPi * hz / formant::kSampleRate);
			std::complex<float> response = 1.0f;
			for (int32_t k = 0; k < formant::kNumFormants; k++) {
				response *= co.a[k] / (1.0f - co.b[k] * z1 - co.c[k] * z1 * z1);
			}
			// Raw runs in parallel to the cascade (pb0 is 0 while it's off)
			response += co.pb0 * (1.0f - z1 * z1) / (1.0f + co.pa1 * z1 + co.pa2 * z1 * z1);
			const float db = 20.0f * std::log10(std::max(std::abs(response), 1.0e-6f));
			const float height = std::clamp((db - kBottomDb) / (kTopDb - kBottomDb), 0.0f, 1.0f);
			const int32_t y = end_y - static_cast<int32_t>(std::lround(height * (end_y - start_y)));
			if (prev_y >= 0) {
				image.drawLine(x - 1, prev_y, x, y);
			}
			// Same dotted fill under the curve as the other filter modes
			if (x % 3 == 0) {
				for (int32_t fill_y = y; fill_y <= end_y; fill_y++) {
					if (fill_y % 3 == 1) {
						image.drawPixel(x, fill_y);
					}
				}
			}
			prev_y = y;
		}
	}

	struct FilterValues {
		int32_t freq_value{0};
		int32_t reso_value{0};
		int32_t morph_value{0};
		bool is_morphable{false};
		bool is_hpf{false};
	};

	[[nodiscard]] FilterValues getFilterValues() const {
		if (morph_item_ != nullptr) {
			// Get from patched params
			auto freq_item = static_cast<FilterParam*>(items_[0]);
			auto reso_item = static_cast<FilterParam*>(items_[1]);
			bool is_morphable = morph_item_->getFilterInfo().isMorphable();
			bool is_hpf = freq_item->getP() == params::LOCAL_HPF_FREQ;
			return {freq_item->getValue(), reso_item->getValue(), morph_item_->getValue(), is_morphable, is_hpf};
		}

		// Get from unpatched params
		auto freq_item = static_cast<UnpatchedFilterParam*>(items_[0]);
		auto reso_item = static_cast<UnpatchedFilterParam*>(items_[1]);
		bool is_morphable = morph_item_unpatched_->getFilterInfo().isMorphable();
		bool is_hpf = freq_item->getP() == params::UNPATCHED_HPF_FREQ;
		return {freq_item->getValue(), reso_item->getValue(), morph_item_unpatched_->getValue(), is_morphable, is_hpf};
	}
};
} // namespace deluge::gui::menu_item::filter