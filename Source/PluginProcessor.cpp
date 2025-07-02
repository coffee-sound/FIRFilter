/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FIRFilterAudioProcessor::FIRFilterAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout()), filterNeedsUpdate(true)
{
    auto* param = apvts.getParameter("cutoff");
    cutoffParam = dynamic_cast<juce::AudioParameterFloat*>(param);
    apvts.addParameterListener("cutoff", this);
}

FIRFilterAudioProcessor::~FIRFilterAudioProcessor()
{
}

//==============================================================================
const juce::String FIRFilterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FIRFilterAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool FIRFilterAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool FIRFilterAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double FIRFilterAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FIRFilterAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int FIRFilterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void FIRFilterAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String FIRFilterAudioProcessor::getProgramName (int index)
{
    return {};
}

void FIRFilterAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void FIRFilterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    firCoeffs.resize(tapSize);

    updateFilter();

    // initialize delay buffer
    auto numChannels = getTotalNumInputChannels();
    delayBuffers.resize(numChannels);

    for (auto& buf : delayBuffers) {
        buf.resize(firCoeffs.size(), 0.0f);
    }

    auto initCoeffs = std::make_shared<std::vector<float>>(tapSize, 0.0f);
    std::atomic_store(&firCoeffsAtomic, initCoeffs);
}

void FIRFilterAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FIRFilterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void FIRFilterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (filterNeedsUpdate) {
        updateFilter();
        for (auto& buf : delayBuffers) {
            buf.assign(firCoeffs.size(), 0.0f);
        }
        filterNeedsUpdate = false;
    }

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    auto numSamples = buffer.getNumSamples();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);


    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        auto& delayBuffer = delayBuffers[channel];

        for (int n = 0; n < numSamples; ++n) {
            // shift input to delaybuffer
            for (int i = delayBuffer.size() - 1; i > 0; --i) {
                delayBuffer.at(i) = delayBuffer.at(i - 1);
            }
            delayBuffer.at(0) = channelData[n];

            // culc FIR
            // FIR: y[n] = \sum b[k] * x[n-k]
            float y = 0.0f;
            auto coeffs = std::atomic_load(&firCoeffsAtomic);
            for (int i = 0; i < coeffs->size(); ++i) {
                y += (*coeffs)[i] * delayBuffer.at(i);
            }

            channelData[n] = y;
        }
    }
}

//==============================================================================
bool FIRFilterAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* FIRFilterAudioProcessor::createEditor()
{
    return new FIRFilterAudioProcessorEditor (*this);
}

//==============================================================================
void FIRFilterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void FIRFilterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FIRFilterAudioProcessor();
}

void FIRFilterAudioProcessor::updateFilter()
{
    float cutoffHz = apvts.getRawParameterValue("cutoff")->load();
    double fs = getSampleRate();

    // LowpassFIR
    auto newCoeffs = std::make_shared<std::vector<float>>(tapSize);

    const int M = (tapSize - 1) / 2;

    // Hamming Window
    auto w_hamming = [=](int n) {
        return 0.54 + 0.46 * std::cos((2.0f * juce::MathConstants<float>::pi * n) / (tapSize - 1));
    };

    // calc FIR coefficients
    for (int i = -M; i <= M; ++i) {

        float normalized_cutoff = juce::jlimit(0.001f, 0.499f, cutoffHz / (float)fs);
        // ideal filter's impulse response
        float sinc = (i != 0)
            ? (normalized_cutoff / juce::MathConstants<float>::pi) * (std::sin(normalized_cutoff * i) / (normalized_cutoff * i))
            : (normalized_cutoff / juce::MathConstants<float>::pi);

        // shifting
        (*newCoeffs)[M + i] = sinc * w_hamming(i);
    }

    float sum = std::accumulate(newCoeffs->begin(), newCoeffs->end(), 0.0f);
    for (auto& c : *newCoeffs) c /= sum;

    std::atomic_store(&firCoeffsAtomic, newCoeffs);
}

void FIRFilterAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "cutoff") {
        triggerAsyncUpdate();
    }
}

void FIRFilterAudioProcessor::handleAsyncUpdate()
{
    filterNeedsUpdate = true;
}

juce::AudioProcessorValueTreeState::ParameterLayout FIRFilterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "cutoff",
        "Freq",
        juce::NormalisableRange<float>{ 100.0f, 10000.0f, 1.0f, 0.5f },
        1000.0f
    ));

    return layout;
}