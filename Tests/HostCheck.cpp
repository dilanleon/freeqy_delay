#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <juce_audio_processors/juce_audio_processors.h>

struct StagedBundle
{
    // Copy the VST3 bundle to the host-like temporary loading location.
    explicit StagedBundle(const std::filesystem::path &source)
    {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        root = std::filesystem::temp_directory_path() /
               ("freeqy-vst3-check-" + std::to_string(nonce));
        bundle = root / source.filename();
        std::filesystem::create_directories(root);
        std::filesystem::copy(source, bundle,
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::copy_symlinks);
    }

    // Remove the temporary VST3 copy after host validation.
    ~StagedBundle()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

    std::filesystem::path root;
    std::filesystem::path bundle;
};

// Validate VST3 discovery, processing, automation, state, and editor creation.
int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2)
        return 2;
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    const auto *installed = std::getenv("FREEQY_TEST_INSTALLED_VST3");
    const auto input = std::filesystem::absolute(installed != nullptr ? installed : argv[1]);
    auto loadPath = input;
#if JUCE_MAC
    // Match an installed host path and avoid macOS 26 dyld stalls on freshly
    // packaged signed bundles inside a sync-managed Documents workspace.
    std::optional<StagedBundle> staged;
    if (installed == nullptr)
        loadPath = staged.emplace(input).bundle;
#endif
    const juce::File path(juce::String(loadPath.c_str()));
    format.findAllTypesForFile(types, path.getFullPathName());
    if (types.isEmpty())
    {
        std::cerr << "No VST3 types found\n";
        return 1;
    }
    juce::String error;
    auto plugin = format.createInstanceFromDescription(*types[0], 48000, 512, error);
    if (!plugin)
    {
        std::cerr << error << "\n";
        return 1;
    }
    bool morphExposed = false, shiftExposed = false, rateExposed = false, sizeExposed = false,
         stereoMorphExposed = false;
    for (auto *parameter : plugin->getParameters())
    {
        const auto name = parameter->getName(100);
        if (name == "Stereo Morph")
            stereoMorphExposed = parameter->isAutomatable();
        if (name.contains("Shift Amount"))
            shiftExposed = parameter->isAutomatable();
        if (name.contains("Shift Rate"))
            rateExposed = parameter->isAutomatable();
        if (name.contains("STFT Size"))
            sizeExposed = parameter->isAutomatable();
        if (parameter->getName(100).contains("Curve Morph"))
            morphExposed = parameter->isAutomatable();
    }
    if (!morphExposed || !shiftExposed || !rateExposed || !sizeExposed || !stereoMorphExposed)
    {
        std::cerr << "Morph automation missing in VST3\n";
        return 1;
    }
    plugin->prepareToPlay(48000, 512);
    if (plugin->getLatencySamples() != 2048)
    {
        std::cerr << "Wrong latency\n";
        return 1;
    }
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    // Silence must stay silent: the effect must never synthesize an output signal.
    for (int block = 0; block < 8; ++block)
    {
        audio.clear();
        plugin->processBlock(audio, midi);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                if (audio.getSample(channel, sample) != 0.f)
                {
                    std::cerr << "Silent input produced audio\n";
                    return 1;
                }
    }
    audio.clear();
    audio.setSample(0, 0, .1f);
    for (int i = 0; i < 100; ++i)
    {
        plugin->processBlock(audio, midi);
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < 512; ++n)
                if (!std::isfinite(audio.getSample(c, n)))
                    return 1;
        audio.clear();
    }
    plugin->getParameters()[0]->setValueNotifyingHost(.73f);
    juce::MemoryBlock before, after;
    plugin->getStateInformation(before);
    plugin->setStateInformation(before.getData(), int(before.getSize()));
    plugin->getStateInformation(after);
    if (before != after)
    {
        std::cerr << "State roundtrip differs\n";
        return 1;
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorAndMakeActive());
    if (!editor)
        return 1;
    plugin->editorBeingDeleted(editor.get());
    editor.reset();
    plugin->releaseResources();
    std::cout << "VST3 scanned, instantiated, processed, state recalled, editor created\n";
}
