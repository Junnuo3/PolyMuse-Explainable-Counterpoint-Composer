#include "ModelBridge.h"
#include <random>

class MockModel : public ModelBridge {
public:
    std::vector<Rationale> scoreCandidates(const std::vector<ContextNote>& ctx,
        const std::vector<int>& candidates, int key, bool major) override
    {
        std::mt19937 rng{ 12345u + (unsigned)ctx.size() + (unsigned)candidates.size() };
        std::uniform_real_distribution<float> U(0.05f, 0.95f);
        std::vector<Rationale> out;
        // Simple "attention": recent notes get higher weights
        double latest = ctx.empty()? 0.0 : ctx.back().endSec;
        for (int c : candidates) {
            Rationale r; r.candidatePitch = c; r.prob = U(rng);
            r.summary = "Mock: favors recent context and diatonic steps.";
            // influences
            for (int i = (int)ctx.size()-1, added=0; i>=0 && added<5; --i,++added) {
                float w = 1.0f - float(added) * 0.18f; if (w<0) w = 0;
                r.influences.push_back({ ctx[i].pitch, ctx[i].startSec, ctx[i].endSec, w });
            }
            out.push_back(std::move(r));
        }
        return out;
    }
};

std::unique_ptr<ModelBridge> ModelBridge::createMock(){ return std::make_unique<MockModel>(); }
