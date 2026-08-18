// ISO measurement suite.
//
//     build/IsoTests_artefacts/<config>/IsoTests
//
// Exit code 0 = every check passed. Every check prints what it measured, so a
// failure is diagnosable without a debugger. Nothing here asserts a number that
// the code above it did not actually measure.

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include <IsoVersion.h>

#include "../Dsp/IsoEngine.h"

// -----------------------------------------------------------------------------
namespace
{
    std::atomic<int> gAllocations { 0 };
    std::atomic<bool> gCountAllocations { false };
}

void* operator new (std::size_t n)
{
    if (gCountAllocations.load (std::memory_order_relaxed))
        gAllocations.fetch_add (1, std::memory_order_relaxed);
    if (auto* p = std::malloc (n == 0 ? 1 : n))
        return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }

// -----------------------------------------------------------------------------
namespace
{
    int gChecks = 0, gFailures = 0;

    void check (bool ok, const char* label, const juce::String& measured)
    {
        ++gChecks;
        if (! ok) ++gFailures;
        std::printf ("  [%s] %-60s %s\n", ok ? "PASS" : "FAIL", label, measured.toRawUTF8());
    }

    void section (const char* name) { std::printf ("\n== %s ==\n", name); }
    juce::String f (double v, int dp = 3) { return juce::String (v, dp); }

    constexpr double kSr = 48000.0;
    using Engine   = iso::IsoEngine;
    using Settings = Engine::Settings;

    Settings unity() { return Settings {}; }

    Settings soloBand (int band, Engine::Slope slope = Engine::Slope::db24)
    {
        Settings s;
        s.slope = slope;
        for (int b = 0; b < 3; ++b) s.kill[(size_t) b] = (b != band);
        return s;
    }

    //  Steady-state gain of the engine at `hz`, in dB, measured by RMS after
    //  the filters have settled. Fresh engine each time so no state leaks.
    double measureDb (const Settings& s, double hz, double sr = kSr)
    {
        Engine e;
        e.setSettings (s);
        e.prepare (sr, 512, 1);

        const int settle  = (int) (0.4 * sr);
        const int measure = (int) (0.5 * sr);
        juce::AudioBuffer<float> buf (1, settle + measure);
        auto* d = buf.getWritePointer (0);
        for (int i = 0; i < settle + measure; ++i)
            d[i] = (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / sr);

        e.process (buf);

        double acc = 0.0;
        for (int i = settle; i < settle + measure; ++i) acc += (double) d[i] * d[i];
        const double rms = std::sqrt (acc / measure);
        return 20.0 * std::log10 (rms / std::sqrt (0.5) + 1e-15);
    }

    double maxAbs (const juce::AudioBuffer<float>& b)
    {
        double m = 0.0;
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                m = juce::jmax (m, (double) std::abs (b.getSample (c, i)));
        return m;
    }

    const double kProbeHz[] = { 30, 50, 80, 120, 200, 250, 320, 500, 800, 1200,
                                2000, 2500, 3200, 5000, 8000, 12000, 16000 };
}

// -----------------------------------------------------------------------------
int main()
{
    std::printf ("ISO %s (%s, %s) - measurement suite @ %.0f Hz\n",
                 iso::kVersion, iso::kGitDescribe, iso::kBuildDate, kSr);

    // 1 -------------------------------------------------------------------------
    section ("1. Unity sum is flat (the isolator property)");
    for (auto slope : { Engine::Slope::db24, Engine::Slope::db12 })
    {
        Settings s = unity();
        s.slope = slope;
        double worst = 0.0;
        for (double hz : kProbeHz)
            worst = juce::jmax (worst, std::abs (measureDb (s, hz)));
        check (worst < 0.05, slope == Engine::Slope::db24 ? "24 dB/oct: |sum| deviation over 17 probes" : "12 dB/oct: |sum| deviation over 17 probes",
               f (worst) + " dB (limit 0.05)");
    }

    // 2 -------------------------------------------------------------------------
    section ("2. Crossover points and slopes");
    {
        //  Low band alone at f1: LR is -6.02 dB at the corner. One octave up:
        //  the analytic figure, which the engine also publishes.
        Settings s = soloBand (0);
        Engine ref; ref.prepare (kSr, 512, 1);
        const double atCorner = measureDb (s, 250.0);
        check (std::abs (atCorner + 6.02) < 0.15, "LR4 low band at 250 Hz corner", f (atCorner) + " dB (want -6.02)");
        const double oct = measureDb (s, 500.0);
        const double octRef = ref.magnitudeDbAt (500.0, s);
        check (std::abs (oct - octRef) < 0.2 && oct < -23.0, "LR4 low band one octave above corner", f (oct) + " dB (analytic " + f (octRef) + ")");

        Settings s2 = soloBand (0, Engine::Slope::db12);
        const double c2 = measureDb (s2, 250.0);
        check (std::abs (c2 + 6.02) < 0.15, "LR2 low band at 250 Hz corner", f (c2) + " dB (want -6.02)");
        const double o2 = measureDb (s2, 500.0);
        check (o2 < -13.0 && o2 > -15.5, "LR2 low band one octave above corner (~ -14 dB)", f (o2) + " dB");

        Settings h = soloBand (2);
        const double hc = measureDb (h, 2500.0);
        check (std::abs (hc + 6.02) < 0.15, "LR4 high band at 2.5 kHz corner", f (hc) + " dB (want -6.02)");

        //  Two bands at unity, third killed: -6 dB corner between them means
        //  the pair sums to unity at their own corner.
        Settings lm = unity(); lm.kill[2] = true;
        const double lmAt250 = measureDb (lm, 250.0);
        check (std::abs (lmAt250) < 0.1, "Low + Mid at 250 Hz (their shared corner) sums to 0 dB", f (lmAt250) + " dB");
    }

    // 3 -------------------------------------------------------------------------
    section ("3. Kills");
    {
        Settings s = unity(); s.kill[1] = true;
        const double midResidual = measureDb (s, 790.0);   // geometric centre of 250..2500
        check (midResidual < -30.0, "Mid killed: residual at 790 Hz (leakage from LR4 skirts)", f (midResidual) + " dB");
        const double lowSafe = measureDb (s, 60.0);
        check (std::abs (lowSafe) < 0.2, "Mid killed: low band still unity at 60 Hz", f (lowSafe) + " dB");
        const double hiSafe = measureDb (s, 10000.0);
        check (std::abs (hiSafe) < 0.2, "Mid killed: high band still unity at 10 kHz", f (hiSafe) + " dB");

        Settings all = unity(); all.kill = { true, true, true };
        const double silence = measureDb (all, 1000.0);
        check (silence < -120.0, "All three killed: output", f (silence) + " dB");

        //  Knob at its floor in ISO mode is a kill: identical to the switch,
        //  down to the mid band's LR4 skirt that both leave behind at 50 Hz.
        Settings knob = unity(); knob.gainDb[0] = iso::law::kGainMinDb;
        Settings sw = unity(); sw.kill[0] = true;
        const double knobKill = measureDb (knob, 50.0), swKill = measureDb (sw, 50.0);
        check (std::abs (knobKill - swKill) < 0.01 && knobKill < -50.0, "Low knob at floor in ISO mode == kill switch (mid skirt remains)",
               f (knobKill) + " dB vs switch " + f (swKill) + " dB");

        Settings eq = knob; eq.floorMode = Engine::Floor::eq26;
        Engine ref; ref.prepare (kSr, 512, 1);
        const double eqFloor = measureDb (eq, 50.0);
        const double eqLaw = 20.0 * std::log10 (Engine::bandLinearGain (iso::law::kGainMinDb, false, Engine::Floor::eq26));
        check (std::abs (eqLaw + 26.0) < 0.01 && std::abs (eqFloor - ref.magnitudeDbAt (50.0, eq)) < 0.05,
               "Same knob in EQ mode floors at -26 dB (DJM curve)", "law " + f (eqLaw, 2) + " dB, measured " + f (eqFloor) + " dB incl. mid skirt");

        //  ISO fade region: -27 dB knob sits between -24 dB and silence.
        Settings fade = unity(); fade.gainDb[0] = -27.0f;
        const double fadeDb = measureDb (fade, 50.0);
        check (fadeDb < -24.5 && fadeDb > -40.0, "ISO knob at -27 dB is inside the fade to silence", f (fadeDb) + " dB");
    }

    // 4 -------------------------------------------------------------------------
    section ("4. Band gain accuracy");
    {
        struct T { int band; double hz; float db; };
        const T tests[] = { { 0, 50.0, 6.0f }, { 0, 50.0, -12.0f }, { 1, 800.0, 12.0f },
                            { 1, 800.0, -6.0f }, { 2, 10000.0, 6.0f }, { 2, 10000.0, -20.0f } };
        for (const auto& t : tests)
        {
            Settings s = unity(); s.gainDb[(size_t) t.band] = t.db;
            const double m = measureDb (s, t.hz);
            char label[96];
            std::snprintf (label, sizeof label, "band %d at %.0f Hz set %+.0f dB", t.band, t.hz, (double) t.db);
            check (std::abs (m - t.db) < 0.25, label, f (m) + " dB");
        }
    }

    // 5 -------------------------------------------------------------------------
    section ("5. Analytic response == measured response");
    {
        //  A handful of deliberately awkward settings; the curve view draws
        //  the analytic figure, so it must be the truth.
        Settings a; a.gainDb = { 4.0f, -9.0f, 7.0f }; a.lowMidHz = 120.0f; a.midHighHz = 6000.0f;
        Settings b; b.slope = Engine::Slope::db12; b.gainDb = { -3.0f, 8.0f, -18.0f }; b.kill[2] = false;
        Settings c; c.filter = -0.55f; c.resonance = 0.6f; c.gainDb = { 2.0f, 0.0f, -4.0f };
        Settings d; d.filter = 0.4f; d.resonance = 0.9f; d.trimDb = -5.0f; d.kill[0] = true;
        Settings e; e.lowMidHz = 900.0f; e.midHighHz = 800.0f; e.gainDb = { -10.0f, 5.0f, 0.0f };   // clamp path

        Engine ref; ref.prepare (kSr, 512, 1);
        int i = 0;
        for (const auto& s : { a, b, c, d, e })
        {
            double worst = 0.0; double worstHz = 0.0;
            for (double hz : kProbeHz)
            {
                const double m = measureDb (s, hz);
                const double an = ref.magnitudeDbAt (hz, s);
                if (an < -60.0) continue;   // below the measurement floor: not compared
                const double err = std::abs (m - an);
                if (err > worst) { worst = err; worstHz = hz; }
            }
            char label[64];
            std::snprintf (label, sizeof label, "settings %c: max |measured - analytic|", (char) ('A' + i++));
            check (worst < 0.15, label, f (worst) + " dB @ " + f (worstHz, 0) + " Hz");
        }
    }

    // 6 -------------------------------------------------------------------------
    section ("6. Sweep filter");
    {
        float cut, q; int mode;
        Engine::filterLaw (0.0f, 1.0f, cut, q, mode);
        check (mode == 0, "centre = off", "mode " + juce::String (mode));
        Engine::filterLaw (-1.0f, 0.0f, cut, q, mode);
        check (mode == -1 && std::abs (cut - iso::law::kFilterLpMinHz) < 0.5f, "full left = LP at its minimum", f (cut, 1) + " Hz");
        Engine::filterLaw (1.0f, 0.0f, cut, q, mode);
        check (mode == 1 && std::abs (cut - iso::law::kFilterHpMaxHz) < 0.5f, "full right = HP at its maximum", f (cut, 1) + " Hz");
        Engine::filterLaw (-0.05f, 1.0f, cut, q, mode);
        check (mode == -1 && q < 0.8f, "just off centre with RES 100 %: resonance still ramped down", "Q " + f (q, 3));
        Engine::filterLaw (-1.0f, 1.0f, cut, q, mode);
        check (std::abs (q - iso::law::kFilterQMax) < 0.01f, "RES 100 % well into travel = Q max", "Q " + f (q, 2));

        Settings lp; lp.filter = -0.5f; lp.resonance = 0.0f;
        Engine::filterLaw (lp.filter, lp.resonance, cut, q, mode);
        const double atCut = measureDb (lp, cut);
        check (std::abs (atCut + 3.01) < 0.2, "LP at half travel: -3 dB at its cutoff", f (atCut) + " dB @ " + f (cut, 0) + " Hz");
        const double octUp = measureDb (lp, cut * 4.0);
        check (octUp < -22.0, "LP two octaves above cutoff (12 dB/oct)", f (octUp) + " dB");

        Settings hp; hp.filter = 0.5f; hp.resonance = 0.0f;
        Engine::filterLaw (hp.filter, hp.resonance, cut, q, mode);
        const double hpAt = measureDb (hp, cut);
        check (std::abs (hpAt + 3.01) < 0.2, "HP at half travel: -3 dB at its cutoff", f (hpAt) + " dB @ " + f (cut, 0) + " Hz");

        Settings dead; dead.filter = 0.03f; dead.resonance = 1.0f;
        double worst = 0.0;
        for (double hz : kProbeHz) worst = juce::jmax (worst, std::abs (measureDb (dead, hz)));
        check (worst < 0.05, "inside dead zone with RES 100 %: flat", f (worst) + " dB");
    }

    // 7 -------------------------------------------------------------------------
    section ("7. Kill switch is click-free and fast");
    {
        Engine e; e.setSettings (unity()); e.prepare (kSr, 256, 1);
        const int n = 9600;
        juce::AudioBuffer<float> buf (1, n);
        auto* d = buf.getWritePointer (0);
        for (int i = 0; i < n; ++i) d[i] = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 30.0 * i / kSr);

        //  Settle, then kill the low band at sample 4800 (a block boundary).
        juce::AudioBuffer<float> first (buf.getArrayOfWritePointers(), 1, 0, 4800);
        e.process (first);
        Settings k = unity(); k.kill[0] = true; e.setSettings (k);
        juce::AudioBuffer<float> second (buf.getArrayOfWritePointers(), 1, 4800, 4800);
        e.process (second);

        double maxStep = 0.0;
        for (int i = 4801; i < n; ++i) maxStep = juce::jmax (maxStep, (double) std::abs (d[i] - d[i - 1]));
        //  The tone's own maximum step is 0.5 * 2pi * 30 / 48000 = 0.0020;
        //  the 6 ms ramp adds at most 0.5 / 288 = 0.0017.
        check (maxStep < 0.006, "max sample-to-sample step through the kill", f (maxStep, 5) + " (limit 0.006)");

        double after = 0.0;
        for (int i = 4800 + 600; i < n; ++i) after = juce::jmax (after, (double) std::abs (d[i]));   // 12.5 ms later
        check (after < 1.0e-3, "residual 12.5 ms after kill (30 Hz: mid skirt is -74 dB)", f (after, 6));
    }

    // 8 -------------------------------------------------------------------------
    section ("8. Crossover clamp and law helpers");
    {
        float f1, f2;
        Engine::effectiveCrossovers (1000.0f, 800.0f, f1, f2);
        check (f2 == 800.0f && std::abs (f1 - 800.0f / iso::law::kMinCrossoverRatio) < 0.01f,
               "low-mid may not cross mid-high (clamped to ratio 1.5)", f (f1, 1) + " / " + f (f2, 1) + " Hz");
        Engine::effectiveCrossovers (250.0f, 2500.0f, f1, f2);
        check (f1 == 250.0f && f2 == 2500.0f, "defaults pass through unchanged", f (f1, 0) + " / " + f (f2, 0));

        check (Engine::bandLinearGain (0.0f, false, Engine::Floor::kill) == 1.0f, "0 dB = unity", "1.0");
        check (Engine::bandLinearGain (0.0f, true,  Engine::Floor::kill) == 0.0f, "kill wins over gain", "0.0");
        check (std::abs (Engine::bandLinearGain (6.0f, false, Engine::Floor::eq26) - 1.99526f) < 1e-3f, "+6 dB linear", f (Engine::bandLinearGain (6.0f, false, Engine::Floor::eq26), 4));
        //  Monotonic through the fade region.
        bool mono = true; float prev = -1.0f;
        for (float db = iso::law::kGainMinDb; db <= 0.0f; db += 0.25f)
        {
            const float g = Engine::bandLinearGain (db, false, Engine::Floor::kill);
            if (g < prev) mono = false;
            prev = g;
        }
        check (mono, "ISO gain law monotonic from floor to 0 dB", mono ? "yes" : "NO");
    }

    // 9 -------------------------------------------------------------------------
    section ("9. Safety: allocation-free, NaN-free, silent in -> silent out, all rates");
    {
        for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            Engine e;
            Settings s; s.gainDb = { 12.0f, 12.0f, 12.0f }; s.filter = -0.9f; s.resonance = 1.0f;
            e.setSettings (s); e.prepare (sr, 512, 2);
            juce::AudioBuffer<float> buf (2, 512);
            juce::Random r (7);
            bool finite = true;
            gAllocations = 0; gCountAllocations = true;
            for (int blk = 0; blk < 200; ++blk)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i)
                        buf.setSample (c, i, r.nextFloat() * 2.0f - 1.0f);
                //  Wiggle everything every block, the way automation would.
                s.lowMidHz = 60.0f + 900.0f * r.nextFloat();
                s.midHighHz = 800.0f + 9000.0f * r.nextFloat();
                s.filter = r.nextFloat() * 2.0f - 1.0f;
                s.kill[0] = r.nextBool();
                s.slope = r.nextBool() ? Engine::Slope::db12 : Engine::Slope::db24;
                e.setSettings (s);
                e.process (buf);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i)
                        if (! std::isfinite (buf.getSample (c, i))) finite = false;
            }
            gCountAllocations = false;
            char label[64];
            std::snprintf (label, sizeof label, "%.0f Hz: 200 blocks of noise + automation, allocations", sr);
            check (gAllocations == 0 && finite, label, juce::String (gAllocations.load()) + (finite ? ", all finite" : ", NON-FINITE OUTPUT"));
        }

        Engine e; e.setSettings (unity()); e.prepare (kSr, 512, 2);
        juce::AudioBuffer<float> z (2, 4096); z.clear();
        e.process (z);
        check (maxAbs (z) == 0.0, "silence in -> exactly silence out", f (maxAbs (z), 9));

        //  One poisoned sample from upstream must not poison the filters for ever.
        {
            Engine ep; ep.setSettings (unity()); ep.prepare (kSr, 256, 1);
            juce::AudioBuffer<float> pb (1, 256); pb.clear();
            pb.setSample (0, 10, std::numeric_limits<float>::quiet_NaN());
            pb.setSample (0, 11, std::numeric_limits<float>::infinity());
            ep.process (pb);
            bool ok = true;
            for (int blk = 0; blk < 10; ++blk)
            {
                for (int i = 0; i < 256; ++i) pb.setSample (0, i, 0.1f);
                ep.process (pb);
                for (int i = 0; i < 256; ++i) if (! std::isfinite (pb.getSample (0, i))) ok = false;
            }
            check (ok, "NaN/Inf input sample: engine recovers, later output finite", ok ? "finite" : "POISONED");
        }

        //  Mono buffer through a stereo-prepared engine must not touch memory it does not own.
        juce::AudioBuffer<float> m (1, 256); m.clear(); m.setSample (0, 0, 1.0f);
        e.process (m);
        check (std::isfinite (m.getSample (0, 255)), "mono buffer through stereo prepare", "ok");
    }

    // 10 ------------------------------------------------------------------------
    section ("10. Throughput");
    {
        Engine e; Settings s; s.filter = -0.5f; e.setSettings (s); e.prepare (kSr, 512, 2);
        juce::AudioBuffer<float> buf (2, 512);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) buf.setSample (c, i, 0.1f);
        const int blocks = (int) (kSr * 60.0 / 512.0);   // one minute of stereo audio
        const auto t0 = juce::Time::getHighResolutionTicks();
        for (int b = 0; b < blocks; ++b) e.process (buf);
        const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        const double xrt = 60.0 / secs;
        //  Informational: never gate on CI hardware (see workspace notes).
        check (true, "one minute of stereo processed in", f (secs * 1000.0, 1) + " ms  (" + f (xrt, 0) + "x realtime)");
    }

    std::printf ("\n%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
