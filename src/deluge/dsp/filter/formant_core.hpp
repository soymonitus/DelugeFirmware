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

// Core of the Vowel (formant) filter: the vowel table, knob -> formant mapping and the resonator cascade. Pure float
// C++ without firmware dependencies, so it can be measured on a computer (see FormantFilter for the firmware side).
//
// A vowel is the shape a vocal tract gives a buzzing source: a few resonances ("formants", F1..F4) whose positions
// tell vowels apart. The filter is a cascade of six two-pole resonators in the form Klatt used for his speech
// synthesiser (y = a*x + b*y1 + c*y2 with a = 1 - b - c): every stage has unity gain at DC, so whatever goes in keeps
// its low end - a bass stays a bass, it just starts to talk. Nothing random, no noise.
//
// F1..F4 come from the vowel table. F5 and F6 sit above F4, where a real vocal tract keeps on resonating: they give the
// vowels their presence (without them the cascade falls away at 48 dB per octave above F4 and every vowel sounds
// muffled). Everything passes through the resonators, so the source never reaches the output unshaped - no raw buzz
// on top of the vowel - and the darker vowels (O, U) stay dark, as their lower formants damp the higher ones.
//
// The cutoff knob walks eight vowels A Ä E I Ü Ö O U - an order in which every neighbour pair glides naturally (the
// tongue rises from a to i, then the lips round from i to u). Resonance makes the formants narrower and more
// pronounced, morph shifts them up to an octave higher (a smaller "speaker"; negative modulation goes the other way).
//
// Literature: Klatt, D. H., "Software for a cascade/parallel formant synthesizer", JASA 67(3), 1980.
//             Fant, G., "Acoustic Theory of Speech Production", Mouton, 1960.

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace deluge::dsp::filter::formant {

inline constexpr int32_t kNumVowels = 8;
inline constexpr int32_t kNumVowelFormants = 4; ///< F1..F4, from the vowel table
inline constexpr int32_t kNumFormants = 6;      ///< The cascade: F1..F4 plus F5 and F6 above them
inline constexpr float kSampleRate = 44100.0f;

/// Vowel names along the cutoff knob (ASCII - the displays have no umlauts)
inline constexpr const char* kVowelNames[kNumVowels] = {"A", "AE", "E", "I", "UE", "OE", "O", "U"};

struct Vowel {
	float hz[kNumVowelFormants];
	float bandwidthHz[kNumVowelFormants];
	float level; ///< Evens out loudness across vowels (open vowels resonate more than closed ones)
};

// A male reference voice - morph moves it up. Formant frequencies after the classic phonetics measurements (Peterson
// & Barney; the German vowel studies for Ä/Ö/Ü), bandwidths after Klatt and Fant. Levels: measured with a sawtooth
// through the whole cascade (F5 and F6 included) so every vowel comes out about as loud as it went in.
//                                       F1      F2      F3      F4          B1     B2      B3      B4
inline constexpr Vowel kVowels[kNumVowels] = {
    {{730.0f, 1150.0f, 2550.0f, 3450.0f}, {90.0f, 100.0f, 130.0f, 160.0f}, 0.444f}, // A
    {{560.0f, 1750.0f, 2500.0f, 3450.0f}, {70.0f, 100.0f, 130.0f, 160.0f}, 0.598f}, // AE
    {{400.0f, 2000.0f, 2650.0f, 3500.0f}, {60.0f, 100.0f, 130.0f, 160.0f}, 0.575f}, // E
    {{280.0f, 2250.0f, 3000.0f, 3600.0f}, {50.0f, 100.0f, 140.0f, 160.0f}, 0.583f}, // I
    {{280.0f, 1750.0f, 2200.0f, 3350.0f}, {50.0f, 90.0f, 120.0f, 160.0f}, 0.575f},  // UE
    {{400.0f, 1450.0f, 2300.0f, 3350.0f}, {60.0f, 90.0f, 120.0f, 160.0f}, 0.557f},  // OE
    {{480.0f, 800.0f, 2500.0f, 3350.0f}, {70.0f, 80.0f, 120.0f, 160.0f}, 0.466f},   // O
    {{320.0f, 700.0f, 2300.0f, 3300.0f}, {50.0f, 70.0f, 120.0f, 160.0f}, 0.539f},   // U
};

inline constexpr float kMinFormantHz = 50.0f;
inline constexpr float kMaxFormantHz = 9000.0f; // Well below Nyquist, and within fastCos' accurate range
inline constexpr float kMaxBandwidthHz = 2000.0f;
inline constexpr float kSharpnessRange = 2.3f;   // Full resonance makes the formants 2^2.3 = ~5x narrower
inline constexpr float kSharpnessLevel = -0.66f; // ...and ~4 dB quieter, to make up for the louder resonances

// F5 and F6, placed relative to F4 (so they follow the vowel and the formant shift). They bring 2-5 kHz back to where
// a voice has it - measured with a saw, 4-4.5 kHz rise by 20-35 dB for the open and front vowels - while the very top
// keeps falling away, so no flat band of raw source buzz sits above. Resonance narrows them at most to half: up here
// a very narrow band holds just a few harmonics, and those of a detuned supersaw beat against each other into a rough,
// noisy hiss.
inline constexpr float kF5Ratio = 1.28f;
inline constexpr float kF6Ratio = 1.6f;
inline constexpr float kF5BandwidthHz = 400.0f;
inline constexpr float kF6BandwidthHz = 600.0f;
inline constexpr float kUpperFormantMinScale = 0.5f;

// Raw: an optional wide band-pass in parallel to the cascade, fed straight from the input, that lets the source's own
// highs through unshaped - the buzzy "original waveform on top" character of the filter's first presence band, now a
// choice (0..50, 0 = off). At 25 it is that band exactly (centre 1.3 x F4, Q 0.7, gain 0.45 x level); 50 doubles it.
inline constexpr int32_t kRawMax = 50;
inline constexpr float kRawRatio = 1.3f;
inline constexpr float kRawQ = 0.7f;
inline constexpr float kRawGainAt25 = 0.45f;

// Output limiting: the resonances can push peaks above full scale (a hot supersaw through a sharp "I" peaks at ~1.6x).
// Clamping them would crackle, so above the knee the output bends smoothly towards full scale instead - transparent
// for everything below it, like a gently saturating analog filter for what goes above.
inline constexpr float kLimiterKnee = 0.6f;

// ============================================================================
// Fast math (runs once per buffer per voice - libm calls would add up)
// ============================================================================

/// 2^x, relative error < 2e-7 (least-squares fit on [0, 1))
inline float fastExp2(float x) {
	x = std::clamp(x, -60.0f, 60.0f);
	const float whole = std::floor(x);
	const float f = x - whole;
	const float p =
	    1.0f + f * (0.693152535f + f * (0.240152444f + f * (0.055836599f + f * (0.008972898f + f * 0.001885404f))));
	return std::bit_cast<float>(std::bit_cast<int32_t>(p) + (static_cast<int32_t>(whole) << 23));
}

/// log2(x) for x > 0, absolute error < 3e-6
inline float fastLog2(float x) {
	const int32_t bits = std::bit_cast<int32_t>(x);
	const float exponent = static_cast<float>(((bits >> 23) & 0xFF) - 127);
	const float m = std::bit_cast<float>((bits & 0x007FFFFF) | 0x3F800000) - 1.0f; // [0, 1)
	// log2(1 + m), least-squares fit on [0, 1)
	return exponent
	       + m
	             * (1.442534780f
	                + m
	                      * (-0.718033590f
	                         + m * (0.457158118f + m * (-0.277341638f + m * (0.121472940f + m * -0.025792342f)))));
}

/// cos(x) for 0 <= x <= 1.3, error < 1e-7 - most accurate for small x, where the low formants' coefficients sit
/// right next to 2
inline float fastCos(float x) {
	const float x2 = x * x;
	return 1.0f
	       + x2
	             * (-0.5f
	                + x2 * (1.0f / 24.0f + x2 * (-1.0f / 720.0f + x2 * (1.0f / 40320.0f + x2 * (-1.0f / 3628800.0f)))));
}

/// e^-x for 0 <= x <= 0.15
inline float expNegSmall(float x) {
	return 1.0f + x * (-1.0f + x * (0.5f + x * (-1.0f / 6.0f + x * (1.0f / 24.0f + x * (-1.0f / 120.0f)))));
}

/// Unchanged up to the knee, then bending smoothly (slope 1 at the knee) towards full scale, never reaching it
[[gnu::always_inline]] inline float softLimit(float x) {
	const float magnitude = std::abs(x);
	if (magnitude <= kLimiterKnee) {
		return x;
	}
	const float over = (magnitude - kLimiterKnee) * (1.0f / (1.0f - kLimiterKnee));
	const float limited = kLimiterKnee + (1.0f - kLimiterKnee) * over / (1.0f + over);
	return (x < 0.0f) ? -limited : limited;
}

// ============================================================================
// Knob -> formants
// ============================================================================

/// The formants at one point of the vowel path
struct Shape {
	float hz[kNumFormants];
	float bandwidthHz[kNumFormants];
	float level;
};

/// position 0 = A ... 1 = U (gliding between neighbouring vowels), shiftOctaves moves all formants, sharpness 0..1
/// narrows them
inline Shape shapeAt(float position, float shiftOctaves, float sharpness) {
	const float v = std::clamp(position, 0.0f, 1.0f) * static_cast<float>(kNumVowels - 1);
	const int32_t i0 = std::min(static_cast<int32_t>(v), kNumVowels - 2);
	const float frac = v - static_cast<float>(i0);
	const Vowel& v0 = kVowels[i0];
	const Vowel& v1 = kVowels[i0 + 1];

	shiftOctaves = std::clamp(shiftOctaves, -1.0f, 1.0f);
	// A smaller voice also has somewhat wider resonances; resonance narrows them
	const float bandwidthScale = fastExp2(shiftOctaves * 0.5f - std::clamp(sharpness, 0.0f, 1.0f) * kSharpnessRange);

	Shape shape;
	for (int32_t k = 0; k < kNumVowelFormants; k++) {
		const float l0 = fastLog2(v0.hz[k]);
		const float l1 = fastLog2(v1.hz[k]);
		shape.hz[k] = std::clamp(fastExp2(l0 + (l1 - l0) * frac + shiftOctaves), kMinFormantHz, kMaxFormantHz);
		shape.bandwidthHz[k] =
		    std::clamp((v0.bandwidthHz[k] + (v1.bandwidthHz[k] - v0.bandwidthHz[k]) * frac) * bandwidthScale, 5.0f,
		               kMaxBandwidthHz);
	}
	// F5 and F6 above F4 (already shifted), narrowed by resonance - but at most to half (see kUpperFormantMinScale)
	const float upperScale = std::max(bandwidthScale, kUpperFormantMinScale);
	shape.hz[4] = std::min(shape.hz[3] * kF5Ratio, kMaxFormantHz);
	shape.hz[5] = std::min(shape.hz[3] * kF6Ratio, kMaxFormantHz);
	shape.bandwidthHz[4] = std::clamp(kF5BandwidthHz * upperScale, 5.0f, kMaxBandwidthHz);
	shape.bandwidthHz[5] = std::clamp(kF6BandwidthHz * upperScale, 5.0f, kMaxBandwidthHz);
	shape.level =
	    (v0.level + (v1.level - v0.level) * frac) * fastExp2(std::clamp(sharpness, 0.0f, 1.0f) * kSharpnessLevel);
	return shape;
}

/// Nearest vowel for a 0..50 cutoff knob value
constexpr int32_t nearestVowelForKnob(int32_t knob) {
	const int32_t clamped = std::clamp<int32_t>(knob, 0, 50);
	return (clamped * (kNumVowels - 1) + 25) / 50;
}

// ============================================================================
// The resonator cascade
// ============================================================================

struct Coefficients {
	float a[kNumFormants];
	float b[kNumFormants];
	float c[kNumFormants];
	// Raw band-pass, in parallel: p = pb0 * (x - x2) - pa1 * p1 - pa2 * p2 (pb0 = 0 when Raw is off)
	float pb0;
	float pa1;
	float pa2;
};

/// Raw 0..50 -> the raw band's gain
[[nodiscard]] inline float rawGainFor(int32_t raw) {
	return static_cast<float>(std::clamp<int32_t>(raw, 0, kRawMax)) * (kRawGainAt25 / 25.0f);
}

/// Klatt resonators (unity gain at DC) plus the raw band. inputGain is folded into the first stage and the raw band,
/// so it ramps with the coefficients.
inline Coefficients coefficientsFor(const Shape& shape, float inputGain, float rawGain = 0.0f) {
	constexpr float kPi = 3.14159265358979f;
	Coefficients co;
	for (int32_t k = 0; k < kNumFormants; k++) {
		const float r = expNegSmall(kPi * shape.bandwidthHz[k] / kSampleRate);
		co.b[k] = 2.0f * r * fastCos(2.0f * kPi * shape.hz[k] / kSampleRate);
		co.c[k] = -r * r;
		co.a[k] = 1.0f - co.b[k] - co.c[k];
	}
	co.a[0] *= inputGain;

	// RBJ band-pass with 0 dB peak gain, around where the vocal tract's F5 sits (at most kMaxFormantHz, inside
	// fastCos' range)
	const float rawHz = std::min(shape.hz[3] * kRawRatio, kMaxFormantHz);
	const float cosW = fastCos(2.0f * kPi * rawHz / kSampleRate);
	const float alpha = std::sqrt(std::max(1.0f - cosW * cosW, 0.0f)) / (2.0f * kRawQ);
	const float norm = 1.0f / (1.0f + alpha);
	co.pb0 = alpha * norm * rawGain * inputGain;
	co.pa1 = -2.0f * cosW * norm;
	co.pa2 = (1.0f - alpha) * norm;
	return co;
}

/// Per-sample increments from one coefficient set to another over numFrames samples. A straight line between two
/// stable two-pole sections stays stable (their stability region is convex), so ramping never blows up.
inline Coefficients rampStep(const Coefficients& from, const Coefficients& to, int32_t numFrames) {
	const float perFrame = 1.0f / static_cast<float>(numFrames);
	Coefficients step;
	for (int32_t k = 0; k < kNumFormants; k++) {
		step.a[k] = (to.a[k] - from.a[k]) * perFrame;
		step.b[k] = (to.b[k] - from.b[k]) * perFrame;
		step.c[k] = (to.c[k] - from.c[k]) * perFrame;
	}
	step.pb0 = (to.pb0 - from.pb0) * perFrame;
	step.pa1 = (to.pa1 - from.pa1) * perFrame;
	step.pa2 = (to.pa2 - from.pa2) * perFrame;
	return step;
}

[[gnu::always_inline]] inline void advance(Coefficients& co, const Coefficients& step) {
	for (int32_t k = 0; k < kNumFormants; k++) {
		co.a[k] += step.a[k];
		co.b[k] += step.b[k];
		co.c[k] += step.c[k];
	}
	co.pb0 += step.pb0;
	co.pa1 += step.pa1;
	co.pa2 += step.pa2;
}

/// Resonator memory of one audio channel
struct Channel {
	float y1[kNumFormants];
	float y2[kNumFormants];
	float x1; ///< Raw band: the last two inputs...
	float x2;
	float p1; ///< ...and outputs
	float p2;

	/// One sample through the cascade, plus the raw band if withRaw (callers skip it while Raw stays at 0)
	template <bool withRaw>
	[[gnu::always_inline]] inline float process(float x, const Coefficients& co) {
		const float in = x;
		for (int32_t k = 0; k < kNumFormants; k++) {
			const float y = co.a[k] * x + co.b[k] * y1[k] + co.c[k] * y2[k];
			y2[k] = y1[k];
			y1[k] = y;
			x = y;
		}
		if constexpr (withRaw) {
			const float p = co.pb0 * (in - x2) - co.pa1 * p1 - co.pa2 * p2;
			x2 = x1;
			x1 = in;
			p2 = p1;
			p1 = p;
			return x + p;
		}
		return x;
	}

	/// The raw band's memory is stale after a stretch without it: start it over
	void clearRaw() { x1 = x2 = p1 = p2 = 0.0f; }

	/// Flush values too small to matter (they'd go denormal in long silences, which is slow) and recover from
	/// anything non-finite
	void tidy() {
		for (int32_t k = 0; k < kNumFormants; k++) {
			if (!(std::abs(y1[k]) < 1.0e6f) || !(std::abs(y2[k]) < 1.0e6f)) {
				*this = {};
				return;
			}
			if (std::abs(y1[k]) < 1.0e-15f && std::abs(y2[k]) < 1.0e-15f) {
				y1[k] = 0.0f;
				y2[k] = 0.0f;
			}
		}
		if (!(std::abs(p1) < 1.0e6f) || !(std::abs(p2) < 1.0e6f)) {
			*this = {};
			return;
		}
		if (std::abs(p1) < 1.0e-15f && std::abs(p2) < 1.0e-15f) {
			p1 = 0.0f;
			p2 = 0.0f;
		}
	}
};

} // namespace deluge::dsp::filter::formant
