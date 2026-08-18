// ISO - a DJ isolator EQ.
//
// The engine is the whole product: a three-way Linkwitz-Riley crossover whose
// bands are recombined through gain stages that go all the way to silence, and
// a Pioneer-style bipolar sweep filter after it. Everything the UI shows and
// everything the tests measure comes from the maths in this file, so it is
// written for one thread, one channel-set, no allocations after prepare().
//
// Topology (per channel):
//
//     x ─┬─ LP(f1) ── AP(f2) ────────────────── × gLow  ─┐
//        └─ HP(f1) ─┬─ LP(f2) ────────────────── × gMid  ─┼─ Σ ── FILTER ── TRIM ── y
//                   └─ HP(f2) ────────────────── × gHigh ─┘
//
// With all three gains at unity the sum is an all-pass: flat magnitude to
// within floating-point error, which is the property that makes an isolator
// an isolator and not "three filters". 24 dB/oct is LR4 (two cascaded
// Butterworth-2 sections; the sum is a 2nd-order all-pass at Q = 1/sqrt 2).
// 12 dB/oct is LR2 (one section at Q = 0.5; the sum is a 1st-order all-pass,
// and the mid band comes out inverted, exactly as it does on a Xone:92).

#pragma once

#include <array>
#include <cmath>
#include <complex>

#include <juce_audio_basics/juce_audio_basics.h>

namespace iso
{
    // -------------------------------------------------------------------------
    //  Design constants. Everything user-facing that is a number lives here so
    //  Parameters.cpp, the tests and the manual quote the same figures.
    // -------------------------------------------------------------------------
    namespace law
    {
        inline constexpr int   kBands        = 3;

        //  Band gain travel. The knob's bottom is the KILL point in ISO mode
        //  (rendered as -inf) and the -26 dB floor in EQ mode. Above the floor
        //  the scale is ordinary decibels. +12 covers the hottest hardware
        //  isolators (+10) with a little to spare; the DJM's +6 is a preset.
        inline constexpr float kGainMinDb    = -30.0f;   // knob floor; == KILL in ISO mode
        inline constexpr float kGainMaxDb    =  12.0f;
        inline constexpr float kEqFloorDb    = -26.0f;   // Pioneer DJM "EQ" curve floor

        inline constexpr float kLowMidMinHz  =  60.0f;
        inline constexpr float kLowMidMaxHz  =  1000.0f;
        inline constexpr float kLowMidDefHz  =  250.0f;  // Xone:92 low / low-mid corner

        inline constexpr float kMidHighMinHz =  800.0f;
        inline constexpr float kMidHighMaxHz =  10000.0f;
        inline constexpr float kMidHighDefHz =  2500.0f; // Xone:92 hi-mid / high corner

        //  The two crossovers may never cross: the low-mid point is clamped at
        //  least this ratio below the mid-high point.
        inline constexpr float kMinCrossoverRatio = 1.5f;

        //  Bipolar sweep filter. Left of centre = low-pass sweeping down from
        //  wide-open, right = high-pass sweeping up. |x| below the dead zone
        //  is "off": the filter is parked at its open extreme with no
        //  resonance so it is inaudible, not bypassed with a click.
        inline constexpr float kFilterDeadZone = 0.04f;
        inline constexpr float kFilterLpOpenHz = 20000.0f;
        inline constexpr float kFilterLpMinHz  = 60.0f;
        inline constexpr float kFilterHpOpenHz = 20.0f;
        inline constexpr float kFilterHpMaxHz  = 12000.0f;
        inline constexpr float kFilterQMin     = 0.7071f;
        inline constexpr float kFilterQMax     = 6.0f;
        //  Resonance fades in over this much of the travel out of the dead
        //  zone so a high RES setting cannot leave a peak at 20 kHz when the
        //  knob is only just off centre.
        inline constexpr float kFilterResRamp  = 0.20f;

        inline constexpr float kTrimMinDb = -12.0f;
        inline constexpr float kTrimMaxDb =  12.0f;

        //  Gain and cutoff smoothing. 6 ms is short enough that a kill still
        //  reads as a switch, long enough that it does not click.
        inline constexpr float kGainRampSeconds = 0.006f;
        inline constexpr float kFreqRampSeconds = 0.020f;
    }

    // -------------------------------------------------------------------------
    //  One TPT state-variable section (Zavalishin). Yields LP, HP and the
    //  BP needed for the all-pass in one pass; exact bilinear mapping so the
    //  analytic response below is the response, not an approximation of it.
    // -------------------------------------------------------------------------
    struct Svf
    {
        float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        float ic1 = 0.0f, ic2 = 0.0f;

        void reset() noexcept { ic1 = ic2 = 0.0f; }

        void set (float hz, float q, double sr) noexcept
        {
            const float nyq = (float) (0.49 * sr);
            hz = juce::jlimit (5.0f, nyq, hz);
            g  = std::tan (juce::MathConstants<float>::pi * hz / (float) sr);
            k  = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        struct Out { float lp, hp, bp; };

        Out tick (float x) noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return { v2, x - k * v1 - v2, v1 };
        }

        //  Analytic response of this section at `hz`, as the analog prototype
        //  evaluated at the pre-warped frequency. Because the SVF is the exact
        //  bilinear transform of that prototype, this is its true response.
        struct Response { std::complex<double> lp, hp, bp; };

        Response responseAt (double hz, double sr) const noexcept
        {
            const double w = std::tan (juce::MathConstants<double>::pi * hz / sr) / (double) g;
            const std::complex<double> s (0.0, w);
            const std::complex<double> d = s * s + (double) k * s + 1.0;
            return { 1.0 / d, (s * s) / d, s / d };
        }
    };

    // -------------------------------------------------------------------------
    class IsoEngine
    {
    public:
        enum class Slope { db12 = 0, db24 = 1 };
        enum class Floor { kill = 0, eq26 = 1 };

        struct Settings
        {
            float lowMidHz  = law::kLowMidDefHz;
            float midHighHz = law::kMidHighDefHz;
            std::array<float, law::kBands> gainDb { 0.0f, 0.0f, 0.0f };
            std::array<bool,  law::kBands> kill   { false, false, false };
            Slope slope = Slope::db24;
            Floor floorMode = Floor::kill;
            float filter    = 0.0f;   // -1 (LP fully closed) .. +1 (HP fully closed)
            float resonance = 0.25f;  // 0..1
            float trimDb    = 0.0f;
            bool  bypass    = false;
        };

        static constexpr int kMaxChannels = 2;

        void prepare (double sampleRate, int maxBlock, int numChannels);
        void reset();

        //  Called once per block, from the audio thread. Never allocates.
        void setSettings (const Settings&) noexcept;
        const Settings& getSettings() const noexcept { return settings; }

        void process (juce::AudioBuffer<float>&) noexcept;

        //  --- derived, shared by the DSP, the UI and the tests -------------------
        //  Effective linear gain of a band after kill and floor law.
        static float bandLinearGain (float gainDb, bool kill, Floor) noexcept;
        //  Effective crossover pair after the anti-crossing clamp.
        static void  effectiveCrossovers (float lowMidHz, float midHighHz,
                                          float& f1, float& f2) noexcept;
        //  Sweep-filter law: given the bipolar knob and RES, what cutoff / Q /
        //  mode is the filter actually at. mode: -1 LP, +1 HP, 0 parked-open.
        static void  filterLaw (float filter, float resonance,
                                float& cutoffHz, float& q, int& mode) noexcept;

        //  Analytic complex response of the whole chain at `hz` for the given
        //  settings at this engine's sample rate: bands summed WITH phase, then
        //  filter, then trim. This is what the curve view draws and what the
        //  measurement suite compares its swept sines against.
        std::complex<double> responseAt (double hz, const Settings&) const noexcept;
        double magnitudeDbAt (double hz, const Settings& s) const noexcept
        {
            return 20.0 * std::log10 (std::abs (responseAt (hz, s)) + 1e-12);
        }

        double getSampleRate() const noexcept { return sr; }

    private:
        struct Channel
        {
            //  f1 stage: A splits, B cascades on lpA, C cascades on hpA (LR4 only).
            Svf f1a, f1b, f1c;
            //  f2 stage on the HP(f1) path, same arrangement.
            Svf f2a, f2b, f2c;
            //  All-pass at f2 on the low band (one section in either slope).
            Svf ap2;
            //  Sweep filter.
            Svf sweep;
        };

        void updateCoefficients (float f1, float f2, float sweepHz, float sweepQ) noexcept;
        void tickCrossover (Channel&, float x, float& lo, float& mid, float& hi) const noexcept;

        double sr = 48000.0;
        int channels = 0;
        Settings settings;

        std::array<Channel, kMaxChannels> ch;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smF1, smF2, smSweepHz;
        juce::SmoothedValue<float> smSweepQ;
        std::array<juce::SmoothedValue<float>, law::kBands> smGain;
        juce::SmoothedValue<float> smTrim;

        //  Sweep mode is not smoothed - it flips at the dead zone where both
        //  modes are parked open and audibly identical.
        int sweepMode = 0;
        Slope slope = Slope::db24;
    };
}
