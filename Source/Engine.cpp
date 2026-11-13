#include "Engine.h"
#include <numbers>
namespace spectral
{
static constexpr double tau = 2 * std::numbers::pi;
void Engine::prepare(double sampleRate, int count)
{
    sr = sampleRate;
    numChannels = count;
    for (int i = 0; i < 8; ++i)
        transforms[i] = std::make_unique<juce::dsp::FFT>(i + 4);
    // The smallest hop has the largest frame count and largest storage ratio (9/2).
    const size_t maxFrames = size_t(std::ceil(sr * 10 / 2)) + 4;
    historyEpoch.resize(maxFrames);
    for (auto &channel : channels)
        channel.history.resize(maxFrames * 9);
    for (int i = 0; i <= 4096; ++i)
        sineTable[i] = float(std::sin(tau * i / 4096));
    reset();
}
void Engine::configureSize(int requested)
{
    size = validSize(requested);
    step = size / 8;
    bins = size / 2 + 1;
    order = int(std::log2(size)) - 4;
    frames = int(std::ceil(sr * 10 / step)) + 4;
    for (int i = 0; i < size; ++i)
        window[i] = float(.5 - .5 * std::cos(tau * i / size));
    pos = hop = frame = 0;
    const auto positions = morphPositions();
    const auto offsets = shiftOffsets();
    for (int ch = 0; ch < 2; ++ch)
        for (int k = 0; k < bins; ++k)
        {
            const float index =
                CurveBanks::shiftedIndex(float(k * (B - 1)) / (bins - 1), offsets[ch], sr);
            channels[ch].delay[k] =
                banks.sample(ch, 0, index, positions[ch]) * settings.maxDelayMs * float(sr) / 1000;
            channels[ch].feedback[k] = banks.sample(ch, 1, index, positions[ch]);
            channels[ch].level[k] = banks.sample(ch, 2, index, positions[ch]);
        }
}
void Engine::reset()
{
    epoch = 1;
    std::fill(historyEpoch.begin(), historyEpoch.end(), 0);
    morphPosition = settings.morphPercent;
    morphDepth = settings.morphDepth;
    morphRate = settings.morphRateHz;
    morphStereoBlend = settings.stereoMorph ? 1.f : 0.f;
    morphHold = settings.morphRateHz == 0 ? 1.f : 0.f;
    morphPhase = std::numbers::pi / 2;
    clearPending = false;
    wetClearGain = 1;
    dryPos = alignPos = 0;
    lfoPhase = std::numbers::pi / 2;
    mix = settings.mix;
    inGain = std::pow(10.f, settings.inputTrimDb / 20);
    outGain = std::pow(10.f, settings.outputTrimDb / 20);
    amount = settings.shiftHz;
    rate = settings.shiftRateHz;
    shiftHold = settings.shiftRateHz == 0 ? 1.f : 0.f;
    configureSize(settings.fftSize);
    for (auto &channel : channels)
    {
        channel.dry.fill(0);
        channel.bypassDry.fill(0);
    }
    clearWetHistory();
}
void Engine::clearWetHistory()
{
    delayAnalyzer.reset();
    if (++epoch == 0)
    {
        std::fill(historyEpoch.begin(), historyEpoch.end(), 0);
        epoch = 1;
    }
    for (auto &channel : channels)
    {
        channel.input.fill(0);
        channel.ola.fill(0);
        channel.align.fill(0);
    }
    // Dry PDC, curve banks, smoothing, and modulation phases are preserved.
}
Engine::C Engine::rotation(float cycles) const
{
    cycles -= std::floor(cycles);
    float position = cycles * 4096;
    int i = int(position);
    float f = position - i;
    int j = (i + 1024) & 4095;
    return {sineTable[j] * (1 - f) + sineTable[j + 1] * f,
            sineTable[i] * (1 - f) + sineTable[i + 1] * f};
}
Engine::C Engine::read(const Channel &channel, int k, float samples, bool current, C now) const
{
    float d = std::clamp(samples / float(step), current ? 0.f : 1.f, float(frames - 2));
    int q = int(d);
    float a = d - q;
    auto get = [&](int age)
    {
        if (age == 0 && current)
            return now;
        size_t index = size_t((frame - age + frames) % frames);
        return historyEpoch[index] == epoch ? channel.history[index * bins + k] : C{};
    };
    float w = float(k) * step / size;
    C result = get(q) * rotation(-w * a) * (1 - a) + get(q + 1) * rotation(w * (1 - a)) * a;
    if (k == 0 || k == bins - 1)
        result = {result.real(), 0};
    return result;
}
void Engine::transform()
{
    // Only evaluate active FFT bins; a 16-sample window must not blend 1025 points per hop.
    const auto positions = morphPositions();
    const auto offsets = shiftOffsets();
    auto curveValue = [&](int ch, int type, float index)
    { return banks.sample(ch, type, index, positions[ch]); };
    float ds = 1 - std::exp(-float(step) / (float(sr) * .05f)),
          gs = 1 - std::exp(-float(step) / (float(sr) * .02f));
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto &channel = channels[ch];
        for (int i = 0; i < size; ++i)
            channel.fft[i] = channel.input[(pos + i) % size] * window[i];
        transforms[order]->performRealOnlyForwardTransform(channel.fft.data(), true);
        for (int k = 0; k < bins; ++k)
        {
            const float index =
                CurveBanks::shiftedIndex(float(k * (B - 1)) / (bins - 1), offsets[ch], sr);
            channel.delay[k] +=
                ds * (curveValue(ch, 0, index) * settings.maxDelayMs * float(sr) / 1000 -
                      channel.delay[k]);
            channel.feedback[k] += gs * (curveValue(ch, 1, index) - channel.feedback[k]);
            channel.level[k] += gs * (curveValue(ch, 2, index) - channel.level[k]);
            float delay = channel.delay[k];
            C feedback = read(channel, k, delay, false);
            C value = C(channel.fft[2 * k], channel.fft[2 * k + 1]) +
                      std::clamp(channel.feedback[k], 0.f, 1.f) * feedback;
            if (!std::isfinite(value.real()) || !std::isfinite(value.imag()))
                value = {};
            C output = (delay >= step ? feedback : read(channel, k, delay, true, value)) *
                       channel.level[k];
            channel.history[size_t(frame) * bins + k] = value;
            channel.fft[2 * k] = output.real();
            channel.fft[2 * k + 1] = output.imag();
        }
        transforms[order]->performRealOnlyInverseTransform(channel.fft.data());
        for (int i = 0; i < size; ++i)
            channel.ola[(pos + i) % size] += channel.fft[i] * window[i] / 3;
    }
    historyEpoch[size_t(frame)] = epoch;
    frame = (frame + 1) % frames;
}
void Engine::process(float *const *data, int count, bool bypass)
{
    juce::ScopedNoDenormals noDenormals;
    const float targetIn = std::pow(10.f, settings.inputTrimDb / 20),
                targetOut = std::pow(10.f, settings.outputTrimDb / 20);
    const float a = 1 - std::exp(-1.f / (float(sr) * .02f)), clearStep = 1.f / float(sr * .005);
    const int requestedSize = validSize(settings.fftSize);
    for (int n = 0; n < count; ++n)
    {
        if (requestedSize != size)
            clearPending = true;
        if (clearPending)
        {
            wetClearGain = std::max(0.f, wetClearGain - clearStep);
            if (wetClearGain == 0)
            {
                if (requestedSize != size)
                    configureSize(settings.fftSize);
                clearWetHistory();
                clearPending = false;
            }
        }
        else
            wetClearGain = std::min(1.f, wetClearGain + clearStep);
        morphPosition += a * (settings.morphPercent - morphPosition);
        morphDepth += a * (settings.morphDepth - morphDepth);
        morphRate += a * (settings.morphRateHz - morphRate);
        morphHold += a * ((settings.morphRateHz == 0 ? 1.f : 0.f) - morphHold);
        morphStereoBlend += a * ((settings.stereoMorph ? 1.f : 0.f) - morphStereoBlend);
        if (morphStereoBlend < 1e-7f)
            morphStereoBlend = 0;
        mix += a * (settings.mix - mix);
        inGain += a * (targetIn - inGain);
        outGain += a * (targetOut - outGain);
        amount += a * (settings.shiftHz - amount);
        rate += a * (settings.shiftRateHz - rate);
        shiftHold += a * ((settings.shiftRateHz == 0 ? 1.f : 0.f) - shiftHold);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto &channel = channels[ch];
            float original = data[ch][n], input = original * inGain, dry = channel.dry[dryPos],
                  rawDry = channel.bypassDry[dryPos];
            channel.dry[dryPos] = input;
            channel.bypassDry[dryPos] = original;
            channel.input[pos] = input;
            float wet = channel.ola[pos];
            channel.ola[pos] = 0;
            channel.align[alignPos] = wet;
            wet = channel.align[(alignPos + MaxFft - (MaxFft - size)) % MaxFft];
            delayAnalyzer.push(ch, wetClearGain * wet);
            data[ch][n] = bypass ? rawDry : ((1 - mix) * dry + mix * wetClearGain * wet) * outGain;
        }
        delayAnalyzer.advance(numChannels);
        dryPos = (dryPos + 1) % Latency;
        alignPos = (alignPos + 1) % MaxFft;
        lfoPhase = std::remainder(lfoPhase + tau * rate / sr, tau);
        morphPhase = std::remainder(morphPhase + tau * morphRate / sr, tau);
        pos = (pos + 1) % size;
        if (++hop == step)
        {
            hop = 0;
            transform();
        }
    }
}
} // namespace spectral
