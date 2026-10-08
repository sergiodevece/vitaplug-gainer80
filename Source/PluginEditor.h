#pragma once

#include "PluginProcessor.h"

class G80Knob final : public juce::Slider
{
public:
    G80Knob(juce::String label, bool isBass, const juce::Image& bodyImage,
            juce::Point<float> imageHub, float sourceMechanicalDiameter);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusLost(FocusChangeType) override;
    void valueChanged() override;

private:
    void setReadoutVisible(bool shouldBeVisible);
    juce::String formatReadout() const;

    juce::String caption;
    bool bassKnob = false;
    juce::Image body;
    juce::Point<float> bodyImageHub;
    float bodySourceMechanicalDiameter = 1.0f;
    bool readoutVisible = false;
};

class G80VuMeter final : public juce::Component, private juce::Timer
{
public:
    G80VuMeter(juce::String label, std::atomic<float>& source, std::atomic<uint64_t>& updateCounter,
               const juce::Image& housingImage);
    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;
    juce::String caption;
    std::atomic<float>& sourceDb;
    std::atomic<uint64_t>& sourceCounter;
    uint64_t lastCounter = 0;
    float displayedDb = -72.0f;
    juce::Image housing;
};

class G80FattyModeSwitch final : public juce::Slider
{
public:
    G80FattyModeSwitch(const juce::Image& baseImage, const juce::Image& upperLeverImage,
                        const juce::Image& lowerLeverImage);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override {}

private:
    juce::Image base;
    juce::Image upperLever;
    juce::Image lowerLever;
};

class VitaPlugGainer80AudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit VitaPlugGainer80AudioProcessorEditor(VitaPlugGainer80AudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    VitaPlugGainer80AudioProcessor& audioProcessor;
    juce::Image chassisImage;
    juce::Image inputVuImage;
    juce::Image outputVuImage;
    juce::Image gainKnobImage;
    juce::Image fattyKnobImage;
    juce::Image switchBaseImage;
    juce::Image switchUpperLeverImage;
    juce::Image switchLowerLeverImage;
    std::atomic<float>& inputVuSource;
    std::atomic<float>& outputVuSource;
    std::atomic<uint64_t>& vuUpdateCounter;
    G80Knob gainSlider { "GAIN", false, gainKnobImage, { 123.5f, 122.5f }, 184.0f };
    G80Knob bassSlider { "FATTY", true, fattyKnobImage, { 163.2f, 161.7f }, 244.0f };
    G80FattyModeSwitch fattyModeSwitch { switchBaseImage, switchUpperLeverImage, switchLowerLeverImage };
    G80VuMeter inputMeter { "INPUT", inputVuSource, vuUpdateCounter, inputVuImage };
    G80VuMeter outputMeter { "OUTPUT", outputVuSource, vuUpdateCounter, outputVuImage };
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    Attachment gainAttachment, bassAttachment, fattyModeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VitaPlugGainer80AudioProcessorEditor)
};
