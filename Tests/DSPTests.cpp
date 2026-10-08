#include "DSP/Gainer80DSP.h"
#include "State/FattyModeState.h"
#include "UI/VuScaleMap.h"

#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

namespace
{
constexpr int sampleRate = 48000;
constexpr int samples = 48000;

float rms(const std::vector<float>& data)
{
    double sum = 0.0;
    for (const auto sample : data) sum += sample * sample;
    return static_cast<float>(std::sqrt(sum / data.size()));
}

float peak(const std::vector<float>& data)
{
    float result = 0.0f;
    for (const auto sample : data) result = std::max(result, std::abs(sample));
    return result;
}

std::vector<float> sine(float frequency)
{
    std::vector<float> result(samples);
    for (int i = 0; i < samples; ++i)
        result[static_cast<size_t>(i)] = 0.1f * std::sin(2.0 * 3.14159265358979323846 * frequency * i / sampleRate);
    return result;
}

bool approximately(float actual, float expected, float tolerance)
{
    return std::abs(actual - expected) <= tolerance;
}

bool verifyVuScaleMap()
{
    using VuScaleMap = vitaplug::gainer80::ui::VuScaleMap;

    constexpr std::array<VuScaleMap::Anchor, 6> expectedAnchors {{
        { -30.0f, 207.0f },
        { -20.0f, 222.0f },
        { -10.0f, 263.0f },
        {   0.0f, 304.0f },
        {   3.0f, 329.0f },
        {   6.0f, 354.0f },
    }};

    for (size_t index = 0; index < expectedAnchors.size(); ++index)
        if (!approximately(VuScaleMap::anchors[index].vu, expectedAnchors[index].vu, 1.0e-4f)
            || !approximately(VuScaleMap::anchors[index].degrees, expectedAnchors[index].degrees, 1.0e-4f))
        {
            std::cerr << "VU anchor definition no longer matches the approved map\n";
            return false;
        }

    for (const auto& anchor : VuScaleMap::anchors)
        if (!approximately(VuScaleMap::angleForVu(anchor.vu), anchor.degrees, 1.0e-4f))
        {
            std::cerr << "VU anchor mapping is incorrect at " << anchor.vu << " VU\n";
            return false;
        }

    auto previous = VuScaleMap::angleForVu(VuScaleMap::minVu);
    for (int index = 1; index <= 3600; ++index)
    {
        const auto vu = VuScaleMap::minVu
                      + (VuScaleMap::maxVu - VuScaleMap::minVu) * static_cast<float>(index) / 3600.0f;
        const auto angle = VuScaleMap::angleForVu(vu);
        if (angle < previous)
        {
            std::cerr << "VU map is not monotonic\n";
            return false;
        }
        previous = angle;
    }

    if (!approximately(VuScaleMap::angleForVu(-100.0f), 207.0f, 1.0e-4f)
        || !approximately(VuScaleMap::angleForVu(100.0f), 354.0f, 1.0e-4f))
    {
        std::cerr << "VU map does not clamp to its movement limits\n";
        return false;
    }

    for (const auto vu : { -20.0f, -10.0f, 0.0f, 3.0f })
        if (!approximately(VuScaleMap::angleForDbfs(vu + VuScaleMap::zeroVuDbfs),
                           VuScaleMap::angleForVu(vu), 1.0e-4f))
        {
            std::cerr << "VU needle and printed-scale angles diverge\n";
            return false;
        }

    return true;
}

float shelfResponseDb(vitaplug::gainer80::dsp::Gainer80DSP& dsp, bool upper, float frequency)
{
    dsp.setFattyMode(upper);
    dsp.setBassDb(12.0f);
    std::vector<float> settle(4096, 0.0f);
    float* mono[] { settle.data() };
    dsp.process(mono, 1, static_cast<int>(settle.size()));
    auto tone = sine(frequency);
    const auto reference = tone;
    mono[0] = tone.data();
    dsp.process(mono, 1, samples);
    return 20.0f * std::log10(rms(tone) / rms(reference));
}
}

int main()
{
    if (!verifyVuScaleMap())
        return 1;

    // Session migration: a pre-fattyMode state deliberately selects LOWER.
    juce::ValueTree oldState { "PARAMETERS" };
    auto oldGain = juce::ValueTree { "PARAM" };
    oldGain.setProperty("id", "gain", nullptr);
    oldState.addChild(oldGain, -1, nullptr);
    if (vitaplug::gainer80::state::containsFattyMode(oldState))
    {
        std::cerr << "Legacy state unexpectedly contains fattyMode\n";
        return 1;
    }
    auto newMode = juce::ValueTree { "PARAM" };
    newMode.setProperty("id", "fattyMode", nullptr);
    oldState.addChild(newMode, -1, nullptr);
    if (!vitaplug::gainer80::state::containsFattyMode(oldState))
    {
        std::cerr << "fattyMode was not recognised in saved state\n";
        return 1;
    }

    vitaplug::gainer80::dsp::Gainer80DSP dsp;
    dsp.prepare(sampleRate, 512, 2);

    auto left = sine(1000.0f);
    auto right = left;
    const auto original = left;
    float* channels[] { left.data(), right.data() };
    dsp.process(channels, 2, samples);
    if (std::memcmp(left.data(), original.data(), left.size() * sizeof(float)) != 0)
    {
        std::cerr << "Neutral controls altered the signal\n";
        return 1;
    }

    dsp.setGainDb(6.0f);
    std::vector<float> gainSettle(4096, 0.0f);
    float* gainSettleChannels[] { gainSettle.data(), gainSettle.data() };
    dsp.process(gainSettleChannels, 2, static_cast<int>(gainSettle.size()));
    left = sine(1000.0f); right = left;
    channels[0] = left.data();
    channels[1] = right.data();
    dsp.process(channels, 2, samples);
    if (!approximately(rms(left) / rms(original), std::pow(10.0f, 6.0f / 20.0f), 0.02f))
    {
        std::cerr << "Gain response is incorrect\n";
        return 1;
    }

    dsp.prepare(sampleRate, 512, 1);
    dsp.setGainDb(0.0f);
    const auto lowBoostDb = shelfResponseDb(dsp, false, 80.0f);
    // A first-order shelf is at its half-gain point at its labelled corner.
    if (!approximately(lowBoostDb, 6.0f, 0.6f))
    {
        std::cerr << "80 Hz shelf response is incorrect: " << lowBoostDb << " dB\n";
        return 1;
    }

    dsp.prepare(sampleRate, 512, 1);
    dsp.setGainDb(0.0f);
    const auto upperBoostDb = shelfResponseDb(dsp, true, 160.0f);
    if (!approximately(upperBoostDb, 6.0f, 0.6f))
    {
        std::cerr << "160 Hz shelf response is incorrect: " << upperBoostDb << " dB\n";
        return 1;
    }

    // The two modes are intentionally distinct around their crossover region.
    dsp.prepare(sampleRate, 512, 1);
    dsp.setGainDb(0.0f);
    const auto lowerAt120Db = shelfResponseDb(dsp, false, 120.0f);
    dsp.prepare(sampleRate, 512, 1);
    dsp.setGainDb(0.0f);
    const auto upperAt120Db = shelfResponseDb(dsp, true, 120.0f);
    if (std::abs(lowerAt120Db - upperAt120Db) < 0.35f)
    {
        std::cerr << "80 Hz and 160 Hz modes are insufficiently distinct\n";
        return 1;
    }

    // With no FATTY gain both filter paths are exact identities, including UPPER.
    dsp.setFattyMode(true);
    dsp.prepare(sampleRate, 512, 1);
    dsp.setGainDb(0.0f);
    dsp.setBassDb(0.0f);
    auto neutralUpper = sine(120.0f);
    const auto neutralReference = neutralUpper;
    float* neutralMono[] { neutralUpper.data() };
    dsp.process(neutralMono, 1, samples);
    if (std::memcmp(neutralUpper.data(), neutralReference.data(), neutralUpper.size() * sizeof(float)) != 0)
    {
        std::cerr << "Upper mode altered neutral FATTY audio\n";
        return 1;
    }

    // A 20 ms output crossfade keeps the shelf handover bounded and finite.
    dsp.setFattyMode(false);
    dsp.prepare(sampleRate, 512, 1);
    dsp.setGainDb(0.0f);
    dsp.setBassDb(12.0f);
    std::vector<float> warmup(4096, 0.0f);
    float* warmupMono[] { warmup.data() };
    dsp.process(warmupMono, 1, static_cast<int>(warmup.size()));
    auto transition = sine(120.0f);
    float* transitionMono[] { transition.data() };
    dsp.setFattyMode(true);
    dsp.process(transitionMono, 1, samples);
    for (const auto sample : transition)
        if (!std::isfinite(sample))
        {
            std::cerr << "Mode transition produced a non-finite sample\n";
            return 1;
        }
    if (peak(transition) > 1.0f || std::abs(transition[1] - transition[0]) > 0.05f)
    {
        std::cerr << "Mode transition produced an anomalous peak or discontinuity\n";
        return 1;
    }

    std::cout << "DSP tests passed: neutral paths, gain, 80/160 Hz shelves, and mode crossfade.\n";
    return 0;
}
