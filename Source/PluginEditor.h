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

    juce::Slider cutoffSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    FIRFilterAudioProcessor& audioProcessor;

    // FFT
    static constexpr int fftOrder = 10; // 1024-point FFT
    static constexpr int fftSize = 1 << fftOrder;

    juce::dsp::FFT fft;
    std::vector<juce::dsp::Complex<float>> fftBuffer;
    std::vector<float> magnitude;

    void timerCallback() override;
    void updateFFT();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FIRFilterAudioProcessorEditor)
};
