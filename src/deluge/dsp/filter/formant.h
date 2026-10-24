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

#include "dsp/filter/filter.h"
#include "dsp/filter/formant_core.hpp"

namespace deluge::dsp::filter {

/// The Vowel LPF mode (see formant_core.hpp): cutoff walks the vowels A..U, resonance sharpens the formants, morph
/// shifts them up. Coefficients ramp per sample from the previous buffer's, so modulating the vowel never steps.
class FormantFilter : public Filter<FormantFilter> {
public:
	FormantFilter() = default;
	// Returns filterGain unchanged: the vowel's level trim is applied inside, so the output is about as loud as the
	// input in every context (voices apply the returned gain before the filter, kits and audio tracks after it)
	q31_t setConfig(q31_t frequency, q31_t resonance, FilterMode lpfMode, q31_t lpfMorph, q31_t filterGain);
	void doFilter(q31_t* startSample, q31_t* endSample, int32_t sampleIncrement);
	void doFilterStereo(q31_t* startSample, q31_t* endSample);
	void resetFilter() {
		left_ = {};
		right_ = {};
		primed_ = false;
		rawWasOn_ = false;
	}
	/// Raw 0..50 (see formant::kRawMax), set before setConfig() - it ramps in with the coefficients
	void setRaw(int32_t raw) { rawGain_ = formant::rawGainFor(raw); }

	/// Where on the vowel path a final LPF frequency param value sits: 0 = A ... 1 = U. The preset knob spans +-5.6
	/// octaves around the param's neutral value (getParamRange()), so knob 0..50 maps onto the whole path.
	static float positionForFrequency(q31_t frequency);

private:
	template <bool kStereo, bool kRaw>
	void process(q31_t* startSample, q31_t* endSample, int32_t stride);
	template <bool kStereo>
	void process(q31_t* startSample, q31_t* endSample, int32_t stride);

	formant::Coefficients from_; ///< The previous buffer's coefficients: this buffer ramps from these...
	formant::Coefficients to_;   ///< ...to these
	formant::Channel left_;
	formant::Channel right_;
	float rawGain_; ///< See setRaw()
	bool primed_;   ///< from_ is valid (false after a reset: the first buffer starts at its own target)
	bool rawWasOn_; ///< The raw band ran last buffer (its memory is current)
};

} // namespace deluge::dsp::filter
