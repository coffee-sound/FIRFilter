/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FIRFilterAudioProcessorEditor::FIRFilterAudioProcessorEditor(FIRFilterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
    fftBuffer(2 * 1024, 0.0f), magnitude(1024 / 2, 0.0f)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (600, 300);
    startTimerHz(30);

    cutoffSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    cutoffSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(cutoffSlider);
    cutoffSlider.setRange(100.0f, 10000.0f, 1.0f);
    cutoffSlider.setSkewFactorFromMidPoint(1000.0f);

    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts,
        "cutoff",
        cutoffSlider
    );

    setSize(200, 200);
}

FIRFilterAudioProcessorEditor::~FIRFilterAudioProcessorEditor()
{
}

//==============================================================================
void FIRFilterAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::lime);

    juce::Path path;
    path.startNewSubPath(0, getHeight());

    for (size_t i = 1; i < magnitude.size(); ++i) {
        float x = getWidth() * (i / (float)magnitude.size());
        float y = juce::jmap(magnitude.at(i), -80.0f, 0.0f, (float)getHeight(), 0.0f);
        path.lineTo(x, y);
    }

    g.strokePath(path, juce::PathStrokeType(2.0f));
}

void FIRFilterAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    cutoffSlider.setBounds(20, 20, getWidth() - 40, 30);
}

void FIRFilterAudioProcessorEditor::updateFFT() {
    auto coeffs = audioProcessor.getFIRCoefficients();

    // Zero padding
    std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
    std::copy(coeffs.begin(), coeffs.end(), fftBuffer.begin());

    fft.performRealOnlyForwardTransform(fftBuffer.data());

    for (int i = 0; i < magnitude.size(); ++i) {
        float re = fftBuffer.at(2 * i);
        float im = fftBuffer.at(2 * i + 1);
        float mag = std::sqrt(re * re + im * im);
        magnitude[i] = juce::Decibels::gainToDecibels(mag, -80.0f);
    }
}

void FIRFilterAudioProcessorEditor::timerCallback() {
    updateFFT();
    repaint();
}

