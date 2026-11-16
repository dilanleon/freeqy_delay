#pragma once
#include "PluginProcessor.h"

namespace spectral::presets
{
// Write the complete version 4 host state to a plug-in preset file.
juce::Result savePlugin(Processor &, const juce::File &);
// Validate and restore a complete plug-in preset file.
juce::Result loadPlugin(Processor &, const juce::File &);
// Write only the selected bank's curves and point editor data.
juce::Result saveBank(Processor &, const juce::File &);
// Validate a bank file, then replace only the currently selected bank.
juce::Result loadBank(Processor &, const juce::File &);
}
