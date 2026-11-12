#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <numbers>
juce::AudioProcessorValueTreeState::ParameterLayout Processor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    auto add =
        [&](const char *id, const char *name, float lo, float hi, float def, float skew = 1.f)
    {
        p.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>(lo, hi, 0, skew), def,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction(
                    [key = juce::String(id)](float value, int)
                    {
                        if (key == "mix")
                            return juce::String(value * 100, 1) + " %";
                        return juce::String(value, key == "maxDelayMs" ? 1 : 2);
                    })
                .withValueFromStringFunction(
                    [key = juce::String(id)](const juce::String &text)
                    { return text.getFloatValue() / ((key == "mix") ? 100.f : 1.f); })));
    };
    add("mix", "Dry / Wet", 0, 1, .5);
    add("maxDelayMs", "Max Delay (ms)", 1, 10000, 1000, .3f);
    add("shiftHz", "Stereo Shift Amount (Hz)", 0, 1000, 0, .5f);
    add("shiftRateHz", "Stereo Shift Rate (Hz)", 0, 10, .3f, .5f);
    add("inputTrimDb", "Input (dB)", -24, 24, 0);
    add("outputTrimDb", "Output (dB)", -24, 24, 0);
    p.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"stftSize", 1}, "STFT Size (samples)",
        juce::StringArray{"16", "32", "64", "128", "256", "512", "1024"}, 6));
    p.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"curveMorph", 1}, "Curve Morph (%)",
        juce::NormalisableRange<float>(0, 100), 0,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction([](float value, int) { return juce::String(value, 1); })
            .withValueFromStringFunction([](const juce::String &text)
                                         { return text.getFloatValue(); })));
    return p;
}
Processor::Processor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "SpectralDelay", layout())
{
    startTimerHz(30);
}
bool Processor::isBusesLayoutSupported(const BusesLayout &l) const
{
    return l.getMainInputChannelSet() == l.getMainOutputChannelSet() &&
           (l.getMainOutputChannelSet() == juce::AudioChannelSet::mono() ||
            l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
}
void Processor::prepareToPlay(double sr, int)
{
    engine.setSettings(readSettings());
    engine.prepare(sr, getTotalNumInputChannels());
    engine.setBanks(getBanks());
    engine.reset();
    input.fill(0);
    inputPos = analyzerHop = 0;
    sinePhase = 0;
    setLatencySamples(SPECTRAL_DIAGNOSTIC ? 0 : spectral::Latency);
}
spectral::Curves Processor::getCurves()
{
    std::lock_guard lock(modelMutex);
    return model.banks[juce::jlimit(0, 3, selectedBank.load())];
}
// Producers serialize on modelMutex; the audio consumer only touches the bounded FIFO.
void Processor::updateCurves(const spectral::Curves &c)
{
    std::lock_guard lock(modelMutex);
    model.banks[juce::jlimit(0, 3, selectedBank.load())] = c;
    pending = true;
    int a, n, b, m;
    curveFifo.prepareToWrite(1, a, n, b, m);
    if (n)
    {
        curveQueue[a] = model;
        curveFifo.finishedWrite(1);
        pending = false;
    }
}
spectral::CurveBanks Processor::getBanks()
{
    std::lock_guard lock(modelMutex);
    return model;
}
void Processor::updateBanks(const spectral::CurveBanks &banks)
{
    {
        std::lock_guard lock(modelMutex);
        model = banks;
        model.count = juce::jlimit(1, 4, model.count);
        pending = true;
    }
    publishPending();
}
void Processor::publishPending()
{
    std::lock_guard lock(modelMutex);
    if (!pending)
        return;
    int a, n, b, m;
    curveFifo.prepareToWrite(1, a, n, b, m);
    if (n)
    {
        curveQueue[a] = model;
        curveFifo.finishedWrite(1);
        pending = false;
    }
}
void Processor::analyze(const juce::AudioBuffer<float> &buffer)
{
    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        float x = 0;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            x += buffer.getSample(c, n);
        input[inputPos] = x / buffer.getNumChannels();
        inputPos = (inputPos + 1) % spectral::N;
        if (++analyzerHop == 1024)
        {
            analyzerHop = 0;
            int a, k, b, m;
            spectrumFifo.prepareToWrite(1, a, k, b, m);
            if (k)
            {
                for (int i = 0; i < spectral::N; ++i)
                    fft[i] = input[(inputPos + i) % spectral::N] *
                             float(.5 - .5 * std::cos(2 * std::numbers::pi * i / spectral::N));
                analyzer.performFrequencyOnlyForwardTransform(fft.data());
                for (int i = 0; i < spectral::B; ++i)
                    spectrumQueue[a][i] = fft[i] * 2 / (spectral::N * .5f);
                spectrumFifo.finishedWrite(1);
            }
        }
    }
}
bool Processor::spectrum(std::array<float, spectral::B> &out)
{
    bool found = false;
    while (spectrumFifo.getNumReady())
    {
        int a, n, b, m;
        spectrumFifo.prepareToRead(1, a, n, b, m);
        if (n)
        {
            out = spectrumQueue[a];
            spectrumFifo.finishedRead(1);
            found = true;
        }
    }
    return found;
}
spectral::Settings Processor::readSettings() const
{
    spectral::Settings s;
    auto v = [&](const char *id) { return state.getRawParameterValue(id)->load(); };
    s.morphPercent = v("curveMorph");
    s.mix = v("mix");
    s.maxDelayMs = v("maxDelayMs");
    s.shiftHz = v("shiftHz");
    s.shiftRateHz = v("shiftRateHz");
    s.fftSize = 16 << juce::jlimit(0, 6, int(v("stftSize")));
    s.inputTrimDb = v("inputTrimDb");
    s.outputTrimDb = v("outputTrimDb");
    return s;
}
void Processor::process(juce::AudioBuffer<float> &buffer, bool bypass)
{
    juce::ScopedNoDenormals guard;
    analyze(buffer);
    for (int remaining = curveFifo.getNumReady(); remaining > 0; --remaining)
    {
        int a, n, b, m;
        curveFifo.prepareToRead(1, a, n, b, m);
        if (n)
        {
            engine.setBanks(curveQueue[a]);
            curveFifo.finishedRead(1);
        }
    }
    engine.setSettings(readSettings());
    if (clearRequested.exchange(false, std::memory_order_acq_rel))
        engine.requestClear();
    if constexpr (SPECTRAL_DIAGNOSTIC)
    {
        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            float x = float(std::sin(sinePhase)) * std::pow(10.f, -18.f / 20);
            sinePhase = std::fmod(sinePhase + 2 * std::numbers::pi * 440 / getSampleRate(),
                                  2 * std::numbers::pi);
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                buffer.setSample(c, n, x);
        }
    }
    else
        engine.process(buffer.getArrayOfWritePointers(), buffer.getNumSamples(), bypass);
}
void Processor::processBlock(juce::AudioBuffer<float> &b, juce::MidiBuffer &)
{
    process(b, false);
}
void Processor::processBlockBypassed(juce::AudioBuffer<float> &b, juce::MidiBuffer &)
{
    process(b, true);
}
juce::AudioProcessorEditor *Processor::createEditor()
{
    return new Editor(*this);
}
void Processor::getStateInformation(juce::MemoryBlock &block)
{
    auto tree = state.copyState();
    tree.setProperty("version", 3, nullptr);
    tree.setProperty("curve", selectedCurve.load(), nullptr);
    tree.setProperty("channel", selectedChannel.load(), nullptr);
    tree.setProperty("linked", linked.load(), nullptr);
    auto banks = getBanks();
    tree.removeProperty("curves", nullptr);
    tree.setProperty("bankCount", banks.count, nullptr);
    tree.setProperty("selectedBank", selectedBank.load(), nullptr);
    juce::MemoryBlock data(banks.banks.data(), sizeof(banks.banks));
    tree.setProperty("banks", data, nullptr);
    juce::MemoryOutputStream stream(block, false);
    tree.writeToStream(stream);
}
void Processor::setStateInformation(const void *data, int size)
{
    auto tree = juce::ValueTree::readFromData(data, size);
    if (!tree.isValid() || !tree.hasType("SpectralDelay") ||
        (int(tree.getProperty("version", 0)) < 1 || int(tree.getProperty("version", 0)) > 3))
        return;
    const bool legacy = int(tree.getProperty("version", 0)) == 1;
    spectral::CurveBanks banks;
    auto curveData = tree.getProperty(legacy ? "curves" : "banks");
    auto *binary = curveData.getBinaryData();
    if (legacy && binary && binary->getSize() == sizeof(spectral::Curves))
    {
        spectral::Curves curves;
        std::memcpy(&curves, binary->getData(), sizeof(curves));
        for (auto &bank : banks.banks)
            bank = curves;
    }
    else if (!legacy && binary && binary->getSize() == sizeof(banks.banks))
        std::memcpy(banks.banks.data(), binary->getData(), sizeof(banks.banks));
    banks.count = legacy ? 4 : juce::jlimit(1, 4, int(tree.getProperty("bankCount", 4)));
    for (auto &bank : banks.banks)
        for (auto &channel : bank.values)
            for (int type = 0; type < 3; ++type)
                for (auto &value : channel[type])
                    value = std::isfinite(value) ? std::clamp(value, 0.f, type == 1 ? .99f : 1.f)
                                                 : (type == 2 ? 1.f : 0.f);
    if (legacy)
    {
        auto morph = tree.getChildWithProperty("id", "curveMorph");
        if (!morph.isValid())
        {
            morph = juce::ValueTree("PARAM");
            morph.setProperty("id", "curveMorph", nullptr);
            tree.addChild(morph, -1, nullptr);
        }
        morph.setProperty("value", 0.f, nullptr);
    }
    // Retire the copy engine without reusing its automation IDs for different effects.
    for (const char *id : {"copies", "spreadMs", "depth", "rateHz", "copyMode"})
        tree.removeChild(tree.getChildWithProperty("id", id), nullptr);
    if (int(tree.getProperty("version", 0)) < 3)
        for (const char *id : {"shiftHz", "shiftRateHz", "stftSize"})
        {
            auto node = tree.getChildWithProperty("id", id);
            if (!node.isValid())
            {
                node = juce::ValueTree("PARAM");
                node.setProperty("id", id, nullptr);
                tree.addChild(node, -1, nullptr);
            }
            auto *parameter = state.getParameter(id);
            node.setProperty("value", parameter->convertFrom0to1(parameter->getDefaultValue()),
                             nullptr);
        }
    // Reject nonfinite parameter payloads before they can reach delay-address calculations.
    for (auto child : tree)
    {
        if (auto *parameter = state.getParameter(child.getProperty("id").toString()))
        {
            float value = float(child.getProperty(
                "value", parameter->convertFrom0to1(parameter->getDefaultValue())));
            const auto &range = parameter->getNormalisableRange();
            value = std::isfinite(value) ? std::clamp(value, range.start, range.end)
                                         : parameter->convertFrom0to1(parameter->getDefaultValue());
            child.setProperty("value", value, nullptr);
        }
    }
    state.replaceState(tree);
    updateBanks(banks);
    selectedBank =
        legacy ? 0 : juce::jlimit(0, banks.count - 1, int(tree.getProperty("selectedBank", 0)));
    selectedCurve = juce::jlimit(0, 2, int(tree.getProperty("curve", 0)));
    selectedChannel = juce::jlimit(0, 1, int(tree.getProperty("channel", 0)));
    linked = bool(tree.getProperty("linked", false));
}
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new Processor;
}
