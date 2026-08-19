#include "PluginEditor.h"

#include <IsoVersion.h>

#include "Core/Presets.h"

using namespace iso;
using namespace iso::ui;

IsoPanel::IsoPanel (IsoAudioProcessor& p)
    : proc (p), curve (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (curve);

    auto& st = proc.getState();
    const char* bandIds[3] = { id::lowGain, id::midGain, id::highGain };
    const char* killIds[3] = { id::lowKill, id::midKill, id::highKill };

    for (int b = 0; b < 3; ++b)
    {
        auto& s = band[(size_t) b];
        setupKnob (s, colour::band (b), 0.70, true);
        bandAtt[(size_t) b] = std::make_unique<SliderAtt> (st, bandIds[b], s);
        s.setDoubleClickReturnValue (true, 0.0);

        auto& k = kill[(size_t) b];
        k.setButtonText ("KILL");
        k.getProperties().set ("accent", (int) colour::band (b).getARGB());
        k.setClickingTogglesState (true);
        addAndMakeVisible (k);
        killAtt[(size_t) b] = std::make_unique<ButtonAtt> (st, killIds[b], k);
    }

    setupKnob (lowMid,  colour::neutral, 0.0, false);
    setupKnob (midHigh, colour::neutral, 0.0, false);
    setupKnob (filter,  colour::filter,  0.5, false);
    setupKnob (resonance, colour::filter, 0.0, false);
    setupKnob (trim,    colour::neutral, 0.5, false);
    lowMidAtt    = std::make_unique<SliderAtt> (st, id::lowMid, lowMid);
    midHighAtt   = std::make_unique<SliderAtt> (st, id::midHigh, midHigh);
    filterAtt    = std::make_unique<SliderAtt> (st, id::filter, filter);
    resonanceAtt = std::make_unique<SliderAtt> (st, id::resonance, resonance);
    trimAtt      = std::make_unique<SliderAtt> (st, id::trim, trim);
    filter.setDoubleClickReturnValue (true, 0.0);
    trim.setDoubleClickReturnValue (true, 0.0);
    lowMid.setDoubleClickReturnValue (true, law::kLowMidDefHz);
    midHigh.setDoubleClickReturnValue (true, law::kMidHighDefHz);

    //  Slope / floor as two-button segments over a choice parameter.
    setupSegment (slope12, "12"); setupSegment (slope24, "24");
    setupSegment (floorIso, "ISO"); setupSegment (floorEq, "EQ");
    slope12.setRadioGroupId (1); slope24.setRadioGroupId (1);
    floorIso.setRadioGroupId (2); floorEq.setRadioGroupId (2);

    if (auto* sp = st.getParameter (id::slope))
    {
        slopeAtt = std::make_unique<juce::ParameterAttachment> (*sp, [this] (float v)
        {
            slope24.setToggleState (v > 0.5f, juce::dontSendNotification);
            slope12.setToggleState (v <= 0.5f, juce::dontSendNotification);
        });
        slopeAtt->sendInitialUpdate();
        slope12.onClick = [this] { slopeAtt->setValueAsCompleteGesture (0.0f); };
        slope24.onClick = [this] { slopeAtt->setValueAsCompleteGesture (1.0f); };
    }
    if (auto* fp = st.getParameter (id::floorMode))
    {
        floorAtt = std::make_unique<juce::ParameterAttachment> (*fp, [this] (float v)
        {
            floorEq.setToggleState (v > 0.5f, juce::dontSendNotification);
            floorIso.setToggleState (v <= 0.5f, juce::dontSendNotification);
            refreshBandText();
        });
        floorAtt->sendInitialUpdate();
        floorIso.onClick = [this] { floorAtt->setValueAsCompleteGesture (0.0f); };
        floorEq.onClick  = [this] { floorAtt->setValueAsCompleteGesture (1.0f); };
    }

    bypass.setButtonText ("BYPASS");
    bypass.setClickingTogglesState (true);
    addAndMakeVisible (bypass);
    bypassAtt = std::make_unique<ButtonAtt> (st, id::bypass, bypass);

    for (int i = 0; i < presets::count(); ++i)
        presets.addItem (presets::name (i), i + 1);
    presets.setTextWhenNothingSelected ("Presets");
    presets.onChange = [this]
    {
        const int idx = presets.getSelectedId() - 1;
        if (idx >= 0) { proc.setCurrentProgram (idx); lastProgram = idx; }
    };
    addAndMakeVisible (presets);

    refreshBandText();
    setSize (metric::width, metric::height);
    startTimerHz (25);
}

IsoPanel::~IsoPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void IsoPanel::setupKnob (juce::Slider& s, juce::Colour accent, double zeroPos, bool big)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, big ? 92 : 70, 18);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.getProperties().set ("accent", (int) accent.getARGB());
    s.getProperties().set ("zeroPos", zeroPos);
    s.getProperties().set ("big", big);
    addAndMakeVisible (s);
}

void IsoPanel::setupSegment (juce::TextButton& b, const char* text)
{
    b.setButtonText (text);
    b.setClickingTogglesState (false);
    addAndMakeVisible (b);
}

void IsoPanel::refreshBandText()
{
    const auto floorMode = proc.getState().getRawParameterValue (id::floorMode)->load() > 0.5f
                         ? IsoEngine::Floor::eq26 : IsoEngine::Floor::kill;
    for (auto& s : band)
    {
        s.textFromValueFunction = [floorMode] (double v) { return formatBandGain ((float) v, floorMode); };
        s.updateText();
    }
}

void IsoPanel::timerCallback()
{
    const float l = proc.outPeakL.load (std::memory_order_relaxed);
    const float r = proc.outPeakR.load (std::memory_order_relaxed);
    //  Fast attack, slow release, like a real bar meter.
    meterL = l > meterL ? l : meterL * 0.88f;
    meterR = r > meterR ? r : meterR * 0.88f;
    repaint (meterRect.expanded (2));

    if (proc.getCurrentProgram() != lastProgram)
    {
        lastProgram = proc.getCurrentProgram();
        presets.setSelectedId (lastProgram + 1, juce::dontSendNotification);
    }
}

void IsoPanel::resized()
{
    captions.clear();
    auto area = getLocalBounds();
    const int m = metric::margin;

    header = area.removeFromTop (metric::headerHeight);
    {
        auto h = header.reduced (m, 0);
        auto right = h.removeFromRight (500);
        //  BYPASS | CURVE ISO/EQ | SLOPE 12/24, right to left
        auto by = right.removeFromRight (74).withSizeKeepingCentre (70, 24).translated (0, 6);
        bypass.setBounds (by);
        right.removeFromRight (14);
        auto fl = right.removeFromRight (84);
        captions.push_back ({ fl.withHeight (14).translated (0, 4), "CUT", colour::tertiary });
        auto flb = fl.withSizeKeepingCentre (84, 24).translated (0, 6);
        floorIso.setBounds (flb.removeFromLeft (42)); floorEq.setBounds (flb);
        right.removeFromRight (14);
        auto sl = right.removeFromRight (84);
        captions.push_back ({ sl.withHeight (14).translated (0, 4), "SLOPE", colour::tertiary });
        auto slb = sl.withSizeKeepingCentre (84, 24).translated (0, 6);
        slope12.setBounds (slb.removeFromLeft (42)); slope24.setBounds (slb);
        right.removeFromRight (14);
        presets.setBounds (right.removeFromRight (200).withSizeKeepingCentre (200, 24).translated (0, 6));
    }

    area.removeFromTop (8);
    curve.setBounds (area.removeFromTop (metric::graphHeight).reduced (m, 0));
    area.removeFromTop (10);
    deck = area.reduced (m, 0).withTrimmedBottom (m);

    //  --- band deck ---------------------------------------------------------
    const int big = 120, small = metric::smallKnob;
    const int bandCx[3] = { deck.getX() + 96, deck.getX() + 286, deck.getX() + 476 };
    const int knobTop = deck.getY() + 26;
    const char* names[3] = { "LOW", "MID", "HIGH" };
    for (int b = 0; b < 3; ++b)
    {
        auto kb = juce::Rectangle<int> (big, big + 22).withCentre ({ bandCx[b], knobTop + (big + 22) / 2 });
        band[(size_t) b].setBounds (kb);
        captions.push_back ({ juce::Rectangle<int> (bandCx[b] - 50, deck.getY() + 8, 100, 14), names[b], colour::band (b) });
        kill[(size_t) b].setBounds (juce::Rectangle<int> (metric::killW, metric::killH).withCentre ({ bandCx[b], kb.getBottom() + 26 }));
    }
    const int xoCx[2] = { (bandCx[0] + bandCx[1]) / 2, (bandCx[1] + bandCx[2]) / 2 };
    const int xoTop = knobTop + 30;
    lowMid.setBounds (juce::Rectangle<int> (small + 12, small + 22).withCentre ({ xoCx[0], xoTop + (small + 22) / 2 }));
    midHigh.setBounds (juce::Rectangle<int> (small + 12, small + 22).withCentre ({ xoCx[1], xoTop + (small + 22) / 2 }));
    captions.push_back ({ juce::Rectangle<int> (xoCx[0] - 34, xoTop - 14, 68, 12), "LOW / MID", colour::textDim });
    captions.push_back ({ juce::Rectangle<int> (xoCx[1] - 34, xoTop - 14, 68, 12), "MID / HIGH", colour::textDim });

    //  --- filter / output --------------------------------------------------------
    divider = juce::Rectangle<int> (deck.getX() + 574, deck.getY() + 14, 1, deck.getHeight() - 28);
    const int fCx = deck.getX() + 660, rCx = deck.getX() + 772;
    auto fb = juce::Rectangle<int> (metric::filterKnob, metric::filterKnob + 22).withCentre ({ fCx, knobTop + 8 + (metric::filterKnob + 22) / 2 });
    filter.setBounds (fb);
    captions.push_back ({ juce::Rectangle<int> (fCx - 50, deck.getY() + 8, 100, 14), "FILTER", colour::filter });
    captions.push_back ({ juce::Rectangle<int> (fb.getX() - 6, fb.getBottom() - 30, 30, 12), "LP", colour::tertiary });
    captions.push_back ({ juce::Rectangle<int> (fb.getRight() - 24, fb.getBottom() - 30, 30, 12), "HP", colour::tertiary });

    resonance.setBounds (juce::Rectangle<int> (small + 12, small + 22).withCentre ({ rCx, knobTop + 12 + (small + 22) / 2 }));
    captions.push_back ({ juce::Rectangle<int> (rCx - 34, deck.getY() + 8, 68, 14), "RES", colour::filter });
    trim.setBounds (juce::Rectangle<int> (small + 12, small + 22).withCentre ({ rCx, knobTop + 122 + (small + 22) / 2 }));
    captions.push_back ({ juce::Rectangle<int> (rCx - 34, knobTop + 108, 68, 12), "OUTPUT", colour::textDim });

    meterRect = juce::Rectangle<int> (deck.getRight() - 30, deck.getY() + 26, 12, deck.getHeight() - 52);
}

void IsoPanel::paint (juce::Graphics& g)
{
    g.fillAll (colour::window);

    paintPanel (g, header.toFloat().reduced (metric::margin * 0.5f, 6.0f), colour::headerTop, colour::headerBottom, metric::radius);
    drawWordmark (g, juce::Rectangle<float> ((float) metric::margin + 8.0f, 13.0f, 260.0f, 26.0f));

    //  The maker's mark, after the product: a hairline, the N, "NAAMAN".
    {
        const float x0 = (float) metric::margin + 8.0f + 26.0f + 12.0f + 62.0f + 96.0f;
        g.setColour (colour::steelDark);
        g.fillRect (juce::Rectangle<float> (x0, 14.0f, 1.0f, 24.0f));
        drawNaamanMark (g, juce::Rectangle<float> (x0 + 12.0f, 11.0f, 30.0f, 30.0f));
        g.setColour (colour::textDim);
        g.setFont (juce::Font (juce::FontOptions (font::caption, juce::Font::bold)).withExtraKerningFactor (0.22f));
        g.drawText ("NAAMAN", juce::Rectangle<float> (x0 + 46.0f, 13.0f, 70.0f, 26.0f), juce::Justification::centredLeft);
    }

    paintPanel (g, deck.toFloat(), colour::deckTop, colour::deckBottom, metric::radius);
    g.setColour (colour::steelDark);
    g.fillRect (divider);

    for (const auto& c : captions)
    {
        g.setColour (c.c);
        g.setFont (juce::Font (juce::FontOptions (font::knobLabel, juce::Font::bold)));
        g.drawText (c.text, c.r, juce::Justification::centred);
    }

    //  Output meter: two thin bars, peak-hold-less, in the neutral hue.
    auto mr = meterRect.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (mr.expanded (2.0f), 4.0f);
    auto lBar = mr.removeFromLeft (5.0f); mr.removeFromLeft (2.0f); auto rBar = mr;
    auto drawBar = [&g] (juce::Rectangle<float> bar, float peak)
    {
        const float db = juce::Decibels::gainToDecibels (peak, -60.0f);
        const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);   // -60 .. +6
        g.setColour (colour::steelDark);
        g.fillRoundedRectangle (bar, 2.0f);
        auto lit = bar.withTrimmedTop (bar.getHeight() * (1.0f - t));
        g.setGradientFill (juce::ColourGradient (colour::low, bar.getX(), bar.getY(), colour::neutral, bar.getX(), bar.getY() + bar.getHeight() * 0.25f, false));
        g.fillRoundedRectangle (lit, 2.0f);
    };
    drawBar (lBar, meterL);
    drawBar (rBar, meterR);
    g.setColour (colour::tertiary);
    g.setFont (juce::Font (juce::FontOptions (font::tiny)));
    g.drawText ("OUT", meterRect.withY (meterRect.getBottom() + 4).withHeight (12).expanded (10, 0), juce::Justification::centred);

    g.setColour (colour::tertiary);
    g.drawText (juce::String ("v") + iso::kVersion, getLocalBounds().removeFromBottom (metric::margin).reduced (metric::margin + 4, 0),
                juce::Justification::centredLeft);
}

// -----------------------------------------------------------------------------
IsoAudioProcessorEditor::IsoAudioProcessorEditor (IsoAudioProcessor& p)
    : AudioProcessorEditor (p), proc (p), panel (p)
{
    addAndMakeVisible (panel);

    constrainer.setFixedAspectRatio ((double) metric::width / (double) metric::height);
    constrainer.setSizeLimits ((int) (metric::width * kMinScale), (int) (metric::height * kMinScale),
                               (int) (metric::width * kMaxScale), (int) (metric::height * kMaxScale));
    setConstrainer (&constrainer);
    setResizable (true, true);

    const float scale = juce::jlimit (kMinScale, kMaxScale, proc.editorScale.load());
    setSize (juce::roundToInt (metric::width * scale), juce::roundToInt (metric::height * scale));
}

void IsoAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (colour::window); }

void IsoAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) metric::width;
    panel.setTransform (juce::AffineTransform::scale (scale));
    panel.setBounds (0, 0, metric::width, metric::height);
    proc.editorScale.store (scale);
}
