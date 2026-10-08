#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace vitaplug::gainer80::dsp
{
class Gainer80DSP
{
public:
    void prepare(double newSampleRate, int, int newNumChannels)
    {
        sampleRate = std::max(1.0, newSampleRate);
        numChannels = std::clamp(newNumChannels, 1, static_cast<int>(states.size()));
        gain.reset();
        bass.reset();
        fattyModeMix.reset(fattyModeUpper ? 1.0f : 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& state : states)
        {
            state.lower = {};
            state.upper = {};
        }
    }

    void setGainDb(float value) noexcept { gainTargetDb = std::clamp(value, -24.0f, 24.0f); }
    void setBassDb(float value) noexcept { bassTargetDb = std::clamp(value, 0.0f, 12.0f); }
    void setFattyMode(bool upper) noexcept { fattyModeUpper = upper; }

    void process(float* const* channelData, int channels, int numSamples) noexcept
    {
        if (channelData == nullptr || numSamples <= 0)
            return;

        const auto activeChannels = std::min(std::clamp(channels, 0, numChannels), static_cast<int>(states.size()));
        if (activeChannels == 0)
            return;

        // The fast path preserves samples exactly when the effect is neutral.
        if (gain.isAt(0.0f) && std::abs(gainTargetDb) < 1.0e-8f
            && bass.isAt(0.0f) && std::abs(bassTargetDb) < 1.0e-8f)
            return;

        const auto rampSamples = std::max(1, static_cast<int>(sampleRate * 0.020));
        gain.setTarget(gainTargetDb, rampSamples);
        // Both shelves run during a mode transition. Crossfading their outputs
        // avoids a discontinuity caused by replacing a live biquad's frequency.
        fattyModeMix.setTarget(fattyModeUpper ? 1.0f : 0.0f, rampSamples);

        const auto bassStart = bass.current;
        bass.setTarget(bassTargetDb, rampSamples);
        const auto bassEnd = bass.advance(numSamples);

        const auto lowerStart = makeLowShelf(bassStart, 80.0);
        const auto lowerEnd = makeLowShelf(bassEnd, 80.0);
        // UPPER retains the same low-shelf topology, slope and gain law as
        // LOWER. Only its labelled corner moves from 100 Hz to 160 Hz.
        const auto upperStart = makeLowShelf(bassStart, 160.0);
        const auto upperEnd = makeLowShelf(bassEnd, 160.0);
        const auto lowerStep = (lowerEnd - lowerStart) / static_cast<float>(numSamples);
        const auto upperStep = (upperEnd - upperStart) / static_cast<float>(numSamples);
        auto lowerCoefficients = lowerStart;
        auto upperCoefficients = upperStart;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto gainLinear = dbToGain(gain.next());
            const auto upperMix = fattyModeMix.next();

            for (int channel = 0; channel < activeChannels; ++channel)
            {
                auto& state = states[static_cast<size_t>(channel)];
                const auto input = channelData[channel][sample];
                const auto lower = processSample(input, lowerCoefficients, state.lower);
                const auto upper = processSample(input, upperCoefficients, state.upper);
                const auto filtered = lower + (upper - lower) * upperMix;
                channelData[channel][sample] = filtered * gainLinear;
            }

            lowerCoefficients += lowerStep;
            upperCoefficients += upperStep;
        }

        if (gain.isAt(0.0f) && std::abs(gainTargetDb) < 1.0e-8f
            && bass.isAt(0.0f) && std::abs(bassTargetDb) < 1.0e-8f)
            reset();
    }

private:
    struct Coefficients
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

        Coefficients operator-(const Coefficients& other) const noexcept
        {
            return { b0 - other.b0, b1 - other.b1, b2 - other.b2, a1 - other.a1, a2 - other.a2 };
        }

        Coefficients operator/(float divisor) const noexcept
        {
            return { b0 / divisor, b1 / divisor, b2 / divisor, a1 / divisor, a2 / divisor };
        }

        Coefficients& operator+=(const Coefficients& other) noexcept
        {
            b0 += other.b0; b1 += other.b1; b2 += other.b2; a1 += other.a1; a2 += other.a2;
            return *this;
        }
    };

    struct BiquadState { float z1 = 0.0f, z2 = 0.0f; };
    struct State { BiquadState lower, upper; };

    struct SmoothedValue
    {
        float current = 0.0f, target = 0.0f, increment = 0.0f;
        int remaining = 0;

        void reset(float value = 0.0f) noexcept { current = target = value; increment = 0.0f; remaining = 0; }
        bool isAt(float value) const noexcept
        {
            return std::abs(current - value) < 1.0e-8f && std::abs(target - value) < 1.0e-8f && remaining == 0;
        }

        void setTarget(float newTarget, int samples) noexcept
        {
            if (std::abs(target - newTarget) < 1.0e-8f)
                return;

            target = newTarget;
            if (std::abs(current - target) < 1.0e-8f) { current = target; remaining = 0; increment = 0.0f; return; }
            remaining = std::max(1, samples);
            increment = (target - current) / static_cast<float>(remaining);
        }

        float next() noexcept
        {
            if (remaining > 0)
            {
                current += increment;
                if (--remaining == 0)
                    current = target;
            }
            return current;
        }

        float advance(int samples) noexcept
        {
            for (int i = 0; i < samples; ++i)
                next();
            return current;
        }
    };

    static float dbToGain(float db) noexcept { return std::pow(10.0f, db / 20.0f); }

    Coefficients makeLowShelf(float gainDb, double frequencyHz) const noexcept
    {
        if (gainDb == 0.0f)
            return {};

        constexpr auto pi = 3.14159265358979323846;
        const auto a = std::pow(10.0, static_cast<double>(gainDb) / 40.0);
        const auto omega = 2.0 * pi * frequencyHz / sampleRate;
        const auto cosine = std::cos(omega);
        const auto alpha = std::sin(omega) * 0.5 * std::sqrt((a + 1.0 / a) * 2.0);
        const auto beta = 2.0 * std::sqrt(a) * alpha;

        const auto b0 = a * ((a + 1.0) - (a - 1.0) * cosine + beta);
        const auto b1 = 2.0 * a * ((a - 1.0) - (a + 1.0) * cosine);
        const auto b2 = a * ((a + 1.0) - (a - 1.0) * cosine - beta);
        const auto a0 = (a + 1.0) + (a - 1.0) * cosine + beta;
        const auto a1 = -2.0 * ((a - 1.0) + (a + 1.0) * cosine);
        const auto a2 = (a + 1.0) + (a - 1.0) * cosine - beta;

        return { static_cast<float>(b0 / a0), static_cast<float>(b1 / a0), static_cast<float>(b2 / a0),
                 static_cast<float>(a1 / a0), static_cast<float>(a2 / a0) };
    }

    static float processSample(float input, const Coefficients& c, BiquadState& state) noexcept
    {
        const auto output = c.b0 * input + state.z1;
        state.z1 = c.b1 * input - c.a1 * output + state.z2;
        state.z2 = c.b2 * input - c.a2 * output;
        return output;
    }

    double sampleRate = 44100.0;
    int numChannels = 2;
    std::array<State, 2> states {};
    SmoothedValue gain, bass, fattyModeMix;
    float gainTargetDb = 0.0f, bassTargetDb = 0.0f;
    bool fattyModeUpper = false;
};
} // namespace vitaplug::gainer80::dsp
