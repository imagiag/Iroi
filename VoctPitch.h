#pragma once

#include <cmath>

/**
 * Pitch maths for the resonator's V/Oct mode.
 *
 * A Pole is a feedback delay: out = lpf(read(d)), mix = dc(in + out), write(mix).
 * One trip round the loop takes d + 1 samples, plus the phase delay of the
 * biquad lowpass and of the DC blocker. The loop rings where the total phase
 * is a full turn, so for a note at f Hz:
 *
 *   d = sr / f - 1 - lpfDelay(f) - dcDelay(f)
 *
 * The DC blocker leads (negative delay) at low notes and the lowpass lags at
 * high notes. With the stock DC blocker (R = 0.995, corner ~38 Hz) the lead is
 * so strong that the fundamental partial of low notes sits far above its
 * harmonics, so V/Oct mode moves the corner down to ~0.8 Hz (kVoctDcBlockerR)
 * and only a small, exactly compensated lead remains.
 */
namespace voct
{

constexpr float kTwoPi = 6.283185307179586f;

// MIDI note to Hz (A4 = 69 = 440 Hz).
inline float NoteToHz(float note)
{
    return 440.f * exp2f((note - 69.f) / 12.f);
}

// Phase delay in samples of an OWL BiquadFilter lowpass (FilterStage::setLowPass)
// at angular frequency w (radians per sample).
inline float LowPassDelay(float w, float fc, float q, float sr)
{
    float K = tanf(fc * float(M_PI) / sr);
    float norm = 1.f / (1.f + K / q + K * K);
    float b0 = K * K * norm;
    float b1 = 2.f * b0;
    float b2 = b0;
    float a1 = 2.f * (K * K - 1.f) * norm;   // standard sign: y += -a1 y1 - a2 y2
    float a2 = (1.f - K / q + K * K) * norm;

    float c1 = cosf(w), s1 = sinf(w), c2 = cosf(2.f * w), s2 = sinf(2.f * w);
    float nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
    float dr = 1.f + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
    float phase = atan2f(ni, nr) - atan2f(di, dr);
    return -phase / w;
}

// Phase delay in samples of DcBlockingFilter (y = x - x1 + R y1) at w.
inline float DcBlockerDelay(float w, float R)
{
    // H = (1 - e^-jw) / (1 - R e^-jw)
    float c = cosf(w), s = sinf(w);
    float phase = atan2f(s, 1.f - c) - atan2f(R * s, 1.f - R * c);
    return -phase / w;
}

// Phase delay of DelayLine::read's linear interpolation for a fractional part a.
inline float LinearInterpDelay(float w, float a)
{
    // H = (1 - a) + a e^-jw
    return atan2f(a * sinf(w), 1.f - a + a * cosf(w)) / w;
}

/**
 * Delay (in samples, as passed to DelayLine::read) that makes a Pole ring at
 * the given note.
 */
inline float DelayForNote(float note, float sr, float lpfFc, float lpfQ, float dcR)
{
    float f = NoteToHz(note);
    float w = kTwoPi * f / sr;
    float d = sr / f - 1.f - LowPassDelay(w, lpfFc, lpfQ, sr) - DcBlockerDelay(w, dcR);

    // Linear interpolation does not delay by exactly its fractional part;
    // one correction step is plenty (error < 0.001 samples).
    float n = floorf(d);
    float a = d - n;
    a += a - LinearInterpDelay(w, a);
    return n + a;
}

} // namespace voct
