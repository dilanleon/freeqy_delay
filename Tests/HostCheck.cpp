#include <iostream>
#include <juce_audio_processors/juce_audio_processors.h>
int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2)
        return 2;
    bool diagnostic = argc > 2;
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    const auto path = juce::File::isAbsolutePath(argv[1])
                          ? juce::File(argv[1])
                          : juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
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
    if (plugin->getLatencySamples() != (diagnostic ? 0 : 2048))
    {
        std::cerr << "Wrong latency\n";
        return 1;
    }
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
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
    if (diagnostic)
    {
        audio.clear();
        plugin->releaseResources();
        plugin->prepareToPlay(48000, 512);
        double phase = 0;
        for (int block = 0; block < 8; ++block)
        {
            audio.clear();
            plugin->processBlock(audio, midi);
            for (int n = 0; n < 512; ++n)
            {
                float expected = float(std::sin(phase)) * std::pow(10.f, -18.f / 20);
                if (std::abs(audio.getSample(0, n) - expected) > 1e-6 ||
                    audio.getSample(0, n) != audio.getSample(1, n))
                    return 1;
                phase =
                    std::fmod(phase + 2 * 3.141592653589793 * 440 / 48000, 2 * 3.141592653589793);
            }
        }
        std::cout << "Diagnostic sine frequency, level, phase, silence input checked\n";
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
