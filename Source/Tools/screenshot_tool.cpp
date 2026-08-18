// Deterministic UI renderer.
//
//     build/IsoShot_artefacts/<config>/IsoShot ui-shots
//
// Builds the real editor over the real processor, sets parameters through the
// APVTS exactly as a host would, and renders the component to a PNG. No screen
// capture, no window server, no timing luck.

#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

#include <juce_gui_extra/juce_gui_extra.h>

#include "../Core/ParameterIds.h"
#include "../Core/Presets.h"
#include "../PluginEditor.h"
#include "../PluginProcessor.h"

namespace
{
    struct Shot
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;
    };

    void setParam (IsoAudioProcessor& p, const char* id, float value)
    {
        if (auto* param = p.getState().getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    int render (const juce::File& outputDir)
    {
        const std::vector<Shot> shots =
        {
            { "default", {} },
            { "mid-kill-low-boost", { { iso::id::midKill, 1.0f }, { iso::id::lowGain, 4.0f }, { iso::id::highGain, -6.0f } } },
            { "lp-sweep", { { iso::id::filter, -0.55f }, { iso::id::resonance, 0.7f }, { iso::id::lowGain, 6.0f } } },
            { "hp-sweep-12db-eq", { { iso::id::filter, 0.5f }, { iso::id::slope, 0.0f }, { iso::id::floorMode, 1.0f },
                                    { iso::id::lowGain, -30.0f }, { iso::id::midGain, 3.0f } } },
            { "wide-xover", { { iso::id::lowMid, 100.0f }, { iso::id::midHigh, 8000.0f }, { iso::id::highKill, 1.0f } } },
            { "bypass", { { iso::id::bypass, 1.0f }, { iso::id::lowGain, -12.0f } } },
        };

        outputDir.createDirectory();

        for (const auto& shot : shots)
        {
            IsoAudioProcessor processor;
            processor.prepareToPlay (48000.0, 512);
            for (const auto& v : shot.values)
                setParam (processor, v.first, v.second);

            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            if (editor == nullptr) { std::fprintf (stderr, "no editor\n"); return 1; }

            //  A little audio so the meter shows something.
            juce::Random rng (99);
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            for (int block = 0; block < 20; ++block)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float v = 0.3f * (rng.nextFloat() * 2.0f - 1.0f);
                    buf.setSample (0, i, v); buf.setSample (1, i, v * 0.7f);
                }
                processor.processBlock (buf, midi);
            }
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);

            juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
            {
                juce::Graphics g (image);
                editor->paintEntireComponent (g, true);
            }

            const auto file = outputDir.getChildFile (juce::String (shot.name) + ".png");
            file.deleteFile();
            juce::FileOutputStream stream (file);
            if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
            {
                std::fprintf (stderr, "could not write %s\n", file.getFullPathName().toRawUTF8());
                return 1;
            }
            std::printf ("wrote %s  (%d x %d)\n", file.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
        }
        return 0;
    }

    int writeParameterTable (const juce::File& out)
    {
        IsoAudioProcessor processor;
        juce::StringArray rows;
        rows.add ("| Control | ID | Range | Default |");
        rows.add ("|---|---|---|---|");
        for (auto* raw : processor.getParameters())
        {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (raw);
            if (rp == nullptr) continue;
            const auto low = rp->getText (0.0f, 0), high = rp->getText (1.0f, 0), def = rp->getText (rp->getDefaultValue(), 0);
            const bool discrete = rp->getNumSteps() > 0 && rp->getNumSteps() < (int) juce::AudioProcessor::getDefaultNumParameterSteps();
            juce::String span = low + " … " + high;
            if (discrete)
            {
                juce::StringArray items;
                for (int i = 0; i < rp->getNumSteps(); ++i)
                    items.add (rp->getText ((float) i / (float) juce::jmax (1, rp->getNumSteps() - 1), 0));
                items.removeDuplicates (false);
                span = items.joinIntoString (" / ");
            }
            rows.add ("| " + rp->getName (128) + " | `" + rp->paramID + "` | " + span + " | " + def + " |");
        }
        rows.add ("");
        rows.add ("| # | Preset |");
        rows.add ("|---|---|");
        for (int i = 0; i < iso::presets::count(); ++i)
            rows.add ("| " + juce::String (i) + " | " + iso::presets::name (i) + " |");
        out.getParentDirectory().createDirectory();
        if (! out.replaceWithText (rows.joinIntoString ("\n") + "\n")) return 1;
        std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
        return 0;
    }
}

namespace
{
    //  The app / bundle icon: the mark on a dark rounded tile, 1024 px.
    //  packaging/icon.png is this, committed; CMake feeds it to ICON_BIG.
    int writeIcon (const juce::File& out, int size)
    {
        juce::Image img (juce::Image::ARGB, size, size, true);
        juce::Graphics g (img);
        const float s = (float) size;
        auto tile = juce::Rectangle<float> (0.0f, 0.0f, s, s).reduced (s * 0.03f);
        g.setGradientFill (juce::ColourGradient (iso::ui::colour::deckTop, 0.0f, 0.0f,
                                                 iso::ui::colour::window, 0.0f, s, false));
        g.fillRoundedRectangle (tile, s * 0.22f);
        g.setColour (iso::ui::colour::steelMid.withAlpha (0.8f));
        g.drawRoundedRectangle (tile.reduced (s * 0.006f), s * 0.22f, s * 0.012f);
        iso::ui::drawLogoMark (g, tile.reduced (s * 0.25f, s * 0.24f));

        out.getParentDirectory().createDirectory();
        out.deleteFile();
        juce::FileOutputStream stream (out);
        if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (img, stream))
            return 1;
        std::printf ("wrote %s  (%d x %d)\n", out.getFullPathName().toRawUTF8(), size, size);
        return 0;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                    : juce::File::getCurrentWorkingDirectory().getChildFile ("ui-shots");
    if (argc > 2 && juce::String (argv[1]) == "--icon")
        return writeIcon (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]), 1024);
    if (argc > 2 && juce::String (argv[1]) == "--params")
        return writeParameterTable (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    return render (out);
}
