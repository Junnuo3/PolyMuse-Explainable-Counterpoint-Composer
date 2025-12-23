#include "PianoRollGrid.h"
#include "PianoRoll.h"
#include <cmath>

// Helper function for consistent timebase
static inline double nowSec() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

// Dynamic note color by intensity - blue → purple → red based on velocity
static juce::Colour getDynamicVelocityColor(int velocity)
{
    float t;
    juce::Colour colorLow = juce::Colour::fromRGB(77, 166, 255);  // blue
    juce::Colour colorMid = juce::Colour::fromRGB(154, 102, 255); // purple
    juce::Colour colorHigh = juce::Colour::fromRGB(255, 77, 77);  // red

    if (velocity < 50)
        return colorLow;
    else if (velocity < 90)
    {
        t = (velocity - 50) / 40.0f;
        return colorLow.interpolatedWith(colorMid, t);
    }
    else
    {
        t = (velocity - 90) / 37.0f;
        return colorMid.interpolatedWith(colorHigh, std::clamp(t, 0.0f, 1.0f));
    }
}

void PianoRollGrid::paint(juce::Graphics& g)
{
    // Fill background
    g.fillAll(juce::Colour(0xff1a1a1a));
    
    // Debug: Check if we have any notes and current time
    DBG("PianoRollGrid::paint - currentTime: " << currentTime << ", activeNotes: " << activeNotes.size() << ", finishedNotes: " << finishedNotes.size());
    
    const float width = (float)getWidth();
    const float height = (float)getHeight();
    
    // Calculate the pitch range we need to draw
    const double topPitch = pitchOffset + pitchRange;
    const double bottomPitch = pitchOffset;
    
    // Calculate how many notes we need to draw (cover the entire content height)
    const int totalNotes = (int)std::ceil(pitchRange);
    
    // Draw grid lines for each note boundary
    for (int i = 0; i <= totalNotes; ++i)
    {
        // Calculate the pitch for this line
        const double currentPitch = topPitch - i;
        
        // Calculate Y position using the same formula as PianoKeyboard
        const float y = (float)((topPitch - currentPitch) * pixelsPerNote);
        
        // Only draw if within visible area (with some margin for smooth scrolling)
        if (y >= -pixelsPerNote && y <= height + pixelsPerNote)
        {
            // Determine if this is a C-B boundary (octave separator)
            const int pitchClass = ((int)std::round(currentPitch) % 12 + 12) % 12;
            const bool isOctaveBoundary = (pitchClass == 11); // B to C boundary
            
            if (isOctaveBoundary)
            {
                // Thick line at C-B boundaries (octave separators)
                g.setColour(juce::Colours::white.withAlpha(0.25f));
                g.drawLine(0.0f, y, width, y, 1.6f);
            }
            else
            {
                // Regular line at all other note boundaries
                g.setColour(juce::Colours::white.withAlpha(0.10f));
                g.drawLine(0.0f, y, width, y, 1.0f);
            }
        }
    }
    
    // Draw notes on top of grid
    drawNotes(g, juce::Rectangle<float>(0, 0, width, height));
    
    // Draw influences
    drawInfluences(g, juce::Rectangle<float>(0, 0, width, height));
}

void PianoRollGrid::drawNotes(juce::Graphics& g, const juce::Rectangle<float>& roll)
{
    const double lowPitch  = pitchOffset;
    const double highPitch = pitchOffset + pitchRange;
    
    double startTime = currentTime - timeWindow;
    double endTime = currentTime;
    
    // Convert to int for compatibility with drawNote
    auto area = roll.toNearestInt();
    
    // Draw finished notes first (so active notes appear on top)
    for (const auto& note : finishedNotes)
    {
        // Skip any note completely outside visible pitch range
        if (note.pitch < lowPitch || note.pitch >= highPitch)
            continue;
            
        // Let drawNote() handle all visibility logic including DAW-like scrolling
        // This ensures both Tutor Mode (voice 0) and Generator Mode (voices 0,1) behave identically
        drawNote(g, note, area, startTime);
    }
    
    // Draw active notes on top (so they appear above finished notes)
    for (const auto& note : activeNotes)
    {
        // Skip any note completely outside visible pitch range
        if (note.pitch < lowPitch || note.pitch >= highPitch)
            continue;
            
        // Let drawNote() handle all visibility logic including DAW-like scrolling
        // This ensures both Tutor Mode (voice 0) and Generator Mode (voices 0,1) behave identically
        drawNote(g, note, area, startTime);
    }
}

void PianoRollGrid::drawNote(juce::Graphics& g, const NoteEvent& note, const juce::Rectangle<int>& area, double startTime)
{
    const double lowPitch  = pitchOffset;
    const double highPitch = pitchOffset + pitchRange;
    const double topPitch  = highPitch;  // alias for clarity
    
    // Debug: Log note details
    DBG("Drawing note - pitch: " << note.pitch << ", startTime: " << note.startTime << ", currentTime: " << currentTime);
    DBG("Pitch range: " << lowPitch << " to " << highPitch << ", topPitch: " << topPitch);
    DBG("Calculated yTop: " << (topPitch - (note.pitch + 1)) * pixelsPerNote << ", yBottom: " << (topPitch - note.pitch) * pixelsPerNote);
    
    // Skip notes outside visible pitch range
    if (note.pitch < lowPitch || note.pitch >= highPitch)
    {
        DBG("Note skipped - outside pitch range: " << note.pitch << " (range: " << lowPitch << "-" << highPitch << ")");
        return;
    }
    
    // DAW-like positioning: notes scroll left with proper scroll offset
    const double current = currentTime;
    
    // Calculate time since note started
    double timeSinceStart = current - note.startTime;
    
    // Calculate note duration (for active notes, use current time as end)
    double noteDuration = (note.endTime > 0) ? (note.endTime - note.startTime) : (current - note.startTime);
    
    // Position: start from right edge, move left as time progresses
    float startX = (float)(area.getRight() - timeSinceStart * pixelsPerSecond);
    float width = (float)(noteDuration * pixelsPerSecond);
    
    // Ensure minimum width for visibility
    width = juce::jmax(width, 2.0f);
    
    // Skip notes that are completely off-screen to the left
    // Use dynamic keyboard width as the left boundary instead of area.getX() (which is 0)
    if (startX + width < keyboardWidth)
        return;
    
    // Skip notes that are completely off-screen to the right
    if (startX > area.getRight())
        return;
    
    // Fixed vertical positioning: align MIDI pitches exactly with piano keys
    // Use same calculation as PianoKeyboard: (topPitch - note.pitch)
    float yTop    = area.getY() + (float)((topPitch - note.pitch) * pixelsPerNote);
    float yBottom = area.getY() + (float)((topPitch - (note.pitch - 1)) * pixelsPerNote);
    
    // Clamp all y-coordinates to stay within roll bounds
    yTop    = juce::jmax((float)area.getY(), yTop);
    yBottom = juce::jmin((float)area.getBottom(), yBottom);
    float y = yTop;
    float h = juce::jmax(1.0f, yBottom - yTop);
    
    // Use the calculated width and position
    float x = startX;
    
    // Dynamic color based on velocity intensity
    juce::Colour col;
    
    if (note.voice == 0) {
        // Input notes - use velocity-based color
        col = getDynamicVelocityColor((int)note.velocity);
    } else if (note.voice == 1) {
        // Generated counterpoint notes - check for voice crossing first
        if (isVoiceCrossing(note)) {
            col = juce::Colours::red; // true overlap or crossing
        } else {
            // Use velocity-based color for generated notes too
            col = getDynamicVelocityColor((int)note.velocity);
        }
    } else {
        // Fallback for other voices
        col = getDynamicVelocityColor((int)note.velocity);
    }
    
    // No fade-out effect needed with simplified positioning
    
    // Draw the note rectangle
    g.setColour(col);
    g.fillRect(x, y, width, h);
    
    // Draw note border
    g.setColour(col.darker(0.3f));
    g.drawRect(x, y, width, h, 1.0f);
    
    // Note drawn successfully - smooth scrolling behavior enabled
}

void PianoRollGrid::drawInfluences(juce::Graphics& g, const juce::Rectangle<float>& roll)
{
    const double lowPitch  = pitchOffset;
    const double highPitch = pitchOffset + pitchRange;
    const double topPitch  = highPitch;
    const double current = currentTime;
    
    for (const auto& inf : influences){
        if (inf.pitch < lowPitch || inf.pitch >= highPitch) continue;
        float yTop    = roll.getY() + (float)((topPitch - (inf.pitch + 1)) * pixelsPerNote);
        float yBottom = roll.getY() + (float)((topPitch - inf.pitch) * pixelsPerNote);
        float x = roll.getRight() - (float)((current - inf.startSec) * pixelsPerSecond);
        float w = (float)((inf.endSec - inf.startSec) * pixelsPerSecond);
        juce::Colour c = juce::Colours::orange.withAlpha(juce::jlimit(0.1f, 0.4f, inf.weight));
        g.setColour(c);
        g.fillRect(x, yTop, w, yBottom - yTop);
    }
}

bool PianoRollGrid::isVoiceCrossing(const NoteEvent& note) const
{
    // Only check voice crossing for generated notes (voice 1)
    if (note.voice != 1) return false;
    
    // Find the corresponding input note (voice 0) that overlaps in time
    for (const auto& inputNote : activeNotes)
    {
        if (inputNote.voice == 0 && inputNote.active)
        {
            // Check if notes overlap in time
            bool timeOverlap = (note.startTime < (inputNote.endTime > 0 ? inputNote.endTime : currentTime) &&
                               (note.endTime > 0 ? note.endTime : currentTime) > inputNote.startTime);
            
            if (timeOverlap)
            {
                // Check for voice crossing based on generation mode
                bool voiceCrossing = false;
                if (generateAbove)
                {
                    // When generating above, crossing occurs if generated note is below or equal to input
                    voiceCrossing = (note.pitch <= inputNote.pitch);
                }
                else
                {
                    // When generating below, crossing occurs if generated note is above or equal to input
                    voiceCrossing = (note.pitch >= inputNote.pitch);
                }
                
                if (voiceCrossing)
                {
                    return true;
                }
            }
        }
    }
    
    // Also check finished notes for recent crossings
    for (const auto& inputNote : finishedNotes)
    {
        if (inputNote.voice == 0)
        {
            // Check if notes overlap in time (with some tolerance for recent notes)
            bool timeOverlap = (note.startTime < inputNote.endTime &&
                               (note.endTime > 0 ? note.endTime : currentTime) > inputNote.startTime);
            
            if (timeOverlap)
            {
                // Check for voice crossing based on generation mode
                bool voiceCrossing = false;
                if (generateAbove)
                {
                    // When generating above, crossing occurs if generated note is below or equal to input
                    voiceCrossing = (note.pitch <= inputNote.pitch);
                }
                else
                {
                    // When generating below, crossing occurs if generated note is above or equal to input
                    voiceCrossing = (note.pitch >= inputNote.pitch);
                }
                
                if (voiceCrossing)
                {
                    return true;
                }
            }
        }
    }
    
    return false;
}

juce::Colour PianoRollGrid::getDynamicVelocityColor(int velocity) const
{
    return ::getDynamicVelocityColor(velocity);
}
