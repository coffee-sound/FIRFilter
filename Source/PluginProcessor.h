/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class FIRFilterAudioProcessor  : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener, private juce::AsyncUpdater
{
public:
    //==============================================================================
    FIRFilterAudioProcessor();
    ~FIRFilterAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    const std::vector<float>& getFIRCoefficients() const { return firCoeffs; }
    
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();


    bool filterNeedsUpdate = true;

private:
    const int tapSize = 65;
    std::vector<float> firCoeffs;
    std::shared_ptr<std::vector<float>> firCoeffsPtr;
    std::shared_ptr<std::vector<float>> firCoeffsAtomic;
    std::vector<std::vector<float>> delayBuffers;  // delay sample buffer

    void updateFilter();

    juce::AudioParameterFloat* cutoffParam;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FIRFilterAudioProcessor)
};
