#include "Engine.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <random>
#include <stdexcept>
static thread_local bool checking = false;
static thread_local int allocations = 0;
void *operator new(std::size_t size)
{
    if (checking)
        ++allocations;
    if (auto *p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept
{
    std::free(p);
}
void *operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete[](void *p) noexcept
{
    ::operator delete(p);
}
using namespace spectral;
void require(bool ok, const char *msg)
{
    if (!ok)
        throw std::runtime_error(msg);
}
constexpr double tau = 6.2831853071795864769;
Curves transparent()
{
    Curves c;
    for (auto &ch : c.values)
    {
        ch[0].fill(0);
        ch[1].fill(0);
    }
    return c;
}
void run(Engine &engine, std::vector<float> &left, std::vector<float> &right, int channels = 2,
         bool bypass = false)
{
    for (int at = 0; at < int(left.size());)
    {
        int count = std::min(1 + at * 37 % 701, int(left.size()) - at);
        float *data[] = {left.data() + at, right.data() + at};
        checking = true;
        engine.process(data, count, bypass);
        checking = false;
        require(allocations == 0, "audio thread allocation");
        at += count;
    }
}
double relativeError(const std::vector<float> &actual, const std::vector<float> &input, int delay)
{
    double error = 0, power = 0;
    for (int n = delay; n < int(actual.size()); ++n)
    {
        error += std::pow(actual[n] - input[n - delay], 2);
        power += std::pow(input[n - delay], 2);
    }
    return 10 * std::log10(error / power);
}
double amplitude(const std::vector<float> &signal, double hz, double sr, int start, int length)
{
    std::complex<double> sum{};
    for (int n = start; n < start + length; ++n)
        sum += double(signal[n]) * std::polar(1., -tau * hz * n / sr);
    return 2 * std::abs(sum) / length;
}
int main()
{
    try
    {
        CurveBanks banks;
        for (int b = 0; b < 4; ++b)
            for (auto &channel : banks.banks[b].values)
                for (auto &curve : channel)
                    curve.fill(float(b) / 4);
        for (int count = 1; count <= 4; ++count)
        {
            banks.count = count;
            for (float percent : {0.f, 100.f / 3, 50.f, 200.f / 3, 100.f})
            {
                Curves output;
                banks.blend(percent, output);
                for (auto &ch : output.values)
                    for (auto &curve : ch)
                        for (float v : curve)
                            require(std::abs(v - percent / 100 * (count - 1) / 4) < 1e-6,
                                    "bank blend");
            }
        }
        for (double sr : {44100., 48000., 96000., 192000.})
            for (int size : {16, 32, 64, 128, 256, 512, 1024})
                for (int channels : {1, 2})
                {
                    Engine engine;
                    Settings settings;
                    settings.mix = 1;
                    settings.fftSize = size;
                    auto curves = transparent();
                    engine.setSettings(settings);
                    engine.setCurves(curves);
                    engine.prepare(sr, channels);
                    std::vector<float> source(8192), left, right;
                    std::mt19937 random(42);
                    for (float &x : source)
                        x = std::uniform_real_distribution<float>(-.2f, .2f)(random);
                    left = source;
                    right = source;
                    run(engine, left, right, channels);
                    require(relativeError(left, source, Latency) < -90, "STFT reconstruction");
                    if (channels == 2)
                        require(left == right, "stereo transparent mismatch");
                    int delay = 3 * size / 8;
                    for (auto &ch : curves.values)
                        ch[0].fill(float(delay) / (float(sr)));
                    engine.setCurves(curves);
                    engine.reset();
                    left = right = source;
                    run(engine, left, right, channels);
                    require(relativeError(left, source, Latency + delay) < -85,
                            "integer hop delay");
                }
        std::cout
            << "All seven STFT sizes reconstruct and delay correctly, mono/stereo, 44.1–192 kHz\n";
        // Independent frequency regions retain their curve locations when the FFT size changes.
        for (int size : {16, 128, 1024})
        {
            Engine engine;
            Settings settings;
            settings.mix = 1;
            settings.fftSize = size;
            auto curves = transparent();
            int delay = 3 * size / 8;
            for (auto &ch : curves.values)
                for (int k = 1; k <= 3; ++k)
                    ch[0][k * (B - 1) / (size / 2)] = float(delay) / 48000;
            engine.setSettings(settings);
            engine.setCurves(curves);
            engine.prepare(48000, 2);
            std::vector<float> input(12000), left, right;
            for (int n = 0; n < int(input.size()); ++n)
                input[n] =
                    float(.05 * std::sin(tau * 2 * n / size) + .05 * std::sin(tau * 6 * n / size));
            left = right = input;
            run(engine, left, right);
            double error = 0, power = 0;
            for (int n = 4000; n < int(left.size()); ++n)
            {
                double expected = .05 * std::sin(tau * 2 * (n - Latency - delay) / size) +
                                  .05 * std::sin(tau * 6 * (n - Latency) / size);
                error += std::pow(left[n] - expected, 2);
                power += expected * expected;
            }
            require(10 * std::log10(error / power) < -80, "frequency curve remapping");
        }
        // Feedback and level controls; dry/bypass latency remains fixed across size switches.
        {
            Engine e;
            Settings s;
            s.mix = 1;
            auto c = transparent();
            for (auto &ch : c.values)
            {
                ch[0].fill(1024.f / 48000);
                ch[1].fill(.5f);
            }
            e.setSettings(s);
            e.setCurves(c);
            e.prepare(48000, 2);
            std::vector<float> left(16000), right = left;
            left[0] = 1;
            run(e, left, right);
            require(std::abs(left[Latency + 1024] - 1) < 1e-5, "first echo");
            require(std::abs(left[Latency + 2048] - .5f) < 1e-5, "feedback decay");
            require(std::abs(left[Latency + 3072] - .25f) < 1e-5, "second decay");
            for (auto &ch : c.values)
                ch[2].fill(0);
            e.setCurves(c);
            e.reset();
            left.assign(left.size(), .1f);
            right = left;
            run(e, left, right);
            for (float x : left)
                require(x == 0, "level mute");
            e.reset();
            left.assign(left.size(), 0);
            right = left;
            left[0] = .5;
            run(e, left, right, 2, true);
            require(left[Latency] == .5f, "bypass latency");
        }
        // True single-sideband frequency shift: additive Hz, opposite directions, no dual image.
        for (double sr : {48000., 192000.})
        {
            Engine e;
            Settings s;
            s.mix = 1;
            s.shiftHz = 100;
            s.shiftRateHz = 0;
            s.fftSize = 16;
            e.setSettings(s);
            e.setCurves(transparent());
            e.prepare(sr, 2);
            double fundamental = sr == 48000 ? 1000 : 4000;
            int length = int(sr / 2), start = int(sr / 4);
            std::vector<float> left(static_cast<size_t>(sr), 0.f), right;
            for (int n = 0; n < int(left.size()); ++n)
                left[n] = float(.1 * std::sin(tau * fundamental * n / sr));
            right = left;
            run(e, left, right);
            double lu = amplitude(left, fundamental + 100, sr, start, length),
                   ld = amplitude(left, fundamental - 100, sr, start, length),
                   rd = amplitude(right, fundamental - 100, sr, start, length),
                   ru = amplitude(right, fundamental + 100, sr, start, length);
            std::cout << sr << " Hz shifter: main " << lu << ", " << rd << " image ratio "
                      << ld / lu << ", " << ru / rd << "\n";
            require(std::abs(lu - .1) < .001 && std::abs(rd - .1) < .001, "shifted tone gain");
            require(ld / lu < .002 && ru / rd < .002, "shift direction or sideband suppression");
        }
        // Moving shifter follows the sinusoidal Hz offset continuously and in opposite directions.
        {
            Engine e;
            Settings s;
            s.mix = 1;
            s.shiftHz = 50;
            s.shiftRateHz = 1;
            e.setSettings(s);
            e.setCurves(transparent());
            e.prepare(48000, 2);
            std::vector<float> left(48000), right;
            for (int n = 0; n < 48000; ++n)
                left[n] = float(.1 * std::sin(tau * 1000 * n / 48000));
            right = left;
            run(e, left, right);
            double phase = 0, error = 0, power = 0;
            for (int n = 0; n < 48000; ++n)
            {
                if (n > 12000)
                {
                    double a = .1 * std::sin(tau * 1000 * (n - Latency) / 48000 + phase),
                           b = .1 * std::sin(tau * 1000 * (n - Latency) / 48000 - phase);
                    error += std::pow(left[n] - a, 2) + std::pow(right[n] - b, 2);
                    power += a * a + b * b;
                }
                phase = std::remainder(phase + tau * 50 * std::cos(tau * n / 48000) / 48000, tau);
            }
            require(10 * std::log10(error / power) < -65, "moving frequency shift");
        }
        // Morphing stays editor-independent with all FFT sizes.
        {
            for (int b = 0; b < 4; ++b)
            {
                banks.banks[b] = transparent();
                for (auto &ch : banks.banks[b].values)
                    ch[2].fill(float(b) / 3);
            }
            banks.count = 4;
            Engine e;
            Settings s;
            s.mix = 1;
            s.morphPercent = 50;
            e.setSettings(s);
            e.setBanks(banks);
            e.prepare(48000, 2);
            std::vector<float> left(24000, .2f), right = left;
            run(e, left, right);
            require(std::abs(left.back() - .1f) < 1e-5, "audible morph");
            s.morphPercent = 100;
            s.fftSize = 16;
            e.setSettings(s);
            left.assign(left.size(), .2f);
            right = left;
            run(e, left, right);
            require(std::abs(left.back() - .2f) < 1e-4, "morph after FFT switch");
        }
        // Clear/repeated clear, shifted tails, and high-feedback transitions; no allocation.
        for (double sr : {48000., 192000.})
        {
            Engine e;
            Settings s;
            s.mix = 1;
            s.shiftHz = 35;
            s.shiftRateHz = .7f;
            s.fftSize = 16;
            auto c = transparent();
            for (auto &ch : c.values)
            {
                ch[0].fill(1024.f / float(sr));
                ch[1].fill(.9f);
            }
            e.setSettings(s);
            e.setCurves(c);
            e.prepare(sr, 2);
            std::vector<float> left(size_t(sr * .15), .02f), right = left;
            run(e, left, right);
            left.assign(left.size(), 0);
            right = left;
            e.requestClear();
            run(e, left, right);
            for (size_t n = size_t(sr * .03); n < left.size(); ++n)
                require(left[n] == 0 && right[n] == 0, "stale clear tail");
            for (int size : {32, 64, 128, 256, 512, 1024, 16})
            {
                s.fftSize = size;
                e.setSettings(s);
                left.assign(left.size(), .01f);
                right = left;
                run(e, left, right);
                require(e.activeSize() == size, "FFT switch not adopted");
                for (float x : left)
                    require(std::isfinite(x) && std::abs(x) < 2, "unstable FFT transition");
            }
            e.requestClear();
            e.requestClear();
            left.assign(left.size(), 0);
            right = left;
            run(e, left, right);
        }
        {
            Engine e;
            Settings s;
            s.mix = 0;
            e.setSettings(s);
            e.prepare(48000, 1);
            std::vector<float> first(4096, .125f), second(4096, .25f), right(4096);
            run(e, first, right, 1);
            s.fftSize = 16;
            e.setSettings(s);
            e.requestClear();
            run(e, second, right, 1);
            for (int n = 0; n < 4096; ++n)
                require(second[n] == (n < Latency ? .125f : .25f),
                        "dry discontinuity on size/clear");
        }
        // Longest integer-hop delay, including a ring wrap after the first impulse.
        {
            Engine e;
            Settings s;
            s.mix = 1;
            s.maxDelayMs = 10000;
            constexpr int delay = 440832, second = 6144;
            auto c = transparent();
            for (auto &ch : c.values)
                ch[0].fill(float(delay) / 441000);
            e.setSettings(s);
            e.setCurves(c);
            e.prepare(44100, 1);
            std::vector<float> left(Latency + delay + second + 2048), right(left.size());
            left[0] = 1;
            left[second] = .5;
            run(e, left, right, 1);
            require(std::abs(left[Latency + delay] - 1) < .005, "maximum delay");
            require(std::abs(left[Latency + delay + second] - .5) < .005, "history wrap");
        }
        std::cout << "All DSP checks passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        checking = false;
        std::cerr << error.what() << "\n";
        return 1;
    }
}
