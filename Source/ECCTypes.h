#pragma once
#include <juce_core/juce_core.h>
#include <vector>

enum class ViolationKind {
    ParallelFifth, ParallelOctave, VoiceCrossing, LargeLeap, DissonanceOnStrongBeat,
    HiddenFifthOctave, DirectMotionToPerfect, RangeExceeded, Consonance, Other
};

struct Violation {
    ViolationKind kind;
    float severity;
    int noteGen;
    int notePrev;
    int noteInput;
    double time;
    juce::String description;
    juce::String suggestion;
    float weight;
};

struct Influence {
    int pitch;                // MIDI pitch in context
    double startSec;
    double endSec;
    float weight;             // attention / importance 0..1
};

struct Rationale {
    // Why did the system choose (or reject) a note?
    int candidatePitch = -1;
    float prob = 0.0f;        // model probability if available
    juce::String summary;     // one-line textual reason
    juce::String detail;      // longer explanation
    std::vector<Influence> influences;  // what in the context mattered
    std::vector<Violation> triggeredRules; // rules at play
};

// NotePair for explanation system (includes time)
struct ExplanationNotePair { 
    int input; 
    int gen; 
    double timeSec; 
    
    ExplanationNotePair(int i, int g, double t) : input(i), gen(g), timeSec(t) {}
};
