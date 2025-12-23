#pragma once
#include <juce_core/juce_core.h>
#include "ECCTypes.h"

struct ContextNote { int pitch; double startSec; double endSec; };

class ModelBridge {
public:
    virtual ~ModelBridge() = default;

    // Score K candidate pitches given context; returns rationales for each candidate.
    virtual std::vector<Rationale> scoreCandidates(
        const std::vector<ContextNote>& context,
        const std::vector<int>& candidatePitches,
        int keyRoot, bool isMajor) = 0;

    // Factory
    static std::unique_ptr<ModelBridge> createMock();
};
