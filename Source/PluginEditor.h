#pragma once

#include <array>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "Ui/CurveView.h"
#include "Ui/Theme.h"

//  The whole interface at its reference size (860 x 520). The editor below
//  owns one of these and scales it uniformly, so every layout number in
//  resized() stays a real pixel at 100 % and the panel never reflows.
class IsoPanel : public juce::Component, private juce::Timer
{
public:
    explicit IsoPanel (IsoAudioProcessor&);
    ~IsoPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void setupKnob (juce::Slider&, juce::Colour accent, double zeroPos, bool big);
    void setupSegment (juce::TextButton&, const char* text);
    void refreshBandText();

    IsoAudioProcessor& proc;
    iso::ui::Look look;

    iso::ui::CurveView curve;

    std::array<juce::Slider, 3> band;
    std::array<juce::ToggleButton, 3> kill;
    juce::Slider lowMid, midHigh, filter, resonance, trim;

    juce::TextButton slope12, slope24, floorIso, floorEq, bypass;
    juce::ComboBox presets;

    std::array<std::unique_ptr<SliderAtt>, 3> bandAtt;
    std::array<std::unique_ptr<ButtonAtt>, 3> killAtt;
    std::unique_ptr<SliderAtt> lowMidAtt, midHighAtt, filterAtt, resonanceAtt, trimAtt;
    std::unique_ptr<ButtonAtt> bypassAtt;
    std::unique_ptr<juce::ParameterAttachment> slopeAtt, floorAtt;

    //  Caption rectangles, computed in resized() and drawn in paint().
    struct Caption { juce::Rectangle<int> r; juce::String text; juce::Colour c; };
    std::vector<Caption> captions;
    juce::Rectangle<int> deck, header, meterRect, divider;

    float meterL = 0.0f, meterR = 0.0f;
    int lastProgram = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IsoPanel)
};

//  Resizable, fixed aspect ratio, 60 % .. 200 % of the reference size. The
//  chosen size is kept in the processor so a reopened window comes back the
//  way it was left.
class IsoAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit IsoAudioProcessorEditor (IsoAudioProcessor&);
    ~IsoAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr float kMinScale = 0.6f, kMaxScale = 2.0f;

private:
    IsoAudioProcessor& proc;
    IsoPanel panel;
    juce::ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IsoAudioProcessorEditor)
};
