#pragma once

#include <algorithm>
#include <array>

namespace vitaplug::gainer80::ui
{
// Pure visual calibration for the G80 VU face. Audio analysis remains in the
// processor; this type only turns already-calibrated VU values into angles.
class VuScaleMap final
{
public:
    struct Anchor
    {
        float vu;
        float degrees;
    };

    static constexpr float minVu = -30.0f;
    static constexpr float maxVu = 6.0f;
    static constexpr float zeroVuDbfs = -18.0f;

    inline static constexpr std::array<Anchor, 6> anchors {{
        { -30.0f, 207.0f },
        { -20.0f, 222.0f },
        { -10.0f, 263.0f },
        {   0.0f, 304.0f },
        {   3.0f, 329.0f },
        {   6.0f, 354.0f },
    }};

    // Monotone cubic Hermite interpolation (PCHIP slopes). The first and last
    // derivatives continue the adjacent branch; the interior derivative at
    // 0 VU is the harmonic mean of the neighbouring positive secants.
    static float angleForVu(float vu) noexcept
    {
        const auto clampedVu = std::clamp(vu, minVu, maxVu);
        auto segment = static_cast<size_t>(0);
        while (segment + 1 < anchors.size() - 1 && clampedVu > anchors[segment + 1].vu)
            ++segment;

        const auto& left = anchors[segment];
        const auto& right = anchors[segment + 1];
        const auto interval = right.vu - left.vu;
        const auto t = (clampedVu - left.vu) / interval;
        const auto t2 = t * t;
        const auto t3 = t2 * t;

        const auto h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
        const auto h10 = t3 - 2.0f * t2 + t;
        const auto h01 = -2.0f * t3 + 3.0f * t2;
        const auto h11 = t3 - t2;

        return h00 * left.degrees + h10 * interval * derivativeAt(segment)
             + h01 * right.degrees + h11 * interval * derivativeAt(segment + 1);
    }

    static float angleForDbfs(float dbfs) noexcept
    {
        return angleForVu(dbfs - zeroVuDbfs);
    }

private:
    static float secant(size_t segment) noexcept
    {
        const auto& left = anchors[segment];
        const auto& right = anchors[segment + 1];
        return (right.degrees - left.degrees) / (right.vu - left.vu);
    }

    static float derivativeAt(size_t index) noexcept
    {
        if (index == 0)
            return secant(0);
        if (index == anchors.size() - 1)
            return secant(anchors.size() - 2);

        const auto previous = secant(index - 1);
        const auto next = secant(index);
        return previous <= 0.0f || next <= 0.0f ? 0.0f
                                                  : 2.0f * previous * next / (previous + next);
    }
};
}
