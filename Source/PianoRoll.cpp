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

PianoRoll::PianoRoll()
    : pixelsPerSecond(120.0)
    , timeWindow(6.0)
    , lastFrameTime(0.0)
    , currentTime(0.0)
    , beatsPerSecond(1.0)
    , keyHeight(20)
    , leftMargin(60)
    , topMargin(30)
    , bottomMargin(30)
    , rightMargin(30)
    , pitchOffset(48.0)    // lowest visible MIDI note (C3)
    , pitchRange(24.0)     // visible semitones (≈2 octaves)
    , pixelsPerNote(18.0)  // taller rows for clarity
    , visibleRangeStart(48) // lowest visible MIDI note
    , visibleRangeEnd(72)   // highest visible MIDI note
    , scrollOffsetY(0.0f)   // vertical scroll offset
{
    setSize(800, 600);
    startTimerHz(60); // 60 FPS for smooth updates
    
    // DAW-style setup
    setOpaque(true);
    setPaintingIsUnclipped(true);
    
    // Create viewport and content component
    viewport = std::make_unique<juce::Viewport>();
    content = std::make_unique<juce::Component>();
    
    // Set up viewport
    viewport->setViewedComponent(content.get(), false);
    viewport->setScrollBarsShown(false, true); // Hide horizontal, show vertical
    addAndMakeVisible(*viewport);
    
    // Create and add components to content
    pianoKeyboard = std::make_unique<PianoKeyboard>();
    grid = std::make_unique<PianoRollGrid>();
    
    content->addAndMakeVisible(*grid);
    content->addAndMakeVisible(*pianoKeyboard);
    grid->toBack(); // ensure it's BEHIND piano keys
    
    // Initialize keyboard with current pitch range
    pianoKeyboard->setPitchRange(pitchOffset, pitchRange);
    
    // Initialize grid with current parameters
    grid->pitchOffset = pitchOffset;
    grid->pitchRange = pitchRange;
    grid->pixelsPerNote = 18.0f;
    
    // ✅ Add component listener for automatic layout updates
    addComponentListener(this);
}

PianoRoll::~PianoRoll()
{
    stopTimer();
}

void PianoRoll::resized()
{
    // Set viewport to fill the entire PianoRoll
    if (viewport)
        viewport->setBounds(getLocalBounds());
    
    // Calculate content size based on pitch range
    const int contentHeight = (int)(pitchRange * keyHeight);
    const int contentWidth = getWidth();
    
    if (content)
    {
        content->setSize(contentWidth, contentHeight);
        
        // Set grid bounds to fill entire content
        if (grid)
            grid->setBounds(0, 0, contentWidth, contentHeight);
        
        // Set keyboard bounds (left side, full height)
        if (pianoKeyboard)
        {
            const int keyboardWidth = 60;
            pianoKeyboard->setBounds(0, 0, keyboardWidth, contentHeight);
        }
    }
    
    // Trigger layout update for dynamic boundary calculations
    updateLayout();
}

void PianoRoll::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    // Handle vertical scrolling with mouse wheel
    if (wheel.deltaY != 0.0)
    {
        // Check for modifier keys (Cmd on Mac, Ctrl on Windows/Linux)
        bool isModifierPressed = event.mods.isCommandDown() || event.mods.isCtrlDown();
        
        if (isModifierPressed)
        {
            // Vertical zoom: adjust pitchRange
            double oldRange = pitchRange;
            pitchRange += wheel.deltaY * (-2.0); // Negative deltaY = zoom in, positive = zoom out
            
            // Clamp pitchRange to reasonable bounds
            pitchRange = juce::jlimit(12.0, 60.0, pitchRange);
            
            // Keep middle note fixed by adjusting pitchOffset
            pitchOffset += (oldRange - pitchRange) / 2.0;
            
            // Clamp pitchOffset to valid range
            pitchOffset = juce::jlimit(0.0, 127.0 - pitchRange, pitchOffset);
            
            // Update visible range variables to match pitchOffset and pitchRange
            visibleRangeStart = (int)pitchOffset;
            visibleRangeEnd = (int)(pitchOffset + pitchRange);
            
            // Update keyboard with new pitch range
            pianoKeyboard->setPitchRange(pitchOffset, pitchRange);
            
            // Update content size and viewport position
            updateViewportForPitchRange();
        }
        else
        {
            // Normal vertical scrolling: use viewport
            if (viewport)
            {
                const int scrollAmount = (int)(wheel.deltaY * 20.0); // Adjust scroll sensitivity
                const int currentY = viewport->getViewPositionY();
                const int newY = juce::jlimit(0, viewport->getViewedComponent()->getHeight() - viewport->getViewHeight(), 
                                            currentY - scrollAmount);
                viewport->setViewPosition(viewport->getViewPositionX(), newY);
                
                // Update grid with viewport position
                updateGridFromViewport();
            }
        }
    }
}

void PianoRoll::setVerticalScroll(double delta)
{
    if (!viewport)
        return;
    
    // Use viewport for scrolling
    const int scrollAmount = (int)(delta * keyHeight);
    const int currentY = viewport->getViewPositionY();
    const int newY = juce::jlimit(0, viewport->getViewedComponent()->getHeight() - viewport->getViewHeight(), 
                                currentY + scrollAmount);
    viewport->setViewPosition(viewport->getViewPositionX(), newY);
    
    // Update grid with viewport position
    updateGridFromViewport();
}

void PianoRoll::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::transparentBlack);  // prevent background overwrite
    // Grid drawing is now handled by the separate PianoRollGrid component
}


void PianoRoll::drawTimeRuler(juce::Graphics& g, const juce::Rectangle<float>& ruler)
{
    // Modern gradient background for time ruler
    juce::ColourGradient rulerGradient(juce::Colour(0xff2d2d2d), 0, 0,
                                       juce::Colour(0xff3a3a3a), 0, ruler.getHeight(), false);
    g.setGradientFill(rulerGradient);
    g.fillRect(ruler);
    
    // Modern border
    g.setColour(juce::Colour(0xff555555));
    g.drawLine(ruler.getX(), ruler.getBottom() - 1, ruler.getRight(), ruler.getBottom() - 1, 1.5f);
    
    // Compute time values
    const double current = nowSec();
    const double leftT = current - timeWindow;
    
    // Tick spacing: 1.0 seconds (or 0.5 if roll width < 800)
    double tickSpacing = (getWidth() < 800) ? 0.5 : 1.0;
    
    // Draw ticks using robust beat-based approach
    g.setColour(juce::Colour(0xffaaaaaa));
    
    // Calculate beat-based spacing for consistent ticks
    const int numBeats = (int)std::ceil(timeWindow * beatsPerSecond);
    const float beatWidth = (float)ruler.getWidth() / (float)numBeats;
    
    for (int i = 0; i <= numBeats; ++i)
    {
        // Use ceilf so last tick always reaches edge, even after rounding
        float tickX = ruler.getX() + std::ceil(i * beatWidth);
        
        // Draw vertical tick
        g.drawLine(tickX, ruler.getY(), tickX, ruler.getBottom() - 1, 1.0f);
        
        // Time labels removed for cleaner look
    }
}

void PianoRoll::drawScrollingGrid(juce::Graphics& g, const juce::Rectangle<float>& roll)
{
    // Use rollArea instead of getLocalBounds()
    g.reduceClipRegion(roll.toNearestInt());
    
    // === ROBUST VERTICAL GRID LINES ===
    // Calculate beat-based spacing for consistent grid
    const int numBeats = (int)std::ceil(timeWindow * beatsPerSecond);
    const float beatWidth = (float)roll.getWidth() / (float)numBeats;
    const float height = roll.getHeight();
    
    // Draw vertical grid lines fully across the component
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    for (int i = 0; i <= numBeats; ++i)
    {
        // Use ceilf so last line always reaches edge, even after rounding
        float x = roll.getX() + std::ceil(i * beatWidth);
        g.drawLine(x, roll.getY(), x, roll.getY() + height, 1.0f);
    }
    
    // === HORIZONTAL KEY LINES ===
    const double lowPitch  = pitchOffset;
    const double highPitch = pitchOffset + pitchRange;
    const double topPitch  = highPitch;
    const int numKeys = (int)std::ceil(pitchRange);
    const float keyHeight = height / (float)numKeys;
    
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    for (int k = 0; k <= numKeys; ++k)
    {
        float y = roll.getY() + std::round(k * keyHeight);
        g.drawLine(roll.getX(), y, roll.getX() + roll.getWidth(), y, 1.0f);
    }
    
    // === ENHANCED C-NOTE LINES ===
    // Draw stronger lines for C notes (every octave)
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    for (int p = (int)std::floor(highPitch - 1); p >= (int)std::ceil(lowPitch); --p)
    {
        if ((p % 12) == 0) // C notes
        {
            float yLine = roll.getY() + (float)((topPitch - p) * pixelsPerNote);
            g.drawLine(roll.getX(), yLine, roll.getX() + roll.getWidth(), yLine, 1.5f);
        }
    }
    
    // Add subtle bounding lines
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.drawLine(roll.getX(), roll.getY(), roll.getX() + roll.getWidth(), roll.getY());           // top border
    g.drawLine(roll.getX(), roll.getY() + height, roll.getX() + roll.getWidth(), roll.getY() + height); // bottom border
}


void PianoRoll::drawNotes(juce::Graphics& g, const juce::Rectangle<float>& roll)
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

void PianoRoll::drawNote(juce::Graphics& g, const NoteEvent& note, const juce::Rectangle<int>& area, double startTime)
{
    const double lowPitch  = pitchOffset;
    const double highPitch = pitchOffset + pitchRange;
    const double topPitch  = highPitch;  // alias for clarity
    
    // Skip notes outside visible pitch range
    if (note.pitch < lowPitch || note.pitch >= highPitch)
        return;
    
    // DAW-like positioning: notes scroll left with proper scroll offset
    const double current = currentTime;
    const double scrollOffset = current - timeWindow; // Time range visible on screen
    
    float startX = (float)(area.getRight() - (current - note.startTime) * pixelsPerSecond);
    float endX = (float)(area.getRight() - (current - (note.endTime > 0 ? note.endTime : current)) * pixelsPerSecond);
    float x = startX;
    float w = (float)(((note.endTime > 0 ? note.endTime : current) - note.startTime) * pixelsPerSecond);
    
    // DAW-like behavior: only skip notes when completely off-screen to the left
    // Allow notes to scroll smoothly until they fully exit the view
    // ✅ Use dynamic noteDisappearanceX instead of fixed keyboard width
    float pianoRollLeftEdge = getNoteDisappearanceX(); // Use dynamic boundary
    float fadeMargin = 30.0f; // Buffer for smooth exit
    
    // Only skip drawing if the note is completely off-screen (right edge left of piano roll)
    if (endX < (pianoRollLeftEdge - fadeMargin))
        return;
    if (x > area.getRight()) return;
    
    // Fixed vertical positioning: align MIDI pitches exactly with piano keys
    float yTop    = area.getY() + (float)((topPitch - (note.pitch + 1)) * pixelsPerNote);
    float yBottom = area.getY() + (float)((topPitch - note.pitch) * pixelsPerNote);
    
    // Clamp all y-coordinates to stay within roll bounds
    yTop    = juce::jmax((float)area.getY(), yTop);
    yBottom = juce::jmin((float)area.getBottom(), yBottom);
    float y = yTop;
    float h = juce::jmax(1.0f, yBottom - yTop);
    
    // Use the calculated width
    float width = w;
    
    // Ensure minimum width for visibility
    width = juce::jmax(width, 2.0f);
    
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
    
    // Optional fade-out effect for smooth visual exit
    if (endX < (pianoRollLeftEdge + fadeMargin))
    {
        float alpha = juce::jlimit(0.0f, 1.0f, (endX - (pianoRollLeftEdge - fadeMargin)) / (fadeMargin * 2.0f));
        col = col.withAlpha(alpha);
    }
    
    // Draw the note rectangle
    g.setColour(col);
    g.fillRect(x, y, width, h);
    
    // Draw note border
    g.setColour(col.darker(0.3f));
    g.drawRect(x, y, width, h, 1.0f);
    
    // Note drawn successfully - smooth scrolling behavior enabled
}

void PianoRoll::drawCurrentTimeIndicator(juce::Graphics& g, const juce::Rectangle<float>& roll)
{
    // Draw playhead indicator at right edge (current time)
    g.setColour(juce::Colours::red.withAlpha(0.4f));
    g.drawLine(roll.getRight(), roll.getY(),
               roll.getRight(), roll.getBottom(), 1.5f);
}

void PianoRoll::drawInfluences(juce::Graphics& g, const juce::Rectangle<float>& roll)
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

void PianoRoll::timerCallback()
{
    // Update current time for scrolling
    currentTime = nowSec();
    
    // Keep past notes slightly longer so they slide off visually
    // Use 1.5x timeWindow to allow notes to scroll completely off-screen
    finishedNotes.erase(
        std::remove_if(finishedNotes.begin(), finishedNotes.end(),
            [this](const NoteEvent& note) {
                return (currentTime - note.endTime) > (timeWindow * 1.5f);
            }),
        finishedNotes.end()
    );
    
    // Update piano keyboard with current pitch range and active notes
    if (pianoKeyboard)
    {
        pianoKeyboard->setPitchRange(pitchOffset, pitchRange);
        pianoKeyboard->setActiveNotes(getActivePitches());
    }
    
    // Update grid with current viewport position
    updateGridFromViewport();
    
    // Sync note data with grid for rendering
    if (grid)
    {
        grid->activeNotes = activeNotes;
        grid->finishedNotes = finishedNotes;
        grid->currentTime = currentTime;
        grid->timeWindow = timeWindow;
        grid->pixelsPerSecond = pixelsPerSecond;
        grid->influences = influences;
        grid->generateAbove = generateAbove;
    }
    
    // Trigger repaint - grid will persist because it's drawn last in paint method
    repaint();
}

void PianoRoll::noteOn(int voice, int pitch, float velocity)
{
    juce::ScopedLock lock(notesLock);
    
    // End any existing note of the same voice and pitch
    noteOff(voice, pitch);
    
    // Use nowSec() for consistent timebase
    const double now = nowSec();
    
    // Add new active note with velocity (endTime = -1.0, active = true)
    activeNotes.emplace_back(voice, pitch, velocity, now, -1.0, true);
}

void PianoRoll::noteOff(int voice, int pitch)
{
    juce::ScopedLock lock(notesLock);
    
    // Find the most recent active note with matching voice and pitch
    auto it = std::find_if(activeNotes.rbegin(), activeNotes.rend(),
        [voice, pitch](const NoteEvent& note) {
            return note.voice == voice && note.pitch == pitch && note.active;
        });
    
    if (it != activeNotes.rend())
    {
        // Use nowSec() for consistent timebase
        const double now = nowSec();
        
        // Convert to finished note
        NoteEvent finishedNote = *it;
        finishedNote.endTime = now;
        finishedNote.active = false;
        
        // Remove from active notes
        activeNotes.erase(std::next(it).base());
        
        // Add to finished notes
        finishedNotes.push_back(finishedNote);
    }
}

void PianoRoll::clearAllNotes()
{
    juce::ScopedLock lock(notesLock);
    activeNotes.clear();
    finishedNotes.clear();
}

void PianoRoll::clearVoice(int voice)
{
    juce::ScopedLock lock(notesLock);
    
    // Remove active notes for this voice
    activeNotes.erase(
        std::remove_if(activeNotes.begin(), activeNotes.end(),
            [voice](const NoteEvent& note) {
                return note.voice == voice;
            }),
        activeNotes.end()
    );
    
    // Remove finished notes for this voice
    finishedNotes.erase(
        std::remove_if(finishedNotes.begin(), finishedNotes.end(),
            [voice](const NoteEvent& note) {
                return note.voice == voice;
            }),
        finishedNotes.end()
    );
}

void PianoRoll::setScrollSpeed(double pixelsPerSecond)
{
    this->pixelsPerSecond = pixelsPerSecond;
    repaint();
}

void PianoRoll::setTimeWindow(double seconds)
{
    this->timeWindow = seconds;
    repaint();
}

void PianoRoll::setBeatsPerSecond(double bps)
{
    this->beatsPerSecond = bps;
    repaint();
}

void PianoRoll::setCurrentTime(double time)
{
    this->currentTime = time;
    repaint();
}

void PianoRoll::addNote(int voice, int midiNote, float velocity)
{
    noteOn(voice, midiNote, velocity);
}

void PianoRoll::addNote(const PianoRollNote& note)
{
    noteOn(0, note.midiNote, note.velocity); // Default to voice 0 with velocity
}

void PianoRoll::clearDisplay()
{
    clearAllNotes();
}

juce::String PianoRoll::getNoteName(int pitch) const
{
    const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = ((pitch - 12) / 12) - 1; // so MIDI 60 → C3 (consistent with PianoKeyboard)
    int note = pitch % 12;
    return juce::String(noteNames[note]) + juce::String(octave);
}

bool PianoRoll::isBlackKey(int pitch) const
{
    int note = pitch % 12;
    return (note == 1 || note == 3 || note == 6 || note == 8 || note == 10);
}

juce::Colour PianoRoll::getVelocityColor(int voice, float velocity) const
{
    // Use the new dynamic velocity color mapping
    juce::Colour col = getDynamicVelocityColor((int)velocity);
    
    // For user voice (0): slightly desaturate and dim
    if (voice == 0)
    {
        col = col.withMultipliedSaturation(0.8f).withMultipliedBrightness(0.9f);
    }
    // For generated voice (1): keep full color
    
    return col;
}

bool PianoRoll::isVoiceCrossing(const NoteEvent& note) const
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

void PianoRoll::setNoteRange(int lowestNote, int highestNote)
{
    // Clamp to valid MIDI range
    lowestNote = juce::jlimit(0, 127, lowestNote);
    highestNote = juce::jlimit(0, 127, highestNote);
    
    // Ensure lowestNote < highestNote
    if (lowestNote >= highestNote)
        highestNote = juce::jmin(127, lowestNote + 12); // Default to 1 octave range
    
    // Update pitch range parameters
    pitchOffset = (double)lowestNote;
    pitchRange = (double)(highestNote - lowestNote);
    
    // Update viewport and content for new range
    updateViewportForPitchRange();
    
    // Trigger repaint to show the new range
    repaint();
}

void PianoRoll::updateViewportForPitchRange()
{
    if (!viewport || !content)
        return;
    
    // Update content size based on new pitch range
    const int contentHeight = (int)(pitchRange * keyHeight);
    const int contentWidth = getWidth();
    content->setSize(contentWidth, contentHeight);
    
    // Update grid bounds
    if (grid)
    {
        grid->setBounds(0, 0, contentWidth, contentHeight);
        grid->pitchOffset = pitchOffset;
        grid->pitchRange = pitchRange;
        grid->pixelsPerNote = 18.0f;
    }
    
    // Update keyboard bounds
    if (pianoKeyboard)
    {
        const int keyboardWidth = 60;
        pianoKeyboard->setBounds(0, 0, keyboardWidth, contentHeight);
    }
    
    // Update grid with current viewport position
    updateGridFromViewport();
}

void PianoRoll::updateLayout()
{
    // Use current bounds for layout update
    updateLayout(getLocalBounds());
}

void PianoRoll::updateLayout(juce::Rectangle<int> parentBounds)
{
    const int newWidth = parentBounds.getWidth();
    const int newHeight = parentBounds.getHeight();
    
    // Update viewport bounds
    if (viewport)
        viewport->setBounds(parentBounds);
    
    // Update content size
    if (content)
    {
        const int contentHeight = (int)(pitchRange * keyHeight);
        content->setSize(newWidth, contentHeight);
        
        // Update grid bounds
        if (grid)
            grid->setBounds(0, 0, newWidth, contentHeight);
        
        // Update keyboard bounds
        if (pianoKeyboard)
        {
            const int keyboardWidth = 60;
            pianoKeyboard->setBounds(0, 0, keyboardWidth, contentHeight);
        }
    }
    
    // ✅ Dynamically recompute right boundary where notes vanish
    noteDisappearanceX = getKeyboardWidth(); // Use current keyboard width
    
    // ✅ Update transformation scale factors
    pixelsPerSecond = (float)newWidth / (float)timeWindow;
    pixelsPerNote = (float)newHeight / (float)pitchRange;
    
    // Update grid with new parameters
    if (grid)
    {
        grid->pitchOffset = pitchOffset;
        grid->pitchRange = pitchRange;
        grid->pixelsPerNote = pixelsPerNote;
        grid->keyboardWidth = noteDisappearanceX;
        grid->repaint();
    }
    
    resizedSinceLastFrame = true;
    
    // Update viewport if needed
    updateGridFromViewport();
}

float PianoRoll::getKeyboardWidth() const
{
    // Return the current keyboard width, which should match the piano keyboard bounds
    if (pianoKeyboard)
    {
        return (float)pianoKeyboard->getWidth();
    }
    return 60.0f; // Default fallback
}

float PianoRoll::getNoteDisappearanceX() const
{
    return noteDisappearanceX;
}

void PianoRoll::componentMovedOrResized(Component& component, bool wasMoved, bool wasResized)
{
    // ✅ Automatic layout updates when component is resized
    if (wasResized && &component == this)
    {
        updateLayout(getLocalBounds());
    }
}

void PianoRoll::updateGridFromViewport()
{
    if (!viewport || !grid)
        return;
    
    // Update grid with current pitch range to match piano keyboard
    grid->pitchOffset = pitchOffset;
    grid->pitchRange = pitchRange;
    grid->pixelsPerNote = 18.0f; // Match PianoKeyboard exactly
    
    // Get viewport position and pass to grid
    const int viewportY = viewport->getViewPositionY();
    grid->scrollOffsetY = (float)viewportY;
    grid->repaint();
}
