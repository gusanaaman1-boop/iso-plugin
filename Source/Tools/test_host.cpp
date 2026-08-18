// ISO host-contract suite.
//
//     build/IsoHostTests_artefacts/<config>/IsoHostTests
//
// IsoTests measures the DSP. This drives the whole AudioProcessor the way a
// host does - parameters, prepare/process lifecycle, bus layouts, state, programs,
// the editor - and asserts what pluginval would assert, directly. Exit 0 = pass.

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

#include <juce_gui_extra/juce_gui_extra.h>

#include <IsoVersion.h>

#include "../Core/ParameterIds.h"
#include "../Core/Presets.h"
#include "../PluginEditor.h"
#include "../PluginProcessor.h"

namespace
{
    std::atomic<int> gAllocations { 0 };
    std::atomic<bool> gCountAllocations { false };
    //  Diagnostics for a failure we cannot reproduce here: the sizes of the
    //  first few counted allocations, printed with the FAIL line.
    std::atomic<std::size_t> gAllocSizes[8];
}

void* operator new (std::size_t n)
{
    if (gCountAllocations.load (std::memory_order_relaxed))
    {
        const int i = gAllocations.fetch_add (1, std::memory_order_relaxed);
        if (i < 8) gAllocSizes[i].store (n, std::memory_order_relaxed);
    }
    if (auto* p = std::malloc (n == 0 ? 1 : n))
        return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }

namespace
{
    int gChecks = 0, gFailures = 0;

    void check (bool ok, const char* label, const juce::String& measured = {})
    {
        ++gChecks;
        if (! ok) ++gFailures;
        std::printf ("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", label, measured.toRawUTF8());
    }
    void section (const char* name) { std::printf ("\n== %s ==\n", name); }
    juce::String f (double v, int dp = 3) { return juce::String (v, dp); }

    const char* const kAllIds[] = {
        iso::id::lowMid, iso::id::midHigh,
        iso::id::lowGain, iso::id::midGain, iso::id::highGain,
        iso::id::lowKill, iso::id::midKill, iso::id::highKill,
        iso::id::slope, iso::id::floorMode,
        iso::id::filter, iso::id::resonance, iso::id::trim, iso::id::bypass };
    constexpr int kNumIds = (int) (sizeof (kAllIds) / sizeof (kAllIds[0]));

    void setParam (IsoAudioProcessor& p, const char* id, float value)
    {
        if (auto* param = p.getState().getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    }
    float getParam (IsoAudioProcessor& p, const char* id)
    {
        auto* param = p.getState().getParameter (id);
        return param != nullptr ? param->convertFrom0to1 (param->getValue()) : 0.0f;
    }

    //  Not the default in any parameter, so a dropped one is visible.
    void applyBusy (IsoAudioProcessor& p)
    {
        setParam (p, iso::id::lowMid, 137.0f);   setParam (p, iso::id::midHigh, 4100.0f);
        setParam (p, iso::id::lowGain, 4.5f);    setParam (p, iso::id::midGain, -11.0f); setParam (p, iso::id::highGain, 7.0f);
        setParam (p, iso::id::lowKill, 1.0f);    setParam (p, iso::id::highKill, 1.0f);
        setParam (p, iso::id::slope, 0.0f);      setParam (p, iso::id::floorMode, 1.0f);
        setParam (p, iso::id::filter, -0.42f);   setParam (p, iso::id::resonance, 0.66f);
        setParam (p, iso::id::trim, -3.5f);      setParam (p, iso::id::bypass, 0.0f);
    }

    void fillNoise (juce::AudioBuffer<float>& b, juce::uint32& seed, float amp = 0.3f)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                seed = seed * 1664525u + 1013904223u;
                b.setSample (c, i, amp * ((float) ((double) (seed >> 8) / 8388608.0 - 1.0)));
            }
    }

    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (c, i))) return false;
        return true;
    }

    double peak (const juce::AudioBuffer<float>& b)
    {
        double m = 0.0;
        for (int c = 0; c < b.getNumChannels(); ++c)
            m = juce::jmax (m, (double) b.getMagnitude (c, 0, b.getNumSamples()));
        return m;
    }

    void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::printf ("ISO %s (%s, %s) - host-contract suite\n", iso::kVersion, iso::kGitDescribe, iso::kBuildDate);

    // 1 -------------------------------------------------------------------------
    section ("1. Parameter contract");
    {
        IsoAudioProcessor p;
        check (p.getParameters().size() == kNumIds, "parameter count", juce::String (p.getParameters().size()) + " (want " + juce::String (kNumIds) + ")");
        bool allThere = true;
        for (auto* id : kAllIds) if (p.getState().getParameter (id) == nullptr) { allThere = false; std::printf ("      missing %s\n", id); }
        check (allThere, "every ID resolves");

        bool named = true, automatable = true;
        for (auto* raw : p.getParameters())
        {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (raw);
            if (rp == nullptr || rp->getName (64).isEmpty()) named = false;
            if (! raw->isAutomatable()) automatable = false;
        }
        check (named, "every parameter has a name");
        check (automatable, "every parameter is automatable");

        //  Text round trip: what the read-out prints must parse back to itself.
        struct RT { const char* id; float v; };
        const RT rts[] = { { iso::id::lowGain, 6.0f }, { iso::id::lowGain, -12.0f }, { iso::id::lowGain, iso::law::kGainMinDb },
                           { iso::id::midHigh, 2500.0f }, { iso::id::lowMid, 250.0f }, { iso::id::trim, -3.0f },
                           { iso::id::filter, 0.0f }, { iso::id::filter, -0.55f }, { iso::id::filter, 0.5f },
                           { iso::id::resonance, 0.4f } };
        for (const auto& rt : rts)
        {
            auto* rp = p.getState().getParameter (rt.id);
            const float norm = rp->convertTo0to1 (rt.v);
            const auto text = rp->getText (norm, 0);
            const float back = rp->convertFrom0to1 (rp->getValueForText (text));
            const float tol = juce::String (rt.id) == iso::id::midHigh || juce::String (rt.id) == iso::id::lowMid ? rt.v * 0.02f
                            : juce::String (rt.id) == iso::id::filter ? 0.02f : 0.06f;
            char label[96]; std::snprintf (label, sizeof label, "text round trip %s: %s", rt.id, text.toRawUTF8());
            check (std::abs (back - rt.v) <= tol, label, "-> " + f (back));
        }
        //  Typing hardware words lands where they mean.
        auto* lg = p.getState().getParameter (iso::id::lowGain);
        check (lg->convertFrom0to1 (lg->getValueForText ("KILL")) <= iso::law::kGainMinDb + 0.01f, "typing 'KILL' into a band knob = floor");
        check (std::abs (lg->convertFrom0to1 (lg->getValueForText ("+3 dB")) - 3.0f) < 0.06f, "typing '+3 dB' = +3.0");
        auto* fp = p.getState().getParameter (iso::id::filter);
        check (std::abs (fp->convertFrom0to1 (fp->getValueForText ("OFF"))) < 1e-4f, "typing 'OFF' into FILTER = centre");

        //  Defaults are the neutral state.
        IsoAudioProcessor q;
        const auto s = iso::readSettings (q.getState());
        //  (JUCE hands a -1..1 parameter's centre back as ~5e-8, not 0; the
        //  filter's dead zone and the gain law both treat that as exactly off.)
        check (s.gainDb[0] == 0.0f && s.gainDb[1] == 0.0f && s.gainDb[2] == 0.0f && ! s.kill[0] && ! s.kill[1] && ! s.kill[2]
               && std::abs (s.filter) < 1e-6f && std::abs (s.trimDb) < 1e-5f && ! s.bypass, "defaults are transparent");
        check (std::abs (s.lowMidHz - iso::law::kLowMidDefHz) < 0.01f && std::abs (s.midHighHz - iso::law::kMidHighDefHz) < 0.1f
               && s.slope == iso::IsoEngine::Slope::db24 && s.floorMode == iso::IsoEngine::Floor::kill, "defaults: 250 / 2.5k, 24 dB, ISO cut");
        //  And the default state processes as bit-exact unity - the isolator promise.
        {
            IsoAudioProcessor u; u.setPlayConfigDetails (2, 2, 48000.0, 512); u.prepareToPlay (48000.0, 512);
            juce::MidiBuffer midi; juce::AudioBuffer<float> in (2, 512), out (2, 512); juce::uint32 seed = 21;
            //  The sum is an ALL-PASS: same magnitude at every frequency, not
            //  the same waveform. So compare energy, not samples.
            double ei = 0.0, eo = 0.0;
            for (int blk = 0; blk < 60; ++blk)
            {
                fillNoise (in, seed); out.makeCopyOf (in); u.processBlock (out, midi);
                if (blk > 20)
                    for (int i = 0; i < 512; ++i) { ei += (double) in.getSample (0, i) * in.getSample (0, i); eo += (double) out.getSample (0, i) * out.getSample (0, i); }
            }
            const double db = 10.0 * std::log10 (eo / ei);
            check (std::abs (db) < 0.1, "default state: energy out / energy in on noise (all-pass sum)", f (db, 4) + " dB");
        }
    }

    // 2 -------------------------------------------------------------------------
    section ("2. Bus layouts");
    {
        IsoAudioProcessor p;
        auto layout = [] (juce::AudioChannelSet in, juce::AudioChannelSet out)
        {
            juce::AudioProcessor::BusesLayout l;
            l.inputBuses.add (in); l.outputBuses.add (out);
            return l;
        };
        check (p.checkBusesLayoutSupported (layout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo())), "stereo in / stereo out accepted");
        check (p.checkBusesLayoutSupported (layout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono())), "mono in / mono out accepted");
        check (! p.checkBusesLayoutSupported (layout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo())), "mono in / stereo out rejected");
        check (! p.checkBusesLayoutSupported (layout (juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1())), "5.1 rejected");
        check (p.getTailLengthSeconds() == 0.0 && p.getLatencySamples() == 0, "zero latency, zero tail");
        check (! p.acceptsMidi() && ! p.producesMidi() && ! p.isMidiEffect(), "no MIDI");
    }

    // 3 -------------------------------------------------------------------------
    section ("3. Lifecycle: rates, block sizes, silence, allocations, NaN");
    {
        juce::MidiBuffer midi;
        for (double sr : { 44100.0, 48000.0, 96000.0 })
            for (int block : { 32, 64, 256, 1024, 4096 })
            {
                IsoAudioProcessor p;
                p.setPlayConfigDetails (2, 2, sr, block);
                p.prepareToPlay (sr, block);
                applyBusy (p);
                juce::AudioBuffer<float> buf (2, block);
                juce::uint32 seed = 7;
                bool finite = true;
                for (int b = 0; b < 8; ++b) { fillNoise (buf, seed); p.processBlock (buf, midi); finite = finite && allFinite (buf); }
                //  Now count allocations on the audio path only. The parameter
                //  is set OUTSIDE the counted region: setValueNotifyingHost is
                //  the host's call, and on Windows JUCE's listener fan-out may
                //  post a message-thread update (one allocation) - that is not
                //  processBlock and never runs on the audio thread.
                gAllocations = 0;
                for (int b = 0; b < 8; ++b)
                {
                    fillNoise (buf, seed);
                    setParam (p, iso::id::lowGain, (float) (b % 5) - 2.0f);
                    gCountAllocations = true;
                    p.processBlock (buf, midi);
                    gCountAllocations = false;
                    finite = finite && allFinite (buf);
                }
                p.releaseResources();
                char label[80]; std::snprintf (label, sizeof label, "%.0f Hz / %d samples: finite, allocations", sr, block);
                check (finite && gAllocations == 0, label, juce::String (gAllocations.load()));
            }

        IsoAudioProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> z (2, 512); z.clear();
        for (int b = 0; b < 4; ++b) p.processBlock (z, midi);
        check (peak (z) == 0.0, "silence in -> exactly silence out", f (peak (z), 9));

        //  Smaller block than prepared for (hosts do this at loop points).
        juce::AudioBuffer<float> small (2, 17); juce::uint32 seed = 3; fillNoise (small, seed);
        p.processBlock (small, midi);
        check (allFinite (small), "17-sample block after a 512 prepare");

        //  Mono buffer into a stereo-prepared processor must not touch channel 1.
        juce::AudioBuffer<float> mono (1, 256); fillNoise (mono, seed);
        p.processBlock (mono, midi);
        check (allFinite (mono), "mono buffer through stereo prepare");

        //  NaN from upstream: the plug-in recovers.
        juce::AudioBuffer<float> nan (2, 256); nan.clear();
        nan.setSample (0, 5, std::nanf ("")); nan.setSample (1, 6, INFINITY);
        p.processBlock (nan, midi);
        bool rec = true;
        for (int b = 0; b < 10; ++b) { fillNoise (nan, seed); p.processBlock (nan, midi); rec = rec && allFinite (nan); }
        check (rec, "NaN / Inf input: output finite ten blocks later");

        //  Two full prepare cycles do not leak state into each other.
        p.prepareToPlay (44100.0, 128); p.releaseResources(); p.prepareToPlay (96000.0, 2048);
        juce::AudioBuffer<float> b2 (2, 2048); fillNoise (b2, seed); p.processBlock (b2, midi);
        check (allFinite (b2), "re-prepare at a different rate");
    }

    // 4 -------------------------------------------------------------------------
    section ("4. Behaviour through the parameter layer");
    {
        juce::MidiBuffer midi;
        auto rms = [] (const juce::AudioBuffer<float>& b)
        {
            double a = 0.0; for (int i = 0; i < b.getNumSamples(); ++i) a += (double) b.getSample (0, i) * b.getSample (0, i);
            return std::sqrt (a / b.getNumSamples());
        };
        auto tone = [] (juce::AudioBuffer<float>& b, double hz, int offset)
        {
            for (int c = 0; c < b.getNumChannels(); ++c)
                for (int i = 0; i < b.getNumSamples(); ++i)
                    b.setSample (c, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * (offset + i) / 48000.0));
        };

        //  Bypass = identity.
        {
            IsoAudioProcessor p; p.setPlayConfigDetails (2, 2, 48000.0, 512); p.prepareToPlay (48000.0, 512);
            applyBusy (p); setParam (p, iso::id::bypass, 1.0f);
            juce::AudioBuffer<float> in (2, 512), out (2, 512);
            tone (in, 440.0, 0); out.makeCopyOf (in);
            for (int b = 0; b < 4; ++b) p.processBlock (out, midi);   // same block repeated; bypass must not touch it
            double d = 0.0; for (int i = 0; i < 512; ++i) d = juce::jmax (d, (double) std::abs (out.getSample (0, i) - in.getSample (0, i)));
            check (d == 0.0, "bypass: output == input bit-exact", f (d, 9));
        }

        //  Kill parameter kills.
        {
            IsoAudioProcessor p; p.setPlayConfigDetails (2, 2, 48000.0, 512); p.prepareToPlay (48000.0, 512);
            juce::AudioBuffer<float> b (2, 512);
            double before = 0.0, after = 0.0;
            for (int blk = 0; blk < 60; ++blk) { tone (b, 60.0, blk * 512); p.processBlock (b, midi); if (blk > 40) before = rms (b); }
            setParam (p, iso::id::lowKill, 1.0f);
            for (int blk = 60; blk < 120; ++blk) { tone (b, 60.0, blk * 512); p.processBlock (b, midi); if (blk > 100) after = rms (b); }
            const double drop = 20.0 * std::log10 (after / (before + 1e-15) + 1e-15);
            check (drop < -50.0, "LOW KILL on a 60 Hz tone", f (drop, 1) + " dB");
        }

        //  Slope switch and floor switch mid-stream: finite, no blow-up.
        {
            IsoAudioProcessor p; p.setPlayConfigDetails (2, 2, 48000.0, 256); p.prepareToPlay (48000.0, 256);
            juce::AudioBuffer<float> b (2, 256); juce::uint32 seed = 11; bool ok = true; double mx = 0.0;
            for (int blk = 0; blk < 200; ++blk)
            {
                fillNoise (b, seed);
                if (blk % 20 == 0) setParam (p, iso::id::slope, (float) ((blk / 20) % 2));
                if (blk % 30 == 0) setParam (p, iso::id::floorMode, (float) ((blk / 30) % 2));
                setParam (p, iso::id::filter, std::sin ((float) blk * 0.1f));
                setParam (p, iso::id::resonance, 1.0f);
                p.processBlock (b, midi);
                ok = ok && allFinite (b); mx = juce::jmax (mx, peak (b));
            }
            check (ok && mx < 4.0, "slope / floor / filter automation storm: finite, bounded", "peak " + f (mx, 2));
        }

        //  Determinism: same input + automation twice = identical output.
        {
            auto run = [&] (std::vector<float>& out)
            {
                IsoAudioProcessor p; p.setPlayConfigDetails (2, 2, 48000.0, 256); p.prepareToPlay (48000.0, 256);
                juce::AudioBuffer<float> b (2, 256); juce::uint32 seed = 5;
                for (int blk = 0; blk < 60; ++blk)
                {
                    fillNoise (b, seed);
                    setParam (p, iso::id::midGain, -20.0f + (float) blk * 0.5f);
                    setParam (p, iso::id::lowMid, 100.0f + (float) blk * 10.0f);
                    if (blk == 30) setParam (p, iso::id::midKill, 1.0f);
                    p.processBlock (b, midi);
                    for (int i = 0; i < 256; ++i) out.push_back (b.getSample (0, i));
                }
            };
            std::vector<float> a, bb; run (a); run (bb);
            check (a == bb, "two identical runs are bit-identical", juce::String ((int) a.size()) + " samples");
        }
    }

    // 5 -------------------------------------------------------------------------
    section ("5. State");
    {
        IsoAudioProcessor a; applyBusy (a); a.editorScale.store (1.35f);
        juce::MemoryBlock blob; a.getStateInformation (blob);
        check (blob.getSize() > 100, "state blob is not empty", juce::String ((int) blob.getSize()) + " bytes");

        IsoAudioProcessor b; b.setStateInformation (blob.getData(), (int) blob.getSize());
        bool same = true;
        for (auto* id : kAllIds)
            if (std::abs (getParam (a, id) - getParam (b, id)) > 1e-4f) { same = false; std::printf ("      %s: %f vs %f\n", id, (double) getParam (a, id), (double) getParam (b, id)); }
        check (same, "every parameter survives a save / load round trip");
        check (std::abs (b.editorScale.load() - 1.35f) < 1e-4f, "editor scale survives the round trip", f (b.editorScale.load(), 2));

        //  Garbage in must not crash or change anything.
        IsoAudioProcessor c; applyBusy (c);
        const char junk[] = "this is not a plug-in state";
        c.setStateInformation (junk, (int) sizeof junk);
        c.setStateInformation (nullptr, 0);
        check (std::abs (getParam (c, iso::id::lowMid) - 137.0f) < 1.0f, "junk state is ignored, parameters untouched");

        //  A state from another plug-in's tag is ignored.
        juce::XmlElement other ("SOMETHING_ELSE"); juce::MemoryBlock ob;
        juce::AudioProcessor::copyXmlToBinary (other, ob);
        c.setStateInformation (ob.getData(), (int) ob.getSize());
        check (std::abs (getParam (c, iso::id::lowMid) - 137.0f) < 1.0f, "foreign state tag is ignored");
    }

    // 6 -------------------------------------------------------------------------
    section ("6. Programs (factory presets)");
    {
        IsoAudioProcessor p;
        check (p.getNumPrograms() == iso::presets::count() && p.getNumPrograms() >= 1, "program count", juce::String (p.getNumPrograms()));
        bool named = true;
        for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i).isEmpty()) named = false;
        check (named, "every program has a name");
        setParam (p, iso::id::lowGain, 5.0f); setParam (p, iso::id::midKill, 1.0f); setParam (p, iso::id::filter, -0.3f);
        p.setCurrentProgram (1);
        check (p.getCurrentProgram() == 1, "setCurrentProgram takes");
        check (getParam (p, iso::id::slope) == 0.0f, "preset 1 (Xone:92 style) sets 12 dB/oct");
        check (std::abs (getParam (p, iso::id::lowGain) - 5.0f) < 0.06f && getParam (p, iso::id::midKill) > 0.5f
               && std::abs (getParam (p, iso::id::filter) + 0.3f) < 0.01f, "presets leave gains, kills and filter alone");
        p.setCurrentProgram (99); p.setCurrentProgram (-1);
        check (p.getCurrentProgram() == 1, "out-of-range program is ignored");
        check (p.getProgramName (99).isEmpty(), "out-of-range program name is empty");
    }

    // 7 -------------------------------------------------------------------------
    section ("7. Editor");
    {
        IsoAudioProcessor p; p.setPlayConfigDetails (2, 2, 48000.0, 512); p.prepareToPlay (48000.0, 512);
        check (p.hasEditor(), "hasEditor");
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        check (ed != nullptr, "createEditor");
        check (ed->getWidth() == iso::ui::metric::width && ed->getHeight() == iso::ui::metric::height, "opens at reference size",
               juce::String (ed->getWidth()) + " x " + juce::String (ed->getHeight()));
        check (ed->isResizable(), "resizable");

        //  Audio while the editor is open, with the message loop running.
        juce::MidiBuffer midi; juce::AudioBuffer<float> b (2, 512); juce::uint32 seed = 9; bool ok = true;
        for (int blk = 0; blk < 20; ++blk)
        {
            fillNoise (b, seed);
            setParam (p, iso::id::midGain, -10.0f + (float) blk);
            setParam (p, iso::id::midHigh, 1000.0f + (float) blk * 200.0f);
            p.processBlock (b, midi); ok = ok && allFinite (b); pump (10);
        }
        check (ok, "audio + parameter changes with the editor open and its timers running");

        ed->setSize (600, 363); pump (30);
        check (ed->getWidth() >= (int) (iso::ui::metric::width * IsoAudioProcessorEditor::kMinScale) - 1, "resize is clamped to the minimum",
               juce::String (ed->getWidth()) + " x " + juce::String (ed->getHeight()));
        ed->setSize (1290, 780); pump (30);
        check (std::abs ((double) ed->getWidth() / ed->getHeight() - 860.0 / 520.0) < 0.02, "aspect ratio held through resize",
               juce::String (ed->getWidth()) + " x " + juce::String (ed->getHeight()));
        check (std::abs (p.editorScale.load() - 1290.0f / 860.0f) < 0.02f, "processor remembers the editor scale", f (p.editorScale.load(), 3));

        //  Paint it, at the large size, to catch anything that only breaks under a transform.
        juce::Image img (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
        { juce::Graphics g (img); ed->paintEntireComponent (g, true); }
        check (img.getPixelAt (5, 5).getAlpha() == 255, "paints opaque at 150 %");

        ed.reset();
        pump (20);
        check (true, "editor destroyed with audio still running");

        //  Reopen: comes back at the remembered size.
        std::unique_ptr<juce::AudioProcessorEditor> ed2 (p.createEditor());
        check (std::abs (ed2->getWidth() - 1290) <= 2, "reopens at the remembered size", juce::String (ed2->getWidth()));
        //  Open / close a few times: leak detector will shout if anything dangles.
        for (int i = 0; i < 5; ++i) { std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor()); pump (5); }
        check (true, "five open / close cycles");
    }

    std::printf ("\n%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
