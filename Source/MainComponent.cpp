#include "MainComponent.h"
#include <cmath>

MainComponent::MainComponent()
    : midiDeviceLabel("MIDI Device", "MIDI DEVICE"),
      titleLabel("Title", "PolyMuse"),
      subtitleLabel("Subtitle", "Explainable Counterpoint Composer"),
      modeToggle("Tutor Mode"),
      aboveBelowToggle("Generate Above"),
      resetPhraseButton("Reset Phrase"),
      currentStatus("Ready")
{
    customLookAndFeel = std::make_unique<PolyMuseLookAndFeel>();
    juce::LookAndFeel::setDefaultLookAndFeel(customLookAndFeel.get());
    
    midiManager = std::make_unique<MidiManager>();
    counterpointEngine = std::make_unique<CounterpointEngine>();
    pianoRoll = std::make_unique<PianoRoll>();
    
    if (counterpointEngine)
        counterpointEngine->setGenerateAbove(isGenerateAbove);
    
    if (pianoRoll)
        pianoRoll->setGenerateAbove(isGenerateAbove);
    
    audioDeviceManager.initialise(0, 2, nullptr, true);
    
    // Setup synth with 8 voices
    synth.clearVoices();
    for (int i = 0; i < 8; ++i)
        synth.addVoice(new SineVoice());
    synth.clearSounds();
    synth.addSound(new SineSound());
    
    setAudioChannels(0, 2);
    
    setupUI();
    addAndMakeVisible(eccPanel);
    
    modeToggle.onClick = [this] {
        isGeneratorMode = !isGeneratorMode;
        eccMode = isGeneratorMode ? ECCMode::Generator : ECCMode::Tutor;
        modeToggle.setButtonText(isGeneratorMode ? "Generator Mode" : "Tutor Mode");
        
        if (isGeneratorMode) {
            buttonAlphaAnim.setTargetValue(1.0f);
            aboveBelowToggle.setInterceptsMouseClicks(true, false);
        } else {
            buttonAlphaAnim.setTargetValue(0.45f);
            aboveBelowToggle.setInterceptsMouseClicks(false, false);
        }
        
        aboveBelowToggle.setVisible(true);
        isAnimating = true;
        startTimerHz(60);
        
        if (isGeneratorMode)
            eccPanel.setStatusText("");
        
        repaint();
    };
    addAndMakeVisible(modeToggle);
    
    aboveBelowToggle.onClick = [this] {
        isGenerateAbove = !isGenerateAbove;
        aboveBelowToggle.setButtonText(isGenerateAbove ? "Generate Above" : "Generate Below");
        
        directionToggleAnim.setTargetValue(1.0f);
        isDirectionAnimating = true;
        startTimerHz(60);
        
        if (counterpointEngine)
            counterpointEngine->setGenerateAbove(isGenerateAbove);
        
        if (pianoRoll)
            pianoRoll->setGenerateAbove(isGenerateAbove);
        
        repaint();
    };
    addAndMakeVisible(aboveBelowToggle);
    
    // Start in Tutor Mode, so direction button is disabled
    aboveBelowToggle.setAlpha(0.45f);
    aboveBelowToggle.setInterceptsMouseClicks(false, false);
    
    addAndMakeVisible(resetPhraseButton);
    resetPhraseButton.onClick = [this] {
        history.clear();
        ruleHistory.clear();
        contextNotes.clear();
        activeNotes.clear();
        activeNoteMapping.clear();
        activeGeneratedNotes.clear();
        inPhrase = false;
        
        if (pianoRoll)
            pianoRoll->clearAllNotes();
        
        eccPanel.setStatusText("");
        synth.allNotesOff(0, true);
        synth.allNotesOff(1, true);
    };
    
    eccLog.reset(new JsonlLogger(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                 .getChildFile("counterpoints_ecc_log.jsonl")));
    refreshMidiInputs();
    startTimer(50);
    addComponentListener(this);
    
}

void MainComponent::mouseDoubleClick(const juce::MouseEvent&)
{
    // Fullscreen handled by macOS
}

void MainComponent::handleFullscreenChange()
{
    if (pianoRoll)
    {
        pianoRoll->updateLayout(getLocalBounds());
        pianoRoll->repaint();
    }
}

void MainComponent::componentMovedOrResized(Component& component, bool wasMoved, bool wasResized)
{
    if (wasResized && &component == this)
    {
        static juce::Rectangle<int> lastBounds;
        auto currentBounds = getBounds();
        
        if (lastBounds.getWidth() > 0 && lastBounds.getHeight() > 0)
        {
            int widthChange = std::abs(currentBounds.getWidth() - lastBounds.getWidth());
            int heightChange = std::abs(currentBounds.getHeight() - lastBounds.getHeight());
            
            if (widthChange > 100 || heightChange > 100)
                handleFullscreenChange();
        }
        
        lastBounds = currentBounds;
    }
}


MainComponent::~MainComponent()
{
    stopTimer();
    
    try {
        synth.allNotesOff(0, true);
        synth.allNotesOff(1, true);
    } catch (...) {}
    
    if (midiManager) {
        try {
            midiManager->removeMidiInputCallback(this);
            midiManager->closeMidiInput();
            midiManager->closeVirtualOutput();
        } catch (...) {}
    }
    
    try {
        shutdownAudio();
    } catch (...) {}
    
    try {
        juce::Thread::getCurrentThread()->sleep(50);
    } catch (...) {}
    
    pianoRoll.reset();
    counterpointEngine.reset();
    midiManager.reset();
}

void MainComponent::paint(juce::Graphics& g)
{
    auto bg = juce::Colour::fromRGB(20, 20, 20);
    auto accent = juce::Colour::fromRGB(180, 180, 180);
    
    g.setColour(bg);
    g.fillAll();
    
    g.setColour(accent);
    g.drawRect(getLocalBounds(), 1);
    
    if (!analysisBoxBounds.isEmpty())
    {
        auto boxArea = analysisBoxBounds.toFloat();
        
        // Red glow for violations
        if (glowAlpha > 0.01f)
        {
            auto glowColor = juce::Colour::fromRGBA(255, 70, 70, (uint8_t)(glowAlpha * 255));
            juce::DropShadow glow(glowColor, 25, {});
            glow.drawForRectangle(g, analysisBoxBounds.expanded(6));
        }
        
        g.setColour(juce::Colour::fromRGB(35, 35, 35));
        g.fillRoundedRectangle(boxArea, 8.0f);
        
        g.setColour(juce::Colour::fromRGB(60, 60, 60));
        g.drawRoundedRectangle(boxArea, 8.0f, 1.0f);
        
        g.setColour(juce::Colours::grey.withAlpha(0.3f));
        g.drawLine(0, analysisBoxBounds.getBottom() + 8, getWidth(), analysisBoxBounds.getBottom() + 8, 1.0f);
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(20);
    const int spacing = 8;
    const int buttonHeight = 35;
    const int buttonWidth = 160;
    const int analysisHeight = juce::jmin(180, getHeight() / 5);
    const int afterButtonsGap = 28;
    const int gapAboveAnalysis = afterButtonsGap;
    const int gapBelowAnalysis = 15;
    const int topSectionHeight = 180;

    auto titleArea = area.removeFromTop(70);
    titleLabel.setBounds(titleArea.withSizeKeepingCentre(300, 40));
    
    const int subtitleGap = 8;
    const int subtitleToDropdown = 10;

    subtitleLabel.setBounds(
        titleLabel.getX(),
        titleLabel.getBottom() + subtitleGap,
        300,
        25
    );

    auto controlArea = area.removeFromTop(topSectionHeight);
    int centerX = getWidth() / 2;

    midiInputComboBox.setBounds(
        centerX - buttonWidth / 2,
        subtitleLabel.getBottom() + subtitleToDropdown,
        buttonWidth,
        buttonHeight
    );
    modeToggle.setBounds(centerX - buttonWidth / 2, midiInputComboBox.getBottom() + spacing, buttonWidth, buttonHeight);
    aboveBelowToggle.setBounds(centerX - buttonWidth / 2, modeToggle.getBottom() + spacing, buttonWidth, buttonHeight);
    resetPhraseButton.setBounds(centerX - buttonWidth / 2, aboveBelowToggle.getBottom() + spacing, buttonWidth, buttonHeight);

    area.removeFromTop(gapAboveAnalysis);

    auto analysisArea = area.removeFromTop(analysisHeight);
    analysisBoxBounds = analysisArea;
    eccPanel.setBounds(analysisBoxBounds);

    area.removeFromTop(gapBelowAnalysis);

    if (pianoRoll)
    {
        pianoRoll->setBounds(area);
        pianoRoll->updateLayout(area);
    }

    // Adjust piano roll note range based on window height
    int windowHeight = getHeight();
    int lowestNote = 36;
    int highestNote = 84;

    if (windowHeight < 700)
    {
        lowestNote = 48;
        highestNote = 84;
    }
    else if (windowHeight < 1000)
    {
        lowestNote = 36;
        highestNote = 84;
    }
    else
    {
        lowestNote = 24;
        highestNote = 96;
    }

    if (pianoRoll->getLowestNote() != lowestNote || pianoRoll->getHighestNote() != highestNote)
    {
        pianoRoll->setNoteRange(lowestNote, highestNote);
        pianoRoll->repaint();
    }
    
    if (pianoRoll)
        pianoRoll->updateLayout();
    
    static int lastWidth = 0, lastHeight = 0;
    int currentWidth = getWidth();
    int currentHeight = getHeight();
    
    if (abs(currentWidth - lastWidth) > 100 || abs(currentHeight - lastHeight) > 100)
    {
        handleFullscreenChange();
        lastWidth = currentWidth;
        lastHeight = currentHeight;
    }

    // Scale fonts with window height
    float scale = juce::jmap<float>(getHeight(), 700.0f, 1600.0f, 1.0f, 1.4f);
    titleLabel.setFont(juce::Font(28.0f * scale, juce::Font::bold));
    subtitleLabel.setFont(juce::Font(16.0f * scale, juce::Font::plain));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
}

void MainComponent::timerCallback()
{
    const float fadeSpeed  = 0.02f;
    const float pulseSpeed = 0.08f;
    const float minAlpha   = 0.2f;
    const float maxAlpha   = 0.5f;

    if (isAnimating)
    {
        buttonAlphaAnim.skip(1);
        aboveBelowToggle.setAlpha(buttonAlphaAnim.getCurrentValue());
        
        if (std::abs(buttonAlphaAnim.getCurrentValue() - buttonAlphaAnim.getTargetValue()) < 0.01f)
            isAnimating = false;
    }
    
    if (isDirectionAnimating)
    {
        directionToggleAnim.skip(1);
        
        float scale = 0.95f + 0.05f * directionToggleAnim.getCurrentValue();
        aboveBelowToggle.setTransform(juce::AffineTransform::scale(scale, scale));
        
        if (std::abs(directionToggleAnim.getCurrentValue() - directionToggleAnim.getTargetValue()) < 0.01f)
        {
            isDirectionAnimating = false;
            aboveBelowToggle.setTransform(juce::AffineTransform());
        }
    }

    if (hasViolation)
    {
        glowPhase += pulseSpeed;
        glowAlpha = minAlpha + (maxAlpha - minAlpha) * (0.5f + 0.5f * std::sin(glowPhase));
    }
    else if (isFadingOut)
    {
        glowAlpha = std::max(0.0f, glowAlpha - fadeSpeed);
        if (glowAlpha <= 0.0f)
        {
            glowAlpha = 0.0f;
            isFadingOut = false;
            // Only stop timer if we're not animating buttons
            if (!isAnimating && !isDirectionAnimating)
                stopTimer();
        }
    }

    repaint();
    
    // Auto-stop stuck notes after 5 seconds
    double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (now - lastNoteOnTime > 5.0)
    {
        synth.allNotesOff(1, true);
        lastNoteOnTime = now;
    }
}

void MainComponent::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& msg)
{
    processMidiMessage(msg);
}

void MainComponent::setupUI()
{
    setupMidiInputComboBox();
    setupButtons();
    setupLabels();
    
    addAndMakeVisible(midiInputComboBox);
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subtitleLabel);
    
    if (pianoRoll)
        addAndMakeVisible(pianoRoll.get());
}

void MainComponent::setupMidiInputComboBox()
{
    auto setupComboBox = [](juce::ComboBox& comboBox) {
        comboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff424242));
        comboBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe0e0e0));
        comboBox.setColour(juce::ComboBox::buttonColourId, juce::Colour(0xff666666));
        comboBox.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffe0e0e0));
        comboBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff666666));
        comboBox.setColour(juce::ComboBox::focusedOutlineColourId, juce::Colour(0xff1976D2));
    };
    
    setupComboBox(midiInputComboBox);
    
    midiInputComboBox.onChange = [this] { onMidiInputChanged(); };
    midiInputComboBox.setSelectedId(0);
}

void MainComponent::setupButtons()
{
    auto setupFlatToggle = [](juce::TextButton& button, const juce::String& text) {
        button.setButtonText(text);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(30, 30, 30));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(30, 30, 30));
        button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    };
    
    auto setupButton = [](juce::TextButton& button, const juce::String& text) {
        button.setButtonText(text);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(30, 30, 30));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(30, 30, 30));
        button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    };
    
    setupFlatToggle(modeToggle, "Tutor Mode");
    setupFlatToggle(aboveBelowToggle, "Generate Above");
    setupButton(resetPhraseButton, "Reset Phrase");
}

void MainComponent::setupLabels()
{
    midiDeviceLabel.setText("MIDI DEVICE", juce::dontSendNotification);
    midiDeviceLabel.setJustificationType(juce::Justification::centred);
    midiDeviceLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 180, 180));
    midiDeviceLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    
    titleLabel.setText("PolyMuse", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont(juce::Font(28.0f, juce::Font::bold));
    
    subtitleLabel.setText("Explainable Counterpoint Composer", juce::dontSendNotification);
    subtitleLabel.setJustificationType(juce::Justification::centred);
    subtitleLabel.setFont(juce::Font(16.0f, juce::Font::plain));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
}



void MainComponent::refreshMidiInputs()
{
    midiInputComboBox.clear();
    
    if (!midiManager)
    {
        return;
    }
    
    auto midiInputs = midiManager->getAvailableMidiInputs();
    
    midiInputComboBox.addItem("No MIDI Input", 1);
    
    for (int i = 0; i < midiInputs.size(); ++i)
    {
        midiInputComboBox.addItem(midiInputs[i], i + 2);
    }
    
    if (midiInputs.isEmpty())
        midiInputComboBox.setSelectedId(1);
    else
        midiInputComboBox.setSelectedId(0);
}

void MainComponent::selectMidiInput(int deviceIndex)
{
    if (!midiManager)
    {
        return;
    }
    
    midiManager->closeMidiInput();
    midiManager->removeMidiInputCallback(this);
    
    if (deviceIndex == 1)
        return;
    
    auto midiInputs = midiManager->getAvailableMidiInputs();
    int actualIndex = deviceIndex - 2;
    
    if (actualIndex >= 0 && actualIndex < midiInputs.size())
    {
        bool success = midiManager->openMidiInput(actualIndex);
        
        if (success)
        {
            midiManager->addMidiInputCallback(this);
        }
        else
        {
        }
    }
}

void MainComponent::enableMidiInput(bool enable)
{
    if (!midiManager)
    {
        return;
    }
    
    if (midiManager->isMidiInputOpen())
    {
        if (enable)
        {
            midiManager->addMidiInputCallback(this);
        }
        else
        {
            midiManager->removeMidiInputCallback(this);
        }
    }
}


void MainComponent::onMidiInputChanged()
{
    int selectedId = midiInputComboBox.getSelectedId();
    selectMidiInput(selectedId);
}





void MainComponent::processMidiMessage(const juce::MidiMessage& message)
{
    double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    
    if (message.isNoteOn())
    {
        lastNoteOnTime = now;
        
        const int inPitch = message.getNoteNumber();
        const float vel = message.getVelocity();
        
        if (pianoRoll)
            pianoRoll->noteOn(0, inPitch, vel);
        
        if (eccMode == ECCMode::Tutor)
        {
            activeNotes.insert(inPitch);
            
            if (activeNotes.size() == 2)
            {
                std::vector<int> notes(activeNotes.begin(), activeNotes.end());
                int lower = std::min(notes[0], notes[1]);
                int upper = std::max(notes[0], notes[1]);

                history.push_back({ lower, upper, now });
                if (history.size() > 64)
                    history.pop_front();

                std::vector<NotePair> histVec;
                for (const auto& pair : history)
                {
                    histVec.emplace_back(pair.input, pair.gen, pair.timeSec);
                }
                auto results = ruleChecker.evaluate(histVec, lower, upper, now);

                juce::String msgText = "Current interval: " +
                                       ruleChecker.intervalName(std::abs(upper - lower) % 12) + "\n";

                bool hasViolation = false;
                for (const auto& v : results)
                {
                    if (v.kind != ViolationKind::Other && v.kind != ViolationKind::Consonance)
                    {
                        hasViolation = true;
                        break;
                    }
                }

                juce::String fullText = "";
                
                if (hasViolation)
                {
                    juce::String violationType = "";
                    for (const auto& v : results)
                    {
                        if (v.kind != ViolationKind::Consonance)
                        {
                            switch (v.kind)
                            {
                                case ViolationKind::ParallelFifth:
                                    violationType = "Parallel 5th";
                                    break;
                                case ViolationKind::ParallelOctave:
                                    violationType = "Parallel octave";
                                    break;
                                case ViolationKind::DissonanceOnStrongBeat:
                                    violationType = "Dissonance";
                                    break;
                                case ViolationKind::HiddenFifthOctave:
                                    violationType = "Hidden fifth/octave";
                                    break;
                                case ViolationKind::VoiceCrossing:
                                    violationType = "Voice crossing";
                                    break;
                                case ViolationKind::LargeLeap:
                                    violationType = "Large leap";
                                    break;
                                case ViolationKind::DirectMotionToPerfect:
                                    violationType = "Direct motion to perfect interval";
                                    break;
                                case ViolationKind::RangeExceeded:
                                    violationType = "Range exceeded";
                                    break;
                                default:
                                    violationType = "Rule violation";
                            break;
                            }
                            break;
                        }
                    }
                    
                    fullText = "Violations detected: " + violationType + "\n" + msgText;
                }
                else
                {
                    fullText = msgText;
                }
                    
                updateAnalysisText(fullText, hasViolation);
            }
        }
        else if (eccMode == ECCMode::Generator && counterpointEngine)
        {
            auto gen = counterpointEngine->generateCounterpoint(message);
            int generatedPitch = gen.getNoteNumber();
            
            history.push_back({ inPitch, generatedPitch, now });
            if (history.size() > 64)
                history.pop_front();
            
            activeNoteMapping[inPitch] = generatedPitch;
            activeGeneratedNotes[inPitch] = generatedPitch;
            
            int interval = std::abs(generatedPitch - inPitch) % 12;
            updateAnalysisText("Current interval: " + ruleChecker.intervalName(interval), false);
            
            if (pianoRoll)
                pianoRoll->noteOn(1, gen.getNoteNumber(), vel);
            
            synth.noteOn(1, gen.getNoteNumber(), (juce::uint8)120);
        }
        
        synth.noteOn(0, inPitch, vel);
    }
    else if (message.isNoteOff())
    {
        int inputPitch = message.getNoteNumber();
        
        if (eccMode == ECCMode::Tutor)
        {
            activeNotes.erase(inputPitch);
            pianoRoll->noteOff(0, inputPitch);
        }
        else if (eccMode == ECCMode::Generator)
        {
            pianoRoll->noteOff(0, inputPitch);
            
            auto it = activeGeneratedNotes.find(inputPitch);
            if (it != activeGeneratedNotes.end())
            {
                int genNote = it->second;
                pianoRoll->noteOff(1, genNote);
                synth.noteOff(1, genNote, 0.0f, false);
                activeGeneratedNotes.erase(it);
            }
        }
        
        synth.noteOff(0, message.getNoteNumber(), 0.0f, false);
    }
    
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    if (sampleRate <= 0.0)
        return;
    
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void MainComponent::releaseResources()
{
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (!bufferToFill.buffer || bufferToFill.numSamples <= 0)
        return;
    
    juce::MidiBuffer midiBuffer;
    bufferToFill.buffer->clear();
    
    if (synth.getNumVoices() > 0)
    {
        try {
            synth.renderNextBlock(*bufferToFill.buffer, midiBuffer, 0, bufferToFill.numSamples);
        } catch (...) {
            bufferToFill.buffer->clear();
        }
    }
}

void MainComponent::updateAnalysisText(const juce::String& message, bool violation)
{
    hasViolation = violation;
    eccPanel.updateAnalysisText(message, false);

    if (hasViolation)
    {
        isFadingOut = false;
        startTimerHz(60);
    }
    else
    {
        isFadingOut = true;
        startTimerHz(60);
    }

    repaint();
}

