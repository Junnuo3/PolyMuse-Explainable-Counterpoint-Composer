#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <deque>
#include <set>
#include <map>
#include <unordered_map>
#include "MidiManager.h"
#include "CounterpointEngine.h"
#include "PianoRoll.h"
#include "SimpleSynth.h"
#include "ExplanationEngine.h"
#include "ECCPanel.h"
#include "ModelBridge.h"
#include "Logger.h"
#include "RuleChecker.h"

// Removes focus outlines from buttons
class PolyMuseLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonFocusOutline(juce::Graphics&, juce::Button&, const juce::Colour&) {}
};

// Button with smooth click animations
class AnimatedButton : public juce::TextButton, private juce::Timer
{
public:
    AnimatedButton(const juce::String& buttonName) : juce::TextButton(buttonName)
    {
        setClickingTogglesState(false);
        setWantsKeyboardFocus(false);
        startTimerHz(60);
    }
    
    void clicked() override
    {
        pulse = 1.0f;
        startTimerHz(60);
    }
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat();
        float cornerRadius = 8.0f;
        
        // Clip to prevent drawing outside button
        juce::Path clipPath;
        clipPath.addRoundedRectangle(bounds, cornerRadius);
        g.reduceClipRegion(clipPath);
        
        // Bounce animation on click
        float scale = 1.0f + 0.05f * std::sin(pulse * juce::MathConstants<float>::pi);
        bounds = bounds.withSizeKeepingCentre(bounds.getWidth() * scale, bounds.getHeight() * scale);
        
        // Button colors
        juce::Colour base = juce::Colour::fromRGB(50, 50, 50);
        juce::Colour hover = juce::Colour::fromRGB(80, 80, 80);
        juce::Colour active = juce::Colour::fromRGB(120, 120, 120);
        juce::Colour fill = base;
        
        if (shouldDrawButtonAsDown)
            fill = active;
        else if (shouldDrawButtonAsHighlighted)
            fill = base.interpolatedWith(hover, 0.7f);
        
        // Use toggle state color if button is toggled
        if (getToggleState())
            fill = findColour(juce::TextButton::buttonOnColourId);
        else
            fill = fill.interpolatedWith(findColour(juce::TextButton::buttonColourId), 0.7f);
        
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, cornerRadius);
        
        // Subtle glow on hover or click
        if (shouldDrawButtonAsHighlighted || pulse > 0.05f)
        {
            float alpha = shouldDrawButtonAsDown ? 0.25f : 0.15f;
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.drawRoundedRectangle(bounds.reduced(0.7f), cornerRadius - 1.0f, 1.2f);
        }
        
        // Draw button text
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        g.drawFittedText(getButtonText(), getLocalBounds(), juce::Justification::centred, 1);
    }

private:
    float pulse = 0.0f;

    void timerCallback() override
    {
        if (pulse > 0.0f)
        {
            pulse -= 0.08f;
            repaint();
        }
        else
            stopTimer();
    }
};

class MainComponent : public juce::AudioAppComponent,
                      public juce::MidiInputCallback,
                      public juce::Timer,
                      public juce::ComponentListener
{
public:
    enum class ECCMode { Tutor, Generator };
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    
    void handleFullscreenChange();
    void componentMovedOrResized(Component& component, bool wasMoved, bool wasResized) override;
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    

private:
    // UI
    std::unique_ptr<PolyMuseLookAndFeel> customLookAndFeel;
    juce::ComboBox midiInputComboBox;
    juce::Label midiDeviceLabel;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    AnimatedButton modeToggle;
    AnimatedButton aboveBelowToggle;
    
    // Core components
    std::unique_ptr<MidiManager> midiManager;
    std::unique_ptr<CounterpointEngine> counterpointEngine;
    std::unique_ptr<PianoRoll> pianoRoll;
    
    // Explanation engine
    ExplanationEngine ecc;
    ECCPanel eccPanel;
    std::deque<ExplanationNotePair> history;
    std::deque<ContextNote> contextNotes;
    std::map<int, int> activeNoteMapping;
    const int contextMax = 32;
    std::unique_ptr<JsonlLogger> eccLog;
    
    // Rule checking
    RuleChecker ruleChecker;
    std::deque<NotePair> ruleHistory;
    
    // Audio
    juce::AudioDeviceManager audioDeviceManager;
    juce::Synthesiser synth;
    std::unordered_map<int, int> activeGeneratedNotes;
    
    // State
    juce::String currentStatus;
    double lastNoteOnTime = 0.0;
    ECCMode eccMode = ECCMode::Tutor;
    bool isGeneratorMode = false;
    bool isGenerateAbove = true;
    std::set<int> activeNotes;
    AnimatedButton resetPhraseButton{"Reset Phrase"};
    bool inPhrase = false;
    
    // Visual effects
    juce::Rectangle<int> analysisBoxBounds;
    bool hasViolation = false;
    float glowPhase = 0.0f;
    float glowAlpha = 0.0f;
    bool isFadingOut = false;
    juce::SmoothedValue<float> buttonAlphaAnim { 1.0f };
    juce::SmoothedValue<float> directionToggleAnim { 1.0f };
    bool isAnimating = false;
    bool isDirectionAnimating = false;
    bool isHoveringModeToggle = false;
    bool isHoveringDirectionToggle = false;
    bool isHoveringResetButton = false;
    
    // Setup
    void setupUI();
    void setupMidiInputComboBox();
    void setupButtons();
    void setupLabels();
    
    // MIDI
    void refreshMidiInputs();
    void selectMidiInput(int deviceIndex);
    void enableMidiInput(bool enable);
    void onMidiInputChanged();
    bool shouldGenerateAbove() const { return isGenerateAbove; }
    void processMidiMessage(const juce::MidiMessage& message);
    
    // UI updates
    void updateAnalysisText(const juce::String& message, bool violation);
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};