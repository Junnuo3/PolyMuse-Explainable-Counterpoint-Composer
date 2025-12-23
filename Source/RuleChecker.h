#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include "ECCTypes.h"

// Forward declaration
struct NotePair;

class RuleChecker {
public:
    // Given last two note pairs and the new candidate (inputPitch -> genPitch), return violations.
    std::vector<Violation> evaluate(const std::vector<ExplanationNotePair>& history,
                                    int inputPitch, int genPitch, double nowSec, bool inPhrase) const;
    
    // Overload for NotePair history (for CounterpointEngine compatibility)
    std::vector<Violation> evaluate(const std::vector<NotePair>& history,
                                    int inputPitch, int genPitch, double nowSec) const;
    
    // Evaluate score for NotePair history (for CounterpointEngine compatibility)
    float evaluateScore(const std::vector<NotePair>& history,
                        int inputPitch, int candidatePitch, double nowSec) const;
    
    // Get interval name from semitones
    juce::String intervalName(int semitones) const;

private:
    bool isPerfect(int semis) const;
    bool isConsonant(int semis) const;
    bool isPerfectInterval(int semitones) const;
};
