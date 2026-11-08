#include "PluginProcessor.h"
#include "PluginEditor.h"

JuneProcessor::JuneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pChorus1 = apvts.getRawParameterValue ("chorus1");
    pChorus2 = apvts.getRawParameterValue ("chorus2");
    pOutput  = apvts.getRawParameterValue ("output");
    pSpeeds  = apvts.getRawParameterValue ("speeds");
    pBypass  = apvts.getRawParameterValue ("bypass");
}

juce::AudioProcessorValueTreeState::ParameterLayout JuneProcessor::createLayout()
{
    using namespace juce;

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "chorus1", 1 }, "Chorus I", true));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "chorus2", 1 }, "Chorus II", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "output", 1 }, "Output",
                                                        StringArray { "Mono", "Stereo" }, 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "speeds", 1 }, "Speed Set",
                                                        StringArray { "Juno-60", "June-60 V1" }, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

void JuneProcessor::applyParameters()
{
    // "False bypass": the signal keeps running through the unit (including the
    // mono sum), only the chorus fades out, so switching is click-free.
    const bool active = pBypass->load() < 0.5f;

    chorus.setMode (active && pChorus1->load() > 0.5f,
                    active && pChorus2->load() > 0.5f,
                    pOutput->load()  > 0.5f,
                    pSpeeds->load()  > 0.5f);
}

void JuneProcessor::prepareToPlay (double sampleRate, int)
{
    chorus.prepare (sampleRate);
    applyParameters();
    chorus.snap();
}

bool JuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    const bool inOk  = (in  == juce::AudioChannelSet::mono() || in  == juce::AudioChannelSet::stereo());
    const bool outOk = (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
    return inOk && outOk;
}

void JuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    applyParameters();

    // Like the pedal, the chorus has a mono input: stereo input is summed.
    float* left = buffer.getWritePointer (0);
    float* right = (numOut > 1) ? buffer.getWritePointer (1) : nullptr;
    const float* inRight = (numIn > 1) ? buffer.getReadPointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float in = (inRight != nullptr) ? 0.5f * (left[i] + inRight[i]) : left[i];

        float outL = 0.0f, outR = 0.0f;
        chorus.process (in, outL, outR);

        if (right != nullptr)
        {
            left[i] = outL;
            right[i] = outR;
        }
        else
        {
            left[i] = 0.5f * (outL + outR);
        }
    }
}

juce::AudioProcessorEditor* JuneProcessor::createEditor()
{
    return new JuneEditor (*this);
}

void JuneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void JuneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JuneProcessor();
}
