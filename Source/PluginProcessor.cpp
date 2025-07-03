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

    param = apvts.getParameter("type");
    filterTypeParam = dynamic_cast<juce::AudioParameterChoice*>(param);

    apvts.addParameterListener("cutoff", this);
    apvts.addParameterListener("type", this);
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
            for (int i = 0; i < firCoeffs.size(); ++i) {
                y += firCoeffs.at(i) * delayBuffer.at(i);
            }

            // Soft clipping
            float limit = 0.95f;
            channelData[n] = juce::jlimit(-limit, limit, y);
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

    // cutoff normalization
    float normalized_cutoff = juce::jlimit(0.001f, 0.499f, cutoffHz / (float)fs);

    const int M = (tapSize - 1) / 2;

    // LowpassFIR
    // Hamming Window
    auto w_hamming = [=](int n) {
        return 0.54 + 0.46 * std::cos((2.0f * juce::MathConstants<float>::pi * n) / (tapSize - 1));
    };

    int filtertype = apvts.getRawParameterValue("type")->load();

    if (filtertype == 0) {
        // calc FIR coefficients
        for (int i = -M; i <= M; ++i) {

            // ideal filter's impulse response
            float sinc = (i != 0)
                ? (normalized_cutoff / juce::MathConstants<float>::pi) * (std::sin(normalized_cutoff * i) / (normalized_cutoff * i))
                : (normalized_cutoff / juce::MathConstants<float>::pi);

            // shifting
            firCoeffs.at(M + i) = sinc * w_hamming(i);
        }

        // normalize dB
        float sum = std::accumulate(firCoeffs.begin(), firCoeffs.end(), 0.0f);

        if (sum > 1e-6f) {
            for (auto& c : firCoeffs)
                c /= sum;
        }

    }
    else {
        // Highpass
        for (int i = -M; i <= M; ++i) {

            float sinc = (i != 0)
                ? -(normalized_cutoff / juce::MathConstants<float>::pi) * (std::sin(normalized_cutoff * i) / (normalized_cutoff * i))
                : 1.0f - (normalized_cutoff / juce::MathConstants<float>::pi);

            firCoeffs.at(M + i) = sinc * w_hamming(i);
        }

        // normalize dB
        float energy = std::sqrt(std::inner_product(
            firCoeffs.begin(), firCoeffs.end(), firCoeffs.begin(), 0.0f));

        if (energy > 1e-6f) {
            for (auto& c : firCoeffs)
                c /= energy;
        }
    }

    for (auto& buf : delayBuffers) {
        buf.assign(firCoeffs.size(), 0.0f);
    }

    //DBG("sum = " << sum);
    //DBG("firCoeffs:");
    //for (auto c : firCoeffs) DBG(c);
}

void FIRFilterAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "cutoff" || parameterID == "type") {
        filterNeedsUpdate = true;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout FIRFilterAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "cutoff",
        "Freq",
        juce::NormalisableRange<float>{ 50.0f, 20000.0f, 1.0f, 0.5f },
        1000.0f
    ));


    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "type",
        "Type",
        juce::StringArray{ "LowPass", "HighPass" },
        0
    ));

    return layout;
}