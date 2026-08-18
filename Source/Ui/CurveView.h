// The response display. Draws the analytic response of the current settings -
// the same maths the audio thread runs, so the picture cannot disagree with
// the sound - with each band's own contribution filled in its hue. The two
// crossover handles drag.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../PluginProcessor.h"
#include "Theme.h"

namespace iso::ui
{
    class CurveView : public juce::Component, private juce::Timer
    {
    public:
        explicit CurveView (IsoAudioProcessor&);
        ~CurveView() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void mouseMove (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp   (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        void timerCallback() override;
        void rebuild();

        float xForHz (double hz) const noexcept;
        double hzForX (float x) const noexcept;
        float yForDb (double db) const noexcept;

        int handleAt (juce::Point<float>) const;

        IsoAudioProcessor& processor;
        IsoEngine::Settings shown;
        juce::Rectangle<float> plot;

        //  One path per band (filled) and the composite line.
        std::array<juce::Path, 3> bandFills;
        juce::Path composite;
        std::array<float, 2> xoverX { 0.0f, 0.0f };

        int hover = -1, dragging = -1;
        juce::RangedAudioParameter* dragParam = nullptr;
    };
}
