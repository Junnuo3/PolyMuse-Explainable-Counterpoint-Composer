#include "ECCPanel.h"

ECCPanel::ECCPanel()
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(textBox);
    
    // Title label setup
    titleLabel.setText("Counterpoint Analysis", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::topLeft);
    titleLabel.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(35, 35, 35)); // Match box background
    titleLabel.setColour(juce::Label::outlineColourId, juce::Colour::fromRGB(35, 35, 35)); // No outline
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white); // Bold white title
    titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    
    // Text box setup
    textBox.setJustificationType(juce::Justification::topLeft);
    textBox.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(35, 35, 35)); // Match box background
    textBox.setColour(juce::Label::outlineColourId, juce::Colour::fromRGB(35, 35, 35)); // No outline
    textBox.setColour(juce::Label::textColourId, juce::Colours::white); // Default white text
    textBox.setFont(juce::Font(16.0f)); // Increased from 14pt to 16pt
    // Note: Line spacing is handled by the font size increase for better readability
}

void ECCPanel::setStatusText(const juce::String& text)
{
    juce::String clean = text.replace("â€¢", "•").replace("â", "");
    textBox.setText(clean, juce::dontSendNotification);
    
    // Default to white text
    textBox.setColour(juce::Label::textColourId, juce::Colours::white);
}

void ECCPanel::updateAnalysisText(const juce::String& message, bool hasViolation)
{
    textBox.setText(message, juce::dontSendNotification);
    textBox.setColour(juce::Label::textColourId,
        hasViolation ? juce::Colours::indianred : juce::Colours::white);
}

void ECCPanel::resized()
{
    // Title positioned at top-left of the box with padding
    titleLabel.setBounds(15, 10, 300, 30);
    
    // Text area below the title with consistent padding
    textBox.setBounds(15, 45, getWidth() - 30, 80);
}

void ECCPanel::paint(juce::Graphics& g)
{
    // No background or border - the main component draws the analysis box
    // The title and text are handled by the labels
}
