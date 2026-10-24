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

#include "gui/ui/ui.h"
#include "hid/button.h"
#include "modulation/lfo_shape.h"

class Sound;

/// Full-screen editor for a custom LFO shape (LFOType::CUSTOM): points joined by segments that can be bent, Serum
/// style. One point is always selected.
///
/// - Pads: set a point in that column at that height (a point already in the column gets the height), shift + pad
///   removes the points in the column. The pads show the visible part of the cycle.
/// - X encoder: moves the selected point in time. Pressed: zooms in / out (x1, x2, x4, x8).
/// - Y encoder: selects the previous / next point. Pressed: to the right splits the visible part into ever more equal
///   parts (2, 4, 8, 16) by adding points along the shape, to the left takes the finest split back out.
///   Press (without turning): adds a point halfway to the next one, on the shape. Shift + press: loads the next
///   preset shape, shift + turn: browses the presets both ways. Zoomed in, a preset only goes into the page shown,
///   squeezed in - the rest of the shape stays.
/// - Upper gold knob: the selected point's height. Press: deletes the selected point.
/// - Lower gold knob: bends the segment after the selected point. Pressed: bends the segment before it.
/// - Synth / Kit: previous point, MIDI / CV: next point. Scale / Cross Screen: the previous / next page while zoomed
///   in, as in the horizontal menus (round and round the cycle).
/// - Select encoder: selects points, like the Y encoder. Shift + select: undo (left) / redo (right), a step per detent.
/// - Sidebar: the first column's pads jump to a page; in the second column the bottom pad undoes and the one above it
///   redoes (lit bright while there's a step to take; turning a knob on and on is one step), and the fourth one up
///   (magenta) plays the page shown backwards - the whole shape when not zoomed in.
/// - Back or a select press: closes the editor, as in every menu. The undo history stays for when it opens again on the
///   same LFO.
class LFOShapeEditor final : public UI {
public:
	LFOShapeEditor() { oledShowsUIUnderneath = false; }

	/// Opens the editor on one LFO of a sound. The LFO must be CUSTOM (so its shape exists).
	bool openFor(Sound* sound, uint8_t lfoId);

	bool opened() override;
	void focusRegained() override;
	ActionResult buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) override;
	ActionResult padAction(int32_t x, int32_t y, int32_t velocity) override;
	ActionResult horizontalEncoderAction(int32_t offset) override;
	ActionResult verticalEncoderAction(int32_t offset, bool inCardRoutine) override;
	void selectEncoderAction(int8_t offset) override;
	void modEncoderAction(int32_t whichModEncoder, int32_t offset) override;
	void modEncoderButtonAction(uint8_t whichModEncoder, bool on) override;
	void modButtonAction(uint8_t whichButton, bool on) override {} // The mod buttons do nothing here
	ActionResult exitUI() override;                                // Holding back (if it got here first)
	ActionResult timerCallback() override;                         // Ends the feedback text, see feedback()
	bool renderMainPads(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
	                    uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth], bool drawUndefinedArea) override;
	bool renderSidebar(uint32_t whichRows, RGB image[][kDisplayWidth + kSideBarWidth],
	                   uint8_t occupancyMask[][kDisplayWidth + kSideBarWidth]) override;
	void renderOLED(deluge::hid::display::oled_canvas::Canvas& canvas) override;
	UIType getUIType() override { return UIType::LFO_SHAPE_EDITOR; }

	/// Draws a whole shape into a rectangle (also used for the menu's small preview)
	static void drawShape(deluge::hid::display::oled_canvas::Canvas& canvas, const deluge::modulation::LFOShape& shape,
	                      int32_t minX, int32_t minY, int32_t maxX, int32_t maxY);

private:
	static constexpr int32_t kMaxZoomLevel = 3;  ///< x8
	static constexpr int32_t kMaxSplitLevel = 4; ///< 16 parts per page - one per pad column
	static constexpr int32_t kUndoPadRow = 0;    ///< In the sidebar's second column
	static constexpr int32_t kRedoPadRow = 1;
	static constexpr int32_t kReversePadRow = 3; ///< Apart from undo / redo

	/// What an edit was, so that turning the same knob on and on makes one undo step
	enum class Edit : uint8_t { NONE, VALUE, MOVE, BEND_AFTER, BEND_BEFORE, PRESET, OTHER };

	/// Undo history: the shape after every step (and the point selected then), for the LFO edited last. It outlives
	/// the editor, so coming back to the same LFO can still undo. It lives in the general allocator - not in internal
	/// RAM.
	struct State {
		deluge::modulation::LFOShape shape;
		int32_t selected;
	};
	static constexpr int32_t kHistorySize = 25; ///< States: 24 steps back

	Sound* sound_{nullptr};
	uint8_t lfoId_{0};
	int32_t selected_{0};
	int32_t zoomLevel_{0};
	int32_t page_{0};
	int32_t presetIndex_{-1}; ///< The preset loaded last, -1 = none yet

	State* history_{nullptr};
	int32_t historyCount_{0};
	int32_t historyCursor_{0};           ///< The state shown
	const Sound* historySound_{nullptr}; ///< Whose LFO the history belongs to - only compared, never followed
	uint8_t historyLfo_{0};
	Edit lastEdit_{Edit::NONE};
	int32_t lastEditPoint_{-1};
	uint32_t lastEditTime_{0};
	bool yTurnedWhilePressed_{false}; ///< The Y encoder split while held: its release adds no point
	bool feedbackShown_{false};
	char feedback_[24]{};

	void startHistory();       ///< On opening: carries on the history of the same LFO, or starts a new one
	void beginEdit(Edit edit); ///< Before changing the shape: starts a new undo step unless it carries on the last
	void commitEdit();         ///< After changing it: records the result
	void endGesture() { lastEdit_ = Edit::NONE; }
	void stepHistory(int32_t steps); ///< Undo (steps < 0) / redo (steps > 0), as far as the history goes
	[[nodiscard]] bool canUndo() const { return history_ != nullptr && historyCursor_ > 0; }
	[[nodiscard]] bool canRedo() const { return history_ != nullptr && historyCursor_ < historyCount_ - 1; }
	void restore(const State& state);
	void closeEditor();

	deluge::modulation::LFOShape* shape();
	[[nodiscard]] int32_t numPages() const { return 1 << zoomLevel_; }
	[[nodiscard]] uint32_t pageWidth() const { return 65536u >> zoomLevel_; }
	[[nodiscard]] uint32_t pageStart() const { return static_cast<uint32_t>(page_) * pageWidth(); }
	[[nodiscard]] uint32_t columnWidth() const { return pageWidth() / kDisplayWidth; }

	void select(int32_t index);     ///< Selects a point and shows its page
	void selectAt(uint32_t x);      ///< Selects the point at x, or the last one before it
	void selectStep(int32_t steps); ///< Previous / next point, wrapping round the cycle
	void showPage(int32_t page);    ///< Turns to a page and selects its first point (if it has one)
	void zoom(int32_t direction);
	void split(int32_t direction);
	[[nodiscard]] bool splitComplete(int32_t level);
	void moveSelected(int32_t offset);
	void changeValue(int32_t offset);
	void bend(int32_t offset, bool segmentBefore);
	void deleteSelected();
	void loadPreset(int32_t index);  ///< Wraps round the presets
	void insertPointAfterSelected(); ///< Halfway to the next point, on the shape (so it doesn't change)
	void reversePage();              ///< Plays the page shown backwards (the whole shape when not zoomed in)
	void renderTitle(deluge::hid::display::oled_canvas::Canvas& canvas);
	void renderShape(deluge::hid::display::oled_canvas::Canvas& canvas, const deluge::modulation::LFOShape& s);
	/// What an edit did, shown for a moment: on the OLED in the title line (a popup would cover the shape), on 7SEG
	/// as a popup
	void feedback(const char* text);

	void changed(); ///< Redraws everything after an edit
	void popupPoint();
	void popupValue();
	void popupPosition();
	void popupPage();
};

extern LFOShapeEditor lfoShapeEditor;
