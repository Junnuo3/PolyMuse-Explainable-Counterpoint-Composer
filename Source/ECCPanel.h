#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ECCTypes.h"

class ECCPanel : public juce::Component {
public:
    ECCPanel();
    void setRationale(const Rationale& r){ rationale = r; repaint(); }
    void setStatusText(const juce::String& text);
    void updateAnalysisText(const juce::String& message, bool hasViolation);
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    Rationale rationale;
    juce::Label textBox;
    juce::Label titleLabel;
};
