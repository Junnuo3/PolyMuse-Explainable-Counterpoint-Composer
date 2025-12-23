#include "RuleChecker.h"
#include "CounterpointEngine.h"  // For NotePair definition
#include <cmath>

static juce::String intervalName(int semitones)
{
    semitones = (semitones % 12 + 12) % 12;
    switch (semitones)
    {
        case 0:  return "unison";
        case 1:  return "minor 2nd";
        case 2:  return "major 2nd";
        case 3:  return "minor 3rd";
        case 4:  return "major 3rd";
        case 5:  return "perfect 4th";
        case 6:  return "tritone";
        case 7:  return "perfect 5th";
        case 8:  return "minor 6th";
        case 9:  return "major 6th";
        case 10: return "minor 7th";
        case 11: return "major 7th";
        default: return "unknown";
    }
}

static int mod12(int x){ x%=12; if(x<0) x+=12; return x; }

bool RuleChecker::isPerfect(int s) const { s=std::abs(s)%12; return s==0 || s==7; }
bool RuleChecker::isConsonant(int s) const { s=std::abs(s)%12; return s==0||s==3||s==4||s==7||s==8||s==9; }

bool RuleChecker::isPerfectInterval(int semitones) const
{
    int interval = semitones % 12;
    return (interval == 0 || interval == 7); // Unison or fifth/octave equivalence
}

std::vector<Violation> RuleChecker::evaluate(const std::vector<ExplanationNotePair>& H,
                                             int inP, int genP, double t, bool inPhrase) const
{
    std::vector<Violation> out;

    // --- Dissonance Check ---
    if (!isConsonant(genP - inP))
    {
        int interval = std::abs(genP - inP) % 12;
        juce::String intervalName = ::intervalName(interval);
        out.push_back({ViolationKind::DissonanceOnStrongBeat, 1.0f, genP, 0, inP, t,
                       "Dissonant interval: " + intervalName + " is not allowed in strict counterpoint.",
                       "Use consonant intervals: unison, 3rd, 4th, 5th, 6th, or octave.", 0.7f});
    }

    // --- Historical Checks ---
    if (H.size() >= 1)
    {
        auto [prevIn, prevGen, prevT] = H.back();

        int prevInt = std::abs(prevGen - prevIn);
        int currInt = std::abs(genP - inP);
        int dirIn = (inP > prevIn) ? 1 : (inP < prevIn ? -1 : 0);
        int dirGen = (genP > prevGen) ? 1 : (genP < prevGen ? -1 : 0);

        // Parallel perfects
        if (isPerfect(prevInt) && isPerfect(currInt) && dirIn == dirGen && dirIn != 0)
        {
            juce::String prevIntervalName = ::intervalName(prevInt);
            juce::String currIntervalName = ::intervalName(currInt);
            
            // Determine if this is a parallel 5th or parallel octave
            ViolationKind violationKind;
            if ((prevInt == 0 || prevInt == 12) && (currInt == 0 || currInt == 12))
                violationKind = ViolationKind::ParallelOctave;
            else if ((prevInt == 7 || prevInt == 19) && (currInt == 7 || currInt == 19))
                violationKind = ViolationKind::ParallelFifth;
            else
                violationKind = ViolationKind::ParallelFifth; // Default fallback
            
            out.push_back({violationKind, 1.0f, genP, 0, inP, t,
                           "Parallel motion between perfect intervals: " + prevIntervalName + " → " + currIntervalName + ".",
                           "Avoid parallel 5ths/8ves; use contrary or oblique motion instead.", 0.9f});
        }

        // Hidden perfects
        if (!isPerfect(prevInt) && isPerfect(currInt) && dirIn == dirGen)
        {
            juce::String prevIntervalName = ::intervalName(prevInt);
            juce::String currIntervalName = ::intervalName(currInt);
            out.push_back({ViolationKind::HiddenFifthOctave, 1.0f, genP, 0, inP, t,
                           "Hidden/direct motion to perfect interval: " + prevIntervalName + " → " + currIntervalName + ".",
                           "Avoid approaching perfect intervals in similar motion; use contrary motion.", 0.6f});
        }
    }

    // --- Final note rules (if this is the last bar) ---
    if (!inPhrase && H.size() > 4)   // simple heuristic for phrase ending
    {
        if (!isPerfect(genP - inP))
            out.push_back({ViolationKind::Other, 1.0f, genP, 0, inP, t,
                           "Final sonority should be perfect (1 or 8).",
                           "End on a perfect consonance.", 0.5f});
    }

    return out;
}

std::vector<Violation> RuleChecker::evaluate(const std::vector<NotePair>& H,
                                             int inP, int genP, double t) const
{
    std::vector<Violation> out;

    int interval = std::abs(genP - inP) % 12;
    auto name = intervalName(interval);
    bool isConsonant = (interval == 0 || interval == 3 || interval == 4 ||
                        interval == 7 || interval == 8 || interval == 9);

    // Always push a result (so current interval always visible)
    if (isConsonant)
        out.push_back({ViolationKind::Consonance, 0.0f, genP, -1, inP, t,
                       "Consonant interval: " + name + " is acceptable.", "", 0.0f});
    else
        out.push_back({ViolationKind::DissonanceOnStrongBeat, 1.0f, genP, -1, inP, t,
                       "Dissonant interval: " + name + " is not allowed in strict counterpoint.", 
                       "Use consonant intervals: unison, 3rd, 4th, 5th, 6th, or octave.", 1.0f});

    // --- Parallel fifths/octaves check ---
    if (H.size() >= 2)
    {
        const NotePair& prev = H[H.size() - 2];

        int prevInt = std::abs(prev.generatedPitch - prev.inputPitch) % 12;
        int currInt = std::abs(genP - inP) % 12;

        int motionIn  = (inP  > prev.inputPitch)   ? 1 : (inP  < prev.inputPitch  ? -1 : 0);
        int motionGen = (genP > prev.generatedPitch) ? 1 : (genP < prev.generatedPitch ? -1 : 0);

        if ((prevInt == 7 || prevInt == 0) && (currInt == 7 || currInt == 0) &&
            motionIn == motionGen && motionIn != 0)
        {
            // Determine if this is a parallel 5th or parallel octave
            ViolationKind violationKind;
            if ((prevInt == 0 || prevInt == 12) && (currInt == 0 || currInt == 12))
                violationKind = ViolationKind::ParallelOctave;
            else if ((prevInt == 7 || prevInt == 19) && (currInt == 7 || currInt == 19))
                violationKind = ViolationKind::ParallelFifth;
            else
                violationKind = ViolationKind::ParallelFifth; // Default fallback
                
            out.push_back({
                violationKind,
                1.0f, genP, prev.generatedPitch, inP, t,
                "Parallel motion between perfect intervals (" +
                    intervalName(prevInt) + " → " + intervalName(currInt) + ").",
                "→ Avoid parallel 5ths/8ves; use contrary motion.", 0.9f
            });
        }
    }

    return out;
}

float RuleChecker::evaluateScore(const std::vector<NotePair>& H,
                                 int inP, int genP, double t) const
{
    auto v = evaluate(H, inP, genP, t);
    // Start from 1.0, subtract penalty for each violation severity
    float score = 1.0f;
    for (auto& x : v)
        score -= 0.3f * x.severity;  // weight violations
    return juce::jlimit(0.0f, 1.0f, score);
}

juce::String RuleChecker::intervalName(int semitones) const
{
    return ::intervalName(semitones);
}
