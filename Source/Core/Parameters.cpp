#include "Parameters.h"

namespace iso
{
    juce::NormalisableRange<float> frequencyRange (float minHz, float maxHz, float centreHz)
    {
        juce::NormalisableRange<float> r (minHz, maxHz);
        r.setSkewForCentre (centreHz);
        return r;
    }

    juce::NormalisableRange<float> bandGainRange()
    {
        //  Piecewise-linear in normalised space: [0, 0.7] covers min..0 dB,
        //  [0.7, 1] covers 0..max. Two straight segments read better under a
        //  finger than a skew curve, and 0 dB lands on an exact stop.
        constexpr float split = 0.70f;
        return juce::NormalisableRange<float> (
            law::kGainMinDb, law::kGainMaxDb,
            [] (float, float, float v)
            {
                return v < split
                    ? law::kGainMinDb + (v / split) * (0.0f - law::kGainMinDb)
                    : ((v - split) / (1.0f - split)) * law::kGainMaxDb;
            },
            [] (float, float, float db)
            {
                return db < 0.0f
                    ? split * (db - law::kGainMinDb) / (0.0f - law::kGainMinDb)
                    : split + (1.0f - split) * (db / law::kGainMaxDb);
            },
            [] (float, float, float db) { return juce::jlimit (law::kGainMinDb, law::kGainMaxDb, std::round (db * 10.0f) / 10.0f); });
    }

    juce::String formatHz (float hz)
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz";
        return juce::String ((int) std::round (hz)) + " Hz";
    }

    juce::String formatBandGain (float db, IsoEngine::Floor floorMode)
    {
        if (floorMode == IsoEngine::Floor::kill && db <= law::kGainMinDb + 1.0e-3f)
            return "KILL";
        if (floorMode == IsoEngine::Floor::eq26 && db <= law::kEqFloorDb)
            db = law::kEqFloorDb;
        return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        using P = juce::AudioParameterFloat;
        using B = juce::AudioParameterBool;
        using Ch = juce::AudioParameterChoice;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

        auto ver = [] (const char* s) { return juce::ParameterID (s, id::stateVersion); };

        auto hzAttr = juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return formatHz (v); })
            .withValueFromStringFunction ([] (const juce::String& t)
            {
                //  "2.50 kHz" is 2500, not 2.5. "1k" too.
                const auto u = t.trim().toUpperCase();
                float v = u.getFloatValue();
                if (u.contains ("K")) v *= 1000.0f;
                return v;
            })
            .withLabel ("Hz");

        p.push_back (std::make_unique<P> (ver (id::lowMid), "Low / Mid",
            frequencyRange (law::kLowMidMinHz, law::kLowMidMaxHz, law::kLowMidDefHz), law::kLowMidDefHz, hzAttr));
        p.push_back (std::make_unique<P> (ver (id::midHigh), "Mid / High",
            frequencyRange (law::kMidHighMinHz, law::kMidHighMaxHz, law::kMidHighDefHz), law::kMidHighDefHz, hzAttr));

        auto gainAttr = juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int)
            {
                //  The host's generic view does not know the floor mode; show
                //  the ISO reading, which is also what the knob shows by default.
                return formatBandGain (v, IsoEngine::Floor::kill);
            })
            .withValueFromStringFunction ([] (const juce::String& t)
            {
                //  Typing what the read-out shows must land where it read.
                const auto u = t.trim().toUpperCase();
                if (u.startsWith ("KILL") || u.contains ("INF") || u == "-")
                    return law::kGainMinDb;
                return juce::jlimit (law::kGainMinDb, law::kGainMaxDb, u.getFloatValue());
            })
            .withLabel ("dB");

        p.push_back (std::make_unique<P> (ver (id::lowGain),  "Low",  bandGainRange(), 0.0f, gainAttr));
        p.push_back (std::make_unique<P> (ver (id::midGain),  "Mid",  bandGainRange(), 0.0f, gainAttr));
        p.push_back (std::make_unique<P> (ver (id::highGain), "High", bandGainRange(), 0.0f, gainAttr));

        p.push_back (std::make_unique<B> (ver (id::lowKill),  "Low Kill",  false));
        p.push_back (std::make_unique<B> (ver (id::midKill),  "Mid Kill",  false));
        p.push_back (std::make_unique<B> (ver (id::highKill), "High Kill", false));

        p.push_back (std::make_unique<Ch> (ver (id::slope), "Slope",
            juce::StringArray { "12 dB/oct", "24 dB/oct" }, 1));
        p.push_back (std::make_unique<Ch> (ver (id::floorMode), "Cut Floor",
            juce::StringArray { "ISO (kill)", "EQ (-26 dB)" }, 0));

        p.push_back (std::make_unique<P> (ver (id::filter), "Filter",
            juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            {
                float cut, q; int mode;
                IsoEngine::filterLaw (v, 0.0f, cut, q, mode);
                if (mode == 0) return juce::String ("OFF");
                return juce::String (mode < 0 ? "LP " : "HP ") + formatHz (cut);
            }).withValueFromStringFunction ([] (const juce::String& t)
            {
                const auto u = t.trim().toUpperCase();
                if (u.startsWith ("OFF")) return 0.0f;
                if (u.startsWith ("LP") || u.startsWith ("HP"))
                {
                    //  Invert the sweep law: "LP 1.2 kHz" -> the knob position
                    //  whose cutoff is 1.2 kHz.
                    float hz = u.fromFirstOccurrenceOf (" ", false, false).getFloatValue();
                    if (u.contains ("KHZ")) hz *= 1000.0f;
                    if (hz <= 0.0f) return 0.0f;
                    float tt;
                    if (u.startsWith ("LP"))
                        tt = std::log (hz / law::kFilterLpOpenHz) / std::log (law::kFilterLpMinHz / law::kFilterLpOpenHz);
                    else
                        tt = std::log (hz / law::kFilterHpOpenHz) / std::log (law::kFilterHpMaxHz / law::kFilterHpOpenHz);
                    tt = juce::jlimit (0.0f, 1.0f, tt);
                    const float a = law::kFilterDeadZone + tt * (1.0f - law::kFilterDeadZone);
                    return u.startsWith ("LP") ? -a : a;
                }
                return juce::jlimit (-1.0f, 1.0f, u.getFloatValue());
            })));

        p.push_back (std::make_unique<P> (ver (id::resonance), "Resonance",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return juce::String ((int) std::round (v * 100.0f)) + " %"; })
            .withValueFromStringFunction ([] (const juce::String& t) { return juce::jlimit (0.0f, 1.0f, t.getFloatValue() / 100.0f); })));

        p.push_back (std::make_unique<P> (ver (id::trim), "Output",
            juce::NormalisableRange<float> (law::kTrimMinDb, law::kTrimMaxDb, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; }).withLabel ("dB")));

        p.push_back (std::make_unique<B> (ver (id::bypass), "Bypass", false));

        return { p.begin(), p.end() };
    }

    IsoEngine::Settings readSettings (const juce::AudioProcessorValueTreeState& s) noexcept
    {
        auto get = [&s] (const char* pid) { return s.getRawParameterValue (pid)->load(); };

        IsoEngine::Settings st;
        st.lowMidHz  = get (id::lowMid);
        st.midHighHz = get (id::midHigh);
        st.gainDb    = { get (id::lowGain), get (id::midGain), get (id::highGain) };
        st.kill      = { get (id::lowKill) > 0.5f, get (id::midKill) > 0.5f, get (id::highKill) > 0.5f };
        st.slope     = get (id::slope) > 0.5f ? IsoEngine::Slope::db24 : IsoEngine::Slope::db12;
        st.floorMode = get (id::floorMode) > 0.5f ? IsoEngine::Floor::eq26 : IsoEngine::Floor::kill;
        st.filter    = get (id::filter);
        st.resonance = get (id::resonance);
        st.trimDb    = get (id::trim);
        st.bypass    = get (id::bypass) > 0.5f;
        return st;
    }
}
