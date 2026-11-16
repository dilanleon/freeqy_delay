#pragma once
#include <array>
#include <juce_dsp/juce_dsp.h>
#include <numbers>
namespace spectral
{
using StereoSpectrum = std::array<std::array<float, 1025>, 2>;
// Preallocated, independent stereo analyzer, shared by input and delay-return metering.
class SpectrumAnalyzer
{
  public:
    // Cache the periodic Hann window for stereo analysis.
    SpectrumAnalyzer()
    {
        for (int i = 0; i < 2048; ++i)
            window[i] = float(.5 - .5 * std::cos(2 * std::numbers::pi * i / 2048));
    }
    // Clear captured samples and published magnitudes.
    void reset()
    {
        for (auto &channel : input)
            channel.fill(0);
        for (auto &channel : values)
            channel.fill(0);
        position = hop = 0;
    }
    // Write one channel sample into the current analysis position.
    void push(int channel, float sample)
    {
        input[channel][position] = sample;
    }
    // Publish a stereo 2048-point spectrum every 256 samples.
    bool advance(int channels)
    {
        // Keep a complete two-channel frame for the GUI even when the host runs mono.
        if (channels == 1)
            input[1][position] = input[0][position];
        position = (position + 1) % 2048;
        if (++hop < 256)
            return false;
        hop = 0;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < 2048; ++i)
                scratch[i] = input[ch][(position + i) % 2048] * window[i];
            fft.performFrequencyOnlyForwardTransform(scratch.data());
            for (int i = 0; i < 1025; ++i)
                values[ch][i] = scratch[i] / 512.f;
        }
        return true;
    }
    // Return the most recently completed stereo spectrum.
    const StereoSpectrum &latest() const
    {
        return values;
    }

  private:
    juce::dsp::FFT fft{11};
    std::array<std::array<float, 2048>, 2> input{};
    std::array<float, 4096> scratch{};
    std::array<float, 2048> window{};
    StereoSpectrum values{};
    int position = 0, hop = 0;
};
} // namespace spectral
