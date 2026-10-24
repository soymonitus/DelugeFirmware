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

#include "dsp/saturation.hpp"
#include "dsp/util.hpp"
#include "util/fixedpoint.h"
#include <cstdint>

namespace deluge::dsp {

/// Per-channel running state for foldBufferWithSymmetryAndTone(). Lives on the Voice next to its
/// other per-voice DSP state and is reset at note-on.
struct WavefoldChannelState {
	q31_t toneLowpass = 0; ///< Tone's clean-lowend crossover state (see saturationCleanLowEnd())
	q31_t dcTracker = 0;   ///< Slow average of the folded output, subtracted to remove Symmetry's DC offset
};

/// ~10Hz one-pole at 44.1kHz (2*pi*10/44100): slow enough to leave every audible frequency alone,
/// fast enough to pull out the DC offset that asymmetric (biased) folding leaves behind.
constexpr q31_t kWavefoldDcAlpha = static_cast<q31_t>(0.001425 * ONE_Q31);

/// The same polynomial folder as foldBufferPolyApproximation(), plus two controls:
///  - Symmetry (`bias`, +-2^30 = up to half the fold curve's input range either way): a DC offset added
///    after the fold gain, so the positive and negative half-waves land on different parts of the fold
///    curve - even harmonics on top of the folder's odd ones. What the bias alone folds to is subtracted
///    again (silence in = silence out), and a slow DC blocker takes out the offset the asymmetry leaves
///    in the folded signal. A large bias pushes one half-wave into the folder's top range sooner, where
///    its first polynomial stage wraps around - the existing "digital" grit, unchanged, just reached
///    from one side first.
///  - Tone (0-50): the same clean-lowend split as the saturation's Tone - the low band bypasses the
///    folder and is added back afterwards, so only the upper part of the signal gets folded.
/// With bias 0, tone 0 and dcBlock off the output is bit-identical to foldBufferPolyApproximation().
/// Processes every `stride`th sample: 1 for mono, 2 for one channel of an interleaved stereo buffer
/// (call once per channel, each with its own state).
inline void foldBufferWithSymmetryAndTone(q31_t* startSample, q31_t* endSample, int32_t stride, q31_t level, q31_t bias,
                                          uint8_t tone, bool dcBlock, WavefoldChannelState& state) {
	q31_t foldLevel = add_saturate(level, FOLD_MIN);
	// Once per buffer: what the bias alone folds to, so it can be subtracted again.
	q31_t biasFolded = polynomialOscillatorApproximation(bias) >> 7;

	for (q31_t* sample = startSample; sample < endSample; sample += stride) {
		q31_t input = *sample;
		q31_t low = saturationCleanLowEnd(input, &state.toneLowpass, tone);
		q31_t high = subtract_saturate(input, low);

		q31_t x = lshiftAndSaturateUnknown(multiply_32x32_rshift32(foldLevel, high), 8);
		x = add_saturate(x, bias);
		// ">> 7" is foldBufferPolyApproximation()'s volume compensation. Both terms are within +-2^24
		// after it, so the subtraction can't overflow.
		q31_t folded = (polynomialOscillatorApproximation(x) >> 7) - biasFolded;

		if (dcBlock) {
			q31_t diff = subtract_saturate(folded, state.dcTracker);
			state.dcTracker = add_saturate(
			    state.dcTracker, saturation_detail::doubleSaturate(multiply_32x32_rshift32(diff, kWavefoldDcAlpha)));
			folded = subtract_saturate(folded, state.dcTracker);
		}

		*sample = add_saturate(folded, low);
	}
}

} // namespace deluge::dsp
