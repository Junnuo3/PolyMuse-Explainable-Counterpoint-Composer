#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>

struct SineSound : public juce::SynthesiserSound {
  bool appliesToNote (int) override { return true; }
  bool appliesToChannel (int) override { return true; }
};

struct SineVoice : public juce::SynthesiserVoice {
  double currentAngle = 0, angleDelta = 0, level = 0, tailOff = 0;
  
  static constexpr int numHarmonics = 6; // Piano-like harmonics with overtones
  double harmonicAngles[numHarmonics] = {0};
  double harmonicDeltas[numHarmonics] = {0};
  double harmonicAmplitudes[numHarmonics] = {1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125};
  
  double envelopeLevel = 0.0; // Envelope for smooth note transitions
  double fadeInSamples = 0.0;
  double fadeOutSamples = 0.0;
  double currentFadeInSample = 0.0;
  double currentFadeOutSample = 0.0;
  bool isFadingIn = false;
  bool isFadingOut = false;
  bool isReleased = false;
  
  void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override {
    double cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
    double sampleRate = getSampleRate();
    
    if (sampleRate <= 0.0) {
      DBG("SineVoice::startNote: Invalid sample rate: " << sampleRate);
      return;
    }
    
    double cyclesPerSample = cyclesPerSecond / sampleRate;
    angleDelta = cyclesPerSample * juce::MathConstants<double>::twoPi;
    level = juce::jlimit (0.0f, 1.0f, velocity / 127.0f) * 0.12f;
    
    for (int i = 0; i < numHarmonics; ++i) {
      harmonicAngles[i] = 0;
      harmonicDeltas[i] = angleDelta * (i + 1);
    }
    
    fadeInSamples = sampleRate * 0.005; // 5ms fade-in
    fadeOutSamples = sampleRate * 0.05; // 50ms fade-out
    if (fadeInSamples <= 0.0) fadeInSamples = 1.0;
    if (fadeOutSamples <= 0.0) fadeOutSamples = 1.0;
    
    currentFadeInSample = 0.0;
    currentFadeOutSample = 0.0;
    envelopeLevel = 0.0;
    isFadingIn = true;
    isFadingOut = false;
    isReleased = false;
    currentAngle = 0.0;
    tailOff = 0.0;
  }
  
  void stopNote (float, bool allowTailOff) override {
    if (allowTailOff) {
      isFadingOut = true;
      isFadingIn = false;
      isReleased = true;
      currentFadeOutSample = 0.0;
      tailOff = 0.0;
    } else {
      clearCurrentNote();
      angleDelta = 0;
      isFadingIn = false;
      isFadingOut = false;
      isReleased = false;
    }
  }
  
  void pitchWheelMoved (int) override {}
  void controllerMoved (int, int) override {}
  bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SineSound*>(s) != nullptr; }
  
  void renderNextBlock (juce::AudioBuffer<float>& output, int start, int num) override {
    if (angleDelta == 0) return;
    if (num <= 0) return;
    
    auto* left  = output.getWritePointer (0, start);
    auto* right = output.getNumChannels() > 1 ? output.getWritePointer (1, start) : nullptr;
    if (!left) return;
    
    while (num--) {
      if (isFadingIn) {
        currentFadeInSample++;
        if (currentFadeInSample >= fadeInSamples) {
          envelopeLevel = 1.0;
          isFadingIn = false;
        } else {
          if (fadeInSamples > 0.0) {
            double progress = currentFadeInSample / fadeInSamples;
            envelopeLevel = juce::jlimit(0.0, 1.0, 1.0 - std::exp(-progress * 6.0));
          } else {
            envelopeLevel = 1.0;
          }
        }
      } else if (isFadingOut) {
        currentFadeOutSample++;
        if (currentFadeOutSample >= fadeOutSamples) {
          clearCurrentNote();
          angleDelta = 0;
          break;
        } else {
          if (fadeOutSamples > 0.0) {
            double progress = currentFadeOutSample / fadeOutSamples;
            envelopeLevel = juce::jlimit(0.0, 1.0, std::exp(-progress * 6.0));
          } else {
            envelopeLevel = 0.0;
          }
        }
      } else {
        envelopeLevel = 1.0;
      }
      
      float sample = 0.0f;
      for (int i = 0; i < numHarmonics; ++i) {
        sample += (float) (std::sin (harmonicAngles[i]) * level * harmonicAmplitudes[i] * envelopeLevel);
        harmonicAngles[i] += harmonicDeltas[i];
      }
      
      if (std::isnan(sample) || std::isinf(sample)) {
        sample = 0.0f;
      }
      
      if (right) { *left++ += sample; *right++ += sample; } else { *left++ += sample; }
      currentAngle += angleDelta;
    }
  }
};
