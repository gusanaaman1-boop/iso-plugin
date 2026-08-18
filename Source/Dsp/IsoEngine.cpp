#include "IsoEngine.h"

namespace iso
{
    namespace
    {
        constexpr float kButterworthQ = 0.70710678f;   // LR4 building block
        constexpr float kLr2Q         = 0.5f;          // LR2: critically damped

        //  Response of a section designed at fcHz / q, evaluated at hz. Same
        //  maths as Svf::responseAt but from a design frequency rather than a
        //  live coefficient, so the UI can draw a Settings that is not yet
        //  (or never) running.
        Svf::Response sectionResponse (double hz, double fcHz, double q, double sr) noexcept
        {
            const double nyq = 0.49 * sr;
            fcHz = juce::jlimit (5.0, nyq, fcHz);
            const double g = std::tan (juce::MathConstants<double>::pi * fcHz / sr);
            const double w = std::tan (juce::MathConstants<double>::pi * juce::jlimit (1.0, nyq, hz) / sr) / g;
            const std::complex<double> s (0.0, w);
            const std::complex<double> d = s * s + (1.0 / q) * s + 1.0;
            return { 1.0 / d, (s * s) / d, s / d };
        }

        float dbToLin (float db) noexcept { return std::pow (10.0f, db / 20.0f); }
    }

    // -------------------------------------------------------------------------
    //  Laws
    // -------------------------------------------------------------------------
    float IsoEngine::bandLinearGain (float gainDb, bool kill, Floor floorMode) noexcept
    {
        if (kill)
            return 0.0f;

        if (floorMode == Floor::eq26)
            return dbToLin (juce::jmax (gainDb, law::kEqFloorDb));

        //  ISO mode: the last 6 dB of knob travel fade linearly (in amplitude)
        //  from -24 dB to silence, so the pot "runs out" the way a real
        //  isolator's does instead of jumping from -30 dB to nothing.
        constexpr float fadeTopDb = law::kGainMinDb + 6.0f;   // -24
        if (gainDb <= law::kGainMinDb + 1.0e-3f)
            return 0.0f;
        if (gainDb < fadeTopDb)
            return dbToLin (fadeTopDb) * (gainDb - law::kGainMinDb) / (fadeTopDb - law::kGainMinDb);

        return dbToLin (gainDb);
    }

    void IsoEngine::effectiveCrossovers (float lowMidHz, float midHighHz, float& f1, float& f2) noexcept
    {
        f2 = juce::jlimit (law::kMidHighMinHz, law::kMidHighMaxHz, midHighHz);
        f1 = juce::jlimit (law::kLowMidMinHz,  law::kLowMidMaxHz,  lowMidHz);
        f1 = juce::jmin (f1, f2 / law::kMinCrossoverRatio);
    }

    void IsoEngine::filterLaw (float filter, float resonance, float& cutoffHz, float& q, int& mode) noexcept
    {
        const float a = std::abs (filter);
        if (a <= law::kFilterDeadZone)
        {
            mode = 0;
            cutoffHz = law::kFilterLpOpenHz;
            q = law::kFilterQMin;
            return;
        }

        const float t = juce::jlimit (0.0f, 1.0f, (a - law::kFilterDeadZone) / (1.0f - law::kFilterDeadZone));
        mode = filter < 0.0f ? -1 : 1;

        if (mode < 0)
            cutoffHz = law::kFilterLpOpenHz * std::pow (law::kFilterLpMinHz / law::kFilterLpOpenHz, t);
        else
            cutoffHz = law::kFilterHpOpenHz * std::pow (law::kFilterHpMaxHz / law::kFilterHpOpenHz, t);

        const float ramp = juce::jmin (1.0f, t / law::kFilterResRamp);
        const float r = juce::jlimit (0.0f, 1.0f, resonance) * ramp;
        q = law::kFilterQMin * std::pow (law::kFilterQMax / law::kFilterQMin, r);
    }

    // -------------------------------------------------------------------------
    void IsoEngine::prepare (double sampleRate, int, int numChannels)
    {
        sr = sampleRate;
        channels = juce::jlimit (0, kMaxChannels, numChannels);

        smF1.reset (sr, law::kFreqRampSeconds);
        smF2.reset (sr, law::kFreqRampSeconds);
        smSweepHz.reset (sr, law::kFreqRampSeconds);
        smSweepQ.reset (sr, law::kFreqRampSeconds);
        for (auto& g : smGain) g.reset (sr, law::kGainRampSeconds);
        smTrim.reset (sr, law::kGainRampSeconds);

        //  Snap everything to the current settings: no ramp on the first block.
        const Settings s = settings;
        setSettings (s);
        smF1.setCurrentAndTargetValue (smF1.getTargetValue());
        smF2.setCurrentAndTargetValue (smF2.getTargetValue());
        smSweepHz.setCurrentAndTargetValue (smSweepHz.getTargetValue());
        smSweepQ.setCurrentAndTargetValue (smSweepQ.getTargetValue());
        for (auto& g : smGain) g.setCurrentAndTargetValue (g.getTargetValue());
        smTrim.setCurrentAndTargetValue (smTrim.getTargetValue());

        reset();
        updateCoefficients (smF1.getCurrentValue(), smF2.getCurrentValue(),
                            smSweepHz.getCurrentValue(), smSweepQ.getCurrentValue());
    }

    void IsoEngine::reset()
    {
        for (auto& c : ch)
        {
            c.f1a.reset(); c.f1b.reset(); c.f1c.reset();
            c.f2a.reset(); c.f2b.reset(); c.f2c.reset();
            c.ap2.reset(); c.sweep.reset();
        }
    }

    void IsoEngine::setSettings (const Settings& s) noexcept
    {
        settings = s;
        slope = s.slope;

        float f1, f2;
        effectiveCrossovers (s.lowMidHz, s.midHighHz, f1, f2);
        smF1.setTargetValue (f1);
        smF2.setTargetValue (f2);

        float cut, q; int mode;
        filterLaw (s.filter, s.resonance, cut, q, mode);
        smSweepHz.setTargetValue (cut);
        smSweepQ.setTargetValue (q);
        sweepMode = mode;

        for (int b = 0; b < law::kBands; ++b)
            smGain[(size_t) b].setTargetValue (bandLinearGain (s.gainDb[(size_t) b], s.kill[(size_t) b], s.floorMode));

        smTrim.setTargetValue (dbToLin (juce::jlimit (law::kTrimMinDb, law::kTrimMaxDb, s.trimDb)));
    }

    void IsoEngine::updateCoefficients (float f1, float f2, float sweepHz, float sweepQ) noexcept
    {
        const float q = slope == Slope::db24 ? kButterworthQ : kLr2Q;
        for (auto& c : ch)
        {
            c.f1a.set (f1, q, sr); c.f1b.set (f1, q, sr); c.f1c.set (f1, q, sr);
            c.f2a.set (f2, q, sr); c.f2b.set (f2, q, sr); c.f2c.set (f2, q, sr);
            c.ap2.set (f2, q, sr);
            c.sweep.set (sweepHz, sweepQ, sr);
        }
    }

    void IsoEngine::tickCrossover (Channel& c, float x, float& lo, float& mid, float& hi) const noexcept
    {
        if (slope == Slope::db24)
        {
            const auto a = c.f1a.tick (x);
            const float low4 = c.f1b.tick (a.lp).lp;
            const float rest = c.f1c.tick (a.hp).hp;

            //  2nd-order all-pass at f2 = x - 2k*bp, k = sqrt 2.
            const auto ap = c.ap2.tick (low4);
            lo = low4 - 2.0f * c.ap2.k * ap.bp;

            const auto d = c.f2a.tick (rest);
            mid = c.f2b.tick (d.lp).lp;
            hi  = c.f2c.tick (d.hp).hp;
        }
        else
        {
            const auto a = c.f1a.tick (x);
            const float low2 = a.lp;
            const float rest = -a.hp;          // LR2 sums with the HP inverted

            //  1st-order all-pass at f2 = lp - hp of a Q = 0.5 section.
            const auto ap = c.ap2.tick (low2);
            lo = ap.lp - ap.hp;

            const auto d = c.f2a.tick (rest);
            mid = d.lp;
            hi  = -d.hp;
        }
    }

    void IsoEngine::process (juce::AudioBuffer<float>& buffer) noexcept
    {
        if (settings.bypass || channels == 0)
            return;

        const int n = buffer.getNumSamples();
        const int nch = juce::jmin (channels, buffer.getNumChannels());
        auto* const* data = buffer.getArrayOfWritePointers();

        for (int i = 0; i < n; ++i)
        {
            const bool moving = smF1.isSmoothing() || smF2.isSmoothing()
                             || smSweepHz.isSmoothing() || smSweepQ.isSmoothing();
            const float f1 = smF1.getNextValue();
            const float f2 = smF2.getNextValue();
            const float sw = smSweepHz.getNextValue();
            const float sq = smSweepQ.getNextValue();
            if (moving)
                updateCoefficients (f1, f2, sw, sq);

            const float g0 = smGain[0].getNextValue();
            const float g1 = smGain[1].getNextValue();
            const float g2 = smGain[2].getNextValue();
            const float trim = smTrim.getNextValue();

            for (int c = 0; c < nch; ++c)
            {
                auto& chan = ch[(size_t) c];
                //  A NaN or Inf from upstream would live in the SVF states
                //  for ever; every filter here is recursive. Scrub it to zero
                //  before anything with memory sees it - identity on finite
                //  input, so the bit-exact unity path is untouched.
                float x = data[c][i];
                if (! std::isfinite (x)) x = 0.0f;

                float lo, mid, hi;
                tickCrossover (chan, x, lo, mid, hi);
                float y = lo * g0 + mid * g1 + hi * g2;

                //  The sweep section always runs so its state stays warm; in
                //  the dead zone its output is simply not used.
                const auto f = chan.sweep.tick (y);
                if (sweepMode < 0)      y = f.lp;
                else if (sweepMode > 0) y = f.hp;

                data[c][i] = y * trim;
            }
        }
    }

    // -------------------------------------------------------------------------
    std::complex<double> IsoEngine::responseAt (double hz, const Settings& s) const noexcept
    {
        using C = std::complex<double>;
        if (s.bypass)
            return C (1.0, 0.0);

        float f1, f2;
        effectiveCrossovers (s.lowMidHz, s.midHighHz, f1, f2);

        C lo, mid, hi;
        if (s.slope == Slope::db24)
        {
            const auto r1 = sectionResponse (hz, f1, kButterworthQ, sr);
            const auto r2 = sectionResponse (hz, f2, kButterworthQ, sr);
            const C low4 = r1.lp * r1.lp;
            const C rest = r1.hp * r1.hp;
            const C ap   = 1.0 - 2.0 * (1.0 / kButterworthQ) * r2.bp;
            lo  = low4 * ap;
            mid = rest * r2.lp * r2.lp;
            hi  = rest * r2.hp * r2.hp;
        }
        else
        {
            const auto r1 = sectionResponse (hz, f1, kLr2Q, sr);
            const auto r2 = sectionResponse (hz, f2, kLr2Q, sr);
            const C rest = -r1.hp;
            lo  = r1.lp * (r2.lp - r2.hp);
            mid = rest * r2.lp;
            hi  = -rest * r2.hp;
        }

        C y = lo  * (double) bandLinearGain (s.gainDb[0], s.kill[0], s.floorMode)
            + mid * (double) bandLinearGain (s.gainDb[1], s.kill[1], s.floorMode)
            + hi  * (double) bandLinearGain (s.gainDb[2], s.kill[2], s.floorMode);

        float cut, q; int mode;
        filterLaw (s.filter, s.resonance, cut, q, mode);
        if (mode != 0)
        {
            const auto rf = sectionResponse (hz, cut, q, sr);
            y *= (mode < 0 ? rf.lp : rf.hp);
        }

        return y * (double) dbToLin (juce::jlimit (law::kTrimMinDb, law::kTrimMaxDb, s.trimDb));
    }
}
