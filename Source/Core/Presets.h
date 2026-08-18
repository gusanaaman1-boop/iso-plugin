// Factory presets, exposed as host programs. Each one is a named starting
// point copied from a piece of hardware DJs already know, so "make it sound
// like the DJM" is one click and the manual can say which knob is which.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterIds.h"
#include "../Dsp/IsoEngine.h"

namespace iso::presets
{
    struct Preset
    {
        const char* name;
        float lowMidHz, midHighHz;
        int slope;      // 0 = 12, 1 = 24 dB/oct
        int floorMode;  // 0 = ISO, 1 = EQ
        float resonance;
    };

    //  Sources: Xone:92 corners 250 / 2.5k, 12 dB/oct infinite LF/HF cut;
    //  Pioneer DJM 3-band EQ +6 / -26 (EQ curve) or -inf (ISO curve);
    //  rotary-mixer isolators (Alpha Recording, E&S, Rane MP2015) 24 dB/oct
    //  LR with variable corners, typically ~300 / 3k.
    inline const Preset kPresets[] = {
        { "Init - Isolator 24 dB",     250.0f,  2500.0f, 1, 0, 0.25f },
        { "Xone:92 style 12 dB",       250.0f,  2500.0f, 0, 0, 0.25f },
        { "DJM EQ curve (-26 dB)",     180.0f,  3000.0f, 1, 1, 0.25f },
        { "DJM ISO curve",             180.0f,  3000.0f, 1, 0, 0.25f },
        { "Rotary isolator 300 / 3k",  300.0f,  3000.0f, 1, 0, 0.35f },
        { "Bass focus 100 / 1.5k",     100.0f,  1500.0f, 1, 0, 0.25f },
        { "Wide mids 120 / 6k",        120.0f,  6000.0f, 1, 0, 0.25f },
    };

    inline int count() { return (int) std::size (kPresets); }
    inline juce::String name (int i) { return kPresets[i].name; }

    inline void setNorm (juce::AudioProcessorValueTreeState& s, const char* pid, float plain)
    {
        if (auto* p = s.getParameter (pid))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }

    inline void apply (int i, juce::AudioProcessorValueTreeState& s)
    {
        const auto& p = kPresets[i];
        setNorm (s, id::lowMid,    p.lowMidHz);
        setNorm (s, id::midHigh,   p.midHighHz);
        setNorm (s, id::slope,     (float) p.slope);
        setNorm (s, id::floorMode, (float) p.floorMode);
        setNorm (s, id::resonance, p.resonance);
        //  A preset changes the character, never the performance: gains,
        //  kills and the filter are left where the DJ has them.
    }
}
