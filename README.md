> **This fork** of [Befaco/Iroi](https://github.com/Befaco/Iroi) adds:
>
> - **Resonator V/Oct:** the resonator follows a 1 V/oct pitch CV on its CV input, with
>   0 V = C2 at noon. Turn it on with SHIFT + Resonator Feedback.
> - **A fix for CVs working upside down after a save**, a bug in the stock patch
>   ([Befaco/Iroi#6](https://github.com/Befaco/Iroi/issues/6)).
>
> ### Install
>
> 1. Open <https://pingdynasty.github.io/OwlWebControl/extended.html> in Chrome, with Iroi
>    connected over USB (the port on the small OWL board).
> 2. Load the latest build, [releases/iroi-v6-tune-3-octaves.syx](releases/iroi-v6-tune-3-octaves.syx),
>    and store it to a slot, the same way as Befaco's official updates. Every build is
>    listed in [releases/RELEASES.md](releases/RELEASES.md).
> 3. To go back to stock, do the same with `releases/iroi-stock-1.0.1.syx` (Befaco's official
>    1.0.1 patch, unchanged).
>
> ### Resonator V/Oct how-to
>
> 1. Patch your pitch CV into the **resonator CV input**, and some audio into Iroi (noise or
>    a fast hi-hat pattern rings it well).
> 2. Hold **SHIFT** and turn **Resonator Feedback** to **3 o'clock**. The right half of the
>    knob is V/Oct mode, the left half the original resonator.
> 3. Long-press **MAP** to save, or it's back to the original mode at the next power-up.
> 4. **Tune:** with 0 V in, noon is C2. The knob is fine near noon and reaches about
>    1.5 octaves down and up at its ends.
> 5. **Scale trim (if needed):** play two notes two octaves apart and compare them on a tuner
>    showing cents. If the high one is flat, hold SHIFT and turn Feedback a little right; if
>    it's sharp, a little left. Keep it right of noon. Then long-press MAP to save.
>
> In V/Oct mode the CV is the pitch, so it no longer modulates Feedback. Dissonance
> (SHIFT + Tune) still spreads the chord in stereo. More detail, and how it works:
> [TWEAKS.md](TWEAKS.md).

# Iroi

Iroi is Oneiroi's effects section in a module.
