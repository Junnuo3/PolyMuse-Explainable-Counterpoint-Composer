#include "MidiManager.h"

MidiManager::MidiManager()
    : virtualOutputName("Counterpoint Out")
{
    updateMidiInputList();
}

MidiManager::~MidiManager()
{
    // Clear all callbacks first
    midiCallbacks.clear();
    
    // Close devices
    closeMidiInput();
    closeVirtualOutput();
}

void MidiManager::updateMidiInputList()
{
    availableMidiInputs = juce::MidiInput::getAvailableDevices();
}

juce::StringArray MidiManager::getAvailableMidiInputs() const
{
    juce::StringArray deviceNames;
    
    for (const auto& device : availableMidiInputs)
    {
        deviceNames.add(device.name);
    }
    
    return deviceNames;
}

bool MidiManager::openMidiInput(int deviceIndex)
{
    if (deviceIndex < 0 || deviceIndex >= availableMidiInputs.size())
    {
        return false;
    }
    
    return openMidiInput(availableMidiInputs[deviceIndex].name);
}

bool MidiManager::openMidiInput(const juce::String& deviceName)
{
    // Close current input if open
    closeMidiInput();
    
    // Find the device by name
    for (const auto& device : availableMidiInputs)
    {
        if (device.name == deviceName)
        {
            currentMidiInput = juce::MidiInput::openDevice(device.identifier, this);
            
            if (currentMidiInput)
            {
                currentMidiInput->start();
                currentMidiInputName = deviceName;
                return true;
            }
            break;
        }
    }
    
    return false;
}

void MidiManager::closeMidiInput()
{
    if (currentMidiInput)
    {
        try
        {
            currentMidiInput->stop();
        }
        catch (...)
        {
            // Ignore exceptions during stop
        }
        currentMidiInput.reset();
        currentMidiInputName = juce::String();
    }
}

bool MidiManager::isMidiInputOpen() const
{
    return currentMidiInput != nullptr;
}

juce::String MidiManager::getCurrentMidiInputName() const
{
    return currentMidiInputName;
}

bool MidiManager::createVirtualOutput()
{
    // Close existing virtual output if open
    closeVirtualOutput();
    
    // Create virtual MIDI output device
    virtualMidiOutput = juce::MidiOutput::createNewDevice(virtualOutputName);
    
    if (virtualMidiOutput)
    {
        return true;
    }
    
    return false;
}

void MidiManager::closeVirtualOutput()
{
    if (virtualMidiOutput)
    {
        virtualMidiOutput.reset();
    }
}

bool MidiManager::isVirtualOutputOpen() const
{
    return virtualMidiOutput != nullptr;
}

juce::String MidiManager::getVirtualOutputName() const
{
    return virtualOutputName;
}

void MidiManager::sendMidiMessage(const juce::MidiMessage& message)
{
    if (virtualMidiOutput)
    {
        virtualMidiOutput->sendMessageNow(message);
    }
}

void MidiManager::sendNoteOn(int channel, int noteNumber, float velocity)
{
    if (virtualMidiOutput)
    {
        juce::MidiMessage message = juce::MidiMessage::noteOn(channel, noteNumber, velocity);
        virtualMidiOutput->sendMessageNow(message);
    }
}

void MidiManager::sendNoteOff(int channel, int noteNumber, float velocity)
{
    if (virtualMidiOutput)
    {
        juce::MidiMessage message = juce::MidiMessage::noteOff(channel, noteNumber, velocity);
        virtualMidiOutput->sendMessageNow(message);
    }
}

void MidiManager::sendControlChange(int channel, int controllerNumber, int value)
{
    if (virtualMidiOutput)
    {
        juce::MidiMessage message = juce::MidiMessage::controllerEvent(channel, controllerNumber, value);
        virtualMidiOutput->sendMessageNow(message);
    }
}

void MidiManager::sendProgramChange(int channel, int programNumber)
{
    if (virtualMidiOutput)
    {
        juce::MidiMessage message = juce::MidiMessage::programChange(channel, programNumber);
        virtualMidiOutput->sendMessageNow(message);
    }
}

void MidiManager::addMidiInputCallback(juce::MidiInputCallback* callback)
{
    if (callback != nullptr && !midiCallbacks.contains(callback))
    {
        midiCallbacks.add(callback);
    }
}

void MidiManager::removeMidiInputCallback(juce::MidiInputCallback* callback)
{
    midiCallbacks.removeAllInstancesOf(callback);
}

void MidiManager::refreshMidiDevices()
{
    updateMidiInputList();
}

void MidiManager::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message)
{
    // Forward the message to all registered callbacks
    notifyMidiCallbacks(message);
}

void MidiManager::notifyMidiCallbacks(const juce::MidiMessage& message)
{
    for (auto* callback : midiCallbacks)
    {
        if (callback != nullptr)
        {
            callback->handleIncomingMidiMessage(currentMidiInput.get(), message);
        }
    }
}



