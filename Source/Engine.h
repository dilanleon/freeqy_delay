#pragma once
#include "SpectrumAnalyzer.h"
#include <array>
#include <complex>
#include <juce_dsp/juce_dsp.h>
#include <vector>
namespace spectral
{
constexpr int N = 2048, H = 256, B = N / 2 + 1;
using Curve = std::array<float, B>;
struct Curves
{
    std::array<std::array<Curve, 3>, 2> values{};
    Curves()
    {
        for (auto &ch : values)
        {
            ch[0].fill(.25f);
            ch[1].fill(.35f);
            ch[2].fill(1.f);
        }
    }
};
// Four complete stereo curve snapshots. Interpolation is allocation-free.
struct CurveBanks
{
    std::array<Curves, 4> banks;
    int count = 4;
    // Frequency-axis lookup; edge values are held outside DC/Nyquist.
    float sample(int ch, int type, float index, float percent) const
    {
        index = std::clamp(index, 0.f, float(B - 1));
        const int lo = int(index), hi = std::min(lo + 1, B - 1);
        const float f = index - lo;
        const float position =
            std::clamp(percent, 0.f, 100.f) * (std::clamp(count, 1, 4) - 1) / 100.f;
        const int a = int(position), b = std::min(a + 1, std::clamp(count, 1, 4) - 1);
        auto at = [&](int bank)
        {
            const auto &c = banks[bank].values[ch][type];
            return c[lo] + f * (c[hi] - c[lo]);
        };
        return at(a) + (position - a) * (at(b) - at(a));
    }
    static float shiftedIndex(float index, float offsetHz, double sampleRate)
    {
        return std::clamp(index - offsetHz * float(N / sampleRate), 0.f, float(B - 1));
    }
    void blend(float percent, Curves &result) const
    {
        const int active = std::clamp(count, 1, 4);
        const float position = std::clamp(percent, 0.f, 100.f) * (active - 1) / 100.f;
        const int first = int(position), second = std::min(first + 1, active - 1);
        const float fraction = position - first;
        for (int ch = 0; ch < 2; ++ch)
            for (int type = 0; type < 3; ++type)
                for (int k = 0; k < B; ++k)
                    result.values[ch][type][k] = banks[first].values[ch][type][k] * (1 - fraction) +
                                                 banks[second].values[ch][type][k] * fraction;
    }
};
// Curve storage and analyzer resolution stay fixed for preset compatibility.
constexpr int MaxFft = 2048, Latency = MaxFft;
struct Settings
{
    float mix = .5f, maxDelayMs = 1000, shiftHz = 0, shiftRateHz = .3f, inputTrimDb = 0,
          outputTrimDb = 0, morphPercent = 0, morphDepth = 25, morphRateHz = .3f;
    bool stereoMorph = false;
    int fftSize = 1024;
};
class Engine
{
  public:
    void prepare(double, int);
    void reset();
    void requestClear()
    {
        clearPending = true;
    }
    void setSettings(Settings value)
    {
        settings = value;
    }
    void setCurves(const Curves &value)
    {
        for (auto &bank : banks.banks)
            bank = value;
        banks.count = 1;
    }
    void setBanks(const CurveBanks &value)
    {
        banks = value;
    }
    void process(float *const *, int, bool bypass = false);
    static std::array<float, 2> stereoPositions(float center, float depth, float movement)
    {
        auto wrap = [](float value) { return value - 100.f * std::floor(value / 100.f); };
        return {wrap(center + depth * movement), wrap(center - depth * movement)};
    }
    std::array<float, 2> morphPositions() const
    {
        if (morphStereoBlend == 0 || morphDepth == 0)
            return {morphPosition, morphPosition};
        return stereoPositions(morphPosition, morphDepth,
                               morphStereoBlend *
                                   float(std::sin(morphPhase) * (1 - morphHold) + morphHold));
    }
    std::array<float, 2> shiftOffsets() const
    {
        const float offset = amount * float(std::sin(lfoPhase) * (1 - shiftHold) + shiftHold);
        return {offset, -offset};
    }
    const StereoSpectrum &delaySpectrum() const
    {
        return delayAnalyzer.latest();
    }
    int activeSize() const
    {
        return size;
    }

  private:
    using C = std::complex<float>;
    struct Channel
    {
        std::array<float, MaxFft> input{}, ola{}, align{};
        std::array<float, Latency> dry{}, bypassDry{};
        std::array<float, 2 * MaxFft> fft{};
        std::array<float, MaxFft / 2 + 1> delay{}, feedback{}, level{};
        std::vector<C> history;
    };
    std::array<Channel, 2> channels;
    std::array<std::unique_ptr<juce::dsp::FFT>, 8> transforms;
    std::array<float, MaxFft> window{};
    std::array<float, 4097> sineTable{};
    std::vector<uint64_t> historyEpoch;
    uint64_t epoch = 1;
    Settings settings;
    SpectrumAnalyzer delayAnalyzer;
    CurveBanks banks;
    double sr = 48000, lfoPhase = 0, morphPhase = 0;
    float morphDepth = 25, morphRate = .3f, morphStereoBlend = 0, morphHold = 0;
    int numChannels = 2, size = 1024, step = 128, bins = 513, order = 6, frames = 0;
    int pos = 0, hop = 0, frame = 0, dryPos = 0, alignPos = 0;
    float mix = .5f, inGain = 1, outGain = 1, morphPosition = 0, amount = 0, rate = .3f,
          shiftHold = 0, wetClearGain = 1;
    bool clearPending = false;
    void configureSize(int);
    void clearWetHistory();
    void transform();
    C rotation(float) const;
    C read(const Channel &, int, float, bool, C = {}) const;
    static int validSize(int value)
    {
        return 1 << std::clamp(int(std::log2(std::max(16, value))), 4, 11);
    }
};
} // namespace spectral
