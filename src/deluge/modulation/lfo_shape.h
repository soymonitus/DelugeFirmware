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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace deluge::modulation {

/// A user-designed LFO cycle (LFOType::CUSTOM), Serum style: breakpoints joined by segments that can be bent into
/// curves. The points are set and shaped in LFOShapeEditor.
class LFOShape {
public:
	static constexpr int32_t kMaxPoints = 64;
	static constexpr int32_t kMaxCurve = 100;
	static constexpr int32_t kMaxValue = 32767;

	struct Point {
		uint16_t x;   ///< Position in the cycle, 0..65535 (the cycle wraps back to the first point)
		int16_t y;    ///< Value, -32767..32767
		int8_t curve; ///< Bend of the segment from this point to the next: <0 shoots off fast, >0 starts slowly
	};

	enum class Preset : uint8_t {
		TRIANGLE,
		SINE,
		SAW_UP,
		SAW_DOWN,
		SQUARE,
		PULSE_25,
		TRAPEZOID,
		STAIRS,
		EXP_DECAY,
		EXP_RISE,
		SWELL,
		SHARK_FIN,
		BUMPS,
		S_CURVE,
		FIN_X3,
		DECAY_X3,
		DECAY_X3_ROUND,
		TRESILLO,
		TRESILLO_ROUND,
		THREE_OVER_TWO,
		THREE_OVER_TWO_ROUND,
		DECAY_X5,
		DECAY_X5_ROUND,
		EUCLID_7_16,
		EUCLID_7_16_ROUND,
		EUCLID_7_12,
		EUCLID_7_12_ROUND,
		TWO_STEP,
		TWO_STEP_ROUND,
		GROUPS_3333_22,
		GROUPS_3333_22_ROUND,
		RATCHET,
		RATCHET_ROUND,
		WOBBLE_332,
		RAMPS_332,
		BOUNCE,
		SWING,
		HEARTBEAT,
		ZIGZAG,
		CHAOS,
		NUM_PRESETS
	};

	LFOShape() { loadPreset(Preset::TRIANGLE); }

	[[nodiscard]] int32_t numPoints() const { return numPoints_; }
	[[nodiscard]] const Point& point(int32_t index) const { return points_[index]; }

	/// The value at an LFO phase, as q31 (bipolar, like the other LFO shapes)
	[[nodiscard]] int32_t valueAt(uint32_t phase) const {
		const int32_t x = static_cast<int32_t>(phase >> 16);
		const int32_t i = segmentAt(static_cast<uint16_t>(x));
		const Point& p0 = points_[i];
		const bool last = (i == numPoints_ - 1);
		const int32_t x1 = last ? 65536 : points_[i + 1].x;
		const int32_t y1 = last ? points_[0].y : points_[i + 1].y;
		const int32_t width = std::max<int32_t>(x1 - p0.x, 1);
		const float t = static_cast<float>(x - p0.x) / static_cast<float>(width);
		const float bent = bend(t, p0.curve);
		const float y = static_cast<float>(p0.y) + static_cast<float>(y1 - p0.y) * bent;
		return static_cast<int32_t>(y * 65536.0f);
	}

	/// Index of the segment (= its starting point) containing x
	[[nodiscard]] int32_t segmentAt(uint16_t x) const {
		int32_t i = numPoints_ - 1;
		while (i > 0 && points_[i].x > x) {
			i--;
		}
		return i;
	}

	/// Index of the point at exactly x, or -1
	[[nodiscard]] int32_t pointAt(uint16_t x) const {
		for (int32_t i = 0; i < numPoints_; i++) {
			if (points_[i].x == x) {
				return i;
			}
		}
		return -1;
	}

	/// The bend for a piece of a bent segment - from `from` to `to` of it (0..1) - so that the piece on its own keeps
	/// the shape it had: exactly when the piece starts where an upward bend (t^e) starts, or ends where a downward
	/// one ends, otherwise as close as a bend gets
	static int32_t curveForPiece(int32_t curve, float from, float to) {
		if (curve == 0 || to <= from || (from <= 0.0f && (to >= 1.0f || curve > 0)) || (to >= 1.0f && curve < 0)) {
			return curve;
		}
		constexpr int32_t kNumSamples = 7;
		const auto c = static_cast<int8_t>(curve);
		const float start = bend(from, c);
		const float span = bend(to, c) - start;
		if (span <= 0.0f) {
			return 0;
		}
		float target[kNumSamples];
		for (int32_t i = 0; i < kNumSamples; i++) {
			const float u = static_cast<float>(i + 1) / (kNumSamples + 1);
			target[i] = (bend(from + (to - from) * u, c) - start) / span;
		}
		auto error = [&](int32_t candidate) {
			float sum = 0.0f;
			for (int32_t i = 0; i < kNumSamples; i++) {
				const float u = static_cast<float>(i + 1) / (kNumSamples + 1);
				const float d = bend(u, static_cast<int8_t>(candidate)) - target[i];
				sum += d * d;
			}
			return sum;
		};
		// Coarse over all bends, then fine around the best one
		int32_t best = 0;
		float bestError = error(0);
		for (int32_t candidate = -kMaxCurve; candidate <= kMaxCurve; candidate += 10) {
			if (const float e = error(candidate); e < bestError) {
				bestError = e;
				best = candidate;
			}
		}
		const int32_t coarse = best;
		for (int32_t candidate = std::max(coarse - 9, -kMaxCurve); candidate <= std::min(coarse + 9, kMaxCurve);
		     candidate++) {
			if (const float e = error(candidate); e < bestError) {
				bestError = e;
				best = candidate;
			}
		}
		return best;
	}

	/// Sets the value at x: moves the point that's there, or inserts a new one (if there's room). Returns its index,
	/// or -1 if the shape is full.
	int32_t setPoint(uint16_t x, int16_t y) {
		y = clampValue(y);
		const int32_t existing = pointAt(x);
		if (existing >= 0) {
			points_[existing].y = y;
			return existing;
		}
		if (numPoints_ >= kMaxPoints) {
			return -1;
		}
		const int32_t at = segmentAt(x) + 1; // Insert after the segment's start point; the new point inherits its bend
		std::memmove(&points_[at + 1], &points_[at], (numPoints_ - at) * sizeof(Point));
		points_[at] = {x, y, points_[at - 1].curve};
		numPoints_++;
		return at;
	}

	/// Removes a point (the first one, at x = 0, always stays: it anchors the cycle)
	void removePoint(int32_t index) {
		if (index <= 0 || index >= numPoints_) {
			return;
		}
		std::memmove(&points_[index], &points_[index + 1], (numPoints_ - index - 1) * sizeof(Point));
		numPoints_--;
	}

	void setValue(int32_t index, int16_t y) { points_[index].y = clampValue(y); }

	void setCurve(int32_t index, int32_t curve) {
		points_[index].curve = static_cast<int8_t>(std::clamp<int32_t>(curve, -kMaxCurve, kMaxCurve));
	}

	/// Moves a point in time, between its neighbours (the first point stays at 0). Returns the new position.
	uint16_t movePoint(int32_t index, int32_t newX) {
		if (index <= 0 || index >= numPoints_) {
			return 0;
		}
		const int32_t low = points_[index - 1].x + 1;
		const int32_t high = (index == numPoints_ - 1) ? 65535 : points_[index + 1].x - 1;
		points_[index].x = static_cast<uint16_t>(std::clamp<int32_t>(newX, low, high));
		return points_[index].x;
	}

	void loadPreset(Preset preset) {
		constexpr int16_t kTop = kMaxValue;
		constexpr int16_t kBottom = -kMaxValue;
		switch (preset) {
		case Preset::TRIANGLE:
			set({{0, kBottom, 0}, {32768, kTop, 0}});
			break;
		case Preset::SINE:
			// Four bent quarters make a close sine (within 1.5%)
			set({{0, 0, -27}, {16384, kTop, 27}, {32768, 0, -27}, {49152, kBottom, 27}});
			break;
		case Preset::SAW_UP:
			set({{0, kBottom, 0}, {65535, kTop, 0}});
			break;
		case Preset::SAW_DOWN:
			set({{0, kTop, 0}, {65535, kBottom, 0}});
			break;
		case Preset::SQUARE:
			set({{0, kTop, 0}, {32767, kTop, 0}, {32768, kBottom, 0}, {65535, kBottom, 0}});
			break;

		// Slopes
		case Preset::PULSE_25:
			set({{0, kTop, 0}, {16383, kTop, 0}, {16384, kBottom, 0}, {65535, kBottom, 0}});
			break;
		case Preset::TRAPEZOID: // Rise, hold, fall, rest
			set({{0, kBottom, 0}, {8192, kTop, 0}, {32768, kTop, 0}, {40960, kBottom, 0}});
			break;
		case Preset::STAIRS: // Four steps up
			set({{0, kBottom, 0},
			     {16383, kBottom, 0},
			     {16384, -10922, 0},
			     {32767, -10922, 0},
			     {32768, 10922, 0},
			     {49151, 10922, 0},
			     {49152, kTop, 0},
			     {65535, kTop, 0}});
			break;

		// Bends
		case Preset::EXP_DECAY: // Drops away fast, then settles - like an envelope's decay
			set({{0, kTop, -70}, {65535, kBottom, 0}});
			break;
		case Preset::EXP_RISE: // Creeps up, then shoots to the top - a riser
			set({{0, kBottom, 70}, {65535, kTop, 0}});
			break;
		case Preset::SWELL: // Shoots up, then levels off
			set({{0, kBottom, -70}, {65535, kTop, 0}});
			break;
		case Preset::SHARK_FIN: // A charging rise, then a quick fall
			set({{0, kBottom, -55}, {49152, kTop, -45}});
			break;
		case Preset::BUMPS: // Two humps per cycle, like a rectified sine
			set({{0, kBottom, -35}, {16384, kTop, 35}, {32768, kBottom, -35}, {49152, kTop, 35}});
			break;
		case Preset::S_CURVE: // A triangle with smooth turns and steep middles
			set({{0, kBottom, 60}, {16384, 0, -60}, {32768, kTop, 60}, {49152, 0, -60}});
			break;

		// Polyrhythms: the cycle in threes (triplets when the LFO is synced to an even length) or mixed groupings
		case Preset::FIN_X3: // Three shark fins
			set({{0, kBottom, -55},
			     {16384, kTop, -45},
			     {21845, kBottom, -55},
			     {38229, kTop, -45},
			     {43691, kBottom, -55},
			     {60075, kTop, -45}});
			break;
		case Preset::DECAY_X3: // Three decays - pumping in triplets
			set({{0, kTop, -70},
			     {21844, kBottom, 0},
			     {21845, kTop, -70},
			     {43690, kBottom, 0},
			     {43691, kTop, -70},
			     {65535, kBottom, 0}});
			break;
		case Preset::DECAY_X3_ROUND: // The same, each hit a round bump
			set({{0, kBottom, 60},
			     {2048, 0, -60},
			     {4096, kTop, 60},
			     {12970, 0, -60},
			     {21845, kBottom, 60},
			     {23893, 0, -60},
			     {25941, kTop, 60},
			     {34816, 0, -60},
			     {43691, kBottom, 60},
			     {45739, 0, -60},
			     {47787, kTop, 60},
			     {56661, 0, -60}});
			break;
		case Preset::TRESILLO: // Decays grouped 3 + 3 + 2 (in eighths)
			set({{0, kTop, -70},
			     {24575, kBottom, 0},
			     {24576, kTop, -70},
			     {49151, kBottom, 0},
			     {49152, kTop, -70},
			     {65535, kBottom, 0}});
			break;
		case Preset::TRESILLO_ROUND: // The same, each hit a round bump
			set({{0, kBottom, 60},
			     {2048, 0, -60},
			     {4096, kTop, 60},
			     {14336, 0, -60},
			     {24576, kBottom, 60},
			     {26624, 0, -60},
			     {28672, kTop, 60},
			     {38912, 0, -60},
			     {49152, kBottom, 60},
			     {51200, 0, -60},
			     {53248, kTop, 60},
			     {59392, 0, -60}});
			break;
		case Preset::THREE_OVER_TWO: // Hemiola: hits on the thirds and on the halves, the downbeat (on both) strongest
			set({{0, kTop, -70},
			     {21844, kBottom, 0},
			     {21845, 19660, -70},
			     {32767, kBottom, 0},
			     {32768, 26214, -70},
			     {43690, kBottom, 0},
			     {43691, 19660, -70},
			     {65535, kBottom, 0}});
			break;
		case Preset::THREE_OVER_TWO_ROUND: // The same, each hit a round bump
			set({{0, kBottom, 60},
			     {2048, 0, -60},
			     {4096, kTop, 60},
			     {12970, 0, -60},
			     {21845, kBottom, 60},
			     {23483, -6554, -60},
			     {25122, 19660, 60},
			     {28945, -6554, -60},
			     {32768, kBottom, 60},
			     {34406, -3276, -60},
			     {36045, 26214, 60},
			     {39868, -3276, -60},
			     {43691, kBottom, 60},
			     {45739, -6554, -60},
			     {47787, 19660, 60},
			     {56661, -6554, -60}});
			break;

		case Preset::DECAY_X5: // Five decays - quintuplets against the bar
			set({{0, kTop, -70},
			     {13106, kBottom, 0},
			     {13107, kTop, -70},
			     {26213, kBottom, 0},
			     {26214, kTop, -70},
			     {39321, kBottom, 0},
			     {39322, kTop, -70},
			     {52428, kBottom, 0},
			     {52429, kTop, -70},
			     {65535, kBottom, 0}});
			break;
		case Preset::DECAY_X5_ROUND: // The same, each hit a round bump
			set({{0, kBottom, 60},     {1966, 0, -60},  {3932, kTop, 60},  {8519, 0, -60},
			     {13107, kBottom, 60}, {15073, 0, -60}, {17039, kTop, 60}, {21626, 0, -60},
			     {26214, kBottom, 60}, {28180, 0, -60}, {30146, kTop, 60}, {34734, 0, -60},
			     {39322, kBottom, 60}, {41288, 0, -60}, {43254, kTop, 60}, {47841, 0, -60},
			     {52429, kBottom, 60}, {54395, 0, -60}, {56361, kTop, 60}, {60948, 0, -60}});
			break;
		case Preset::EUCLID_7_16: // 7 hits spread evenly over 16 steps (2 2 2 3 2 2 3)
			set({{0, kTop, -70},
			     {8191, kBottom, 0},
			     {8192, kTop, -70},
			     {16383, kBottom, 0},
			     {16384, kTop, -70},
			     {24575, kBottom, 0},
			     {24576, kTop, -70},
			     {36863, kBottom, 0},
			     {36864, kTop, -70},
			     {45055, kBottom, 0},
			     {45056, kTop, -70},
			     {53247, kBottom, 0},
			     {53248, kTop, -70},
			     {65535, kBottom, 0}});
			break;
		case Preset::EUCLID_7_16_ROUND: // The same, each hit a round bump
			set({{0, kBottom, 60},     {1229, 0, -60},  {2458, kTop, 60},  {5325, 0, -60},
			     {8192, kBottom, 60},  {9421, 0, -60},  {10650, kTop, 60}, {13517, 0, -60},
			     {16384, kBottom, 60}, {17613, 0, -60}, {18842, kTop, 60}, {21709, 0, -60},
			     {24576, kBottom, 60}, {26419, 0, -60}, {28262, kTop, 60}, {32563, 0, -60},
			     {36864, kBottom, 60}, {38093, 0, -60}, {39322, kTop, 60}, {42189, 0, -60},
			     {45056, kBottom, 60}, {46285, 0, -60}, {47514, kTop, 60}, {50381, 0, -60},
			     {53248, kBottom, 60}, {55091, 0, -60}, {56934, kTop, 60}, {61235, 0, -60}});
			break;
		case Preset::EUCLID_7_12: // 7 hits over 12 triplet steps, the bell pattern (2 2 1 2 2 2 1)
			set({{0, kTop, -70},
			     {10922, kBottom, 0},
			     {10923, kTop, -70},
			     {21844, kBottom, 0},
			     {21845, kTop, -70},
			     {27306, kBottom, 0},
			     {27307, kTop, -70},
			     {38228, kBottom, 0},
			     {38229, kTop, -70},
			     {49151, kBottom, 0},
			     {49152, kTop, -70},
			     {60074, kBottom, 0},
			     {60075, kTop, -70},
			     {65535, kBottom, 0}});
			break;
		case Preset::EUCLID_7_12_ROUND: // The same, each hit a round bump
			set({{0, kBottom, 60},     {1638, 0, -60},  {3277, kTop, 60},  {7100, 0, -60},
			     {10923, kBottom, 60}, {12561, 0, -60}, {14200, kTop, 60}, {18022, 0, -60},
			     {21845, kBottom, 60}, {22664, 0, -60}, {23484, kTop, 60}, {25395, 0, -60},
			     {27307, kBottom, 60}, {28945, 0, -60}, {30584, kTop, 60}, {34406, 0, -60},
			     {38229, kBottom, 60}, {39867, 0, -60}, {41506, kTop, 60}, {45329, 0, -60},
			     {49152, kBottom, 60}, {50790, 0, -60}, {52429, kTop, 60}, {56252, 0, -60},
			     {60075, kBottom, 60}, {60894, 0, -60}, {61713, kTop, 60}, {63624, 0, -60}});
			break;

		// Grooves: drum & bass patterns in 16ths, for a cycle of one bar (bass and filter movement)
		case Preset::TWO_STEP: // Drum & bass two-step: hits on the kicks (16ths 1 and 11)
			set({{0, kTop, -70}, {40959, kBottom, 0}, {40960, kTop, -70}, {65535, kBottom, 0}});
			break;
		case Preset::TWO_STEP_ROUND: // The same, each hit a round bump
			set({{0, kBottom, 60},
			     {2048, 0, -60},
			     {4096, kTop, 60},
			     {22528, 0, -60},
			     {40960, kBottom, 60},
			     {43008, 0, -60},
			     {45056, kTop, 60},
			     {55296, 0, -60}});
			break;
		case Preset::GROUPS_3333_22: // 16ths grouped 3 3 3 3 2 2, accented
			set({{0, kTop, -70},
			     {12287, kBottom, 0},
			     {12288, 22937, -70},
			     {24575, kBottom, 0},
			     {24576, 27852, -70},
			     {36863, kBottom, 0},
			     {36864, 22937, -70},
			     {49151, kBottom, 0},
			     {49152, kTop, -70},
			     {57343, kBottom, 0},
			     {57344, 22937, -70},
			     {65535, kBottom, 0}});
			break;
		case Preset::GROUPS_3333_22_ROUND: // The same, each hit a round bump
			set({{0, kBottom, 60},     {1843, 0, -60},      {3686, kTop, 60},   {7987, 0, -60},
			     {12288, kBottom, 60}, {14131, -4915, -60}, {15974, 22937, 60}, {20275, -4915, -60},
			     {24576, kBottom, 60}, {26419, -2458, -60}, {28262, 27852, 60}, {32563, -2458, -60},
			     {36864, kBottom, 60}, {38707, -4915, -60}, {40550, 22937, 60}, {44851, -4915, -60},
			     {49152, kBottom, 60}, {50381, 0, -60},     {51610, kTop, 60},  {54477, 0, -60},
			     {57344, kBottom, 60}, {58573, -4915, -60}, {59802, 22937, 60}, {62669, -4915, -60}});
			break;
		case Preset::RATCHET: // Speeding up: a quarter, two eighths, eight 16ths
			set({{0, kTop, -70},     {16383, kBottom, 0}, {16384, kTop, -70}, {24575, kBottom, 0},
			     {24576, kTop, -70}, {32767, kBottom, 0}, {32768, kTop, -70}, {36863, kBottom, 0},
			     {36864, kTop, -70}, {40959, kBottom, 0}, {40960, kTop, -70}, {45055, kBottom, 0},
			     {45056, kTop, -70}, {49151, kBottom, 0}, {49152, kTop, -70}, {53247, kBottom, 0},
			     {53248, kTop, -70}, {57343, kBottom, 0}, {57344, kTop, -70}, {61439, kBottom, 0},
			     {61440, kTop, -70}, {65535, kBottom, 0}});
			break;
		case Preset::RATCHET_ROUND: // The same, each hit a round bump
			set({{0, kBottom, 60},     {2048, 0, -60},  {4096, kTop, 60},  {10240, 0, -60},
			     {16384, kBottom, 60}, {17613, 0, -60}, {18842, kTop, 60}, {21709, 0, -60},
			     {24576, kBottom, 60}, {25805, 0, -60}, {27034, kTop, 60}, {29901, 0, -60},
			     {32768, kBottom, 60}, {33382, 0, -60}, {33997, kTop, 60}, {35430, 0, -60},
			     {36864, kBottom, 60}, {37478, 0, -60}, {38093, kTop, 60}, {39526, 0, -60},
			     {40960, kBottom, 60}, {41574, 0, -60}, {42189, kTop, 60}, {43622, 0, -60},
			     {45056, kBottom, 60}, {45670, 0, -60}, {46285, kTop, 60}, {47718, 0, -60},
			     {49152, kBottom, 60}, {49766, 0, -60}, {50381, kTop, 60}, {51814, 0, -60},
			     {53248, kBottom, 60}, {53862, 0, -60}, {54477, kTop, 60}, {55910, 0, -60},
			     {57344, kBottom, 60}, {57958, 0, -60}, {58573, kTop, 60}, {60006, 0, -60},
			     {61440, kBottom, 60}, {62054, 0, -60}, {62669, kTop, 60}, {64102, 0, -60}});
			break;
		case Preset::WOBBLE_332: // Three rounded humps, 3 + 3 + 2 eighths long
			set({{0, kBottom, -35},
			     {12288, kTop, 35},
			     {24576, kBottom, -35},
			     {36864, kTop, 35},
			     {49152, kBottom, -35},
			     {57344, kTop, 35}});
			break;
		case Preset::RAMPS_332: // Three rising ramps, 3 + 3 + 2 eighths long
			set({{0, kBottom, 40},
			     {24575, kTop, 0},
			     {24576, kBottom, 40},
			     {49151, kTop, 0},
			     {49152, kBottom, 40},
			     {65535, kTop, 0}});
			break;

		// Experimental
		case Preset::BOUNCE: // A dropped ball: falls (accelerating), bounces ever lower (decelerating up)
			set({{0, kTop, 45},
			     {15052, kBottom, -45},
			     {26220, 3277, 45},
			     {37388, kBottom, -45},
			     {45638, -13107, 45},
			     {53884, kBottom, -45},
			     {59710, -22937, 45},
			     {65535, kBottom, 0}});
			break;
		case Preset::SWING: // Two decays, long and short - a shuffle when synced to quarter notes
			set({{0, kTop, -55}, {43689, kBottom, 0}, {43690, kTop, -55}, {65535, kBottom, 0}});
			break;
		case Preset::HEARTBEAT: // Two spikes, then a long rest
			set({{0, -16384, 0},
			     {4096, -16384, 0},
			     {5632, kTop, 0},
			     {7680, kBottom, -40},
			     {10240, -16384, 0},
			     {14336, -16384, 0},
			     {16384, 6554, 0},
			     {18432, -24576, -40},
			     {20480, -16384, 0}});
			break;
		case Preset::ZIGZAG: // Slopes of every steepness
			set({{0, kBottom, 0},
			     {12000, kTop, 0},
			     {20000, -8192, 0},
			     {30000, 24576, 0},
			     {36000, -24576, 0},
			     {48000, 16384, 0},
			     {56000, -16384, 0}});
			break;
		case Preset::CHAOS: // Irregular jumps and curves
			set({{0, 0, 70},
			     {7000, kTop, -50},
			     {13000, -20000, 30},
			     {21000, 12000, -80},
			     {26000, kBottom, 60},
			     {38000, 26000, -40},
			     {44000, -6000, 90},
			     {52000, 20000, -70},
			     {58000, -28000, 40}});
			break;
		default:
			break;
		}
	}

	/// Compact text form for song / preset files: 10 hex digits per point (x, y, curve). The file reader cuts values
	/// off at 255 characters, so a shape is saved in chunks of kHexChunkPoints points: toHex() writes the chunk
	/// starting at firstPoint (an empty string once there are no more), fromHex() reads the first chunk and
	/// appendHex() each further one.
	static constexpr int32_t kHexChunkPoints = 25;
	static constexpr int32_t kHexBufferSize = kHexChunkPoints * 10 + 1;
	void toHex(char* buffer, int32_t firstPoint) const {
		static constexpr char kDigits[] = "0123456789ABCDEF";
		char* out = buffer;
		const int32_t end = std::min(firstPoint + kHexChunkPoints, numPoints_);
		for (int32_t i = firstPoint; i < end; i++) {
			const uint32_t y = static_cast<uint16_t>(points_[i].y);
			const uint32_t c = static_cast<uint8_t>(points_[i].curve);
			const uint32_t fields[3] = {points_[i].x, y, c};
			const int32_t digits[3] = {4, 4, 2};
			for (int32_t f = 0; f < 3; f++) {
				for (int32_t d = digits[f] - 1; d >= 0; d--) {
					*out++ = kDigits[(fields[f] >> (d * 4)) & 0xF];
				}
			}
		}
		*out = 0;
	}

	/// Reads the first chunk of toHex()'s format. Returns false (shape unchanged) if it isn't valid.
	bool fromHex(const char* text) { return readHex(text, false); }
	/// Reads a further chunk, adding its points after the ones there. Returns false (shape unchanged) if it isn't
	/// valid or doesn't fit.
	bool appendHex(const char* text) { return readHex(text, true); }

private:
	bool readHex(const char* text, bool append) {
		Point read[kMaxPoints];
		int32_t count = append ? numPoints_ : 0;
		if (append) {
			std::memcpy(read, points_, numPoints_ * sizeof(Point));
		}
		const int32_t length = static_cast<int32_t>(std::strlen(text));
		if (length < 10 || length % 10 != 0 || count + length / 10 > kMaxPoints) {
			return false;
		}
		for (int32_t i = 0; i < length / 10; i++) {
			uint32_t values[3] = {0, 0, 0};
			const int32_t digits[3] = {4, 4, 2};
			const char* in = text + i * 10;
			for (int32_t f = 0; f < 3; f++) {
				for (int32_t d = 0; d < digits[f]; d++) {
					const int32_t nibble = hexValue(*in++);
					if (nibble < 0) {
						return false;
					}
					values[f] = (values[f] << 4) | static_cast<uint32_t>(nibble);
				}
			}
			read[count++] = {
			    static_cast<uint16_t>(values[0]), clampValue(static_cast<int16_t>(values[1])),
			    static_cast<int8_t>(std::clamp<int32_t>(static_cast<int8_t>(values[2]), -kMaxCurve, kMaxCurve))};
		}
		// Points must start at 0 and rise strictly
		if (read[0].x != 0) {
			return false;
		}
		for (int32_t i = 1; i < count; i++) {
			if (read[i].x <= read[i - 1].x) {
				return false;
			}
		}
		std::memcpy(points_, read, count * sizeof(Point));
		numPoints_ = count;
		return true;
	}

public:
	bool operator==(const LFOShape& other) const {
		if (numPoints_ != other.numPoints_) {
			return false;
		}
		for (int32_t i = 0; i < numPoints_; i++) {
			const Point& a = points_[i];
			const Point& b = other.points_[i];
			if (a.x != b.x || a.y != b.y || a.curve != b.curve) {
				return false;
			}
		}
		return true;
	}

private:
	int32_t numPoints_{0};
	Point points_[kMaxPoints]{};

	template <int32_t N>
	void set(const Point (&preset)[N]) {
		std::memcpy(points_, preset, sizeof(preset));
		numPoints_ = N;
	}

	static int16_t clampValue(int32_t y) { return static_cast<int16_t>(std::clamp<int32_t>(y, -kMaxValue, kMaxValue)); }

	static int32_t hexValue(char c) {
		if (c >= '0' && c <= '9') {
			return c - '0';
		}
		if (c >= 'A' && c <= 'F') {
			return c - 'A' + 10;
		}
		if (c >= 'a' && c <= 'f') {
			return c - 'a' + 10;
		}
		return -1;
	}

	/// Serum-style bend: a power curve with an exponent of up to 8 at |curve| = 100. Positive curves start slowly and
	/// end steep, negative ones are the mirror image (shoot off steeply, then settle).
	static float bend(float t, int8_t curve) {
		if (curve == 0 || t <= 0.0f || t >= 1.0f) {
			return std::clamp(t, 0.0f, 1.0f);
		}
		const float exponent = std::exp2(static_cast<float>(std::abs(curve)) * (3.0f / kMaxCurve));
		return (curve > 0) ? std::pow(t, exponent) : 1.0f - std::pow(1.0f - t, exponent);
	}
};

} // namespace deluge::modulation
