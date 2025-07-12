/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FIRFilterAudioProcessorEditor::FIRFilterAudioProcessorEditor(FIRFilterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), fft(fftOrder)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (600, 300);
    fftBuffer.resize(fftSize, 0.0f);
    magnitude.resize(fftSize / 2, 0.0f);
    startTimerHz(30);  // repaint every 30fps

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
}

FIRFilterAudioProcessorEditor::~FIRFilterAudioProcessorEditor()
{
}

//==============================================================================
void FIRFilterAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float width = bounds.getWidth();
    const float height = bounds.getHeight();

    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::lime);

    const float minFreq = 20.0f;
    const float maxFreq = 20000.0f;
    const float minDB = -60.0f;
    const float maxDB = 0.0f;

    const float fs = audioProcessor.getSampleRate();
    const int fftBins = magnitude.size();

    auto freqToX = [&](float freq) {
        float logMin = std::log10(minFreq);
        float logMax = std::log10(maxFreq);
        float logFreq = std::log10(juce::jlimit(minFreq, maxFreq, freq));
        return juce::jmap(logFreq, logMin, logMax, 0.0f, width);
    };

    auto dBToY = [&](float dB) {
        float clipped = juce::jlimit(minDB, maxDB, dB);
        return juce::jmap(clipped, minDB, maxDB, height, 0.0f);
    };

    // axis
    g.setColour(juce::Colours::darkgrey);
    juce::Array<float> freqsToDraw = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (auto freq : freqsToDraw) {
        float x = freqToX(freq);
        g.drawVerticalLine((int)x, 0.0f, height);
        g.drawText(juce::String(freq, 0) + "Hz", (int)x + 2, height - 20, 60, 15, juce::Justification::left);
    }

    for (int dB = -60; dB <= 0; dB += 10)
    {
        float y = dBToY((float)dB);
        g.drawHorizontalLine((int)y, 0.0f, width);
        g.drawText(juce::String(dB) + " dB", 2, (int)y - 8, 50, 16, juce::Justification::left);
    }

    // freq response
    juce::Path path;
    path.startNewSubPath(freqToX((float)20), dBToY(magnitude.at(1)));

    for (int i = 1; i < fftBins; ++i) {
        float freq = ((float)i / fftBins) * (fs / 2.0f);
        float x = freqToX(freq);
        float y = dBToY(magnitude.at(i));
        path.lineTo(x, y);
    }

    g.setColour(juce::Colours::lime);
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

void FIRFilterAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    cutoffSlider.setBounds(20, 20, getWidth() - 40, 30);
}

void FIRFilterAudioProcessorEditor::updateFFT() {

    fftBuffer.assign(fftSize, juce::dsp::Complex<float>{0.0f, 0.0f});

    const auto& coeffs = audioProcessor.getFIRCoefficients();
    for (int i = 0; i < coeffs.size(); ++i) {
        fftBuffer.at(i).real(coeffs[i]);
        fftBuffer.at(i).imag(0.0f);
    }

    //for (int i = 0; i < 10; ++i)
    //    DBG("Before FFT: fftBuffer[" << i << "] = ("
    //        << fftBuffer[i].real() << ", "
    //        << fftBuffer[i].imag() << ")");

    fft.perform(fftBuffer.data(), fftBuffer.data(), false);

    //for (int i = 0; i < 10; ++i)
    //    DBG("After FFT: fftBuffer[" << i << "] = ("
    //        << fftBuffer[i].real() << ", "
    //        << fftBuffer[i].imag() << ")");

    magnitude.resize(fftSize / 2);

    float maxMag = 0.0f;
    for (int i = 0; i < magnitude.size(); ++i) {
        float re = fftBuffer.at(i).real();
        float im = fftBuffer.at(i).imag();
        float mag = std::sqrt(re * re + im * im) / float(fftSize);
        magnitude.at(i) = mag;
        if (mag > maxMag) maxMag = mag;
    }

    if (maxMag > 1e-6f) {
        for (auto& m : magnitude) {
            m = juce::Decibels::gainToDecibels(m / maxMag, -100.0f);
        }
    }

    //DBG("magnitude[0] = " << magnitude[0]);
    //DBG("magnitude[1] = " << magnitude[1]);
    //DBG("magnitude[2] = " << magnitude[2]);

    //float sum = std::accumulate(coeffs.begin(), coeffs.end(), 0.0f);
    //DBG("FIR coeff sum = " << sum);


}

void FIRFilterAudioProcessorEditor::timerCallback() {
    updateFFT();
    repaint();
}

