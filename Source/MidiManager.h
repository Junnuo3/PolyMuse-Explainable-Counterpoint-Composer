#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>

/**
 * MidiManager class handles all MIDI device operations including:
 * - Listing available MIDI input devices
 * - Opening/closing MIDI input devices with callbacks
 * - Creating and managing virtual MIDI output device
 * - Sending MIDI messages through virtual output
 */
class MidiManager : public juce::MidiInputCallback
{
public:
    MidiManager();
    ~MidiManager();
    
    // MIDI Input Device Management
    juce::StringArray getAvailableMidiInputs() const;
    bool openMidiInput(int deviceIndex);
    bool openMidiInput(const juce::String& deviceName);
    void closeMidiInput();
    bool isMidiInputOpen() const;
    juce::String getCurrentMidiInputName() const;
    
    // MIDI Output Device Management
    bool createVirtualOutput();
    void closeVirtualOutput();
    bool isVirtualOutputOpen() const;
    juce::String getVirtualOutputName() const;
    
    // MIDI Message Handling
    void sendMidiMessage(const juce::MidiMessage& message);
    void sendNoteOn(int channel, int noteNumber, float velocity);
    void sendNoteOff(int channel, int noteNumber, float velocity);
    void sendControlChange(int channel, int controllerNumber, int value);
    void sendProgramChange(int channel, int programNumber);
    
    // Callback Management
    void addMidiInputCallback(juce::MidiInputCallback* callback);
    void removeMidiInputCallback(juce::MidiInputCallback* callback);
    
    // Device Refresh
    void refreshMidiDevices();
    
    // MIDI Input Callback Implementation
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

private:
    // MIDI Input Management
    std::unique_ptr<juce::MidiInput> currentMidiInput;
    juce::String currentMidiInputName;
    juce::Array<juce::MidiInputCallback*> midiCallbacks;
    
    // MIDI Output Management
    std::unique_ptr<juce::MidiOutput> virtualMidiOutput;
    juce::String virtualOutputName;
    
    // Device Lists
    juce::Array<juce::MidiDeviceInfo> availableMidiInputs;
    
    // Internal Methods
    void updateMidiInputList();
    void notifyMidiCallbacks(const juce::MidiMessage& message);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiManager)
};