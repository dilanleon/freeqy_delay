#include "PluginProcessor.h"
#include "PluginEditor.h"
// Define stable automatable host parameters and defaults.
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
    add("shiftHz", "Stereo Curve Shift Amount (Hz)", 0, 1000, 0, .5f);
    add("shiftRateHz", "Stereo Curve Shift Rate (Hz)", 0, 10, .3f, .5f);
    add("inputTrimDb", "Input (dB)", -24, 24, 0);
    add("outputTrimDb", "Output (dB)", -24, 24, 0);
    p.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"stftSize", 1}, "STFT Size (samples)",
        juce::StringArray{"16", "32", "64", "128", "256", "512", "1024", "2048"}, 6));
    p.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"curveMorph", 1}, "Curve Morph (%)",
        juce::NormalisableRange<float>(0, 100), 0,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction([](float value, int) { return juce::String(value, 1); })
            .withValueFromStringFunction([](const juce::String &text)
                                         { return text.getFloatValue(); })));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"stereoMorph", 1},
                                                     "Stereo Morph", false));
    add("morphDepth", "Stereo Morph Depth (%)", 0, 100, 0);
    add("morphRateHz", "Stereo Morph Rate (Hz)", 0, 10, .3f, .5f);
    return p;
}
// Initialize the host processor and its versioned APVTS root.
Processor::Processor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "SpectralDelay", layout())
{
    startTimerHz(30);
}
// Accept matching mono or stereo input and output layouts.
bool Processor::isBusesLayoutSupported(const BusesLayout &l) const
{
    return l.getMainInputChannelSet() == l.getMainOutputChannelSet() &&
           (l.getMainOutputChannelSet() == juce::AudioChannelSet::mono() ||
            l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
}
// Allocate DSP state and establish the fixed host latency.
void Processor::prepareToPlay(double sr, int)
{
    {
        std::lock_guard lock(modelMutex);
        if (pointRate != sr)
        {
            pointModel = {};
            pointRate = sr;
            modelRevision.fetch_add(1, std::memory_order_release);
        }
        preparedRate.store(sr, std::memory_order_release);
    }
    engine.setSettings(readSettings());
    engine.prepare(sr, getTotalNumInputChannels());
    engine.setBanks(getBanks());
    engine.reset();
    for (int ch = 0; ch < 2; ++ch)
    {
        liveMorph[ch].store(engine.morphPositions()[ch], std::memory_order_relaxed);
        liveShift[ch].store(engine.shiftOffsets()[ch], std::memory_order_relaxed);
    }
    inputAnalyzer.reset();
    spectrumReady = false;
    spectrumFifo.reset();
    meterSamples = 0;
    meterSampleRate = sr;
    latestMeterSeconds.store(0, std::memory_order_release);
    setLatencySamples(spectral::Latency);
}
// Copy GUI point metadata under the model lock.
spectral::PointCurves Processor::getPointCurves()
{
    std::lock_guard lock(modelMutex);
    return pointModel;
}
// Replace GUI point metadata without touching DSP state.
void Processor::setPointCurves(const spectral::PointCurves &points, double sourceRate)
{
    std::lock_guard lock(modelMutex);
    const double current = preparedRate.load(std::memory_order_acquire);
    if (sourceRate > 0 && current > 0 && sourceRate != current)
        return;
    pointModel = points;
    pointRate = current > 0 ? current : (sourceRate > 0 ? sourceRate : pointRate);
}
// Read point metadata provenance under the model lock.
double Processor::pointSampleRate()
{
    std::lock_guard lock(modelMutex);
    return pointRate;
}
// Read the selected edit-bank curves under the model lock.
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
    updateTailFlag();
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
// Copy all stored banks under the model lock.
spectral::CurveBanks Processor::getBanks()
{
    std::lock_guard lock(modelMutex);
    return model;
}
// Replace stored banks and schedule an audio-thread snapshot.
void Processor::updateBanks(const spectral::CurveBanks &banks)
{
    {
        std::lock_guard lock(modelMutex);
        model = banks;
        model.count = juce::jlimit(1, 4, model.count);
        updateTailFlag();
        pending = true;
    }
    publishPending();
}
// Called only while the non-audio model mutex is held.
void Processor::updateTailFlag()
{
    bool holds = false;
    for (int b = 0; b < model.count; ++b)
        for (const auto &channel : model.banks[b].values)
            for (int i = 0; i < spectral::B; ++i)
                holds = holds || channel[1][i] >= 1.f;
    infiniteFeedback.store(holds, std::memory_order_relaxed);
}
// Retry bounded curve publication off the audio thread.
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
// Feed pre-effect samples to the stereo input analyzer.
void Processor::analyze(const juce::AudioBuffer<float> &buffer, int offset, int count)
{
    for (int n = offset; n < offset + count; ++n)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            inputAnalyzer.push(ch, buffer.getSample(ch, n));
        spectrumReady = inputAnalyzer.advance(buffer.getNumChannels()) || spectrumReady;
    }
}
// Read the next non-stale analyzer frame in timestamp order.
bool Processor::nextSpectrum(Spectrum &out)
{
    // A visual reset can race a block already in flight. Such a block may enqueue
    // afterward, so reject by epoch as well as draining what is currently queued.
    for (;;)
    {
        int a, n, b, m;
        spectrumFifo.prepareToRead(1, a, n, b, m);
        if (!n)
            return false;
        out = spectrumQueue[a];
        spectrumFifo.finishedRead(1);
        if (out.epoch == spectrumEpoch.load(std::memory_order_acquire))
            return true;
    }
}
// Read the newest available analyzer frame.
bool Processor::spectrum(Spectrum &out)
{
    bool found = false;
    while (nextSpectrum(out))
        found = true;
    return found;
}
// Drain queued analyzer frames without resetting the producer.
void Processor::discardSpectra()
{
    Spectrum discarded;
    for (int remaining = spectrumFifo.getNumReady(); remaining > 0; --remaining)
        if (!nextSpectrum(discarded))
            break;
}
// Invalidate in-flight frames and empty visual transfer.
void Processor::restartSpectra()
{
    spectrumEpoch.fetch_add(1, std::memory_order_acq_rel);
    discardSpectra();
}
// Bound GUI work and optionally discard stalled frames.
int Processor::drainSpectra(Spectrum *out, int capacity, double maxAgeSeconds)
{
    // Compare against audio time, not the GUI clock: hosts can pause without
    // advancing either analyzer. Snapshot the FIFO count to bound one GUI drain.
    const double newest = latestMeterSeconds.load(std::memory_order_acquire);
    Spectrum frame;
    int count = 0;
    for (int remaining = spectrumFifo.getNumReady(); remaining > 0; --remaining)
    {
        if (!nextSpectrum(frame))
            break;
        if (newest - frame.seconds <= maxAgeSeconds && count < capacity)
            out[count++] = frame;
    }
    return count;
}
// Snapshot atomic host parameters for one audio block.
spectral::Settings Processor::readSettings() const
{
    spectral::Settings s;
    auto v = [&](const char *id) { return state.getRawParameterValue(id)->load(); };
    s.morphPercent = v("curveMorph");
    s.stereoMorph = v("stereoMorph") > .5f;
    s.morphDepth = v("morphDepth");
    s.morphRateHz = v("morphRateHz");
    s.mix = v("mix");
    s.maxDelayMs = v("maxDelayMs");
    s.shiftHz = v("shiftHz");
    s.shiftRateHz = v("shiftRateHz");
    s.fftSize = 16 << juce::jlimit(0, 7, int(v("stftSize")));
    s.inputTrimDb = v("inputTrimDb");
    s.outputTrimDb = v("outputTrimDb");
    return s;
}
// Bridge host buffers, control snapshots, DSP, and analyzer transfer.
void Processor::process(juce::AudioBuffer<float> &buffer, bool bypass)
{
    juce::ScopedNoDenormals guard;
    // One tag covers the whole host block, including hops enqueued after a GUI reset.
    const uint64_t captureEpoch = spectrumEpoch.load(std::memory_order_acquire);
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
    // Preserve every analyzer hop, including several hops within a large host block.
    for (int offset = 0; offset < buffer.getNumSamples();)
    {
        const int count = std::min(256 - int(meterSamples % 256), buffer.getNumSamples() - offset);
        analyze(buffer, offset, count);
        float *data[] = {buffer.getWritePointer(0, offset),
                         buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, offset)
                                                     : nullptr};
        engine.process(data, count, bypass);
        meterSamples += count;
        if (spectrumReady)
        {
            int a, n, b, m;
            spectrumFifo.prepareToWrite(1, a, n, b, m);
            // The wet analyzer still contains pre-clear samples during the fade.
            if (n && !engine.wetClearActive())
            {
                spectrumQueue[a].input = inputAnalyzer.latest();
                spectrumQueue[a].delay = engine.delaySpectrum();
                spectrumQueue[a].seconds = double(meterSamples) / meterSampleRate;
                spectrumQueue[a].epoch = captureEpoch;
                spectrumFifo.finishedWrite(1);
            }
            spectrumReady = false;
        }
        offset += count;
    }
    latestMeterSeconds.store(double(meterSamples) / meterSampleRate, std::memory_order_release);
    for (int ch = 0; ch < 2; ++ch)
    {
        liveMorph[ch].store(engine.morphPositions()[ch], std::memory_order_relaxed);
        liveShift[ch].store(engine.shiftOffsets()[ch], std::memory_order_relaxed);
    }
}
// Process normal audio through the spectral engine.
void Processor::processBlock(juce::AudioBuffer<float> &b, juce::MidiBuffer &)
{
    process(b, false);
}
// Process bypass with the same dry-path latency.
void Processor::processBlockBypassed(juce::AudioBuffer<float> &b, juce::MidiBuffer &)
{
    process(b, true);
}
// Construct the native JUCE editor.
juce::AudioProcessorEditor *Processor::createEditor()
{
    return new Editor(*this);
}
// Serialize version 4 parameters, banks, and editor state.
void Processor::getStateInformation(juce::MemoryBlock &block)
{
    auto tree = state.copyState();
    tree.setProperty("version", 4, nullptr);
    tree.setProperty("curve", selectedCurve.load(), nullptr);
    tree.setProperty("pointMode", pointMode.load(), nullptr);
    auto points = getPointCurves();
    tree.setProperty("pointCurves", juce::MemoryBlock(&points, sizeof(points)), nullptr);
    tree.setProperty("pointSampleRate", pointSampleRate(), nullptr);
    tree.setProperty("channel", selectedChannel.load(), nullptr);
    tree.setProperty("linked", linked.load(), nullptr);
    tree.setProperty("spectrogramSpeed", spectrogramSpeed.load(), nullptr);
    auto banks = getBanks();
    tree.setProperty("bankCount", banks.count, nullptr);
    tree.setProperty("selectedBank", selectedBank.load(), nullptr);
    juce::MemoryBlock data(banks.banks.data(), sizeof(banks.banks));
    tree.setProperty("banks", data, nullptr);
    juce::MemoryOutputStream stream(block, false);
    tree.writeToStream(stream);
}
// Validate and restore a version 4 state payload.
void Processor::setStateInformation(const void *data, int size)
{
    auto tree = juce::ValueTree::readFromData(data, size);
    // State loading is v4-only. Reject unsupported payloads before replacing APVTS
    // or changing any curve/editor model, leaving the current instance intact.
    if (!tree.isValid() || !tree.hasType("SpectralDelay") ||
        int(tree.getProperty("version", 0)) != 4)
        return;
    spectral::CurveBanks banks;
    auto curveData = tree.getProperty("banks");
    auto *binary = curveData.getBinaryData();
    if (binary && binary->getSize() == sizeof(banks.banks))
        std::memcpy(banks.banks.data(), binary->getData(), sizeof(banks.banks));
    banks.count = juce::jlimit(1, 4, int(tree.getProperty("bankCount", 4)));
    for (auto &bank : banks.banks)
        for (auto &channel : bank.values)
            for (int type = 0; type < 3; ++type)
                for (auto &value : channel[type])
                    value = std::isfinite(value) ? std::clamp(value, 0.f, 1.f)
                                                 : (type == 2 ? 1.f : 0.f);
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
    spectral::PointCurves points;
    auto pointData = tree.getProperty("pointCurves");
    if (auto *bytes = pointData.getBinaryData(); bytes && bytes->getSize() == sizeof(points))
    {
        std::memcpy(&points, bytes->getData(), sizeof(points));
        for (auto &bank : points.banks)
            for (auto &channel : bank)
                for (auto &curve : channel)
                {
                    if (curve.count < 2 || curve.count > spectral::PointCurve::capacity) { curve.count = 0; continue; }
                    float previous = -1.f;
                    for (int i = 0; i < curve.count; ++i)
                    {
                        auto &pt = curve.points[i];
                        if (!std::isfinite(pt.x) || !std::isfinite(pt.value) || !std::isfinite(pt.shape) ||
                            pt.x < 0 || pt.x > 1 || pt.x <= previous)
                        { curve.count = 0; break; }
                        previous = pt.x;
                        pt.value = std::clamp(pt.value, 0.f, 1.f);
                        pt.shape = std::clamp(pt.shape, -1.f, 1.f);
                    }
                }
    }
    const double savedRate = double(tree.getProperty("pointSampleRate", 48000.0));
    const double currentRate = preparedRate.load(std::memory_order_acquire);
    if (!std::isfinite(savedRate) || savedRate <= 0 ||
        (currentRate > 0 && currentRate != savedRate))
        points = {};
    setPointCurves(points, currentRate > 0 ? currentRate : savedRate);
    pointMode = bool(tree.getProperty("pointMode", false));
    state.replaceState(tree);
    updateBanks(banks);
    selectedBank = juce::jlimit(0, banks.count - 1, int(tree.getProperty("selectedBank", 0)));
    selectedCurve = juce::jlimit(0, 2, int(tree.getProperty("curve", 0)));
    selectedChannel = juce::jlimit(0, 1, int(tree.getProperty("channel", 0)));
    linked = bool(tree.getProperty("linked", false));
    // Some hosts resend parameter-only state during transport changes. Preserve the
    // current display choice when that host payload omits this editor property.
    if (tree.hasProperty("spectrogramSpeed"))
        spectrogramSpeed = juce::jlimit(0, 3, int(tree.getProperty("spectrogramSpeed")));
    modelRevision.fetch_add(1, std::memory_order_release);
}
// Create the processor instance requested by JUCE hosts.
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new Processor;
}
