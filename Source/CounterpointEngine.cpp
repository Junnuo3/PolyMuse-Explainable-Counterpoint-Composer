#include "CounterpointEngine.h"

CounterpointEngine::CounterpointEngine() {
    model = ModelBridge::createMock();
}

juce::MidiMessage CounterpointEngine::generateCounterpoint(const juce::MidiMessage& userMsg)
{
    const int inPitch = userMsg.getNoteNumber();
    double now = juce::Time::getMillisecondCounterHiRes() * 0.001;

    int validPitch = generateValidCounterpoint(inPitch, now);
    
    activePairs[inPitch] = validPitch;
    int interval = std::abs(validPitch - inPitch) % 12;
    std::cout << "Generated valid counterpoint: input=" << inPitch << " -> generated=" << validPitch 
              << " (interval: " << ruleChecker.intervalName(interval).toStdString() << ")" << std::endl;
    std::cout << "activePairs size after generation=" << activePairs.size() << std::endl;

    history.push_back({ inPitch, validPitch, now });
    if (history.size() > 32) history.pop_front();

    return juce::MidiMessage::noteOn(1, validPitch, userMsg.getVelocity());
}

juce::MidiMessage CounterpointEngine::noteOffForInput(int inputPitch)
{
    std::cout << "noteOffForInput called with inputPitch=" << inputPitch << std::endl;
    std::cout << "activePairs size=" << activePairs.size() << std::endl;
    for (const auto& pair : activePairs)
    {
        std::cout << "  activePairs[" << pair.first << "] = " << pair.second << std::endl;
    }
    
    auto it = activePairs.find(inputPitch);
    if (it == activePairs.end())
    {
        std::cout << "No mapping found for inputPitch=" << inputPitch << std::endl;
        return juce::MidiMessage();
    }

    int genPitch = it->second;
    activePairs.erase(it);
    
    std::cout << "Found mapping: input=" << inputPitch << " -> generated=" << genPitch << std::endl;

    return juce::MidiMessage::noteOff(1, genPitch);
}

bool CounterpointEngine::isTritone(int inputPitch, int generatedPitch) const
{
    int interval = std::abs(generatedPitch - inputPitch) % 12;
    return (interval == 6);
}

int CounterpointEngine::generateValidCounterpoint(int inputPitch, double now)
{
    struct IntervalOption {
        int semitones;
        float weight;
    };

    static std::vector<IntervalOption> consonantIntervals = {
        {3, 0.25f},
        {4, 0.25f},
        {7, 0.10f},
        {8, 0.20f},
        {9, 0.15f},
        {12, 0.05f}
    };

    auto chooseWeightedInterval = [&]() -> int {
        float total = 0.f;
        for (auto& i : consonantIntervals)
            total += i.weight;

        float r = juce::Random::getSystemRandom().nextFloat() * total;
        for (auto& i : consonantIntervals)
        {
            if (r < i.weight) return i.semitones;
            r -= i.weight;
        }
        return consonantIntervals[0].semitones;
    };

    int motion = 0;
    if (lastInputNote != -1)
    {
        if (inputPitch > lastInputNote) motion = -1;
        else if (inputPitch < lastInputNote) motion = 1;
        else motion = juce::Random::getSystemRandom().nextBool() ? 1 : -1;
    }
    else motion = juce::Random::getSystemRandom().nextBool() ? 1 : -1;

    int genNote = inputPitch;
    int tries = 0;
    
    while (tries < 8)
    {
        bool valid = true;
        int interval = chooseWeightedInterval();
        
        if (generateAbove)
            genNote = inputPitch + interval;
        else
            genNote = inputPitch - interval;
        
        if (genNote < 24 || genNote > 96)
        {
            if (generateAbove && genNote > 96)
                genNote = inputPitch + interval - 12;
            else if (!generateAbove && genNote < 24)
                genNote = inputPitch - interval + 12;
            
            if (genNote < 24 || genNote > 96)
            {
                if (generateAbove)
                    genNote = inputPitch - interval;
                else
                    genNote = inputPitch + interval;
            }
        }

        if (isTritone(inputPitch, genNote))
        {
            valid = false;
            std::cout << "🚫 Tritone interval detected, retrying... (attempt " << (tries + 1) << ")" << std::endl;
        }
        
        if (valid && lastGeneratedNote != -1 && lastInputNote != -1)
        {
            int currentInterval = std::abs(genNote - inputPitch) % 12;
            int prevInterval = std::abs(lastGeneratedNote - lastInputNote) % 12;

            bool sameDirection =
                ((genNote > lastGeneratedNote && inputPitch > lastInputNote) ||
                 (genNote < lastGeneratedNote && inputPitch < lastInputNote));

            if (sameDirection &&
                ((prevInterval == 7 && currentInterval == 7) ||
                 (prevInterval == 0 && currentInterval == 0)))
            {
                valid = false;
                std::cout << "🚫 Parallel perfect interval detected, retrying... (attempt " << (tries + 1) << ")" << std::endl;
            }
        }

        if (valid) break;

        tries++;
        motion = juce::Random::getSystemRandom().nextBool() ? 1 : -1;
    }

    const int minSeparation = 3;

    if (generateAbove)
    {
        if (genNote <= inputPitch + minSeparation)
        {
            genNote = inputPitch + 7;
            std::cout << "🔧 Voice crossing prevention: pushed above to " << genNote << std::endl;
        }
    }
    else
    {
        if (genNote >= inputPitch - minSeparation)
        {
            genNote = inputPitch - 7;
            std::cout << "🔧 Voice crossing prevention: pushed below to " << genNote << std::endl;
        }
    }

    genNote = juce::jlimit(36, 84, genNote);
    
    if (isTritone(inputPitch, genNote))
    {
        std::cout << "🚫 Final tritone check failed, using fallback" << std::endl;
        if (generateAbove)
            genNote = inputPitch + 4;
        else
            genNote = inputPitch - 4;
        
        genNote = juce::jlimit(36, 84, genNote);
    }

    std::cout << "🎵 Generated note: " << genNote << " (interval=" << std::abs(genNote - inputPitch) % 12 
              << ", direction=" << (generateAbove ? "above" : "below") << ", attempts=" << (tries + 1) << ")" << std::endl;

    history.push_back({inputPitch, genNote, now});
    
    if (history.size() > 50)
        history.erase(history.begin(), history.begin() + 10);

    return genNote;
}

int CounterpointEngine::suggestAlternativeNote(int inputPitch, int rejectedPitch, double now)
{
    std::vector<std::pair<int, float>> consonantIntervals = {
        {3, 1.0f}, {4, 1.0f}, {8, 0.9f}, {9, 0.9f}, {12, 0.8f}, {7, 0.6f}, {0, 0.5f}
    };
    
    std::vector<std::pair<int, float>> alternatives;
    
    for (const auto& [interval, weight] : consonantIntervals) {
        int above = inputPitch + interval;
        int below = inputPitch - interval;
        if (above >= 24 && above <= 96) alternatives.push_back({above, weight});
        if (below >= 24 && below <= 96) alternatives.push_back({below, weight});
    }
    
    if (!history.empty()) {
        const NotePair& lastPair = history.back();
        int lastGenPitch = lastPair.generatedPitch;
        for (int step = 1; step <= 2; ++step) {
            int stepUp = lastGenPitch + step;
            int stepDown = lastGenPitch - step;
            if (stepUp >= 24 && stepUp <= 96) alternatives.push_back({stepUp, 0.7f});
            if (stepDown >= 24 && stepDown <= 96) alternatives.push_back({stepDown, 0.7f});
        }
    }
    
    float bestScore = -1.0f;
    int bestAlternative = inputPitch;
    
    for (const auto& [alt, baseWeight] : alternatives) {
        if (alt == rejectedPitch) continue;
        if (isTritone(inputPitch, alt)) continue;
        
        float ruleScore = ruleChecker.evaluateScore(
            std::vector<NotePair>(history.begin(), history.end()), inputPitch, alt, now);
        auto rationale = model->scoreCandidates({}, {alt}, 0, true);
        float prob = rationale.empty() ? 0.5f : rationale[0].prob;
        float combined = (ruleScore * prob * baseWeight);
        
        if (combined > bestScore)
        {
            bestScore = combined;
            bestAlternative = alt;
        }
    }
    
    std::cout << "Suggested alternative: " << bestAlternative << " (score: " << bestScore << ")" << std::endl;
    return bestAlternative;
}