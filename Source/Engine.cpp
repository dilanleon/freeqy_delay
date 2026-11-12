#include "Engine.h"
#include <numbers>
namespace spectral
{
static constexpr double tau = 2 * std::numbers::pi;
void Engine::prepare(double sampleRate, int count)
{
    sr = sampleRate;
    numChannels = count;
    for (int i = 0; i < 7; ++i)
        transforms[i] = std::make_unique<juce::dsp::FFT>(i + 4);
    // The smallest hop has the largest frame count and largest storage ratio (9/2).
    const size_t maxFrames = size_t(std::ceil(sr * 10 / 2)) + 4;
    historyEpoch.resize(maxFrames);
    for (auto &channel : channels)
        channel.history.resize(maxFrames * 9);
    for (int i = 0; i <= 4096; ++i)
        sineTable[i] = float(std::sin(tau * i / 4096));
    // Windowed ideal Hilbert FIR; reverse coefficients for contiguous SIMD dot products.
    for (int j = 0; j < ShifterLength; ++j)
    {
        const int m = j - ShifterDelay;
        const double w =
            j < 511 ? .42 - .5 * std::cos(tau * j / 510) + .08 * std::cos(2 * tau * j / 510) : 0;
        hilbert[ShifterLength - 1 - j] =
            (m != 0 && (std::abs(m) & 1)) ? float(2 * w / (std::numbers::pi * m)) : 0.f;
    }
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
    banks.blend(morphPosition, curves);
    for (int ch = 0; ch < 2; ++ch)
        for (int k = 0; k < bins; ++k)
        {
            int index = k * (B - 1) / (bins - 1);
            channels[ch].delay[k] =
                curves.values[ch][0][index] * settings.maxDelayMs * float(sr) / 1000;
            channels[ch].feedback[k] = curves.values[ch][1][index];
            channels[ch].level[k] = curves.values[ch][2][index];
        }
}
void Engine::reset()
{
    epoch = 1;
    std::fill(historyEpoch.begin(), historyEpoch.end(), 0);
    morphPosition = settings.morphPercent;
    configureSize(settings.fftSize);
    clearPending = false;
    wetClearGain = 1;
    dryPos = alignPos = shiftPos = 0;
    lfoPhase = std::numbers::pi / 2;
    carrierPhase = 0;
    mix = settings.mix;
    inGain = std::pow(10.f, settings.inputTrimDb / 20);
    outGain = std::pow(10.f, settings.outputTrimDb / 20);
    amount = settings.shiftHz;
    rate = settings.shiftRateHz;
    shiftBlend = amount > 0 ? 1.f : 0.f;
    for (auto &channel : channels)
    {
        channel.dry.fill(0);
        channel.bypassDry.fill(0);
    }
    clearWetHistory();
}
void Engine::clearWetHistory()
{
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
        channel.shifter.fill(0);
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
    banks.blend(morphPosition, curves);
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
            int index = k * (B - 1) / (bins - 1);
            channel.delay[k] +=
                ds * (curves.values[ch][0][index] * settings.maxDelayMs * float(sr) / 1000 -
                      channel.delay[k]);
            channel.feedback[k] += gs * (curves.values[ch][1][index] - channel.feedback[k]);
            channel.level[k] += gs * (curves.values[ch][2][index] - channel.level[k]);
            float delay = channel.delay[k];
            C feedback = read(channel, k, delay, false);
            C value = C(channel.fft[2 * k], channel.fft[2 * k + 1]) +
                      std::clamp(channel.feedback[k], 0.f, .99f) * feedback;
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
        mix += a * (settings.mix - mix);
        inGain += a * (targetIn - inGain);
        outGain += a * (targetOut - outGain);
        amount += a * (settings.shiftHz - amount);
        rate += a * (settings.shiftRateHz - rate);
        shiftBlend += a * ((settings.shiftHz > 0 ? 1.f : 0.f) - shiftBlend);
        if (shiftBlend < 1e-7f)
            shiftBlend = 0;
        const double angle = carrierPhase;
        float cosine = float(std::cos(angle)), sine = float(std::sin(angle));
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
            channel.shifter[shiftPos] = channel.shifter[shiftPos + ShifterLength] = wet;
            float inPhase =
                channel.shifter[(shiftPos + ShifterLength - ShifterDelay) % ShifterLength];
            float shifted = inPhase;
            if (shiftBlend > 0)
            {
                const float *signal = channel.shifter.data() + shiftPos + 1;
                float q0 = 0, q1 = 0, q2 = 0, q3 = 0;
                // Every second Hilbert coefficient is exactly zero.
                for (int j = 1; j < ShifterLength; j += 8)
                {
                    q0 += signal[j] * hilbert[j];
                    q1 += signal[j + 2] * hilbert[j + 2];
                    q2 += signal[j + 4] * hilbert[j + 4];
                    q3 += signal[j + 6] * hilbert[j + 6];
                }
                float quadrature = q0 + q1 + q2 + q3;
                shifted = inPhase * cosine - quadrature * (ch == 0 ? sine : -sine);
            }
            wet = inPhase + shiftBlend * (shifted - inPhase);
            data[ch][n] = bypass ? rawDry : ((1 - mix) * dry + mix * wetClearGain * wet) * outGain;
        }
        dryPos = (dryPos + 1) % Latency;
        alignPos = (alignPos + 1) % MaxFft;
        shiftPos = (shiftPos + 1) % ShifterLength;
        carrierPhase = std::remainder(carrierPhase + tau * amount * std::sin(lfoPhase) / sr, tau);
        lfoPhase = std::remainder(lfoPhase + tau * rate / sr, tau);
        pos = (pos + 1) % size;
        if (++hop == step)
        {
            hop = 0;
            transform();
        }
    }
}
} // namespace spectral
