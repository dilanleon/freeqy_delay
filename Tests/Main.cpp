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
            for (int size : {16, 32, 64, 128, 256, 512, 1024, 2048})
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
                    const int fractionalDelay = 3 * size / 16;
                    for (auto &ch : curves.values)
                        ch[0].fill(float(fractionalDelay) / float(sr));
                    engine.setCurves(curves);
                    engine.reset();
                    for (int n = 0; n < int(source.size()); ++n)
                        source[n] = float(.1 * std::sin(tau * 3 * n / size));
                    left = right = source;
                    run(engine, left, right, channels);
                    require(relativeError(left, source, Latency + fractionalDelay) < -25,
                            "fractional delay phase/amplitude");
                }
        std::cout
            << "All eight STFT sizes reconstruct and delay correctly, mono/stereo, 44.1–192 kHz\n";
        // Independent frequency regions retain their curve locations when the FFT size changes.
        for (int size : {16, 128, 1024, 2048})
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
            // Wait until both the analysis and synthesis windows have cleared startup.
            for (int n = Latency + size + delay; n < int(left.size()); ++n)
            {
                double expected = .05 * std::sin(tau * 2 * (n - Latency - delay) / size) +
                                  .05 * std::sin(tau * 6 * (n - Latency) / size);
                error += std::pow(left[n] - expected, 2);
                power += expected * expected;
            }
            std::cout << "remapping size " << size << ": " << 10 * std::log10(error / power)
                      << " dB\n";
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
                ch[1].fill(1);
            e.setCurves(c);
            e.reset();
            left.assign(left.size(), 0);
            right = left;
            left[0] = 1;
            run(e, left, right);
            require(std::abs(left[Latency + 1024] - 1) < 1e-5 &&
                        std::abs(left[Latency + 8192] - 1) < 1e-5,
                    "100 percent feedback decays");
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
        // Curve-axis shifts never transpose audio when maps are flat, including modulation.
        for (double sr : {48000., 192000.})
            for (float rate : {0.f, 1.f})
            {
                Engine e;
                Settings s;
                s.mix = 1;
                s.shiftHz = 1000;
                s.shiftRateHz = rate;
                e.setSettings(s);
                e.setCurves(transparent());
                e.prepare(sr, 2);
                std::vector<float> source(48000), left, right;
                for (int n = 0; n < int(source.size()); ++n)
                    source[n] = float(.1 * std::sin(tau * 1000 * n / sr));
                left = right = source;
                run(e, left, right);
                require(relativeError(left, source, Latency) < -90 &&
                            relativeError(right, source, Latency) < -90,
                        "curve shift must preserve audio frequency with flat maps");
            }
        // Entering the zero-rate hold is smoothed instead of snapping to full offset.
        {
            Engine e;
            Settings s;
            s.mix = 0;
            s.shiftHz = 1000;
            s.shiftRateHz = 1;
            s.stereoMorph = true;
            s.morphPercent = 50;
            s.morphDepth = 40;
            s.morphRateHz = 1;
            e.setSettings(s);
            e.setCurves(transparent());
            e.prepare(48000, 2);
            std::vector<float> left(6576), right(left.size());
            run(e, left, right);
            const auto shiftBefore = e.shiftOffsets();
            const auto morphBefore = e.morphPositions();
            s.shiftRateHz = s.morphRateHz = 0;
            e.setSettings(s);
            left.assign(1, 0);
            right.assign(1, 0);
            run(e, left, right);
            require(std::abs(e.shiftOffsets()[0] - shiftBefore[0]) < 5 &&
                        std::abs(e.morphPositions()[0] - morphBefore[0]) < .2f,
                    "zero-rate hold transition jumped");
            left.assign(48000, 0);
            right.assign(48000, 0);
            run(e, left, right);
            require(std::abs(e.shiftOffsets()[0] - 1000) < .1f &&
                        std::abs(e.morphPositions()[0] - 90) < .1f &&
                        std::abs(e.morphPositions()[1] - 10) < .1f,
                    "zero-rate hold did not settle at full opposite offsets");
        }
        // All three maps use fractional frequency lookups with held edge values.
        {
            CurveBanks b;
            b.count = 2;
            for (int bank = 0; bank < 2; ++bank)
                for (auto &ch : b.banks[bank].values)
                    for (int t = 0; t < 3; ++t)
                        for (int k = 0; k < B; ++k)
                            ch[t][k] = float(k) / (B - 1) * (.2f + .2f * t) + .1f * bank;
            for (int ch = 0; ch < 2; ++ch)
                for (int t = 0; t < 3; ++t)
                {
                    const float index =
                        CurveBanks::shiftedIndex(100, ch == 0 ? 100.f : -100.f, 48000);
                    require(std::abs(b.sample(ch, t, index, 25) -
                                     (index / (B - 1) * (.2f + .2f * t) + .025f)) < 1e-6,
                            "fractional curve shift and morph");
                }
            require(CurveBanks::shiftedIndex(0, 1000, 48000) == 0 &&
                        CurveBanks::shiftedIndex(B - 1, -1000, 48000) == B - 1,
                    "curve shift holds spectrum edges");
        }
        // A shifted level band passes the original tone on one side, without new pitches.
        for (float offset : {500.f, -500.f})
        {
            Engine e;
            Settings s;
            s.mix = 1;
            s.fftSize = 2048;
            s.shiftHz = std::abs(offset);
            s.shiftRateHz = 0;
            auto c = transparent();
            for (auto &ch : c.values)
                for (int k = 0; k < B; ++k)
                {
                    float hz = float(k) * 48000 / N;
                    ch[2][k] = std::abs(hz - (1500 - offset)) < 200 ? 1.f : 0.f;
                }
            e.setSettings(s);
            e.setCurves(c);
            e.prepare(48000, 2);
            std::vector<float> left(48000), right;
            for (int n = 0; n < int(left.size()); ++n)
                left[n] = float(.1 * std::sin(tau * 1500 * n / 48000));
            right = left;
            run(e, left, right);
            const auto &pass = offset > 0 ? left : right;
            const auto &stop = offset > 0 ? right : left;
            require(std::abs(amplitude(pass, 1500, 48000, 12000, 24000) - .1) < .001,
                    "shifted level map direction / original tone");
            require(amplitude(stop, 1500, 48000, 12000, 24000) < .00001,
                    "opposite level map shift");
            require(amplitude(pass, 2000, 48000, 12000, 24000) < .00001,
                    "curve shift must not translate audio bins");
        }
        // Shifted delay and feedback maps act on the same unchanged spectral bins.
        for (float feedback : {0.f, .4f})
        {
            Engine e;
            Settings s;
            s.mix = 1;
            s.fftSize = 2048;
            s.shiftHz = 500;
            s.shiftRateHz = 0;
            constexpr double frequency = 1476.5625;
            constexpr int delay = 768;
            auto c = transparent();
            for (auto &ch : c.values)
                for (int k = 0; k < B; ++k)
                    if (std::abs(float(k) * 48000 / N - (frequency - 500)) < 300)
                    {
                        ch[0][k] = float(delay) / 48000;
                        ch[1][k] = feedback;
                    }
            e.setSettings(s);
            e.setCurves(c);
            e.prepare(48000, 2);
            std::vector<float> left(48000), right;
            for (int n = 0; n < int(left.size()); ++n)
                left[n] = float(.1 * std::sin(tau * frequency * n / 48000));
            right = left;
            run(e, left, right);
            const auto phase = std::polar(1., -tau * frequency * delay / 48000);
            const auto gain = phase / (1. - double(feedback) * phase);
            double error = 0, power = 0;
            for (int n = 16000; n < int(left.size()); ++n)
            {
                const auto tone = .1 * std::polar(1., tau * frequency * (n - Latency) / 48000);
                error += std::pow(left[n] - (tone * gain).imag(), 2) +
                         std::pow(right[n] - tone.imag(), 2);
                power += std::norm(tone);
            }
            require(10 * std::log10(error / power) < -70, "shifted delay / feedback map routing");
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
        // Stereo morph uses opposite bounded bank positions, independently of curve shifting.
        {
            for (float center : {0.f, 50.f, 100.f})
                for (float movement : {-1.f, 0.f, 1.f})
                {
                    auto positions = Engine::stereoPositions(center, 100, movement);
                    require(positions[0] >= 0 && positions[0] <= 100 && positions[1] >= 0 &&
                                positions[1] <= 100,
                            "stereo morph bounds");
                }
            auto upper = Engine::stereoPositions(95, 10, 1),
                 lower = Engine::stereoPositions(5, 10, 1);
            require(upper[0] == 5 && upper[1] == 85 && lower[0] == 15 && lower[1] == 95,
                    "stereo morph does not wrap");
            CurveBanks banks;
            for (int b = 0; b < 4; ++b)
            {
                banks.banks[b] = transparent();
                for (auto &ch : banks.banks[b].values)
                    ch[2].fill(float(b) / 3);
            }
            Engine e;
            Settings s;
            s.mix = 1;
            s.morphPercent = 50;
            s.stereoMorph = true;
            s.morphDepth = 25;
            s.morphRateHz = 0;
            e.setBanks(banks);
            e.setSettings(s);
            e.prepare(48000, 2);
            std::vector<float> left(24000, .2f), right = left;
            run(e, left, right);
            require(std::abs(left.back() - .15f) < 1e-5 && std::abs(right.back() - .05f) < 1e-5,
                    "fixed stereo morph");
            s.morphRateHz = 1;
            e.setSettings(s);
            left.assign(48000, .2f);
            right = left;
            run(e, left, right);
            float separation = 0;
            for (int n = 5000; n < 48000; ++n)
            {
                require(std::abs(left[n] + right[n] - .2f) < 1e-4, "opposite stereo morph sum");
                separation = std::max(separation, std::abs(left[n] - right[n]));
            }
            require(separation > .05f, "stereo morph not moving");
            s.stereoMorph = false;
            e.setSettings(s);
            left.assign(24000, .2f);
            right = left;
            run(e, left, right);
            require(std::abs(left.back() - .1f) < 1e-5 && std::abs(right.back() - .1f) < 1e-5,
                    "stereo morph disable");
            s.stereoMorph = true;
            s.morphPercent = 95;
            s.morphDepth = 10;
            s.morphRateHz = 0;
            e.setSettings(s);
            e.reset();
            left.assign(24000, .2f);
            right = left;
            run(e, left, right);
            require(std::abs(left.back() - .01f) < 1e-5 && std::abs(right.back() - .17f) < 1e-5,
                    "audible wrapped morph");
            s.stereoMorph = false;
            s.morphPercent = 100;
            e.setSettings(s);
            e.reset();
            left.assign(24000, .2f);
            right = left;
            run(e, left, right);
            require(std::abs(left.back() - .2f) < 1e-5 && left == right,
                    "manual morph 100 endpoint changed");
            banks.count = 1;
            for (auto &ch : banks.banks[0].values)
                ch[2].fill(.6f);
            s.stereoMorph = true;
            e.setSettings(s);
            e.setBanks(banks);
            e.reset();
            left.assign(24000, .2f);
            right = left;
            run(e, left, right);
            require(std::abs(left.back() - .12f) < 1e-5 && left == right,
                    "single bank stereo morph");
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
                ch[1].fill(1.f);
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
            for (int size : {32, 64, 128, 256, 512, 1024, 2048, 16})
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
