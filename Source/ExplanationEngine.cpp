#include "ExplanationEngine.h"
#include <algorithm>

ExplanationEngine::ExplanationEngine(){ model = ModelBridge::createMock(); }

static std::vector<int> candidateSet(int p){
    return std::vector<int>{ p-9, p-8, p-5, p-4, p-3, p+3, p+4, p+5, p+8, p+9 };
}

Rationale ExplanationEngine::explainChoice(const std::vector<ExplanationNotePair>& hist,
    const std::vector<ContextNote>& ctx, int inPitch, int genPitch,
    int keyRoot, bool isMajor, double nowSec, bool inPhrase)
{
    // 1) Rule violations
    auto v = rules.evaluate(hist, inPitch, genPitch, nowSec, inPhrase);

    // 2) Model probabilities & influences for this candidate
    auto rlist = model->scoreCandidates(ctx, candidateSet(inPitch), keyRoot, isMajor);
    Rationale chosen = {};
    for (auto& r : rlist) if (r.candidatePitch == genPitch) { chosen = r; break; }
    if (chosen.candidatePitch < 0 && !rlist.empty()) chosen = rlist[0];

    // 3) Augment text
    juce::String why = "Model favors consonant contrary motion; ";
    why += "context length=" + juce::String((int)ctx.size());
    chosen.summary = why;
    chosen.detail = "Top influences are most recent notes; diatonic bias applied.";

    for (auto& vi : v) chosen.triggeredRules.push_back(vi);

    // 4) Cheap occlusion: drop each influence and observe delta-prob (mocked)
    chosen = occlusionExplain(ctx, chosen, keyRoot, isMajor);
    return chosen;
}

Rationale ExplanationEngine::occlusionExplain(const std::vector<ContextNote>& ctx,
                                              const Rationale& base, int keyRoot, bool isMajor)
{
    Rationale r = base;
    if (ctx.size() < 2) return r;

    // Re-score by masking last-k notes to estimate importance
    const int K = std::min<int>(5, (int)ctx.size());
    for (int k=0; k<K; ++k) {
        std::vector<ContextNote> masked = ctx;
        // mask note k-from-end
        auto idx = (int)masked.size()-1-k;
        auto erased = masked[idx];
        masked.erase(masked.begin()+idx);
        auto rescored = model->scoreCandidates(masked, {r.candidatePitch}, keyRoot, isMajor);
        if (!rescored.empty()) {
            float delta = r.prob - rescored[0].prob; // how much prob drops when removing note
            // attach to influence matching erased pitch/time
            for (auto& inf : r.influences) {
                if (inf.pitch==erased.pitch && std::abs(inf.startSec-erased.startSec)<0.01) {
                    inf.weight = juce::jlimit(0.0f, 1.0f, inf.weight + delta);
                    break;
                }
            }
        }
    }
    return r;
}

std::vector<Violation> ExplanationEngine::evaluateRules(const std::vector<ExplanationNotePair>& history,
                                                       int inputPitch, int genPitch, 
                                                       double nowSec, bool inPhrase)
{
    return rules.evaluate(history, inputPitch, genPitch, nowSec, inPhrase);
}
