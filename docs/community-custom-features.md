# Community Firmware Custom Build - Feature Manual

The Deluge community firmware (`main`, October 2026) with these additions:

- **Vowel filter**;
- **custom LFO shapes** with a shape editor, and **8-bar LFO sync**;
- **Saturation editor**;
- **Wavefold editor**.

Everything else works like the community firmware.

**Pad positions** are given as *column* (1-16, counted from the left) and *row* (1-8, counted from the bottom). The two
sidebar columns to the right of the grid are the *left* (mute) and *right* (audition) sidebar column. **"Press twice"**
means pressing the same shortcut pad again while its menu is open; this opens the pad's second layer.

In **horizontal menus** (several controls side by side on the OLED), **Synth / Kit / MIDI / CV** select the 1st / 2nd /
3rd / 4th control, the **select encoder** changes its value, and **Scale / Cross Screen** switch the page.

## Contents

1. [Vowel filter](#1-vowel-filter)
2. [Custom LFO shapes and the shape editor](#2-custom-lfo-shapes-and-the-shape-editor)
3. [8-bar LFO sync](#3-8-bar-lfo-sync)
4. [Saturation editor](#4-saturation-editor)
5. [Wavefold editor](#5-wavefold-editor)
6. [Compatibility](#6-compatibility)
7. [Shortcut pad overview](#7-shortcut-pad-overview)

---

## 1. Vowel filter

An LPF mode that filters the sound into vowels.

### Switching it on

Set **LPF mode** (column 9, row 6) to **Vowel** (after Drive). Available on synths, kit rows, kits, audio tracks and
the song.

### Controls

| LPF control | Pad | In Vowel mode |
|-------------|-----|---------------|
| **Frequency** | column 9, row 8 | Selects the vowel: **A, AE (Ä), E, I, UE (Ü), OE (Ö), O, U**, with smooth transitions in between. The display shows the nearest vowel. |
| **Resonance** | column 9, row 7 | Narrows the formants. |
| **Morph** (shown as **Formant shift** / SHFT) | column 9, row 5 | Shifts all formants up by up to one octave. Modulation works in both directions (up and down), also with Formant shift at 0. |
| **Raw** | 5th item of the LPF menu, only in Vowel mode. **CV** pressed again on Formant shift switches to Raw and back. | 0-50, 0 = off. Mixes the unfiltered highs of the source back in. Saved with the sound or track. |

---

## 2. Custom LFO shapes and the shape editor

LFO shape **Custom**: up to 64 points per cycle, joined by segments that can be bent. Saved with the sound. The first
point is fixed at the start of the cycle; the last segment runs back to the first point.

### Opening the editor

1. Press an LFO's **Shape** pad: LFO 1 is column 13, row 3, LFO 2 is column 14, row 3. Press twice for LFO 3 / 4.
2. Set **Shape** to **Custom**.
3. In the LFO menu (Shape / Draw / Sync / Rate), select **Draw** with its Synth / Kit / MIDI / CV button, then press
   **select** or the same button again.

### The display

- **OLED:** the title shows the LFO, the selected point and the number of points (`LFO1 P3/12`), the page is at the
  top right (`2/4`). The selected point is the large filled square.
- **While editing,** the title line shows the value being changed (e.g. `+42%`, `Bend > +12`, `Square 5/40`).
- **Pads:** the grid shows the visible part of the cycle as bars from the centre line. Points are cyan, the selected
  point is white.

### Controls

| Control | Action |
|---------|--------|
| **Pad** | Sets a point in that column at the pad's height. A point already in the column gets the new height. |
| **Shift + pad** | Removes the points in that column (the first point of the cycle stays). |
| **X encoder** | Moves the selected point in time. Shift: finer steps. |
| **X encoder pressed + turned** | Zoom x1 / x2 / x4 / x8. The page is shown at the top right and on the left sidebar column. |
| **Y encoder** | Selects the previous / next point. |
| **Y encoder press** (without turning) | Adds a point halfway between the selected point and the next one, on the curve (the shape does not change). The new point is selected. |
| **Y encoder pressed + turned** | Right: splits the visible part into 2 / 4 / 8 / 16 equal parts by adding points on the curve. Left: removes the finest split. |
| **Shift + Y encoder press** | Loads the next preset. |
| **Shift + Y encoder turned** | Browses the presets in both directions; the display shows name and number (e.g. `Bounce 36/40`). Zoomed in, the preset is loaded into the visible page only (see [Presets](#presets)). |
| **Upper gold knob** | Height of the selected point: 2 % per detent, 0.5 % with shift. |
| **Upper gold knob press** | Deletes the selected point. |
| **Lower gold knob** | Bends the segment after the selected point. Turning right bends it upwards. |
| **Lower gold knob pressed + turned** | Bends the segment before the selected point. |
| **Synth / Kit** | Previous point. |
| **MIDI / CV** | Next point. |
| **Scale / Cross Screen** | Previous / next page while zoomed in (after the last page comes the first). |
| **Select encoder** | Selects points, like the Y encoder. |
| **Shift + select encoder** | **Undo** (left) / **redo** (right), one step per detent. The title line shows the position, e.g. `UNDO 3/7`. |
| **Left sidebar column** | Jumps to a page while zoomed in. The visible page is white. |
| **Right sidebar column, bottom pad (red)** | **Undo**. Lit bright while a step can be undone. |
| **Right sidebar column, 2nd pad from the bottom (green)** | **Redo**. Lit bright while a step can be redone. |
| **Right sidebar column, 4th pad from the bottom (magenta)** | **Reverse**: reverses the visible page in time; not zoomed in, the whole shape. Pressing again restores it. |
| **Back** or **select press** | Leaves the editor. |
| **Hold back** | Leaves the whole menu. |

### Presets

40 presets, loaded with shift + Y (press: next, turn: both directions). Consecutive preset changes count as one undo
step.

- **Basics:** Triangle, Sine, Saw up, Saw down, Square.
- **Slopes:** Pulse 25%, Trapezoid, Stairs.
- **Bends:** Exp decay, Exp rise, Swell, Shark fin, Bumps, S-curve.
- **Polyrhythms:** Fin x3 (3 shark fins per cycle), Decay x3 (3 decays per cycle), Tresillo (decays at 3 + 3 + 2
  eighths), 3 over 2 (decays on the thirds and on the halves), Decay x5 (5 decays per cycle), Euclid 7/16 (7 decays
  over 16 steps: 2 2 2 3 2 2 3), Euclid 7/12 (7 decays over 12 triplet steps: 2 2 1 2 2 2 1).
- **Grooves** (16th patterns for a cycle of one bar): Two-step (decays on 16ths 1 and 11), 3-3-3-3-2-2 (decays on
  16ths grouped 3 3 3 3 2 2, accented), Ratchet (decays of a quarter, two eighths and eight 16ths), Wobble 3-3-2
  (three rounded humps of 3 + 3 + 2 eighths), Ramps 3-3-2 (three rising ramps of 3 + 3 + 2 eighths).
- **Round versions:** Decay x3, Tresillo, 3 over 2, Decay x5, Euclid 7/16, Euclid 7/12, Two-step, 3-3-3-3-2-2 and
  Ratchet each also come as a round version ("... round", right after the original): every hit is a smooth bump
  instead of a spike.
- **Experimental:** Bounce, Swing, Heartbeat, Zigzag, Chaos.

**Zoomed in,** a preset is loaded into the visible page only, compressed to the page width; the rest of the shape is
kept. Where the values at the page edges differ, the shape jumps.

### Undo and redo

- Up to 24 steps. Continuously turning a knob counts as one step.
- The history is kept when leaving the editor and continues when the same LFO is opened again, as long as the shape
  was not changed elsewhere (e.g. by loading a preset). Opening another LFO starts a new history.

---

## 3. 8-bar LFO sync

**LFO Sync** has an additional setting, **8 bars**, between *Off* and 4 bars (at the default song resolution; it is
always twice the longest regular setting).

---

## 4. Saturation editor

Saturation editor on its own pad, **column 8, row 7**. Also in the **FX** menu and in the horizontal menu chain.

| Control | Range | Function |
|---------|-------|----------|
| **Saturation** (Drive) | 0-50, 0 = off | Amount of saturation. |
| **Asymmetry** | 0-50, default 25 | Adds even harmonics. 0 = symmetric. |
| **Mix** | 0-50, default 50 | Dry/wet. 50 = fully wet. |
| **Tone** | 0-50, default 0 | Keeps the lows below a crossover unsaturated. 0 = the whole signal is saturated; at 50 the crossover is about 1.5 kHz. |

- The original Saturation pad (column 7, row 8) controls the same drive.
- **Audio tracks:** Tone is in the audio track's **Distortion** menu.

---

## 5. Wavefold editor

Wavefold editor on the **Fold pad (column 8, row 8)**; press it twice to go straight to Symmetry. Also in the **FX**
menu and in the horizontal menu chain. Available on synths and kit rows.

| Control | Range | Function |
|---------|-------|----------|
| **Wavefold** (Amount) | 0-50 | Fold amount. |
| **Symmetry** | -25 to +25, 0 = symmetric | Shifts the signal before folding, so the positive and negative half-waves fold differently (even harmonics). Can be modulated in the mod matrix and automated. |
| **Tone** | 0-50, default 0 | Keeps the lows below a crossover out of the folder. 0 = the whole signal is folded. |

---

## 6. Compatibility

| Situation | Result |
|-----------|--------|
| Songs from the regular firmware with saturation | Sound the same: the old 0-15 drive is converted to the 0-50 scale on loading; Asymmetry starts at 0. |
| Songs saved with this build, opened in the regular firmware | The regular firmware reads the 0-50 drive as much more drive. Custom LFO shapes load as Triangle, 8-bar sync as 4 bars; Wavefold Symmetry / Tone are ignored; a Vowel filter loads as Off. |
| Songs from the Owlet custom build | Saturation, Wavefold, Vowel filter and LFO shapes load as saved. |

---

## 7. Shortcut pad overview

Pads used by the additions above:

| Pad (column, row) | Opens |
|-------------------|-------|
| 7, 8 | Saturation (drive), original Distortion menu. |
| 8, 7 | **Saturation editor**. |
| 8, 8 | **Wavefold editor**. **Press twice:** Symmetry. |
| 9, 5 / 6 / 7 / 8 | LPF Morph (Formant shift) / Mode (→ Vowel) / Resonance / Frequency (vowel). |
| 13 / 14, 3 | LFO 1 / LFO 2 Shape (→ Custom → Draw). **Press twice:** LFO 3 / LFO 4. |
