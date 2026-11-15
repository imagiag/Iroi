#pragma once

#include "Commons.h"
#include "DelayLine.h"
#include "BiquadFilter.h"
#include "EnvFollower.h"
#include "DcBlockingFilter.h"
#include "Compressor.h"
#include "VoctPitch.h"

class Pole
{
public:
    Pole(float sampleRate)
    {
        sampleRate_ = sampleRate;
        msr_ = sampleRate_ / 1000.f;

        for (size_t i = 0; i < 2; i++)
        {
            delays_[i] = DelayLine::create(kResoBufferSize);
            lpfs_[i] = BiquadFilter::create(sampleRate_);
            dc_[i] = DcBlockingFilter::create(kDcBlockerR);
            ef_[i] = EnvFollower::create();
        }

        reso_ = FilterStage::BUTTERWORTH_Q;
        offset_ = 0;
        feedback_ = 0;
        filter_ = 0;
        detune_ = 0;
        infinite_ = false;

        for (size_t i = 0; i < 2; i++)
        {
            delayTimes_[i] = 0;
            delayIncs_[i] = 0;
            voctNotes_[i] = 0;
        }
    }
    ~Pole()
    {
        for (size_t i = 0; i < 2; i++)
        {
            DelayLine::destroy(delays_[i]);
            BiquadFilter::destroy(lpfs_[i]);
            DcBlockingFilter::destroy(dc_[i]);
            EnvFollower::destroy(ef_[i]);
        }
    }

    static Pole* create(float sampleRate)
    {
        return new Pole(sampleRate);
    }

    static void destroy(Pole* pole)
    {
        delete pole;
    }

    float GetSemiOffset()
    {
        return offset_;
    }

    /**
     * @param offset Semitones, -24 to 24
     */
    void SetSemiOffset(float offset)
    {
        offset_ = offset * -0.5f + 17.667f;
        SetNote();
    }

    void SetFilter(float filter)
    {
        filter_ = filter;
        SetFreq();
    }

    /**
     * V/Oct mode: tune each channel to a MIDI note (at block rate). The delay
     * glides to the new value over the next `samples` samples, compensating
     * for the delay of the filters in the loop so the pole rings at the note.
     * Call after SetFilter/SetReso so the compensation uses the current filter.
     */
    void SetVoctNotes(float leftNote, float rightNote, size_t samples)
    {
        voctNotes_[LEFT_CHANNEL] = leftNote;
        voctNotes_[RIGHT_CHANNEL] = rightNote;
        // Keep the low-pass where SetFreq would put it in the original mode
        // (its tiny note-dependent offset is irrelevant at 5-10 kHz).
        lf_ = rf_ = 0;
        SetFreq();

        float inc = 1.f / samples;
        for (size_t i = 0; i < 2; i++)
        {
            dc_[i]->setTimeConstant(1.f / (1.f - kVoctDcBlockerR));
            float target = voct::DelayForNote(voctNotes_[i], sampleRate_,
                M2F(0) + filter_, reso_, kVoctDcBlockerR);
            target = Clamp(target, 1.f, kResoBufferSize - 2);
            delayIncs_[i] = (target - delayTimes_[i]) * inc;
        }
    }

    void SetFeedback(float feedback)
    {
        infinite_ = feedback > kResoInfiniteFeedbackThreshold;
        feedback_ = VariableCrossFade(0.f, 1.f, feedback, kResoInfiniteFeedbackThreshold - 0.01f);
    }

    void SetDissonance(float detune)
    {
        detune_ = detune;
        SetNote();
    }

    void SetReso(float reso)
    {
        reso_ = reso;
        SetFreq();
    }

    // Process just one of the two channels.
    float Process(float in, int channel)
    {
        float out = lpfs_[channel]->process(outs_[channel]) * feedback_;

        float mix = HardClip(dc_[channel]->process(in + out));

        // Handle infinite feedback.
        if (infinite_)
        {
            mix *= feedback_ * kResoInfiniteFeedbackLevel - ef_[channel]->process(mix);
        }

        delays_[channel]->write(mix);
        delayTimes_[channel] += delayIncs_[channel];
        outs_[channel] = delays_[channel]->read(delayTimes_[channel]);

        return out;
    }

    void Process(float leftIn, float rightIn, float &leftOut, float &rightOut)
    {
        leftOut = lpfs_[LEFT_CHANNEL]->process(outs_[LEFT_CHANNEL]) * feedback_;
        rightOut = lpfs_[RIGHT_CHANNEL]->process(outs_[RIGHT_CHANNEL]) * feedback_;

        float leftMix = HardClip(dc_[LEFT_CHANNEL]->process(leftIn + leftOut));
        float rightMix = HardClip(dc_[RIGHT_CHANNEL]->process(rightIn + rightOut));

        // Handle infinite feedback.
        if (infinite_)
        {
            leftMix *= feedback_ * kResoInfiniteFeedbackLevel - ef_[LEFT_CHANNEL]->process(leftMix);
            rightMix *= feedback_ * kResoInfiniteFeedbackLevel - ef_[RIGHT_CHANNEL]->process(rightMix);
        }

        delays_[LEFT_CHANNEL]->write(leftMix);
        delays_[RIGHT_CHANNEL]->write(rightMix);

        delayTimes_[LEFT_CHANNEL] += delayIncs_[LEFT_CHANNEL];
        delayTimes_[RIGHT_CHANNEL] += delayIncs_[RIGHT_CHANNEL];
        outs_[LEFT_CHANNEL] = delays_[LEFT_CHANNEL]->read(delayTimes_[LEFT_CHANNEL]);
        outs_[RIGHT_CHANNEL] = delays_[RIGHT_CHANNEL]->read(delayTimes_[RIGHT_CHANNEL]);
    }

private:
    DelayLine *delays_[2];
    BiquadFilter *lpfs_[2];
    EnvFollower *ef_[2];
    DcBlockingFilter* dc_[2];
    float delayTimes_[2], delayIncs_[2], outs_[2];
    float voctNotes_[2];

    float sampleRate_, msr_;
    float lf_, rf_;
    float reso_;
    float offset_;
    float feedback_;
    float filter_;
    float detune_;

    bool infinite_;

    void SetFreq()
    {
        lpfs_[LEFT_CHANNEL]->setLowPass(M2F(lf_) + filter_, reso_);
        lpfs_[RIGHT_CHANNEL]->setLowPass(M2F(rf_) + filter_, reso_);
    }

    void SetNote()
    {
        lf_ = offset_ + detune_;
        rf_ = offset_ - detune_;

        delayTimes_[LEFT_CHANNEL] = Clamp(msr_ * Db2A(lf_), 0, kResoBufferSize);
        delayTimes_[RIGHT_CHANNEL] = Clamp(msr_ * Db2A(rf_), 0, kResoBufferSize);
        delayIncs_[LEFT_CHANNEL] = delayIncs_[RIGHT_CHANNEL] = 0;
        dc_[LEFT_CHANNEL]->setTimeConstant(1.f / (1.f - kDcBlockerR));
        dc_[RIGHT_CHANNEL]->setTimeConstant(1.f / (1.f - kDcBlockerR));

        SetFreq();
    }
};

/**
 * @brief This is taken from my Reaktor ensemble Aerosynth.
 *        https://www.native-instruments.com/de/reaktor-community/reaktor-user-library/entry/show/3431/
 *
 */
class Resonator
{
private:
    PatchCtrls* patchCtrls_;
    PatchCvs* patchCvs_;
    PatchState* patchState_;
    Pole* poles_[3];

    BiquadFilter *notches_[2];
    BiquadFilter *hs_[2];
    EnvFollower *ef_[2];

    Compressor* compressor_;

    float amp_;
    float dryWet_;
    float range_;
    float tune_;
    float oldTuning_;
    int ranges_[3];
    bool voct_;

    int task_;

    /**
     * @param idx 0 - 3
     * @param offset -24/24
     */
    void SetSemiOffset(int idx, float offset)
    {
        if (idx == 0)
        {
            poles_[0]->SetSemiOffset(offset);
            poles_[1]->SetSemiOffset(offset + poles_[1]->GetSemiOffset());
            poles_[2]->SetSemiOffset(offset + poles_[2]->GetSemiOffset());

        }
        else if (idx == 1)
        {
            poles_[1]->SetSemiOffset(offset + poles_[1]->GetSemiOffset());
        }
        else if (idx == 2)
        {
            poles_[2]->SetSemiOffset(offset + poles_[2]->GetSemiOffset());
        }
    }

    void SetTune(float value)
    {
        tune_ = value;

        // Spread the updates in time.
        task_ = (task_ + 1) % 6;
        if (task_ == 0)
        {
            SetSemiOffset(0, Map(tune_, 0.f, 1.f, -24, ranges_[0]));
        }
        if (tune_ < 0.5f && task_ == 1)
        {
            SetSemiOffset(1, Map(tune_, 0.f, 0.5f, -12, ranges_[1]));
        }
        if (tune_ < 0.3f && task_ == 2)
        {
            SetSemiOffset(2, Map(tune_, 0.f, 0.3f, -6, ranges_[2]));
        }
        if (tune_ >= 0.3f && tune_ < 0.7f && task_ == 3)
        {
            SetSemiOffset(2, Map(tune_, 0.3f, 0.7f, -6, ranges_[2]));
        }
        if (tune_ >= 0.5f && task_ == 4)
        {
            SetSemiOffset(1, Map(tune_, 0.5f, 1.f, -12, ranges_[1]));
        }
        if (tune_ >= 0.7f && task_ == 5)
        {
            SetSemiOffset(2, Map(tune_, 0.7f, 1.f, -6, ranges_[2]));
        }
    }

    void SetFeedback(float value, bool init = false)
    {
        float feedback = Map(value, 0.f, 1.f, 0.85f, 1.f);
        float reso = Map(value, 0.f, 1.f, 0.5f, 0.6f);
        float filter = Map(value, 0.f, 1.f, 5000.f, 10000.f);
        //amp_ = Map(value, 0.f, 1.f, kResoGainMax, kResoGainMin) * 0.577f;
        for (int i = 0; i < 3; i++)
        {
            poles_[i]->SetFeedback(feedback);
            poles_[i]->SetReso(reso);
            poles_[i]->SetFilter(filter);
        }
    }

    /*
    void SetRange(float range)
    {
        range_ = Map(range, 0.f, kOne, 0.1f, 1.f);
    }
    */

    // Fold a note into the range a pole can ring at, keeping its pitch class.
    static float FoldNote(float note)
    {
        while (note < kVoctMinNote)
        {
            note += 12.f;
        }
        while (note > kVoctMaxNote)
        {
            note -= 12.f;
        }

        return note;
    }

    /**
     * V/Oct mode: the three poles keep fixed intervals above the played note
     * (pole 0 on it, pole 2 an octave up, pole 1 an octave and a fifth up,
     * close to the original chord with Tune at noon) so the chord transposes
     * as a whole. Dissonance spreads each pole's channels apart, as in the
     * original mode (±2, ±4 and ±6 semitones at full).
     */
    void SetVoctNotes(float note, float dissonance, size_t samples)
    {
        float d0 = dissonance * 2.f;
        float d1 = dissonance * 4.f;
        float d2 = dissonance * 6.f;

        poles_[0]->SetVoctNotes(FoldNote(note - d0), FoldNote(note + d0), samples);
        poles_[1]->SetVoctNotes(FoldNote(note + kVoctPole1Interval - d1), FoldNote(note + kVoctPole1Interval + d1), samples);
        poles_[2]->SetVoctNotes(FoldNote(note + kVoctPole2Interval - d2), FoldNote(note + kVoctPole2Interval + d2), samples);
    }

    void SetDissonance(float value)
    {
        ranges_[0] = Map(value, 0.f, 1.f, 24, 16);
        ranges_[1] = Map(value, 0.f, 1.f, 12, 7);
        ranges_[2] = Map(value, 0.f, 1.f, 6, 13);

        poles_[0]->SetDissonance(value);
        poles_[1]->SetDissonance(value * 2.f);
        poles_[2]->SetDissonance(value * 3.f);
    }

public:
    Resonator(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        patchCtrls_ = patchCtrls;
        patchCvs_ = patchCvs;
        patchState_ = patchState;

        for (int i = 0; i < 3; i++)
        {
            poles_[i] = Pole::create(patchState_->sampleRate);
        }

        for (size_t i = 0; i < 2; i++)
        {
            notches_[i] = BiquadFilter::create(patchState_->sampleRate);
            notches_[i]->setNotch(8000.f, FilterStage::SALLEN_KEY_Q);
            hs_[i] = BiquadFilter::create(patchState_->sampleRate);
            hs_[i]->setHighShelf(8000.f, -24.f);
            ef_[i] = EnvFollower::create();
        }

        compressor_ = Compressor::create(patchState_->sampleRate);
        compressor_->setRatio(3.f);
        compressor_->setAttack(20.f);

        amp_ = 1.f;
        range_ = 1.f;
        task_ = 0;
        voct_ = false;
        oldTuning_ = 0;
        dryWet_ = 0;

        SetDissonance(0);
        SetTune(0);
        SetFeedback(0);
    }
    ~Resonator()
    {
        for (int i = 0; i < 3; i++)
        {
            Pole::destroy(poles_[i]);
        }
        for (size_t i = 0; i < 2; i++)
        {
            BiquadFilter::destroy(notches_[i]);
            BiquadFilter::destroy(hs_[i]);
            EnvFollower::destroy(ef_[i]);
        }

        Compressor::destroy(compressor_);
    }

    static Resonator* create(PatchCtrls* patchCtrls, PatchCvs* patchCvs, PatchState* patchState)
    {
        return new Resonator(patchCtrls, patchCvs, patchState);
    }

    static void destroy(Resonator* obj)
    {
        delete obj;
    }

    void process(AudioBuffer &input, AudioBuffer &output)
    {
        size_t size = output.getSize();
        FloatArray leftIn = input.getSamples(LEFT_CHANNEL);
        FloatArray rightIn = input.getSamples(RIGHT_CHANNEL);
        FloatArray leftOut = output.getSamples(LEFT_CHANNEL);
        FloatArray rightOut = output.getSamples(RIGHT_CHANNEL);

        voct_ = patchCtrls_->resonatorVoct >= kVoctModeThreshold;

        // In V/Oct mode the shared resonator CV input is the pitch, so it
        // doesn't modulate Tune (as a knob offset) nor Feedback.
        float tuneCvAmount = voct_ ? 0.f : patchCtrls_->resonatorTuneCvAmount;
        float feedbackCvAmount = voct_ ? 0.f : patchCtrls_->resonatorFeedbackCvAmount;

        if (!voct_)
        {
            SetDissonance(patchCtrls_->resonatorDissonance);
        }

        float t = Modulate(patchCtrls_->resonatorTune, patchCtrls_->resonatorTuneModAmount, patchState_->modValue, tuneCvAmount, patchCvs_->resonatorTune, -1.f, 1.f, patchState_->modAttenuverters, voct_ ? false : patchState_->cvAttenuverters);
        ParameterInterpolator tuningParam(&oldTuning_, t, size);

        float f = Modulate(patchCtrls_->resonatorFeedback, patchCtrls_->resonatorFeedbackModAmount, patchState_->modValue, feedbackCvAmount, patchCvs_->resonatorFeedback, -1.f, 1.f, patchState_->modAttenuverters, voct_ ? false : patchState_->cvAttenuverters);
        SetFeedback(f);

        if (voct_)
        {
            // Tune knob: ±1.5 octaves around C2 (at 0 V), fine near noon and coarser
            // towards the ends, plus the CV in semitones.
            float u = 2.f * (t - 0.5f);
            CONSTRAIN(u, -1.f, 1.f);
            float note = kVoctCenterNote + kVoctKnobRange * (0.5f * u + 0.5f * u * u * u) + patchCvs_->resonatorVoct;
            SetVoctNotes(note, patchCtrls_->resonatorDissonance, size);
        }

        ParameterInterpolator volParam = ParameterInterpolator(&dryWet_, patchCtrls_->resonatorVol, size);

        for (size_t i = 0; i < size; i++)
        {
            float tune = tuningParam.Next();
            if (!voct_)
            {
                SetTune(tune);
            }

            float lIn = Clamp(leftIn[i], -3.f, 3.f);
            float rIn = Clamp(rightIn[i], -3.f, 3.f);

            float left = poles_[1]->Process(lIn, LEFT_CHANNEL);
            float right = poles_[2]->Process(rIn, RIGHT_CHANNEL);

            float oLeft = left * 0.75f + right * 0.25f;
            float oRight = left * 0.25f + right * 0.75f;

            left = 0;
            right = 0;
            poles_[0]->Process(lIn, rIn, left, right);
            oLeft += left;
            oRight += right;

            oLeft *= 1.f - ef_[LEFT_CHANNEL]->process(oLeft);
            oRight *= 1.f - ef_[RIGHT_CHANNEL]->process(oRight);

            oLeft *= amp_;
            oRight *= amp_;

            oLeft = notches_[LEFT_CHANNEL]->process(oLeft);
            oRight = notches_[RIGHT_CHANNEL]->process(oRight);

            oLeft = hs_[LEFT_CHANNEL]->process(oLeft);
            oRight = hs_[RIGHT_CHANNEL]->process(oRight);

            float v = volParam.Next();

            leftOut[i] = CheapEqualPowerCrossFade(lIn, oLeft * kResoMakeupGain, v);
            rightOut[i] = CheapEqualPowerCrossFade(rIn, oRight * kResoMakeupGain, v);
        }

        compressor_->process(output, output);
    }
};