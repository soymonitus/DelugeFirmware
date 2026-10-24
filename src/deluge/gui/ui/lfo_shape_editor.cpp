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

#include "gui/ui/lfo_shape_editor.h"
#include "definitions_cxx.hpp"
#include "gui/colour/colour.h"
#include "gui/ui/ui.h"
#include "gui/ui_timer_manager.h"
#include "hid/buttons.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "memory/general_memory_allocator.h"
#include "processing/engines/audio_engine.h"
#include "processing/sound/sound.h"
#include "util/functions.h"
#include <cstdio>
#include <cstring>
#include <new>
#include <span>
#include <type_traits>

using deluge::modulation::LFOShape;
using namespace deluge::gui;
using namespace deluge::hid::display;

LFOShapeEditor lfoShapeEditor{};

namespace {

constexpr int32_t kNumPresets = static_cast<int32_t>(LFOShape::Preset::NUM_PRESETS);
constexpr const char* kPresetNames[kNumPresets] = {
    "Triangle",
    "Sine",
    "Saw up",
    "Saw down",
    "Square",
    "Pulse 25%",
    "Trapezoid",
    "Stairs",
    "Exp decay",
    "Exp rise",
    "Swell",
    "Shark fin",
    "Bumps",
    "S-curve",
    "Fin x3",
    "Decay x3",
    "Decay x3 round",
    "Tresillo",
    "Tresillo round",
    "3 over 2",
    "3 over 2 round",
    "Decay x5",
    "Decay x5 round",
    "Euclid 7/16",
    "Euclid 7/16 round",
    "Euclid 7/12",
    "Euclid 7/12 round",
    "Two-step",
    "Two-step round",
    "3-3-3-3-2-2",
    "3-3-3-3-2-2 round",
    "Ratchet",
    "Ratchet round",
    "Wobble 3-3-2",
    "Ramps 3-3-2",
    "Bounce",
    "Swing",
    "Heartbeat",
    "Zigzag",
    "Chaos",
};

constexpr int32_t kValueStep = LFOShape::kMaxValue / 50;      ///< 2 % of the height per detent...
constexpr int32_t kFineValueStep = LFOShape::kMaxValue / 200; ///< ...0.5 % with shift
constexpr int32_t kBendStep = 4;                              ///< Of LFOShape::kMaxCurve = 100
constexpr int32_t kMoveStepsPerPage = 64;                     ///< X encoder steps across the visible part
constexpr int32_t kFeedbackMs = 600;                          ///< How long an edit's text stays after the last change

/// Pad row (0 = bottom) showing a value: the height split into eight equal bands
int32_t rowForValue(int32_t value) {
	return std::clamp<int32_t>((value + LFOShape::kMaxValue) * kDisplayHeight / (2 * LFOShape::kMaxValue + 1), 0,
	                           kDisplayHeight - 1);
}

/// Value a pad row sets: bottom = -100 %, top = +100 %
int16_t valueForRow(int32_t row) {
	return static_cast<int16_t>(row * 2 * LFOShape::kMaxValue / (kDisplayHeight - 1) - LFOShape::kMaxValue);
}

int32_t percent(int32_t value) {
	return (value * 100 + (value >= 0 ? LFOShape::kMaxValue / 2 : -LFOShape::kMaxValue / 2)) / LFOShape::kMaxValue;
}

bool sameShape(const LFOShape& a, const LFOShape& b) {
	if (a.numPoints() != b.numPoints()) {
		return false;
	}
	for (int32_t i = 0; i < a.numPoints(); i++) {
		const LFOShape::Point& p = a.point(i);
		const LFOShape::Point& q = b.point(i);
		if (p.x != q.x || p.y != q.y || p.curve != q.curve) {
			return false;
		}
	}
	return true;
}

/// Puts new content into the part of a shape from start to start + width (the page shown while zoomed in), the rest
/// of the shape as it was. The content: its points, in order, the first one at start - and the value its last segment
/// runs to at the part's end. Where content and rest meet, the shape jumps if it has to, so that both keep their
/// values, and segments cut there keep their bends' look. Returns false if the points don't fit (result is then
/// incomplete).
bool replacePart(const LFOShape& shape, LFOShape& result, uint32_t start, uint32_t width,
                 std::span<const LFOShape::Point> content, int16_t contentEnd) {
	const uint32_t end = start + width; // Up to 65536, the cycle's end
	const bool wholeCycle = (start == 0 && end >= 65536);
	auto oldValue = [&](uint32_t x) { return static_cast<int16_t>(shape.valueAt((x & 0xFFFF) << 16) >> 16); };
	auto segmentEnd = [&](int32_t i) {
		return (i == shape.numPoints() - 1) ? 65536u : static_cast<uint32_t>(shape.point(i + 1).x);
	};
	auto add = [&](uint32_t x, int16_t y, int32_t curve) {
		const int32_t index = result.setPoint(static_cast<uint16_t>(x), y);
		if (index >= 0) {
			result.setCurve(index, curve);
		}
		return index >= 0;
	};

	result = shape;
	for (int32_t i = result.numPoints() - 1; i > 0; i--) { // The first point stays: the content's first replaces it
		if (result.point(i).x >= start && result.point(i).x < end) {
			result.removePoint(i);
		}
	}

	if (!wholeCycle) {
		const uint32_t beforeStart = (start + 65535) & 0xFFFF; // Wraps round to the cycle's end for the first page
		// After the part, the shape carries on as it was: from a point at the part's end (one that was there stays)
		if (end < 65536 && shape.pointAt(static_cast<uint16_t>(end)) < 0) {
			const int32_t i = shape.segmentAt(static_cast<uint16_t>(end));
			const uint32_t x0 = shape.point(i).x;
			const float from = static_cast<float>(end - x0) / static_cast<float>(segmentEnd(i) - x0);
			if (!add(end, oldValue(end), LFOShape::curveForPiece(shape.point(i).curve, from, 1.0f))) {
				return false;
			}
		}
		// Before it, the shape runs up to the value it had and jumps from there
		const bool jumpIn = oldValue(start) != content[0].y && shape.pointAt(static_cast<uint16_t>(beforeStart)) < 0;
		if (jumpIn && !add(beforeStart, oldValue(beforeStart), 0)) {
			return false;
		}
		// The segment running in, now cut short, keeps its look
		const int32_t i = shape.segmentAt(static_cast<uint16_t>(beforeStart));
		const uint32_t x0 = shape.point(i).x;
		const uint32_t newEnd = jumpIn ? beforeStart : ((start == 0) ? 65536u : start);
		const int32_t index = result.pointAt(static_cast<uint16_t>(x0));
		if (index >= 0 && (start > 0 || i > 0) && newEnd > x0) {
			const float to = static_cast<float>(newEnd - x0) / static_cast<float>(segmentEnd(i) - x0);
			result.setCurve(index, LFOShape::curveForPiece(shape.point(i).curve, 0.0f, to));
		}
	}

	for (const LFOShape::Point& p : content) {
		if (!add(p.x, p.y, p.curve)) {
			return false;
		}
	}
	// The content's end: its last segment runs to contentEnd (jumping on to the rest of the shape right at the end)
	const int16_t valueAtEnd = wholeCycle ? content[0].y : oldValue(end);
	if (contentEnd != valueAtEnd && content.back().x < end - 1 && !add(end - 1, contentEnd, 0)) {
		return false;
	}
	return true;
}

/// A preset squeezed into the part of a shape from start to start + width, see replacePart(). Over the whole cycle
/// it's just the preset.
bool loadPresetInto(const LFOShape& shape, LFOShape& result, LFOShape::Preset preset, uint32_t start, uint32_t width) {
	LFOShape full;
	full.loadPreset(preset);
	LFOShape::Point content[LFOShape::kMaxPoints];
	int32_t count = 0;
	int32_t lastX = -1;
	for (int32_t i = 0; i < full.numPoints(); i++) {
		const LFOShape::Point& p = full.point(i);
		// Points that land on the same spot (a jump) move apart
		const int32_t x = std::max<int32_t>(static_cast<int32_t>(start + ((p.x * width) >> 16)), lastX + 1);
		if (x >= static_cast<int32_t>(start + width)) {
			break;
		}
		content[count++] = {static_cast<uint16_t>(x), p.y, p.curve};
		lastX = x;
	}
	return replacePart(shape, result, start, width, {content, static_cast<size_t>(count)}, full.point(0).y);
}

/// The part of a shape from start to start + width played backwards, see replacePart()
bool reversePart(const LFOShape& shape, LFOShape& result, uint32_t start, uint32_t width) {
	const uint32_t end = start + width;
	auto oldValue = [&](uint32_t x) { return static_cast<int16_t>(shape.valueAt((x & 0xFFFF) << 16) >> 16); };

	// The part as nodes - its start, the points in it, its end - with the bend of the piece from each to the next
	struct Node {
		uint32_t x;
		int16_t y;
		int32_t curve;
	};
	Node nodes[LFOShape::kMaxPoints + 2];
	int32_t count = 0;
	nodes[count++] = {start, oldValue(start), 0};
	for (int32_t i = 0; i < shape.numPoints(); i++) {
		if (shape.point(i).x > start && shape.point(i).x < end) {
			nodes[count++] = {shape.point(i).x, shape.point(i).y, 0};
		}
	}
	nodes[count++] = {end, oldValue(end), 0}; // What the part runs to
	// A jump right at the part's end (a point one step before it) belongs to the edge, not to the part: the part runs
	// to that point's value. Backwards it would otherwise start with a one-step spike.
	if (count >= 3 && nodes[count - 2].x == end - 1) {
		nodes[count - 2].x = end;
		count--;
	}
	for (int32_t j = 0; j < count - 1; j++) {
		const int32_t i = shape.segmentAt(static_cast<uint16_t>(nodes[j].x));
		const uint32_t x0 = shape.point(i).x;
		const uint32_t x1 = (i == shape.numPoints() - 1) ? 65536u : static_cast<uint32_t>(shape.point(i + 1).x);
		const float length = static_cast<float>(x1 - x0);
		nodes[j].curve = LFOShape::curveForPiece(shape.point(i).curve, static_cast<float>(nodes[j].x - x0) / length,
		                                         static_cast<float>(std::min(nodes[j + 1].x, x1) - x0) / length);
	}

	// Backwards: mirrored in time, the last piece first, every bend turned round
	LFOShape::Point content[LFOShape::kMaxPoints + 1];
	int32_t numContent = 0;
	for (int32_t j = count - 1; j >= 1; j--) {
		content[numContent++] = {static_cast<uint16_t>(start + end - nodes[j].x), nodes[j].y,
		                         static_cast<int8_t>(-nodes[j - 1].curve)};
	}
	return replacePart(shape, result, start, width, {content, static_cast<size_t>(numContent)}, nodes[0].y);
}

} // namespace

LFOShape* LFOShapeEditor::shape() {
	return (sound_ != nullptr) ? sound_->lfoConfig[lfoId_].shape : nullptr;
}

bool LFOShapeEditor::openFor(Sound* sound, uint8_t lfoId) {
	sound_ = sound;
	lfoId_ = lfoId;
	if (shape() == nullptr) {
		return false;
	}
	return openUI(this);
}

bool LFOShapeEditor::opened() {
	// The sound editor underneath keeps blinking its shortcut pads otherwise, right over the shape. It sets the blink
	// up again when it comes back.
	uiTimerManager.unsetTimer(TimerName::SHORTCUT_BLINK);
	zoomLevel_ = 0;
	page_ = 0;
	feedbackShown_ = false;
	yTurnedWhilePressed_ = true; // A Y encoder held since before opening adds nothing on release
	select(0);
	startHistory();
	focusRegained();
	return true;
}

ActionResult LFOShapeEditor::exitUI() {
	closeEditor();
	return ActionResult::DEALT_WITH;
}

void LFOShapeEditor::closeEditor() {
	endGesture();           // The history stays, see startHistory()
	feedbackShown_ = false; // closeUI() stops its timer
	closeUI(this);
}

void LFOShapeEditor::feedback(const char* text) {
	if (!display->haveOLED()) {
		display->displayPopup(text);
		return;
	}
	snprintf(feedback_, sizeof(feedback_), "%s", text);
	feedbackShown_ = true;
	uiTimerManager.setTimer(TimerName::UI_SPECIFIC, kFeedbackMs); // Counts again from each new change
	                                                              // Every edit ends with changed(), which draws it
}

ActionResult LFOShapeEditor::timerCallback() {
	if (feedbackShown_) {
		feedbackShown_ = false;
		renderUIsForOled();
	}
	return ActionResult::DEALT_WITH;
}

void LFOShapeEditor::insertPointAfterSelected() {
	LFOShape* s = shape();
	if (s == nullptr) {
		return;
	}
	if (s->numPoints() >= LFOShape::kMaxPoints) {
		feedback("FULL");
		return;
	}
	const uint32_t x0 = s->point(selected_).x;
	const uint32_t x1 = (selected_ == s->numPoints() - 1) ? 65536u : s->point(selected_ + 1).x;
	if (x1 - x0 < 2) {
		feedback("NO ROOM");
		return;
	}
	const uint32_t x = x0 + (x1 - x0) / 2;
	beginEdit(Edit::OTHER);
	// On the shape where it goes, and it takes over the segment's bend (see LFOShape::setPoint())
	const int32_t index = s->setPoint(static_cast<uint16_t>(x), static_cast<int16_t>(s->valueAt(x << 16) >> 16));
	select(index);
	commitEdit();
	char buffer[24];
	snprintf(buffer, sizeof(buffer), "Added %d/%d", static_cast<int>(selected_ + 1), static_cast<int>(s->numPoints()));
	feedback(buffer);
}

void LFOShapeEditor::reversePage() {
	LFOShape* s = shape();
	if (s == nullptr) {
		return;
	}
	const uint32_t start = pageStart();
	const uint32_t end = start + pageWidth();
	LFOShape result;
	if (!reversePart(*s, result, start, pageWidth())) {
		feedback("FULL");
		return;
	}
	const uint32_t selectedX = s->point(selected_).x;
	beginEdit(Edit::OTHER);
	*s = result;
	// The selected point goes along with its part of the shape
	selectAt((selectedX > start && selectedX < end) ? start + end - selectedX : start);
	commitEdit();
	if (display->haveOLED() && numPages() > 1) {
		char buffer[24];
		snprintf(buffer, sizeof(buffer), "Reversed %d/%d", static_cast<int>(page_ + 1), static_cast<int>(numPages()));
		feedback(buffer);
	}
	else {
		feedback(display->haveOLED() ? "Reversed" : "REV");
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Undo / redo

static_assert(std::is_trivially_copyable_v<deluge::modulation::LFOShape>, "States get moved with memmove");
static_assert(std::is_trivially_destructible_v<deluge::modulation::LFOShape>, "States get freed without destructors");

void LFOShapeEditor::startHistory() {
	endGesture();
	LFOShape* s = shape();

	// Back on the LFO the history belongs to, and nothing else changed its shape meanwhile (a preset load, another
	// song...): carry on where it left off, so leaving the editor doesn't cost the undo steps
	if (history_ != nullptr && historyCount_ > 0 && sound_ == historySound_ && lfoId_ == historyLfo_ && s != nullptr
	    && sameShape(history_[historyCursor_].shape, *s)) {
		select(history_[historyCursor_].selected);
		return;
	}

	if (history_ == nullptr) {
		// Kept from then on: a few KB, and the next LFO's history reuses it
		void* memory = GeneralMemoryAllocator::get().allocLowSpeed(sizeof(State) * kHistorySize);
		if (memory != nullptr) {
			history_ = static_cast<State*>(memory);
			for (int32_t i = 0; i < kHistorySize; i++) {
				new (&history_[i]) State{};
			}
		}
	}
	historyCount_ = 0;
	historyCursor_ = 0;
	historySound_ = sound_;
	historyLfo_ = lfoId_;
	if (history_ != nullptr && s != nullptr) {
		history_[0] = {*s, selected_};
		historyCount_ = 1;
	}
	// Without the memory the editor just works without undo
}

void LFOShapeEditor::beginEdit(Edit edit) {
	const uint32_t now = AudioEngine::audioSampleTimer;
	// Presets replace the whole shape, so browsing on carries on whichever point was selected
	const bool samePoint = (edit == Edit::PRESET) || selected_ == lastEditPoint_;
	const bool carriesOn = edit != Edit::OTHER && edit == lastEdit_ && samePoint
	                       && now - lastEditTime_ < static_cast<uint32_t>(kSampleRate);
	lastEdit_ = edit;
	lastEditPoint_ = selected_;
	lastEditTime_ = now;
	if (carriesOn || history_ == nullptr || historyCount_ == 0) {
		return;
	}
	// A new step: what could have been redone is gone, and the oldest step goes when the history is full
	if (historyCursor_ == kHistorySize - 1) {
		std::memmove(&history_[0], &history_[1], sizeof(State) * (kHistorySize - 1));
		historyCursor_--;
	}
	historyCursor_++;
	historyCount_ = historyCursor_ + 1;
}

void LFOShapeEditor::commitEdit() {
	if (history_ != nullptr && historyCount_ > 0 && shape() != nullptr) {
		history_[historyCursor_] = {*shape(), selected_};
	}
}

void LFOShapeEditor::restore(const State& state) {
	if (LFOShape* s = shape()) {
		*s = state.shape;
		select(state.selected);
	}
}

void LFOShapeEditor::stepHistory(int32_t steps) {
	endGesture();
	const bool backwards = steps < 0;
	int32_t target = historyCursor_;
	if (history_ != nullptr && historyCount_ > 0) {
		target = std::clamp<int32_t>(historyCursor_ + steps, 0, historyCount_ - 1);
	}
	if (target == historyCursor_) {
		feedback(backwards ? "NO UNDO" : "NO REDO");
		return;
	}
	historyCursor_ = target;
	restore(history_[historyCursor_]);
	if (display->haveOLED()) {
		// Where in the history that leaves it: "UNDO 3/7" = step 3 of 7
		char buffer[24];
		snprintf(buffer, sizeof(buffer), "%s %d/%d", backwards ? "UNDO" : "REDO", static_cast<int>(historyCursor_),
		         static_cast<int>(historyCount_ - 1));
		feedback(buffer);
	}
	else {
		feedback(backwards ? "UNDO" : "REDO");
	}
}

void LFOShapeEditor::focusRegained() {
	changed();
}

void LFOShapeEditor::changed() {
	uiNeedsRendering(this, 0xFFFFFFFF, 0xFFFFFFFF);
	if (display->haveOLED()) {
		renderUIsForOled();
	}
	else {
		char buffer[8];
		snprintf(buffer, sizeof(buffer), "P%d", static_cast<int>(selected_ + 1));
		display->setText(buffer);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Selection and view

void LFOShapeEditor::select(int32_t index) {
	LFOShape* s = shape();
	if (s == nullptr) {
		return;
	}
	selected_ = std::clamp<int32_t>(index, 0, s->numPoints() - 1);
	page_ = std::min<int32_t>(static_cast<int32_t>(s->point(selected_).x / pageWidth()), numPages() - 1);
}

void LFOShapeEditor::selectAt(uint32_t x) {
	if (LFOShape* s = shape()) {
		select(s->segmentAt(static_cast<uint16_t>(std::min<uint32_t>(x, 65535))));
	}
}

void LFOShapeEditor::selectStep(int32_t steps) {
	LFOShape* s = shape();
	if (s == nullptr) {
		return;
	}
	endGesture();
	const int32_t n = s->numPoints();
	select(((selected_ + steps) % n + n) % n);
}

void LFOShapeEditor::showPage(int32_t page) {
	LFOShape* s = shape();
	if (s == nullptr) {
		return;
	}
	endGesture();
	page_ = std::clamp<int32_t>(page, 0, numPages() - 1);
	const uint32_t start = pageStart();
	const uint32_t end = start + pageWidth();
	for (int32_t i = 0; i < s->numPoints(); i++) {
		if (s->point(i).x >= start && s->point(i).x < end) {
			selected_ = i;
			break;
		}
	}
}

void LFOShapeEditor::zoom(int32_t direction) {
	endGesture();
	zoomLevel_ = std::clamp<int32_t>(zoomLevel_ + ((direction > 0) ? 1 : -1), 0, kMaxZoomLevel);
	select(selected_); // The page with the selected point
	char buffer[24];
	snprintf(buffer, sizeof(buffer), "Zoom x%d", static_cast<int>(numPages()));
	feedback(buffer);
}

// ---------------------------------------------------------------------------------------------------------------
// Editing

bool LFOShapeEditor::splitComplete(int32_t level) {
	LFOShape* s = shape();
	const uint32_t step = pageWidth() >> level;
	for (uint32_t x = pageStart(); x < pageStart() + pageWidth(); x += step) {
		if (s->pointAt(static_cast<uint16_t>(x)) < 0) {
			return false;
		}
	}
	return true;
}

// Splits the visible part into 2, 4, 8, 16 equal parts, one level per detent - or takes the finest split back out.
// The new points sit on the shape as it is, so splitting changes nothing until they're moved.
void LFOShapeEditor::split(int32_t direction) {
	LFOShape* s = shape();
	if (s == nullptr) {
		return;
	}
	const uint16_t keepX = s->point(selected_).x;
	char buffer[24];

	if (direction > 0) {
		for (int32_t level = 1; level <= kMaxSplitLevel; level++) {
			if (splitComplete(level)) {
				continue;
			}
			// Read the shape first: every point added bends the segments it splits a little differently
			const uint32_t step = pageWidth() >> level;
			uint16_t newX[1 << kMaxSplitLevel];
			int16_t newY[1 << kMaxSplitLevel];
			int32_t count = 0;
			for (uint32_t x = pageStart(); x < pageStart() + pageWidth(); x += step) {
				if (s->pointAt(static_cast<uint16_t>(x)) < 0) {
					newX[count] = static_cast<uint16_t>(x);
					newY[count] = static_cast<int16_t>(s->valueAt(x << 16) >> 16);
					count++;
				}
			}
			if (s->numPoints() + count > LFOShape::kMaxPoints) {
				feedback("FULL");
				return;
			}
			beginEdit(Edit::OTHER);
			for (int32_t i = 0; i < count; i++) {
				s->setPoint(newX[i], newY[i]);
			}
			selectAt(keepX);
			commitEdit();
			snprintf(buffer, sizeof(buffer), "%d parts", 1 << level);
			feedback(buffer);
			return;
		}
		feedback("MAX");
		return;
	}

	for (int32_t level = kMaxSplitLevel; level >= 1; level--) {
		if (!splitComplete(level)) {
			continue;
		}
		// Every other point of this split goes - the coarser split's points stay
		const uint32_t step = pageWidth() >> level;
		beginEdit(Edit::OTHER);
		for (uint32_t x = pageStart() + step; x < pageStart() + pageWidth(); x += 2 * step) {
			s->removePoint(s->pointAt(static_cast<uint16_t>(x)));
		}
		selectAt(keepX);
		commitEdit();
		if (level > 1) {
			snprintf(buffer, sizeof(buffer), "%d parts", 1 << (level - 1));
			feedback(buffer);
		}
		else {
			feedback("NO SPLIT");
		}
		return;
	}
	feedback("NO SPLIT");
}

void LFOShapeEditor::moveSelected(int32_t offset) {
	LFOShape* s = shape();
	if (s == nullptr) {
		return;
	}
	if (selected_ == 0) {
		feedback("FIXED"); // The first point anchors the cycle at its start
		return;
	}
	const int32_t steps = Buttons::isShiftButtonPressed() ? kMoveStepsPerPage * 4 : kMoveStepsPerPage;
	const int32_t step = std::max<int32_t>(static_cast<int32_t>(pageWidth()) / steps, 1);
	beginEdit(Edit::MOVE);
	const uint16_t x = s->movePoint(selected_, s->point(selected_).x + offset * step);
	commitEdit();
	page_ = std::min<int32_t>(static_cast<int32_t>(x / pageWidth()), numPages() - 1);
	popupPosition();
}

void LFOShapeEditor::changeValue(int32_t offset) {
	LFOShape* s = shape();
	if (s == nullptr) {
		return;
	}
	const int32_t step = Buttons::isShiftButtonPressed() ? kFineValueStep : kValueStep;
	beginEdit(Edit::VALUE);
	s->setValue(selected_, static_cast<int16_t>(std::clamp<int32_t>(s->point(selected_).y + offset * step,
	                                                                -LFOShape::kMaxValue, LFOShape::kMaxValue)));
	commitEdit();
	popupValue();
}

// Turning right bows the segment upwards, whichever way it runs (the curve's sign depends on that)
void LFOShapeEditor::bend(int32_t offset, bool segmentBefore) {
	LFOShape* s = shape();
	if (s == nullptr) {
		return;
	}
	const int32_t n = s->numPoints();
	const int32_t segment = segmentBefore ? (selected_ - 1 + n) % n : selected_;
	const int32_t y0 = s->point(segment).y;
	const int32_t y1 = s->point((segment + 1) % n).y;
	const int32_t sign = (y1 > y0) ? -1 : 1;
	beginEdit(segmentBefore ? Edit::BEND_BEFORE : Edit::BEND_AFTER);
	s->setCurve(segment, s->point(segment).curve + sign * offset * kBendStep);
	commitEdit();
	char buffer[24];
	snprintf(buffer, sizeof(buffer), "%s %+d", segmentBefore ? "Bend <" : "Bend >",
	         static_cast<int>(s->point(segment).curve * sign));
	feedback(buffer);
}

void LFOShapeEditor::deleteSelected() {
	LFOShape* s = shape();
	if (s == nullptr) {
		return;
	}
	if (selected_ == 0) {
		feedback("FIXED");
		return;
	}
	const int32_t index = selected_;
	beginEdit(Edit::OTHER);
	s->removePoint(index);
	select(index - 1);
	commitEdit();
	feedback("DELETED");
}

void LFOShapeEditor::loadPreset(int32_t index) {
	LFOShape* s = shape();
	if (s == nullptr) {
		return;
	}
	presetIndex_ = ((index % kNumPresets) + kNumPresets) % kNumPresets;
	// Zoomed in, it goes into the page shown (squeezed in, the rest of the shape stays), otherwise it's the whole shape
	LFOShape result;
	if (!loadPresetInto(*s, result, static_cast<LFOShape::Preset>(presetIndex_), pageStart(), pageWidth())) {
		char buffer[24];
		snprintf(buffer, sizeof(buffer), "%s FULL", kPresetNames[presetIndex_]);
		feedback(display->haveOLED() ? buffer : "FULL");
		return;
	}
	// Browsing through the presets on and on makes a single undo step
	beginEdit(Edit::PRESET);
	*s = result;
	if (display->haveOLED()) {
		char buffer[24];
		snprintf(buffer, sizeof(buffer), "%s %d/%d", kPresetNames[presetIndex_], static_cast<int>(presetIndex_ + 1),
		         static_cast<int>(kNumPresets));
		feedback(buffer);
	}
	else {
		feedback(kPresetNames[presetIndex_]);
	}
	selectAt(pageStart()); // Its first point
	commitEdit();
}

void LFOShapeEditor::popupPoint() {
	LFOShape* s = shape();
	if (s == nullptr) {
		return;
	}
	char buffer[24];
	snprintf(buffer, sizeof(buffer), "Point %d/%d", static_cast<int>(selected_ + 1), static_cast<int>(s->numPoints()));
	feedback(buffer);
}

void LFOShapeEditor::popupValue() {
	LFOShape* s = shape();
	if (s == nullptr) {
		return;
	}
	char buffer[24];
	snprintf(buffer, sizeof(buffer), "%+d%%", static_cast<int>(percent(s->point(selected_).y)));
	feedback(buffer);
}

void LFOShapeEditor::popupPosition() {
	LFOShape* s = shape();
	if (s == nullptr) {
		return;
	}
	const int32_t permille = static_cast<int32_t>(s->point(selected_).x) * 1000 / 65536;
	char buffer[24];
	snprintf(buffer, sizeof(buffer), "%d.%d%%", static_cast<int>(permille / 10), static_cast<int>(permille % 10));
	feedback(buffer);
}

void LFOShapeEditor::popupPage() {
	char buffer[24];
	snprintf(buffer, sizeof(buffer), "Page %d/%d", static_cast<int>(page_ + 1), static_cast<int>(numPages()));
	feedback(buffer);
}

// ---------------------------------------------------------------------------------------------------------------
// Controls

ActionResult LFOShapeEditor::buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) {
	using namespace deluge::hid::button;
	// Only these buttons belong to the editor. Everything else goes on to the usual handling - shift above all, whose
	// release must get through or shift stays held (also after leaving).
	if (b != BACK && b != SELECT_ENC && b != SYNTH && b != KIT && b != MIDI && b != CV && b != Y_ENC && b != SCALE_MODE
	    && b != CROSS_SCREEN_EDIT) {
		return ActionResult::NOT_DEALT_WITH;
	}
	if (inCardRoutine) {
		return ActionResult::REMIND_ME_OUTSIDE_CARD_ROUTINE;
	}

	// Y encoder: shift + press loads the next preset; a press without turning (turning splits) adds a point on release
	if (b == Y_ENC) {
		if (on) {
			yTurnedWhilePressed_ = false;
			if (Buttons::isShiftButtonPressed()) {
				yTurnedWhilePressed_ = true; // Not a point to add on release
				loadPreset(presetIndex_ + 1);
				changed();
			}
		}
		else if (!yTurnedWhilePressed_) {
			insertPointAfterSelected();
			changed();
		}
		return ActionResult::DEALT_WITH;
	}

	if (!on) {
		return ActionResult::DEALT_WITH;
	}
	// Back leaves, as in every menu - straight away, so holding it on leaves the sound editor as well (its long-press
	// timer is already running). Undo / redo have the sidebar pads and shift + select.
	if (b == BACK || b == SELECT_ENC) {
		closeEditor();
		return ActionResult::DEALT_WITH;
	}
	// Scale / Cross Screen turn the page while zoomed in, as in the horizontal menus - round and round the cycle
	if (b == SCALE_MODE || b == CROSS_SCREEN_EDIT) {
		if (numPages() > 1) {
			showPage((page_ + ((b == SCALE_MODE) ? numPages() - 1 : 1)) % numPages());
			popupPage();
		}
		else {
			feedback("ZOOM IN");
		}
	}
	else {
		selectStep((b == SYNTH || b == KIT) ? -1 : 1);
		popupPoint();
	}
	changed();
	return ActionResult::DEALT_WITH;
}

ActionResult LFOShapeEditor::padAction(int32_t x, int32_t y, int32_t velocity) {
	if (velocity == 0) {
		return ActionResult::DEALT_WITH;
	}
	LFOShape* s = shape();
	if (s == nullptr) {
		return ActionResult::DEALT_WITH;
	}

	// Sidebar: the first column's pads jump to a page, the second column has undo and redo
	if (x >= kDisplayWidth) {
		if (x == kDisplayWidth && y < numPages()) {
			showPage(y);
			popupPage();
			changed();
		}
		else if (x == kDisplayWidth + 1 && (y == kUndoPadRow || y == kRedoPadRow)) {
			stepHistory((y == kUndoPadRow) ? -1 : 1);
			changed();
		}
		else if (x == kDisplayWidth + 1 && y == kReversePadRow) {
			reversePage();
			changed();
		}
		return ActionResult::DEALT_WITH;
	}

	const uint32_t columnStart = pageStart() + static_cast<uint32_t>(x) * columnWidth();
	const uint32_t columnEnd = columnStart + columnWidth();
	auto inColumn = [&](int32_t i) { return s->point(i).x >= columnStart && s->point(i).x < columnEnd; };

	int32_t firstInColumn = -1;
	for (int32_t i = 0; i < s->numPoints(); i++) {
		if (inColumn(i)) {
			firstInColumn = i;
			break;
		}
	}

	if (Buttons::isShiftButtonPressed()) {
		// Removes the points in the column (the first point of the cycle stays)
		if (firstInColumn >= 0 && (firstInColumn > 0 || (s->numPoints() > 1 && inColumn(1)))) {
			beginEdit(Edit::OTHER);
			for (int32_t i = s->numPoints() - 1; i > 0; i--) {
				if (inColumn(i)) {
					s->removePoint(i);
				}
			}
			selectAt(columnStart);
			commitEdit();
		}
	}
	else if (firstInColumn < 0 && s->numPoints() >= LFOShape::kMaxPoints) {
		feedback("FULL");
	}
	else {
		// A point already in this column gets the new height, otherwise there's a new point
		beginEdit(Edit::OTHER);
		int32_t index = firstInColumn;
		if (index >= 0) {
			s->setValue(index, valueForRow(y));
		}
		else {
			index = s->setPoint(static_cast<uint16_t>(columnStart), valueForRow(y));
		}
		select(index);
		commitEdit();
		popupValue();
	}
	changed();
	return ActionResult::DEALT_WITH;
}

ActionResult LFOShapeEditor::horizontalEncoderAction(int32_t offset) {
	if (Buttons::isButtonPressed(deluge::hid::button::X_ENC)) {
		zoom(offset);
	}
	else {
		moveSelected(offset);
	}
	changed();
	return ActionResult::DEALT_WITH;
}

ActionResult LFOShapeEditor::verticalEncoderAction(int32_t offset, bool inCardRoutine) {
	if (inCardRoutine) {
		return ActionResult::REMIND_ME_OUTSIDE_CARD_ROUTINE;
	}
	if (Buttons::isButtonPressed(deluge::hid::button::Y_ENC)) {
		yTurnedWhilePressed_ = true;
		split(offset);
	}
	else if (Buttons::isShiftButtonPressed()) {
		// Browse the presets; from none yet, left starts at the last one
		loadPreset((presetIndex_ < 0 && offset < 0) ? kNumPresets - 1 : presetIndex_ + offset);
	}
	else {
		selectStep(offset);
		popupPoint();
	}
	changed();
	return ActionResult::DEALT_WITH;
}

void LFOShapeEditor::selectEncoderAction(int8_t offset) {
	if (Buttons::isShiftButtonPressed()) {
		stepHistory(offset); // Runs through the undo history, a step per detent
	}
	else {
		selectStep(offset);
		popupPoint();
	}
	changed();
}

void LFOShapeEditor::modEncoderAction(int32_t whichModEncoder, int32_t offset) {
	if (whichModEncoder == 1) { // Upper gold knob
		changeValue(offset);
	}
	else {
		bend(offset, Buttons::isButtonPressed(deluge::hid::button::MOD_ENCODER_0));
	}
	changed();
}

void LFOShapeEditor::modEncoderButtonAction(uint8_t whichModEncoder, bool on) {
	// The lower knob's press only changes what turning it does, see modEncoderAction()
	if (whichModEncoder == 1 && on) {
		deleteSelected();
		changed();
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Drawing

bool LFOShapeEditor::renderMainPads(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                                    uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], bool drawUndefinedArea) {
	if (image == nullptr) {
		return true;
	}
	LFOShape* s = shape();
	const int32_t zeroRow = rowForValue(0);
	for (int32_t x = 0; x < kDisplayWidth; x++) {
		for (int32_t y = 0; y < kDisplayHeight; y++) {
			image[y][x] = colours::black;
		}
		if (s == nullptr) {
			continue;
		}
		// The shape as a bar from the centre line, read in the middle of the column
		const uint32_t columnStart = pageStart() + static_cast<uint32_t>(x) * columnWidth();
		const int32_t row = rowForValue(s->valueAt((columnStart + columnWidth() / 2) << 16) >> 16);
		for (int32_t y = std::min(row, zeroRow); y <= std::max(row, zeroRow); y++) {
			image[y][x] = colours::blue.dim(2);
		}
		image[row][x] = colours::blue;

		// Points in this column, at their own height - the selected one white
		for (int32_t i = 0; i < s->numPoints(); i++) {
			const uint32_t pointX = s->point(i).x;
			if (pointX >= columnStart && pointX < columnStart + columnWidth()) {
				image[rowForValue(s->point(i).y)][x] = (i == selected_) ? colours::white : colours::cyan;
			}
		}
	}
	return true;
}

bool LFOShapeEditor::renderSidebar(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
                                   uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) {
	if (image == nullptr) {
		return true;
	}
	// The pages while zoomed in, bottom up: the visible one bright
	for (int32_t y = 0; y < kDisplayHeight; y++) {
		image[y][kDisplayWidth] = (numPages() > 1 && y < numPages())
		                              ? ((y == page_) ? colours::white : colours::blue.dim(2))
		                              : colours::black;
		image[y][kDisplayWidth + 1] = colours::black;
	}
	// Undo and redo: bright while there's a step to take (dark without the memory for a history)
	if (history_ != nullptr) {
		image[kUndoPadRow][kDisplayWidth + 1] = canUndo() ? colours::red : colours::red.dim(3);
		image[kRedoPadRow][kDisplayWidth + 1] = canRedo() ? colours::green : colours::green.dim(3);
	}
	image[kReversePadRow][kDisplayWidth + 1] = colours::magenta; // Plays the page shown backwards
	return true;
}

void LFOShapeEditor::drawShape(oled_canvas::Canvas& canvas, const LFOShape& shape, int32_t minX, int32_t minY,
                               int32_t maxX, int32_t maxY) {
	const int32_t width = maxX - minX;
	const int32_t height = maxY - minY;
	int32_t previousY = -1;
	for (int32_t px = 0; px <= width; px++) {
		const uint32_t phase = static_cast<uint32_t>((static_cast<uint64_t>(px) << 32) / (width + 1));
		const int32_t value = shape.valueAt(phase) >> 16; // -32767..32767
		const int32_t y = maxY - (value + LFOShape::kMaxValue) * height / (2 * LFOShape::kMaxValue);
		if (previousY >= 0) {
			canvas.drawLine(minX + px - 1, previousY, minX + px, y);
		}
		else {
			canvas.drawPixel(minX + px, y);
		}
		previousY = y;
	}
}

void LFOShapeEditor::renderOLED(oled_canvas::Canvas& canvas) {
	LFOShape* s = shape();
	if (feedbackShown_) {
		// Instead of the title and page, in the title's font if it fits, like "+42%" - longer ones (preset names) small
		constexpr int32_t y = 1 + OLED_MAIN_TOPMOST_PIXEL;
		if (canvas.getStringWidthInPixels(feedback_, kTextTitleSizeY) <= OLED_MAIN_WIDTH_PIXELS) {
			canvas.drawString(feedback_, 0, y, kTextTitleSpacingX, kTextTitleSizeY);
		}
		else {
			canvas.drawString(feedback_, 0, y + 1, kTextSpacingX, kTextSpacingY);
		}
		canvas.drawHorizontalLine(kScreenTitleSeparatorY, 0, OLED_MAIN_WIDTH_PIXELS - 1);
	}
	else {
		renderTitle(canvas);
	}
	if (s == nullptr) {
		return;
	}
	renderShape(canvas, *s);
}

void LFOShapeEditor::renderTitle(oled_canvas::Canvas& canvas) {
	LFOShape* s = shape();
	char title[24];
	if (s != nullptr) {
		snprintf(title, sizeof(title), "LFO%d P%d/%d", static_cast<int>(lfoId_ + 1), static_cast<int>(selected_ + 1),
		         static_cast<int>(s->numPoints()));
	}
	else {
		snprintf(title, sizeof(title), "LFO%d", static_cast<int>(lfoId_ + 1));
	}
	canvas.drawScreenTitle(title);

	// The page top right, like the horizontal menus do it: "2/4"
	{
		constexpr int32_t y = 1 + OLED_MAIN_TOPMOST_PIXEL;
		char pageText[8];
		snprintf(pageText, sizeof(pageText), "%d/%d", static_cast<int>(page_ + 1), static_cast<int>(numPages()));
		canvas.drawStringAlignRight(pageText, y, kTextSpacingX, kTextSpacingY);
	}
}

void LFOShapeEditor::renderShape(oled_canvas::Canvas& canvas, const LFOShape& s) {
	constexpr int32_t kMinX = 2;
	constexpr int32_t kMaxX = OLED_MAIN_WIDTH_PIXELS - 3;
	constexpr int32_t kMinY = kScreenTitleSeparatorY + 3;
	constexpr int32_t kMaxY = OLED_MAIN_HEIGHT_PIXELS - 4;
	constexpr int32_t kWidth = kMaxX - kMinX;
	constexpr int32_t kHeight = kMaxY - kMinY;
	const uint32_t start = pageStart();
	const uint32_t visible = pageWidth();

	auto toScreenY = [&](int32_t value) {
		return kMaxY - (value + LFOShape::kMaxValue) * kHeight / (2 * LFOShape::kMaxValue);
	};

	// Centre line, dotted, and a tick under every pad column
	const int32_t zeroY = toScreenY(0);
	for (int32_t x = kMinX; x <= kMaxX; x += 3) {
		canvas.drawPixel(x, zeroY);
	}
	for (int32_t column = 0; column <= kDisplayWidth; column++) {
		canvas.drawPixel(kMinX + column * kWidth / kDisplayWidth, kMaxY + 2);
	}

	// The visible part of the shape
	int32_t previousY = -1;
	for (int32_t px = 0; px <= kWidth; px++) {
		const uint32_t x = start + static_cast<uint32_t>(static_cast<uint64_t>(px) * visible / (kWidth + 1));
		const int32_t y = toScreenY(s.valueAt(x << 16) >> 16);
		if (previousY >= 0) {
			canvas.drawLine(kMinX + px - 1, previousY, kMinX + px, y);
		}
		previousY = y;
	}

	// Its points: small squares, the selected one bigger and filled
	for (int32_t i = 0; i < s.numPoints(); i++) {
		const uint32_t pointX = s.point(i).x;
		if (pointX < start || pointX >= start + visible) {
			continue;
		}
		const int32_t px = kMinX + static_cast<int32_t>(static_cast<uint64_t>(pointX - start) * kWidth / visible);
		const int32_t py = toScreenY(s.point(i).y);
		if (i == selected_) {
			canvas.invertArea(px - 2, 5, py - 2, py + 2);
			canvas.drawRectangle(px - 3, py - 3, px + 3, py + 3);
		}
		else {
			canvas.drawRectangle(px - 1, py - 1, px + 1, py + 1);
		}
	}
}
