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

#include "dsp/filter/formant.h"

namespace deluge::dsp::filter {

namespace {
constexpr float kQ31ToFloat = 1.0f / 2147483648.0f;
constexpr float kFloatToQ31 = 2147483520.0f; // Largest float below 2^31: full scale without overflowing

/// Resonance peaks above full scale bend smoothly instead of clipping (see formant::softLimit())
[[gnu::always_inline]] inline q31_t toQ31(float value) {
	return static_cast<q31_t>(std::clamp(formant::softLimit(value), -1.0f, 1.0f) * kFloatToQ31);
}
} // namespace

float FormantFilter::positionForFrequency(q31_t frequency) {
	if (frequency <= 0) {
		return 0.0f;
	}
	constexpr float kLog2Neutral = 20.931568569f; // log2(2000000), the LPF frequency param's neutral value
	constexpr float kKnobOctaves = 11.2f;         // getParamRange(LOCAL_LPF_FREQ) = 1.4 * 2^29: +-5.6 octaves
	return (formant::fastLog2(static_cast<float>(frequency)) - kLog2Neutral) / kKnobOctaves + 0.5f;
}

q31_t FormantFilter::setConfig(q31_t frequency, q31_t resonance, FilterMode lpfMode, q31_t lpfMorph, q31_t filterGain) {
	(void)lpfMode;
	const float position = positionForFrequency(frequency);
	// Raw resonance runs 0..2^29 (see SVFilter::setConfig)
	const float sharpness = static_cast<float>(resonance) * (1.0f / 536870912.0f);
	// Morph arrives as q31 (configure() shifted it), 1 = an octave: the knob goes up to an octave higher, and on voices
	// modulation adds on both ways (see Voice::render()), so it can go lower too
	const float shiftOctaves = static_cast<float>(lpfMorph) * kQ31ToFloat;

	const formant::Shape shape = formant::shapeAt(position, shiftOctaves, sharpness);
	const formant::Coefficients target = formant::coefficientsFor(shape, shape.level, rawGain_);
	from_ = primed_ ? to_ : target;
	to_ = target;
	primed_ = true;
	return filterGain;
}

template <bool kStereo>
void FormantFilter::process(q31_t* startSample, q31_t* endSample, int32_t stride) {
	// The raw band only runs while Raw is up (or ramping to / from 0) - otherwise it costs nothing
	const bool raw = (from_.pb0 != 0.0f) || (to_.pb0 != 0.0f);
	if (raw && !rawWasOn_) {
		left_.clearRaw();
		right_.clearRaw();
	}
	rawWasOn_ = raw;
	if (raw) {
		process<kStereo, true>(startSample, endSample, stride);
	}
	else {
		process<kStereo, false>(startSample, endSample, stride);
	}
}

template <bool kStereo, bool kRaw>
[[gnu::hot]] void FormantFilter::process(q31_t* startSample, q31_t* endSample, int32_t stride) {
	const int32_t numFrames = static_cast<int32_t>((endSample - startSample + stride - 1) / stride);
	if (numFrames <= 0) {
		return;
	}

	// Per-sample coefficient ramp from the previous buffer's values
	formant::Coefficients co = from_;
	const formant::Coefficients step = formant::rampStep(from_, to_, numFrames);

	formant::Channel left = left_;
	formant::Channel right = right_;
	q31_t* sample = startSample;
	for (int32_t n = 0; n < numFrames; n++) {
		formant::advance(co, step);
		sample[0] = toQ31(left.process<kRaw>(static_cast<float>(sample[0]) * kQ31ToFloat, co));
		if constexpr (kStereo) {
			sample[1] = toQ31(right.process<kRaw>(static_cast<float>(sample[1]) * kQ31ToFloat, co));
		}
		sample += stride;
	}

	left.tidy();
	left_ = left;
	if constexpr (kStereo) {
		right.tidy();
		right_ = right;
	}
}

[[gnu::hot]] void FormantFilter::doFilter(q31_t* startSample, q31_t* endSample, int32_t sampleIncrement) {
	process<false>(startSample, endSample, sampleIncrement);
}

[[gnu::hot]] void FormantFilter::doFilterStereo(q31_t* startSample, q31_t* endSample) {
	process<true>(startSample, endSample, 2);
}

} // namespace deluge::dsp::filter
