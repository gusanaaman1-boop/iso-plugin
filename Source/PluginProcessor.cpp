#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/Presets.h"

IsoAudioProcessor::IsoAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ISO", iso::createParameterLayout())
{
}

void IsoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setSettings (iso::readSettings (apvts));
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

bool IsoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void IsoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    engine.setSettings (iso::readSettings (apvts));
    engine.process (buffer);

    const int n = buffer.getNumSamples();
    outPeakL.store (buffer.getNumChannels() > 0 ? buffer.getMagnitude (0, 0, n) : 0.0f, std::memory_order_relaxed);
    outPeakR.store (buffer.getNumChannels() > 1 ? buffer.getMagnitude (1, 0, n) : outPeakL.load (std::memory_order_relaxed), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* IsoAudioProcessor::createEditor()
{
    return new IsoAudioProcessorEditor (*this);
}

// --- programs -----------------------------------------------------------------
int IsoAudioProcessor::getNumPrograms() { return iso::presets::count(); }

void IsoAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= iso::presets::count())
        return;
    currentProgram = index;
    iso::presets::apply (index, apvts);
}

const juce::String IsoAudioProcessor::getProgramName (int index)
{
    return index >= 0 && index < iso::presets::count() ? iso::presets::name (index) : juce::String();
}

// --- state --------------------------------------------------------------------
void IsoAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", iso::id::stateVersion, nullptr);
    state.setProperty ("editorScale", (double) editorScale.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void IsoAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            editorScale.store ((float) (double) tree.getProperty ("editorScale", 1.0));
            apvts.replaceState (tree);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new IsoAudioProcessor();
}
