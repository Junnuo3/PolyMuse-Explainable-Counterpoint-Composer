#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <vector>

class PianoKeyboard : public juce::Component
{
public:
    PianoKeyboard() = default;

    void setPitchRange(double lowPitch, double range)
    {
        pitchOffset = lowPitch;
        pitchRange  = range;
        repaint();
    }

    void setActiveNotes(const std::vector<int>& active)
    {
        activeNotes = active;
        repaint();
    }

    float getYForNote(int midiNote) const
    {
        const double topPitch = pitchOffset + pitchRange;
        const float pixelsPerNote = 18.0f;
        return (float)((topPitch - midiNote) * pixelsPerNote);
    }

    void paint(juce::Graphics& g) override
    {
        const double topPitch = pitchOffset + pitchRange;
        const float pixelsPerNote = 18.0f;
        auto bounds = getLocalBounds().toFloat();

        for (int p = (int)std::floor(topPitch - 1); p >= (int)std::ceil(pitchOffset); --p)
        {
            float yTop = (float)((topPitch - (p + 0)) * pixelsPerNote);
            float yBottom = (float)((topPitch - (p - 1)) * pixelsPerNote);
            float height = yBottom - yTop;

            bool isBlack = isBlackKey(p % 12);
            juce::Colour keyColour = isBlack ? juce::Colours::darkgrey : juce::Colours::white;

            if (std::find(activeNotes.begin(), activeNotes.end(), p) != activeNotes.end())
                keyColour = keyColour.brighter(0.4f);

            g.setColour(keyColour);
            g.fillRect(bounds.getX(), yTop, bounds.getWidth(), height);

            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.drawRect(bounds.getX(), yTop, bounds.getWidth(), height, 1.0f);

            // Label only "C" keys for readability
            if (!isBlack && (p % 12) == 0)
            {
                int octave = ((p - 12) / 12) - 1;  // shifts everything down one octave
                juce::String label = "C" + juce::String(octave);
                g.setColour(juce::Colours::black);
                g.setFont(12.0f);
                g.drawText(label, 2, (int)(yTop + 2), (int)bounds.getWidth() - 4, (int)height - 2,
                           juce::Justification::centredLeft);
            }
        }
    }

private:
    bool isBlackKey(int pitchClass) const
    {
        return pitchClass == 1 || pitchClass == 3 || pitchClass == 6 ||
               pitchClass == 8 || pitchClass == 10;
    }

    double pitchOffset = 48.0;
    double pitchRange  = 24.0;
    std::vector<int> activeNotes;
};
