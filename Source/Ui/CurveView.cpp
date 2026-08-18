#include "CurveView.h"

namespace iso::ui
{
    namespace
    {
        constexpr int kPoints = 240;

        bool same (const IsoEngine::Settings& a, const IsoEngine::Settings& b) noexcept
        {
            using juce::exactlyEqual;
            return exactlyEqual (a.lowMidHz, b.lowMidHz) && exactlyEqual (a.midHighHz, b.midHighHz)
                && a.gainDb == b.gainDb && a.kill == b.kill && a.slope == b.slope && a.floorMode == b.floorMode
                && exactlyEqual (a.filter, b.filter) && exactlyEqual (a.resonance, b.resonance)
                && exactlyEqual (a.trimDb, b.trimDb) && a.bypass == b.bypass;
        }
    }

    CurveView::CurveView (IsoAudioProcessor& p) : processor (p)
    {
        setOpaque (false);
        shown = readSettings (processor.getState());
        startTimerHz (30);
    }

    CurveView::~CurveView() { stopTimer(); }

    void CurveView::timerCallback()
    {
        const auto now = readSettings (processor.getState());
        if (! same (now, shown))
        {
            shown = now;
            rebuild();
            repaint();
        }
    }

    void CurveView::resized()
    {
        plot = getLocalBounds().toFloat().reduced (10.0f, 8.0f).withTrimmedBottom (14.0f).withTrimmedLeft (26.0f);
        rebuild();
    }

    float CurveView::xForHz (double hz) const noexcept
    {
        const double t = std::log (hz / metric::displayMinHz) / std::log (metric::displayMaxHz / metric::displayMinHz);
        return plot.getX() + (float) t * plot.getWidth();
    }

    double CurveView::hzForX (float x) const noexcept
    {
        const double t = juce::jlimit (0.0, 1.0, (double) (x - plot.getX()) / plot.getWidth());
        return metric::displayMinHz * std::pow (metric::displayMaxHz / metric::displayMinHz, t);
    }

    float CurveView::yForDb (double db) const noexcept
    {
        const double t = (metric::displayTopDb - db) / (metric::displayTopDb - metric::displayBottomDb);
        return plot.getY() + (float) juce::jlimit (-0.2, 1.2, t) * plot.getHeight();
    }

    void CurveView::rebuild()
    {
        if (plot.isEmpty())
            return;

        const auto& engine = processor.getEngine();
        const float floorY = plot.getBottom();

        for (int b = 0; b < 3; ++b)
        {
            IsoEngine::Settings solo = shown;
            for (int o = 0; o < 3; ++o)
                if (o != b) solo.kill[(size_t) o] = true;
            //  A band's own contribution, drawn before the sweep filter and
            //  trim so the fill shows what the band knob is doing.
            solo.filter = 0.0f; solo.trimDb = 0.0f; solo.bypass = false;

            auto& path = bandFills[(size_t) b];
            path.clear();
            path.startNewSubPath (plot.getX(), floorY);
            for (int i = 0; i <= kPoints; ++i)
            {
                const float x = plot.getX() + plot.getWidth() * (float) i / (float) kPoints;
                const double db = engine.magnitudeDbAt (hzForX (x), solo);
                path.lineTo (x, yForDb (db));
            }
            path.lineTo (plot.getRight(), floorY);
            path.closeSubPath();
        }

        composite.clear();
        for (int i = 0; i <= kPoints; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) kPoints;
            const double db = engine.magnitudeDbAt (hzForX (x), shown);
            const float y = yForDb (db);
            if (i == 0) composite.startNewSubPath (x, y); else composite.lineTo (x, y);
        }

        float f1, f2;
        IsoEngine::effectiveCrossovers (shown.lowMidHz, shown.midHighHz, f1, f2);
        xoverX = { xForHz (f1), xForHz (f2) };
    }

    void CurveView::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        paintPanel (g, r, colour::graphTop, colour::graphBottom, metric::radius);

        //  Grid
        g.setFont (juce::Font (juce::FontOptions (font::tiny)));
        for (int hzI : { 50, 100, 200, 500, 1000, 2000, 5000, 10000 })
        {
            const double hz = hzI;
            const float x = xForHz (hz);
            const bool strong = hzI == 100 || hzI == 1000 || hzI == 10000;
            g.setColour (strong ? colour::gridStrong : colour::grid);
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            if (strong || hzI == 50 || hzI == 200 || hzI == 500 || hzI == 2000 || hzI == 5000)
            {
                g.setColour (colour::tertiary);
                g.drawText (formatHz ((float) hz).replace (" Hz", "").replace (".00 kHz", "k").replace (".0 kHz", "k"),
                            juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 1.0f, 40.0f, 12.0f), juce::Justification::centred);
            }
        }
        for (int dbI : { 12, 6, 0, -6, -12, -18, -24, -30, -36 })
        {
            const double db = dbI;
            const float y = yForDb (db);
            g.setColour (dbI == 0 ? colour::gridStrong : colour::grid);
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            if (dbI == 12 || dbI == 0 || dbI == -12 || dbI == -24 || dbI == -36)
            {
                g.setColour (colour::tertiary);
                g.drawText (juce::String ((int) db), juce::Rectangle<float> (r.getX() + 2.0f, y - 6.0f, 22.0f, 12.0f), juce::Justification::centredRight);
            }
        }

        {
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (plot.toNearestInt());

            for (int b = 0; b < 3; ++b)
            {
                const auto c = colour::band (b);
                const bool dead = shown.kill[(size_t) b]
                               || IsoEngine::bandLinearGain (shown.gainDb[(size_t) b], false, shown.floorMode) <= 0.0f;
                g.setColour (c.withAlpha (dead ? 0.06f : 0.22f));
                g.fillPath (bandFills[(size_t) b]);
                g.setColour (c.withAlpha (dead ? 0.25f : 0.9f));
                g.strokePath (bandFills[(size_t) b], juce::PathStrokeType (1.2f));
            }

            //  Composite
            g.setColour (colour::response.withAlpha (0.25f));
            g.strokePath (composite, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (shown.bypass ? colour::textDim : colour::response);
            g.strokePath (composite, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        //  Crossover handles
        for (int i = 0; i < 2; ++i)
        {
            const float x = xoverX[(size_t) i];
            const bool hot = hover == i || dragging == i;
            g.setColour (colour::neutral.withAlpha (hot ? 0.9f : 0.45f));
            const float dash[] = { 3.0f, 3.0f };
            g.drawDashedLine (juce::Line<float> (x, plot.getY(), x, plot.getBottom()), dash, 2, 1.0f);
            auto handle = juce::Rectangle<float> (14.0f, 10.0f).withCentre ({ x, plot.getY() + 6.0f });
            g.setColour (hot ? colour::neutral : colour::steelBright);
            g.fillRoundedRectangle (handle, 3.0f);
            g.setColour (colour::window);
            g.drawVerticalLine ((int) x, handle.getY() + 2.0f, handle.getBottom() - 2.0f);
        }

        if (shown.bypass)
        {
            g.setColour (colour::textDim);
            g.setFont (juce::Font (juce::FontOptions (font::knobLabel, juce::Font::bold)));
            g.drawText ("BYPASSED", plot.removeFromTop (24.0f).reduced (24.0f, 0.0f), juce::Justification::centredRight);
        }
    }

    int CurveView::handleAt (juce::Point<float> p) const
    {
        if (! plot.contains (p)) return -1;
        int best = -1; float bestD = 9.0f;
        for (int i = 0; i < 2; ++i)
        {
            const float d = std::abs (p.x - xoverX[(size_t) i]);
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    void CurveView::mouseMove (const juce::MouseEvent& e)
    {
        const int h = handleAt (e.position);
        if (h != hover) { hover = h; repaint(); }
        setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }

    void CurveView::mouseExit (const juce::MouseEvent&) { hover = -1; repaint(); }

    void CurveView::mouseDown (const juce::MouseEvent& e)
    {
        dragging = handleAt (e.position);
        if (dragging < 0) return;
        dragParam = processor.getState().getParameter (dragging == 0 ? id::lowMid : id::midHigh);
        if (dragParam) dragParam->beginChangeGesture();
    }

    void CurveView::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragging < 0 || dragParam == nullptr) return;
        const float hz = (float) hzForX (e.position.x);
        dragParam->setValueNotifyingHost (dragParam->convertTo0to1 (hz));
    }

    void CurveView::mouseUp (const juce::MouseEvent&)
    {
        if (dragParam) dragParam->endChangeGesture();
        dragParam = nullptr;
        dragging = -1;
        repaint();
    }
}
