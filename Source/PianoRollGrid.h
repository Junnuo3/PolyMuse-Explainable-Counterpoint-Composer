#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "ECCTypes.h"

// Forward declaration
struct NoteEvent;

class PianoRollGrid : public juce::Component
{
public:
    float pixelsPerNote = 18.0f;  // Match PianoKeyboard exactly
    float scrollOffsetY = 0.0f;
    double pitchOffset = 48.0;    // Match PianoKeyboard
    double pitchRange = 24.0;     // Match PianoKeyboard
    
    // Note data for rendering
    std::vector<NoteEvent> activeNotes;
    std::vector<NoteEvent> finishedNotes;
    double currentTime = 0.0;
    double timeWindow = 6.0;
    double pixelsPerSecond = 120.0;
    std::vector<Influence> influences;
    bool generateAbove = true;
    float keyboardWidth = 60.0f; // Dynamic keyboard width for boundary calculations

    void paint (juce::Graphics& g) override;
    
private:
    void drawNotes(juce::Graphics& g, const juce::Rectangle<float>& roll);
    void drawNote(juce::Graphics& g, const NoteEvent& note, const juce::Rectangle<int>& area, double startTime);
    void drawInfluences(juce::Graphics& g, const juce::Rectangle<float>& roll);
    bool isVoiceCrossing(const NoteEvent& note) const;
    juce::Colour getDynamicVelocityColor(int velocity) const;
};
