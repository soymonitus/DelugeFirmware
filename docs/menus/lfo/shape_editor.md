# LFO Draw (custom shape editor)

Opens the editor for an LFO whose shape is set to **Custom**. The horizontal menu shows the drawn cycle.

The shape is made of points joined by segments that can be bent (Serum style). One point is always selected; the display shows it as a filled square and the pads light it white.

- **Pads**: set a point in that column at that height (a point already in the column just gets the new height). Shift + pad removes the points in the column. The pads show the visible part of the cycle.
- **X encoder** (black, top left): moves the selected point in time (shift: finer). **Pressed and turned**: zooms in and out (x1, x2, x4, x8) - the page you're on shows top right on the display (2/4) and on the sidebar.
- **Y encoder** (black, bottom left): selects the previous / next point. **Press** (without turning): adds a point halfway between the selected point and the next one, right on the shape. **Pressed and turned right**: splits the visible part into ever more equal parts (2, 4, 8, 16) by adding points along the shape; **pressed and turned left** takes the finest split back out. **Shift + press**: loads the next preset shape, **shift + turn**: browses the presets both ways. Zoomed in, a preset goes into the visible page only, squeezed in - the rest of the shape stays. 40 presets: Triangle, Sine, Saw up, Saw down, Square, Pulse 25%, Trapezoid, Stairs, Exp decay, Exp rise, Swell, Shark fin, Bumps, S-curve, Fin x3, Decay x3, Tresillo, 3 over 2, Decay x5, Euclid 7/16, Euclid 7/12, Two-step, 3-3-3-3-2-2, Ratchet (each of the rhythms from Decay x3 to Ratchet also as a round version, right after it), Wobble 3-3-2, Ramps 3-3-2, Bounce, Swing, Heartbeat, Zigzag, Chaos.
- **Upper gold knob**: the selected point's height (shift: finer). **Press**: deletes the selected point.
- **Lower gold knob**: bends the segment after the selected point - turning right always bows it upwards. **Pressed and turned**: bends the segment before it.
- **Synth / Kit**: previous point, **MIDI / CV**: next point. **Scale / Cross Screen**: previous / next page while zoomed in (after the last page comes the first); the pads of the first sidebar column jump to a page too.
- **Select encoder**: selects points, like the Y encoder.
- **Reverse**: the **fourth pad up in the second sidebar column** (magenta) plays the visible page backwards - the whole shape when not zoomed in.
- **Undo / redo**: the second sidebar column - the **bottom pad undoes** (red), the **pad above it redoes** (green); each lights up bright while there's a step to take. **Shift + select encoder**: left undoes, right redoes, a step per detent - quick for many steps. Turning a knob on and on counts as one step; up to 24 steps.
- **Leaving**: **back** or a **press on the select encoder**, as in every menu (holding back leaves the whole menu). The undo history stays: opening the editor again on the same LFO carries on with it, as long as nothing else changed the shape in the meantime.

While you edit, the display's title line shows what you're changing for a moment, without covering the shape.

The first point sits at the start of the cycle and stays there. Up to 64 points per cycle. The shape is saved with the sound.

Available when the LFO shape is set to Custom.
