#include "Theme.h"

namespace iso::ui
{
    void paintPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour top, juce::Colour bottom, float corner)
    {
        g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);
        g.setColour (colour::steelDark);
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
        //  A one-pixel highlight along the top edge: the panel is a slab, not a print.
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawLine (r.getX() + corner, r.getY() + 1.0f, r.getRight() - corner, r.getY() + 1.0f, 1.0f);
    }

    void drawLogoMark (juce::Graphics& g, juce::Rectangle<float> b)
    {
        //  Three bars on a 5-unit grid: bar, gap, bar, gap, bar. Heights 1,
        //  0.55, 1 - the mid band killed. Rounded ends, a soft glow of each
        //  bar's own hue underneath so it reads lit rather than printed.
        const float h = b.getHeight();
        const float unit = b.getWidth() / 5.0f;
        const float barW = unit * 1.15f, step = unit * 1.925f;
        const float heights[3] = { 1.0f, 0.55f, 1.0f };
        float x = b.getX();
        for (int i = 0; i < 3; ++i)
        {
            const float bh = h * heights[i];
            juce::Rectangle<float> bar (x, b.getBottom() - bh, barW, bh);
            const auto c = colour::band (i);
            g.setColour (c.withAlpha (0.28f));
            g.fillRoundedRectangle (bar.expanded (barW * 0.28f), barW * 0.6f);
            g.setColour (c);
            g.fillRoundedRectangle (bar, barW * 0.38f);
            x += step;
        }
    }

    void drawWordmark (juce::Graphics& g, juce::Rectangle<float> a)
    {
        const float h = a.getHeight();
        drawLogoMark (g, juce::Rectangle<float> (a.getX(), a.getY(), h * 0.95f, h));
        const float x = a.getX() + h * 0.95f + h * 0.45f;

        g.setColour (colour::text);
        g.setFont (juce::Font (juce::FontOptions (font::wordmark, juce::Font::bold)));
        g.drawText ("ISO", juce::Rectangle<float> (x, a.getY() - 2.0f, 70.0f, h), juce::Justification::centredLeft);
        g.setColour (colour::tertiary);
        g.setFont (juce::Font (juce::FontOptions (font::caption)));
        g.drawText ("DJ ISOLATOR EQ", juce::Rectangle<float> (x + 62.0f, a.getY() + 3.0f, 120.0f, h), juce::Justification::centredLeft);
    }

    void drawNaamanMark (juce::Graphics& g, juce::Rectangle<float> b)
    {
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        auto box = b.withSizeKeepingCentre (d, d);
        const float s = d / 40.0f;
        auto at = [&box, s] (float x, float y) { return juce::Point<float> (box.getX() + x * s, box.getY() + y * s); };
        const juce::Colour bone (0xffeae7e0), brass (0xffc9a86a);

        g.setColour (bone.withAlpha (0.28f));
        g.drawEllipse (box.reduced (1.75f * s), 1.0f * s);
        g.setColour (bone);
        g.drawLine ({ at (13.2f, 27.4f), at (13.2f, 12.6f) }, 1.5f * s);
        g.drawLine ({ at (26.8f, 27.4f), at (26.8f, 12.6f) }, 1.5f * s);
        g.setColour (brass);
        g.drawLine ({ at (13.2f, 12.6f), at (26.8f, 27.4f) }, 1.5f * s);
    }

    // -------------------------------------------------------------------------
    Look::Look()
    {
        setColour (juce::Slider::textBoxTextColourId, colour::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, colour::text);
        setColour (juce::PopupMenu::backgroundColourId, colour::headerBottom);
        setColour (juce::PopupMenu::textColourId, colour::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colour::steelMid);
        setColour (juce::PopupMenu::highlightedTextColourId, colour::text);
        setColour (juce::ComboBox::textColourId, colour::text);
        setColour (juce::TooltipWindow::backgroundColourId, colour::headerTop);
    }

    void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                 float pos, float startAngle, float endAngle, juce::Slider& s)
    {
        const auto accent = juce::Colour ((juce::uint32) (int) s.getProperties().getWithDefault ("accent", (int) colour::neutral.getARGB()));
        const float zeroPos = (float) (double) s.getProperties().getWithDefault ("zeroPos", 0.0);
        const bool big = (bool) s.getProperties().getWithDefault ("big", false);

        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const float d = juce::jmin (area.getWidth(), area.getHeight());
        auto bounds = area.withSizeKeepingCentre (d, d);
        const auto c = bounds.getCentre();
        const float r = d * 0.5f;

        const float arcW = big ? 5.0f : 3.5f;
        const float arcR = r - arcW * 0.5f - 1.0f;

        //  Track
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (colour::steelDark);
        g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        //  Value arc from the zero position
        const float a0 = startAngle + zeroPos * (endAngle - startAngle);
        const float a1 = startAngle + pos * (endAngle - startAngle);
        if (std::abs (a1 - a0) > 0.005f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1), true);
            g.setColour (accent.withAlpha (s.isEnabled() ? 1.0f : 0.35f));
            g.strokePath (arc, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            //  Glow
            g.setColour (accent.withAlpha (0.18f));
            g.strokePath (arc, juce::PathStrokeType (arcW * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        //  Zero tick
        {
            const float tr0 = arcR + arcW * 0.5f + 1.5f, tr1 = tr0 + 4.0f;
            juce::Line<float> tick (c.getPointOnCircumference (tr0, a0), c.getPointOnCircumference (tr1, a0));
            g.setColour (colour::textDim);
            g.drawLine (tick, 1.5f);
        }

        //  Body
        const float bodyR = arcR - arcW * 0.5f - (big ? 7.0f : 5.0f);
        auto body = juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (body.translated (0.0f, 2.0f).expanded (1.5f));
        g.setGradientFill (juce::ColourGradient (colour::knobRim, c.x, body.getY(), colour::knobFace, c.x, body.getBottom(), false));
        g.fillEllipse (body);
        g.setColour (colour::steelMid.withAlpha (0.8f));
        g.drawEllipse (body.reduced (0.5f), 1.0f);
        //  Machined ring
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawEllipse (body.reduced (bodyR * 0.22f), 1.0f);

        //  Pointer
        {
            const float p0 = bodyR * 0.45f, p1 = bodyR * 0.88f;
            juce::Line<float> ptr (c.getPointOnCircumference (p0, a1), c.getPointOnCircumference (p1, a1));
            g.setColour (accent);
            g.drawLine (ptr, big ? 4.0f : 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawLine (ptr, big ? 1.5f : 1.0f);
        }
    }

    void Look::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool down)
    {
        //  KILL pads and the small segment toggles both come through here; the
        //  "pad" property picks the lamp-style pad with a band accent.
        const auto accent = juce::Colour ((juce::uint32) (int) b.getProperties().getWithDefault ("accent", (int) colour::neutral.getARGB()));
        const bool on = b.getToggleState();
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 6.0f);

        if (on)
        {
            g.setGradientFill (juce::ColourGradient (accent.brighter (0.15f), r.getX(), r.getY(), accent.darker (0.35f), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (accent.withAlpha (0.35f));
            g.drawRoundedRectangle (r.expanded (2.0f), 8.0f, 2.0f);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (highlighted ? colour::steelMid : colour::steelDark, r.getX(), r.getY(),
                                                     colour::knobFace, r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (colour::steelMid);
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        }
        if (down)
        {
            g.setColour (juce::Colours::black.withAlpha (0.2f));
            g.fillRoundedRectangle (r, 6.0f);
        }

        g.setColour (on ? juce::Colours::black.withAlpha (0.85f) : colour::textDim);
        g.setFont (juce::Font (juce::FontOptions (font::knobLabel, juce::Font::bold)));
        g.drawText (b.getButtonText(), r, juce::Justification::centred);
    }

    void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? colour::steelBright : (highlighted ? colour::steelMid : colour::steelDark));
        g.fillRoundedRectangle (r, 5.0f);
        if (down) { g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (r, 5.0f); }
    }

    void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
    {
        g.setColour (b.getToggleState() ? colour::window : colour::textDim);
        g.setFont (juce::Font (juce::FontOptions (font::caption, juce::Font::bold)));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }

    void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
        g.setColour (colour::knobFace);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (colour::steelMid);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        juce::Path arrow;
        const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (colour::textDim);
        g.fillPath (arrow);
    }

    void Look::positionComboBoxText (juce::ComboBox& c, juce::Label& l)
    {
        l.setBounds (8, 0, c.getWidth() - 26, c.getHeight());
        l.setFont (getComboBoxFont (c));
    }

    juce::Font Look::getComboBoxFont (juce::ComboBox&) { return juce::Font (juce::FontOptions (font::caption + 1.0f)); }

    void Look::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
    {
        g.fillAll (colour::headerBottom);
        g.setColour (colour::steelMid);
        g.drawRect (0, 0, w, h);
    }

    juce::Label* Look::createSliderTextBox (juce::Slider& s)
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (juce::Font (juce::FontOptions (font::value, juce::Font::bold)));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, colour::text);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::textWhenEditingColourId, colour::text);
        l->setColour (juce::TextEditor::highlightColourId, colour::steelMid);
        return l;
    }

    void Look::drawLabel (juce::Graphics& g, juce::Label& l)
    {
        if (l.isBeingEdited())
            return;
        //  Slider read-outs sit in a dark recessed pill; plain labels draw bare.
        if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr)
        {
            auto r = l.getLocalBounds().toFloat().reduced (2.0f, 1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        }
        g.setColour (l.findColour (juce::Label::textColourId));
        g.setFont (l.getFont());
        g.drawText (l.getText(), l.getLocalBounds(), l.getJustificationType(), false);
    }
}
