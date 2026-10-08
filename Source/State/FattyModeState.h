#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace vitaplug::gainer80::state
{
inline bool containsFattyMode(const juce::ValueTree& state)
{
    for (int index = 0; index < state.getNumChildren(); ++index)
    {
        const auto child = state.getChild(index);
        if (child.getProperty("id").toString() == "fattyMode")
            return true;
    }

    return false;
}
} // namespace vitaplug::gainer80::state
