#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

#include "JuneChorus.h"

class JuneProcessor : public juce::AudioProcessor
{
public:
    JuneProcessor();
    ~JuneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void applyParameters();

    std::atomic<float>* pChorus1 = nullptr;
    std::atomic<float>* pChorus2 = nullptr;
    std::atomic<float>* pOutput  = nullptr;   // 0 = Mono, 1 = Stereo
    std::atomic<float>* pSpeeds  = nullptr;   // 0 = Juno-60, 1 = June-60 V1
    std::atomic<float>* pBypass  = nullptr;   // 0 = chorus active, 1 = bypassed

    june60::JuneChorus chorus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuneProcessor)
};
