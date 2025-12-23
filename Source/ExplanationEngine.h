#pragma once
#include <juce_core/juce_core.h>
#include "ECCTypes.h"
#include "RuleChecker.h"
#include "ModelBridge.h"

class ExplanationEngine {
public:
    ExplanationEngine();
    // Build rationale for a proposed generated note, given context
    Rationale explainChoice(const std::vector<ExplanationNotePair>& history,
                            const std::vector<ContextNote>& context,
                            int inputPitch, int genPitch,
                            int keyRoot, bool isMajor, double nowSec, bool inPhrase);
    
    // Direct rule evaluation for tutor mode
    std::vector<Violation> evaluateRules(const std::vector<ExplanationNotePair>& history,
                                        int inputPitch, int genPitch, 
                                        double nowSec, bool inPhrase);

private:
    RuleChecker rules;
    std::unique_ptr<ModelBridge> model;
    Rationale occlusionExplain(const std::vector<ContextNote>& context,
                               const Rationale& base, int keyRoot, bool isMajor);
};
