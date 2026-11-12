#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try
    {
        Processor p;
        auto banks = p.getBanks();
        banks.count = 3;
        for (int b = 0; b < 4; ++b)
            banks.banks[b].values[1][0][123] = float(b) * .2f;
        p.updateBanks(banks);
        p.selectedBank = 2;
        auto *morph = p.state.getParameter("curveMorph");
        check(morph && morph->isAutomatable(), "missing automation");
        morph->setValueNotifyingHost(.67f);
        p.state.getParameter("shiftHz")->setValueNotifyingHost(
            p.state.getParameter("shiftHz")->convertTo0to1(123));
        p.state.getParameter("shiftRateHz")
            ->setValueNotifyingHost(p.state.getParameter("shiftRateHz")->convertTo0to1(1.7f));
        p.state.getParameter("stftSize")->setValueNotifyingHost(.5f);
        check(p.state.getParameter("copies") == nullptr, "copy engine still exposed");
        if (argc > 1)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
            if (argc > 3)
                editor->setSize(juce::String(argv[2]).getIntValue(),
                                juce::String(argv[3]).getIntValue());
            juce::PNGImageFormat png;
            auto output = juce::File(argv[1]).createOutputStream();
            check(output != nullptr, "snapshot output");
            check(png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),
                                         *output),
                  "snapshot render");
            p.editorBeingDeleted(editor.get());
        }
        juce::MemoryBlock bytes;
        p.getStateInformation(bytes);
        p.prepareToPlay(48000, 512);
        p.requestClearBuffer();
        juce::AudioBuffer<float> audio(2, 512);
        audio.clear();
        juce::MidiBuffer midi;
        p.processBlock(audio, midi);
        juce::MemoryBlock afterClear;
        p.getStateInformation(afterClear);
        check(bytes == afterClear, "clear changed stored settings");
        Processor restored;
        restored.setStateInformation(bytes.getData(), int(bytes.getSize()));
        auto saved = restored.getBanks();
        check(saved.count == 3 && restored.selectedBank == 2, "bank settings recall");
        for (int b = 0; b < 4; ++b)
            check(saved.banks[b].values[1][0][123] == banks.banks[b].values[1][0][123],
                  "bank recall");
        check(std::abs(restored.state.getRawParameterValue("curveMorph")->load() - 67) < .001,
              "morph recall");
        check(restored.state.getRawParameterValue("shiftHz")->load() == 123, "shift recall");
        check(std::abs(restored.state.getRawParameterValue("shiftRateHz")->load() - 1.7f) < .0001,
              "rate recall");
        check(restored.state.getRawParameterValue("stftSize")->load() == 3, "STFT size recall");
        check(p.getLatencySamples() == spectral::Latency, "fixed host latency");
        auto tree = juce::ValueTree::readFromData(bytes.getData(), bytes.getSize());
        tree.setProperty("version", 1, nullptr);
        tree.removeProperty("banks", nullptr);
        spectral::Curves legacy;
        legacy.values[0][0][17] = .83f;
        tree.setProperty("curves", juce::MemoryBlock(&legacy, sizeof(legacy)), nullptr);
        juce::MemoryBlock old;
        {
            juce::MemoryOutputStream out(old, false);
            tree.writeToStream(out);
        }
        restored.setStateInformation(old.getData(), int(old.getSize()));
        saved = restored.getBanks();
        for (auto &bank : saved.banks)
            check(bank.values[0][0][17] == .83f, "legacy curve migration");
        check(restored.selectedBank == 0 &&
                  restored.state.getRawParameterValue("curveMorph")->load() == 0,
              "legacy morph reset");
        check(restored.state.getRawParameterValue("shiftHz")->load() == 0 &&
                  restored.state.getRawParameterValue("stftSize")->load() == 6,
              "legacy effect defaults");
        // Reject nonfinite stored curves and out-of-range counts.
        tree.setProperty("version", 2, nullptr);
        juce::ValueTree retired("PARAM");
        retired.setProperty("id", "copies", nullptr);
        retired.setProperty("value", 16, nullptr);
        tree.addChild(retired, -1, nullptr);
        saved.banks[0].values[0][0][0] = std::numeric_limits<float>::quiet_NaN();
        tree.setProperty("banks", juce::MemoryBlock(saved.banks.data(), sizeof(saved.banks)),
                         nullptr);
        tree.setProperty("bankCount", 99, nullptr);
        old.reset();
        {
            juce::MemoryOutputStream out(old, false);
            tree.writeToStream(out);
        }
        restored.setStateInformation(old.getData(), int(old.getSize()));
        check(restored.getBanks().count == 4 &&
                  std::isfinite(restored.getBanks().banks[0].values[0][0][0]),
              "state sanitization");
        juce::MemoryBlock migrated;
        restored.getStateInformation(migrated);
        auto modern = juce::ValueTree::readFromData(migrated.getData(), migrated.getSize());
        check(int(modern.getProperty("version")) == 3 &&
                  !modern.getChildWithProperty("id", "copies").isValid(),
              "v2 migration cleanup");
        std::cout << "Bank state, automation, legacy migration and sanitization passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
