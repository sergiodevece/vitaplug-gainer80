#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "State/FattyModeState.h"

namespace
{
juce::AudioParameterFloatAttributes dbAttributes()
{
    return juce::AudioParameterFloatAttributes()
        .withLabel("dB")
        .withStringFromValueFunction([] (float value, int) { return juce::String(value, 1) + " dB"; });
}
}

VitaPlugGainer80AudioProcessor::VitaPlugGainer80AudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    gainParameter = parameters.getRawParameterValue("gain");
    bassParameter = parameters.getRawParameterValue("bass");
    fattyModeParameter = parameters.getRawParameterValue("fattyMode");
}

juce::AudioProcessorValueTreeState::ParameterLayout VitaPlugGainer80AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "gain", 1 }, "Gain",
                                                           juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
                                                           0.0f, dbAttributes()));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "bass", 1 }, "Bass",
                                                           juce::NormalisableRange<float> { 0.0f, 12.0f, 0.1f },
                                                           0.0f, dbAttributes()));
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "fattyMode", 1 },
                                                            "Fatty Frequency",
                                                            juce::StringArray { "LOWER (80 Hz)", "UPPER (160 Hz)" },
                                                            0));
    return layout;
}

void VitaPlugGainer80AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    inputVuMeanSquare = outputVuMeanSquare = 0.0f;
    inputVuDb.store(-72.0f, std::memory_order_relaxed);
    outputVuDb.store(-72.0f, std::memory_order_relaxed);
    dsp.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

void VitaPlugGainer80AudioProcessor::releaseResources() { dsp.reset(); }

bool VitaPlugGainer80AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return (input == output) && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

void VitaPlugGainer80AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    updateVuMeters(buffer, true);
    dsp.setGainDb(gainParameter->load());
    dsp.setBassDb(bassParameter->load());
    dsp.setFattyMode(fattyModeParameter->load() >= 0.5f);

    std::array<float*, 2> channels {};
    const auto channelCount = std::min(2, buffer.getNumChannels());
    for (int channel = 0; channel < channelCount; ++channel)
        channels[static_cast<size_t>(channel)] = buffer.getWritePointer(channel);

    dsp.process(channels.data(), channelCount, buffer.getNumSamples());
    updateVuMeters(buffer, false);
    vuUpdateCounter.fetch_add(1, std::memory_order_relaxed);
}

void VitaPlugGainer80AudioProcessor::updateVuMeters(const juce::AudioBuffer<float>& buffer, bool isInput) noexcept
{
    const auto channels = std::min(2, buffer.getNumChannels());
    const auto samples = buffer.getNumSamples();
    if (channels <= 0 || samples <= 0)
        return;

    // Stereo power is averaged, not summed, so anti-phase channels cannot cancel.
    double energy = 0.0;
    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* data = buffer.getReadPointer(channel);
        for (int sample = 0; sample < samples; ++sample)
            energy += static_cast<double>(data[sample]) * data[sample];
    }

    const auto meanSquare = static_cast<float>(energy / static_cast<double>(channels * samples));
    const auto coefficient = std::exp(static_cast<float>(-samples / (currentSampleRate * 0.300)));
    auto& smoothedSquare = isInput ? inputVuMeanSquare : outputVuMeanSquare;
    smoothedSquare = coefficient * smoothedSquare + (1.0f - coefficient) * meanSquare;
    const auto db = 10.0f * std::log10(std::max(smoothedSquare, 1.0e-12f));
    (isInput ? inputVuDb : outputVuDb).store(juce::jlimit(-72.0f, 18.0f, db), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* VitaPlugGainer80AudioProcessor::createEditor()
{
    return new VitaPlugGainer80AudioProcessorEditor(*this);
}

void VitaPlugGainer80AudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    if (const auto state = parameters.copyState().createXml())
        copyXmlToBinary(*state, destinationData);
}

void VitaPlugGainer80AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
        {
            const auto restoredState = juce::ValueTree::fromXml(*xml);
            const auto hasFattyMode = vitaplug::gainer80::state::containsFattyMode(restoredState);
            parameters.replaceState(restoredState);

            // Projects saved before fattyMode existed retain the old 80 Hz
            // behaviour instead of inheriting an arbitrary host value.
            if (!hasFattyMode)
                if (auto* parameter = parameters.getParameter("fattyMode"))
                    parameter->setValueNotifyingHost(0.0f);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VitaPlugGainer80AudioProcessor();
}
