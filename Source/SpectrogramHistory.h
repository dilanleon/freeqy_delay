#pragma once
#include "BucketDisplay.h"
#include "FreeqyPalette.h"
#include <juce_graphics/juce_graphics.h>
namespace spectral
{
// GUI-owned variable-duration history. Never used or allocated by the audio thread.
class SpectrogramHistory
{
  public:
    static constexpr int Rows = 180;
    SpectrogramHistory() : frames(Rows)
    {
        clear();
    }
    static double secondsForSpeed(int index)
    {
        constexpr double seconds[] = {1.5, 3., 6., 12.};
        return seconds[std::clamp(index, 0, 3)];
    }
    void setSpeed(int index)
    {
        const double seconds = secondsForSpeed(index);
        if (duration != seconds)
        {
            duration = seconds;
            clear();
        }
    }
    double secondsPerRow() const
    {
        return duration / Rows;
    }
    void clear()
    {
        for (auto &frame : frames)
            for (auto &channel : frame)
                channel.fill(0);
        for (auto &channel : held)
            channel.fill(0);
        head = 0;
        lastTick = -1;
        lastFresh = -1;
        dirty.fill(true);
    }
    void append(double now, double rate, const StereoSpectrum *incoming)
    {
        if (rate != sampleRate)
        {
            clear();
            sampleRate = rate;
        }
        const auto tick = int64_t(std::floor(now / secondsPerRow()));
        if (lastTick >= 0 && tick < lastTick)
            clear();
        if (incoming)
        {
            held = *incoming;
            lastFresh = now;
        }
        const StereoSpectrum *source = (lastFresh >= 0 && now - lastFresh <= .15) ? &held : nullptr;
        if (tick == lastTick)
        {
            if (incoming)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int k = 0; k < B; ++k)
                        frames[head][ch][k] = std::max(frames[head][ch][k], (*incoming)[ch][k]);
                dirty[head] = true;
            }
            return;
        }
        const int steps = lastTick < 0 ? 1 : int(std::min<int64_t>(Rows, tick - lastTick));
        for (int n = 0; n < steps; ++n)
        {
            head = (head + Rows - 1) % Rows;
            for (auto &channel : frames[head])
                channel.fill(0);
            dirty[head] = true;
        }
        if (source)
            frames[head] = *source;
        lastTick = tick;
    }
    const StereoSpectrum &atAge(int age) const
    {
        return frames[(head + std::clamp(age, 0, Rows - 1)) % Rows];
    }
    const juce::Image &image(int width, int channel)
    {
        width = std::max(1, width);
        channel = std::clamp(channel, 0, 1);
        BucketDisplay layout{N, sampleRate, 0, float(width)};
        if (!raster.isValid() || width != raster.getWidth() || channel != cachedChannel ||
            sampleRate != cachedRate)
        {
            raster = juce::Image(juce::Image::ARGB, width, Rows, true);
            dirty.fill(true);
            cachedChannel = channel;
            cachedRate = sampleRate;
            levels.resize(size_t(width));
            for (int k = 0; k < B; ++k)
            {
                const auto [left, right] = layout.edges(k);
                bounds[k] = {std::max(0, int(std::floor(left))),
                             std::min(width - 1, int(std::ceil(right)) - 1)};
            }
        }
        juce::Image::BitmapData pixels(raster, juce::Image::BitmapData::writeOnly);
        for (int row = 0; row < Rows; ++row)
        {
            if (!dirty[row])
                continue;
            std::fill(levels.begin(), levels.end(), 0);
            for (int k = 0; k < B; ++k)
            {
                float gain = layout.magnitude(frames[row][channel], k);
                int intensity = int(std::round(
                    std::clamp((juce::Decibels::gainToDecibels(gain, -90.f) + 90) / 90, 0.f, 1.f) *
                    255));
                const auto [first, last] = bounds[k];
                for (int x = first; x <= last; ++x)
                    levels[size_t(x)] = std::max(levels[size_t(x)], uint8_t(intensity));
            }
            for (int x = 0; x < width; ++x)
                pixels.setPixelColour(x, row, color(levels[size_t(x)]));
            dirty[row] = false;
        }
        return raster;
    }
    void draw(juce::Graphics &g, juce::Rectangle<int> target, int channel)
    {
        const auto &source = image(target.getWidth(), channel);
        const int firstRows = Rows - head,
                  firstHeight = int(std::round(target.getHeight() * float(firstRows) / Rows));
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        if (firstHeight > 0)
            g.drawImage(source, target.getX(), target.getY(), target.getWidth(), firstHeight, 0,
                        head, source.getWidth(), firstRows);
        if (head > 0 && firstHeight < target.getHeight())
            g.drawImage(source, target.getX(), target.getY() + firstHeight, target.getWidth(),
                        target.getHeight() - firstHeight, 0, 0, source.getWidth(), head);
    }

  public:
    static juce::Colour color(uint8_t intensity)
    {
        // Reference palette: ink → indigo → blue → cyan. Keep black at silence
        // so low-level spectral detail remains readable against the panel.
        if (intensity <= 85)
            return freeqy::ink.interpolatedWith(freeqy::indigo, intensity / 85.f);
        if (intensity <= 170)
            return freeqy::indigo.interpolatedWith(freeqy::blue, (intensity - 85) / 85.f);
        return freeqy::blue.interpolatedWith(freeqy::cyan, (intensity - 170) / 85.f);
    }

  private:
    std::vector<StereoSpectrum> frames;
    StereoSpectrum held{};
    int head = 0, cachedChannel = -1;
    int64_t lastTick = -1;
    double lastFresh = -1, sampleRate = 48000, cachedRate = 0, duration = 6;
    std::array<bool, Rows> dirty{};
    std::array<std::pair<int, int>, B> bounds{};
    std::vector<uint8_t> levels;
    juce::Image raster;
};
} // namespace spectral
