#pragma once
#include "Engine.h"
#include <algorithm>
#include <cmath>

namespace spectral
{
// Point coordinates use the logarithmic 20 Hz–20 kHz display axis. The shape belongs
// to the segment following the point: negative bends one way, positive the other.
struct CurvePoint { float x = 0, value = 0, shape = 0; };
struct PointCurve
{
    static constexpr int capacity = 64;
    int count = 0;
    std::array<CurvePoint, capacity> points{};
    static PointCurve fromCurve(const Curve &source, double sampleRate)
    {
        auto sampleAt = [&](double hz)
        {
            const float index = float(std::clamp(hz * N / sampleRate, 0., double(B - 1)));
            const int lo = int(index), hi = std::min(lo + 1, B - 1);
            return source[lo] + (index - lo) * (source[hi] - source[lo]);
        };
        std::array<CurvePoint, B + 2> candidates{};
        int available = 0;
        candidates[available++] = {0, sampleAt(20.), 0};
        for (int i = 1; i < B; ++i)
        {
            const double hz = i * sampleRate / N;
            if (hz > 20. && hz < 20000.)
                candidates[available++] =
                    {float(std::log(hz / 20.) / std::log(1000.)), source[i], 0};
        }
        candidates[available++] = {1, sampleAt(20000.), 0};

        // Add the point with the largest interpolation error until the visible
        // curve is represented or the editor-state capacity is reached.
        std::array<int, capacity> chosen{};
        chosen[0] = 0;
        chosen[1] = available - 1;
        PointCurve result;
        result.count = 2;
        while (result.count < capacity)
        {
            float largestError = .001f;
            int bestSegment = -1, bestCandidate = -1;
            for (int segment = 0; segment < result.count - 1; ++segment)
            {
                const auto &a = candidates[chosen[segment]];
                const auto &b = candidates[chosen[segment + 1]];
                for (int i = chosen[segment] + 1; i < chosen[segment + 1]; ++i)
                {
                    const auto &candidate = candidates[i];
                    const float fraction = (candidate.x - a.x) / (b.x - a.x);
                    const float error = std::abs(candidate.value -
                                                 (a.value + fraction * (b.value - a.value)));
                    if (error > largestError)
                    {
                        largestError = error;
                        bestSegment = segment;
                        bestCandidate = i;
                    }
                }
            }
            if (bestCandidate < 0)
                break;
            for (int i = result.count; i > bestSegment + 1; --i)
                chosen[i] = chosen[i - 1];
            chosen[bestSegment + 1] = bestCandidate;
            ++result.count;
        }
        for (int i = 0; i < result.count; ++i)
            result.points[i] = candidates[chosen[i]];
        return result;
    }
    float valueAt(float x) const
    {
        if (count < 1) return 0;
        if (x <= points[0].x) return points[0].value;
        for (int i = 0; i + 1 < count; ++i)
            if (x <= points[i + 1].x)
            {
                const auto &a = points[i];
                const auto &b = points[i + 1];
                float t = std::clamp((x - a.x) / std::max(0.000001f, b.x - a.x), 0.f, 1.f);
                const float s = std::clamp(a.shape, -1.f, 1.f) * 6.f;
                if (std::abs(s) > .0001f) t = std::expm1(s * t) / std::expm1(s);
                return a.value + t * (b.value - a.value);
            }
        return points[count - 1].value;
    }
    // Invert the segment's value at its horizontal midpoint. Flat segments retain
    // their existing shape because the handle has no meaningful vertical range.
    bool setMidpointValue(int segment, float value)
    {
        if (segment < 0 || segment + 1 >= count) return false;
        auto &a = points[segment];
        const float delta = points[segment + 1].value - a.value;
        if (std::abs(delta) < 1.e-6f) return false;
        const float edge = 1.f / (1.f + std::exp(3.f));
        const float fraction = std::clamp((value - a.value) / delta, edge, 1.f - edge);
        a.shape = std::clamp(std::log((1.f - fraction) / fraction) / 3.f, -1.f, 1.f);
        return true;
    }
    void render(Curve &curve, double sampleRate) const
    {
        for (int i = 0; i < B; ++i)
        {
            const double hz = i * sampleRate / N;
            const float x = float(std::log(std::clamp(hz, 20., 20000.) / 20.) / std::log(1000.));
            curve[i] = valueAt(x);
        }
    }
};
struct PointCurves
{
    std::array<std::array<std::array<PointCurve, 3>, 2>, 4> banks{};
};
}
