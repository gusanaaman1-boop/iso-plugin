#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ParameterIds.h"
#include "../Dsp/IsoEngine.h"

namespace iso
{
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //  Log frequency travel anchored so the knob's centre lands on `centreHz`.
    juce::NormalisableRange<float> frequencyRange (float minHz, float maxHz, float centreHz);

    //  Band gain travel: 0 dB sits at about 70 % of the knob so the cut side,
    //  where a DJ lives, has most of the resolution.
    juce::NormalisableRange<float> bandGainRange();

    IsoEngine::Settings readSettings (const juce::AudioProcessorValueTreeState&) noexcept;

    juce::String formatHz (float hz);
    juce::String formatBandGain (float db, IsoEngine::Floor);
}
