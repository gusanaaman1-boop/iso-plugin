// The ONLY place ISO's colours and metrics live.
//
// Colour law: each band owns one hue and it appears nowhere else - low is
// ember, mid is gold, high is ice. The sweep filter is violet. Everything
// neutral is smoked steel on black glass. A DJ reads this panel at 2 a.m.
// under a strobe: the three hues have to be tellable apart from across the
// booth, which is why they sit a third of the wheel from one another.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace iso::ui
{
    namespace colour
    {
        inline const juce::Colour window       { 0xff07080C };
        inline const juce::Colour deckTop      { 0xff13161E };
        inline const juce::Colour deckBottom   { 0xff0B0D13 };
        inline const juce::Colour headerTop    { 0xff181C26 };
        inline const juce::Colour headerBottom { 0xff0E1119 };
        inline const juce::Colour graphTop     { 0xff0A0D14 };
        inline const juce::Colour graphBottom  { 0xff06080D };
        inline const juce::Colour grid         { 0xff161B25 };
        inline const juce::Colour gridStrong   { 0xff222835 };

        inline const juce::Colour steelBright  { 0xff6C7788 };
        inline const juce::Colour steelMid     { 0xff3D4553 };
        inline const juce::Colour steelDark    { 0xff1C212B };
        inline const juce::Colour knobFace     { 0xff11141B };
        inline const juce::Colour knobRim      { 0xff2A303C };

        inline const juce::Colour text         { 0xffF2F4FA };
        inline const juce::Colour textDim      { 0xff9AA3B4 };
        inline const juce::Colour tertiary     { 0xff626B7C };

        inline const juce::Colour low          { 0xffFF5A47 };   // ember
        inline const juce::Colour mid          { 0xffFFC247 };   // gold
        inline const juce::Colour high         { 0xff47D6FF };   // ice
        inline const juce::Colour filter       { 0xffA98CFF };   // violet
        inline const juce::Colour neutral      { 0xffDDE3F0 };   // crossovers, output
        inline const juce::Colour response     { 0xffF6F8FF };
        inline const juce::Colour kill         { 0xffFF3B30 };

        inline juce::Colour band (int i) { return i == 0 ? low : i == 1 ? mid : high; }
    }

    namespace metric
    {
        inline constexpr int width  = 860;
        inline constexpr int height = 520;

        inline constexpr int headerHeight = 52;
        inline constexpr int margin       = 14;
        inline constexpr int graphHeight  = 178;

        inline constexpr int bigKnob   = 128;
        inline constexpr int smallKnob = 58;
        inline constexpr int filterKnob= 104;
        inline constexpr int killH     = 30;
        inline constexpr int killW     = 96;

        inline constexpr float radius = 12.0f;

        inline constexpr float displayMinHz   = 20.0f;
        inline constexpr float displayMaxHz   = 20000.0f;
        inline constexpr float displayTopDb   = 15.0f;
        inline constexpr float displayBottomDb= -42.0f;
    }

    namespace font
    {
        inline constexpr float tiny      = 9.0f;
        inline constexpr float caption   = 10.0f;
        inline constexpr float knobLabel = 11.5f;
        inline constexpr float value     = 13.0f;
        inline constexpr float wordmark  = 24.0f;
    }

    void paintPanel (juce::Graphics&, juce::Rectangle<float>, juce::Colour top, juce::Colour bottom, float corner);
    //  The MARK alone: three bars in the three band hues, the middle one cut
    //  short - a three-band isolator with the mid killed. Fills `bounds`.
    void drawLogoMark (juce::Graphics&, juce::Rectangle<float> bounds);
    void drawWordmark (juce::Graphics&, juce::Rectangle<float> area);
    //  The maker's mark - Naaman's N in a circle, bone strokes and one brass
    //  diagonal - the same geometry FOUR COLOR and the website draw (a 40x40
    //  box: verticals at x 13.2 / 26.8 from y 12.6 to 27.4, circle r 18.25).
    void drawNaamanMark (juce::Graphics&, juce::Rectangle<float> bounds);

    //  One knob renderer. Per-control variation travels in the Slider's
    //  property set:
    //    "accent"   ARGB of the value arc
    //    "zeroPos"  normalised position the arc grows from (0 = min, 0.5 =
    //               centre, 0.7 = the band knobs' 0 dB stop)
    //    "big"      thicker rim and pointer for the three band knobs
    class Look : public juce::LookAndFeel_V4
    {
    public:
        Look();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                               float pos, float startAngle, float endAngle, juce::Slider&) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
        void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
        juce::Label* createSliderTextBox (juce::Slider&) override;
        void drawLabel (juce::Graphics&, juce::Label&) override;
    };
}
