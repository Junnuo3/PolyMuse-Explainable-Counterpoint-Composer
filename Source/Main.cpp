#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "MainComponent.h"

class CounterpointsApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Counterpoints"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }

    void initialise (const juce::String&) override {
        mainWindow.reset(new MainWindow(getApplicationName()));
    }
    void shutdown() override { mainWindow = nullptr; }

private:
    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow(juce::String name)
            : juce::DocumentWindow(name,
                juce::Desktop::getInstance().getDefaultLookAndFeel()
                    .findColour(juce::ResizableWindow::backgroundColourId),
                allButtons)
        {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setContentOwned(new MainComponent(), true);
            centreWithSize(1000, 700);  // Larger window for better layout
            
            // Enable native macOS fullscreen
            setFullScreen(false);
            setResizeLimits(800, 600, 4000, 2500);
            setTitleBarButtonsRequired(DocumentWindow::allButtons, true);
            
            setVisible(true);
        }
        void closeButtonPressed() override {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(CounterpointsApplication)