#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "PianoKeyboard.h"
#include "PianoRollGrid.h"
#include "ECCTypes.h"

/**
 * NoteEvent represents a single note event in the DAW-style piano roll
 */
struct NoteEvent
{
    int voice;          // Voice/track number (0 = cantus firmus, 1 = counterpoint)
    int pitch;          // MIDI pitch (0-127)
    float velocity;     // Note velocity (0.0-127.0)
    double startTime;   // Start time in seconds
    double endTime;     // End time in seconds (-1.0 if note is still active)
    bool active;        // Whether the note is currently playing
    
    NoteEvent(int v, int p, float vel, double start, double end = -1.0, bool a = true)
        : voice(v), pitch(p), velocity(vel), startTime(start), endTime(end), active(a) {}
};

/**
 * Legacy PianoRollNote for backward compatibility
 */
struct PianoRollNote
{
    int midiNote;
    double timestamp;
    double duration;
    float velocity;
    juce::Colour color;
    
    PianoRollNote(int note, double time, double dur, float vel, juce::Colour col)
        : midiNote(note), timestamp(time), duration(dur), velocity(vel), color(col) {}
};

/**
 * DAW-style scrolling piano roll component that displays note events
 * with proper time-based scrolling and visualization.
 */
class PianoRoll : public juce::Component, public juce::Timer, public juce::ComponentListener
{
public:
    PianoRoll();
    ~PianoRoll() override;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override; // Supports Cmd/Ctrl+scroll for vertical zoom
    
    // Timer override for 60 FPS updates
    void timerCallback() override;

    // Note management
    void noteOn(int voice, int pitch, float velocity = 64.0f);
    void noteOff(int voice, int pitch);
    void clearAllNotes();
    void clearVoice(int voice);
    
    // DAW-style controls
    void setScrollSpeed(double pixelsPerSecond);
    void setTimeWindow(double seconds);
    void setBeatsPerSecond(double bps);
    void setCurrentTime(double time);
    
    // Vertical scrolling controls
    void setVerticalScroll(double delta);
    
    // Display settings
    void setNoteHeight(float height);
    void setLaneHeight(float height);
    void setVoiceColor(int voice, juce::Colour color);
    void setBackgroundColor(juce::Colour color);
    
    // Legacy compatibility methods
    void addNote(int voice, int midiNote, float velocity = 1.0f);
    void addNote(const PianoRollNote& note);
    void clearDisplay();
    void setNoteRadius(float radius);
    void setDefaultNoteDuration(float duration);
    
    // Influence highlighting API
    void setInfluences(const std::vector<Influence>& infl){ influences = infl; }
    
    // Voice crossing detection
    bool isVoiceCrossing(const NoteEvent& note) const;
    
    // Generation mode control
    void setGenerateAbove(bool above) { generateAbove = above; }
    
    // Note range control for dynamic scaling
    void setNoteRange(int lowestNote, int highestNote);
    int getLowestNote() const { return (int)pitchOffset; }
    int getHighestNote() const { return (int)(pitchOffset + pitchRange); }
    
    // Dynamic layout updates for fullscreen/window resize
    void updateLayout();
    void updateLayout(juce::Rectangle<int> parentBounds);
    
    // Get current keyboard width for dynamic boundary calculations
    float getKeyboardWidth() const;
    
    // Dynamic boundary tracking for note disappearance
    float getNoteDisappearanceX() const;
    
    // ComponentListener implementation
    void componentMovedOrResized(Component& component, bool wasMoved, bool wasResized) override;

private:
    // Note storage
    std::vector<NoteEvent> activeNotes;    // Currently playing notes
    std::vector<NoteEvent> finishedNotes;  // Completed notes for display
    
    // Time and scrolling
    double currentTime = 0.0;              // Current playback time in seconds
    double scrollSpeed = 100.0;            // Pixels per second
    double timeWindow = 6.0;               // seconds visible across the whole width
    double pixelsPerSecond = 120.0;        // x-scale (tune later)
    double lastFrameTime = 0.0;            // sec, for smooth scroll if needed
    double beatsPerSecond = 1.0;           // Beats per second for grid lines
    
    // Layout parameters
    float keyHeight = 20.0f;               // Height of each piano key
    float leftMargin = 60.0f;              // Left margin for piano keys
    float topMargin = 30.0f;               // Top margin for time labels
    float bottomMargin = 30.0f;            // Bottom margin
    float rightMargin = 30.0f;             // Right margin
    
    // Vertical scrolling parameters
    double pitchOffset = 48.0;             // lowest visible MIDI note (C3)
    double pitchRange = 24.0;              // visible semitones (≈2 octaves)
    double pixelsPerNote = 18.0;           // vertical scale (taller rows for clarity)
    
    // Range and scroll variables for keyboard alignment
    int visibleRangeStart = 48;            // lowest visible MIDI note
    int visibleRangeEnd = 72;              // highest visible MIDI note
    float scrollOffsetY = 0.0f;            // vertical scroll offset
    
    // Display parameters
    float noteHeight = 12.0f;              // Height of note rectangles
    float laneHeight = 50.0f;              // Height of each voice lane
    float laneSpacing = 5.0f;              // Spacing between lanes
    float pianoKeyWidth = 30.0f;           // Width of piano key area
    
    // Colors
    juce::Colour voice0Color = juce::Colour(0xff4CAF50); // Green for cantus firmus
    juce::Colour voice1Color = juce::Colour(0xff2196F3); // Blue for counterpoint
    juce::Colour backgroundColor = juce::Colour(0xff2C2C2C);
    juce::Colour gridColor = juce::Colour(0xff404040);
    juce::Colour pianoKeyColor = juce::Colour(0xff1A1A1A);
    
    // Thread safety
    juce::CriticalSection notesLock;
    
    // Viewport and content component for proper scrolling
    std::unique_ptr<juce::Viewport> viewport;
    std::unique_ptr<juce::Component> content;
    
    // Piano keyboard component
    std::unique_ptr<PianoKeyboard> pianoKeyboard;
    
    // Grid overlay component
    std::unique_ptr<PianoRollGrid> grid;
    
    // Influence highlighting
    std::vector<Influence> influences;
    
    // Generation mode
    bool generateAbove = true; // Default to generating above
    
    // Dynamic boundary tracking
    float noteDisappearanceX = 60.0f; // Dynamic right boundary where notes vanish
    bool resizedSinceLastFrame = false; // Flag for layout updates
    
    // Helper methods
    void updateCurrentTime();
    void moveFinishedNotes();
    double timeToPixel(double time) const;
    double pixelToTime(double pixel) const;
    float getNoteYPosition(int pitch, int voice) const;
    void drawPianoKeys(juce::Graphics& g);
    void drawTimeGrid(juce::Graphics& g);
    void drawNotes(juce::Graphics& g, const juce::Rectangle<float>& roll);
    void drawNote(juce::Graphics& g, const NoteEvent& note, const juce::Rectangle<int>& area, double startTime);
    void drawLane(juce::Graphics& g, int voice);
    juce::String getNoteName(int pitch) const;
    bool isBlackKey(int pitch) const;
    juce::Colour getVelocityColor(int voice, float velocity) const;
    
    // Viewport helper methods
    void updateViewportForPitchRange();
    void updateGridFromViewport();
    
    // New drawing methods
    void drawTimeRuler(juce::Graphics& g, const juce::Rectangle<float>& ruler);
    void drawScrollingGrid(juce::Graphics& g, const juce::Rectangle<float>& roll);
    void drawCurrentTimeIndicator(juce::Graphics& g, const juce::Rectangle<float>& roll);
    void drawInfluences(juce::Graphics& g, const juce::Rectangle<float>& roll);
    void drawGrid(juce::Graphics& g);
    
    // Helper method to get active pitches for piano keyboard
    const std::vector<int> getActivePitches() const
    {
        std::vector<int> actives;
        for (const auto& n : activeNotes) actives.push_back(n.pitch);
        return actives;
    }
    
};