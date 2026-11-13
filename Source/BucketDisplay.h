#pragma once
#include "Engine.h"
namespace spectral
{
// Sample-rate-independent logical buckets for drawing and hit-testing. The analyzer and saved
// Nyquist-relative curve grid are resampled at each bucket's physical frequency.
struct BucketDisplay
{
    static constexpr double MinHz = 20., MaxHz = 20000.;
    int size;
    double sampleRate;
    float left, width;
    double spacing() const
    {
        return (MaxHz - MinHz) / (size / 2);
    }
    double frequency(int k) const
    {
        return MinHz + std::clamp(k, 0, size / 2) * spacing();
    }
    double boundaryFrequency(int edge) const
    {
        if (edge <= 0)
            return MinHz;
        if (edge > size / 2)
            return MaxHz;
        return .5 * (frequency(edge - 1) + frequency(edge));
    }
    float xAt(double frequency) const
    {
        return left + float(std::log(std::clamp(frequency, MinHz, MaxHz) / MinHz) /
                            std::log(MaxHz / MinHz)) *
                          width;
    }
    int binAt(float x) const
    {
        const double fraction = std::clamp((x - left) / width, 0.f, 1.f);
        const double frequency = MinHz * std::pow(MaxHz / MinHz, fraction);
        return std::clamp(int(std::round((frequency - MinHz) / spacing())), 0, size / 2);
    }
    std::pair<float, float> edges(int k) const
    {
        return {xAt(boundaryFrequency(k)), xAt(boundaryFrequency(k + 1))};
    }
    float curvePosition(int k) const
    {
        return float(std::clamp(frequency(k) * N / sampleRate, 0., double(B - 1)));
    }
    int curveIndex(int k) const
    {
        return std::clamp(int(std::round(curvePosition(k))), 0, B - 1);
    }
    std::pair<int, int> cell(int k) const
    {
        const double lo = boundaryFrequency(k) * N / sampleRate;
        const double hi = boundaryFrequency(k + 1) * N / sampleRate;
        int first = std::clamp(int(std::ceil(lo)), 0, B - 1);
        int last = std::clamp(int(std::floor(hi)), 0, B - 1);
        if (first > last)
            first = last = curveIndex(k);
        first = std::min(first, curveIndex(k));
        last = std::max(last, curveIndex(k));
        return {first, last};
    }
    float curveValue(const Curve &curve, int k) const
    {
        const float position = curvePosition(k);
        const int lo = int(position), hi = std::min(lo + 1, B - 1);
        return curve[lo] + (position - lo) * (curve[hi] - curve[lo]);
    }
    // Resample the independent analyzer over this logical bucket's frequency interval.
    float magnitude(const Curve &spectrum, int k) const
    {
        const double lo = std::max(0., boundaryFrequency(k) * N / sampleRate);
        const double hi = std::min(double(B - 1), boundaryFrequency(k + 1) * N / sampleRate);
        double power = 0, total = 0;
        for (int i = std::max(0, int(std::floor(lo))); i <= std::min(B - 1, int(std::ceil(hi)));
             ++i)
        {
            const double weight = std::max(0., std::min(hi, i + .5) - std::max(lo, i - .5));
            power += spectrum[i] * spectrum[i] * weight;
            total += weight;
        }
        // Normalize only sub-bin interpolation. Wider display cells retain integrated power.
        return total > 0 ? float(std::sqrt(power / (std::min(total, 1.) * 1.5))) : 0.f;
    }
};
} // namespace spectral
