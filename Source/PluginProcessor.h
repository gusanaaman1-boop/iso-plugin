#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Core/Parameters.h"
#include "Dsp/IsoEngine.h"

class IsoAudioProcessor : public juce::AudioProcessor
{
public:
    IsoAudioProcessor();
    ~IsoAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

   #if defined (JucePlugin_Name)
    const juce::String getName() const override { return JucePlugin_Name; }
   #else
    const juce::String getName() const override { return "ISO"; }
   #endif
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    iso::IsoEngine& getEngine() noexcept { return engine; }

    //  Editor size, kept with the state so a reopened window comes back as left.
    std::atomic<float> editorScale { 1.0f };

    //  Peak of the last processed block per channel, for the UI meter.
    std::atomic<float> outPeakL { 0.0f }, outPeakR { 0.0f };

private:
    juce::AudioProcessorValueTreeState apvts;
    iso::IsoEngine engine;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IsoAudioProcessor)
};
