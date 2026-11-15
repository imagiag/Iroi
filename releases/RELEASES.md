# Iroi tweaked patch builds

Send one .syx with the OWL web tool (see TWEAKS.md). To go back to stock, send iroi-stock-1.0.1.syx the same way.

| Build | File | Date | Code | What it adds |
|---|---|---|---|---|
| v1 | iroi-v1-resonator-voct.syx | 2026-11-14 | d52218e | Resonator follows V/Oct: SHIFT + Resonator Feedback, left half original, right half V/Oct (position = scale trim, 3 o'clock nominal) |
| v2 | iroi-v2-voct-polarity.syx | 2026-11-15 | 17b8727 | V/Oct follows the right way up (CV inputs read inverted on the module); 0 V reference mirrored to ~0.70 |
| v3 | iroi-v3-voct-polarity-back.syx | 2026-11-15 | dc0f455 | Undoes v2's flip: V/Oct as in v1 (the inverted CVs were a negative CV amount in the saved mapping, not the input) |
| v4 | iroi-v4-voct-c2-at-0v.syx | 2026-11-15 | 8ec8541 | 0 V = C2 with Tune at noon (was C3), more room for bass and keyboard range |
| v5 | iroi-v5-fix-inverted-cv-after-save.syx | 2026-11-15 | fe3a7b1 | CV amounts no longer invert after a save (stock bug, 1.0 wrapped to -1.0); V/Oct Tune knob ±1 octave, fine near noon |
