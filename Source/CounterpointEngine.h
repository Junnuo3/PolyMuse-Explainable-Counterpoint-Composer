#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <deque>
#include <unordered_map>
#include "RuleChecker.h"
#include "ModelBridge.h"

struct NotePair
{
    int inputPitch;
    int generatedPitch;
    double timestamp;
    
    NotePair(int input, int generated, double time) 
        : inputPitch(input), generatedPitch(generated), timestamp(time) {}
};

class CounterpointEngine {
public:
    CounterpointEngine();

    juce::MidiMessage generateCounterpoint(const juce::MidiMessage& userMsg);
    juce::MidiMessage noteOffForInput(int inputPitch);
    
    void setGenerateAbove(bool above) { generateAbove = above; }

private:
    int generateValidCounterpoint(int inputPitch, double now);
    int suggestAlternativeNote(int inputPitch, int rejectedPitch, double now);
    bool isTritone(int inputPitch, int generatedPitch) const;

    RuleChecker ruleChecker;
    std::unique_ptr<ModelBridge> model;
    std::deque<NotePair> history;
    std::unordered_map<int, int> activePairs;
    
    int lastGeneratedNote = -1;
    int lastInputNote = -1;
    bool generateAbove = true;
};