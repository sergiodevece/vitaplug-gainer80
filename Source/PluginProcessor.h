#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "DSP/Gainer80DSP.h"

class VitaPlugGainer80AudioProcessor final : public juce::AudioProcessor
{
public:
    VitaPlugGainer80AudioProcessor();
    ~VitaPlugGainer80AudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    float getInputVuDb() const noexcept { return inputVuDb.load(std::memory_order_relaxed); }
    float getOutputVuDb() const noexcept { return outputVuDb.load(std::memory_order_relaxed); }
    std::atomic<float>& getInputVuSource() noexcept { return inputVuDb; }
    std::atomic<float>& getOutputVuSource() noexcept { return outputVuDb; }
    std::atomic<uint64_t>& getVuUpdateCounter() noexcept { return vuUpdateCounter; }

    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    vitaplug::gainer80::dsp::Gainer80DSP dsp;
    std::atomic<float>* gainParameter = nullptr;
    std::atomic<float>* bassParameter = nullptr;
    std::atomic<float>* fattyModeParameter = nullptr;
    std::atomic<float> inputVuDb { -72.0f };
    std::atomic<float> outputVuDb { -72.0f };
    std::atomic<uint64_t> vuUpdateCounter { 0 };
    float inputVuMeanSquare = 0.0f;
    float outputVuMeanSquare = 0.0f;
    double currentSampleRate = 44100.0;

    void updateVuMeters(const juce::AudioBuffer<float>& buffer, bool isInput) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VitaPlugGainer80AudioProcessor)
};
