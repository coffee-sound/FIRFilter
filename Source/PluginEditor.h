/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class FIRFilterAudioProcessorEditor  : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    FIRFilterAudioProcessorEditor (FIRFilterAudioProcessor&);
    ~FIRFilterAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateFFT();

    juce::Slider cutoffSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FIRFilterAudioProcessor& audioProcessor;

    juce::dsp::FFT fft{ 10 }; // 1024 point
    std::vector<float> fftBuffer;
    std::vector<float> magnitude;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FIRFilterAudioProcessorEditor)
};
