#include "BucketDisplay.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "SpectrogramHistory.h"
#include <iostream>
#include <stdexcept>
void check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try
    {
        // Values use the same display convention at automation boundaries as in the editor.
        juce::Slider maximumDelay;
        maximumDelay.setName("maxDelayMs");
        maximumDelay.setRange(1, 10000);
        maximumDelay.setValue(999.9);
        check(EditorStyle::sliderText(maximumDelay, {}) == "999.9 ms",
              "max delay milliseconds display");
        maximumDelay.setValue(1000.0);
        check(EditorStyle::sliderText(maximumDelay, {}) == "1.00 s", "max delay seconds display");
        check(EditorStyle::delayValueFromText("500", 999.9) == 500 &&
                  EditorStyle::delayValueFromText("2 s", 999.9) == 2000 &&
                  EditorStyle::delayValueFromText("2 ms", 999.9) == 2,
              "millisecond delay entry semantics");
        check(EditorStyle::delayValueFromText("2", 1000) == 2000 &&
                  EditorStyle::delayValueFromText("500 ms", 1000) == 500,
              "seconds delay entry semantics");
        juce::Slider mix;
        mix.setName("mix");
        mix.setRange(0, 1);
        mix.setValue(.5);
        check(EditorStyle::sliderText(mix, {}) == "50.0 %", "seven-segment unit bulb text");
        for (int size : {16, 32, 64, 128, 256, 512, 1024, 2048})
            for (double sr : {44100., 48000., 96000., 192000.})
            {
                spectral::BucketDisplay layout{size, sr, 100, 900};
                spectral::BucketDisplay reference{size, 44100., 100, 900};
                float previous = 100;
                for (int k = 0; k <= size / 2; ++k)
                {
                    const auto [left, right] = layout.edges(k);
                    const auto [referenceLeft, referenceRight] = reference.edges(k);
                    check(std::abs(left - referenceLeft) < .001 &&
                              std::abs(right - referenceRight) < .001,
                          "sample rate changed visible bucket count or bounds");
                    if (right <= left)
                        continue;
                    check(std::abs(left - previous) < .001, "bucket edge gap");
                    check(layout.binAt((left + right) / 2) == k, "bucket hit-test mismatch");
                    const auto [first, last] = layout.cell(k);
                    check(first <= layout.curveIndex(k) && last >= layout.curveIndex(k),
                          "bucket edit cell mismatch");
                    previous = right;
                }
                check(std::abs(previous - 1000) < .001, "bucket range incomplete");
                spectral::Curve meter{};
                constexpr double toneHz = 1000.;
                const int analyzerBin = int(std::round(toneHz * spectral::N / sr));
                meter[analyzerBin] = .2f;
                const int displayBin = layout.binAt(layout.xAt(analyzerBin * sr / spectral::N));
                check(layout.magnitude(meter, displayBin) > .1f,
                      "sample-rate-resampled bucket meter mapping");
                check(layout.edges(0).first == 100 &&
                          std::abs(layout.edges(size / 2).second - 1000) < .001,
                      "all logical buckets must remain visible");
            }
        {
            check(spectral::SpectrogramHistory::color(0) == freeqy::ink, "spectrogram ink anchor");
            check(spectral::SpectrogramHistory::color(85) == freeqy::indigo,
                  "spectrogram indigo anchor");
            check(spectral::SpectrogramHistory::color(170) == freeqy::blue,
                  "spectrogram blue anchor");
            check(spectral::SpectrogramHistory::color(255) == freeqy::cyan,
                  "spectrogram cyan anchor");
            spectral::SpectrogramHistory history;
            spectral::StereoSpectrum a{}, b{};
            a[0][43] = 1;
            a[1][79] = .5f;
            b[0][96] = .8f;
            b[1][118] = .7f;
            history.append(10, 48000, &a);
            history.append(10 + 1.01 / 30, 48000, &b);
            check(history.atAge(0)[0][96] == .8f && history.atAge(1)[0][43] == 1,
                  "spectrogram chronology");
            auto render = [&](int width, int channel)
            {
                juce::Image picture(juce::Image::ARGB, width, 180, true);
                juce::Graphics graphics(picture);
                history.draw(graphics, picture.getBounds(), channel);
                return picture;
            };
            auto picture = render(600, 0);
            spectral::BucketDisplay map{spectral::N, 48000, 0, 600};
            check(
                picture.getPixelAt(int(map.xAt(96 * 48000. / spectral::N)), 0).getBrightness() >
                        .05f &&
                    picture.getPixelAt(int(map.xAt(43 * 48000. / spectral::N)), 1).getBrightness() >
                        .05f,
                "spectrogram newest row not at top");
            auto right = render(600, 1);
            check(right.getPixelAt(int(map.xAt(118 * 48000. / spectral::N)), 0).getBrightness() >
                      .05f,
                  "spectrogram right channel");
            render(900, 0);
            check(history.atAge(1)[0][43] == 1, "spectrogram resize lost history");
            history.append(10 + 1.02 / 30, 48000, &a);
            check(history.atAge(0)[0][43] == 1 && history.atAge(0)[0][96] == .8f,
                  "same-frame peak aggregation");
            history.append(20, 48000, nullptr);
            check(history.atAge(0)[0][43] == 0 && history.atAge(179)[0][43] == 0,
                  "spectrogram stale gap");
            for (int i = 0; i < 220; ++i)
            {
                a[0][43] = float(i) / 300;
                history.append(30 + i * 1.01 / 30, 48000, &a);
            }
            check(std::abs(history.atAge(0)[0][43] - 219.f / 300) < 1e-6 &&
                      history.atAge(179)[0][43] > 0,
                  "spectrogram ring wrap");
            history.append(40, 96000, &b);
            check(history.atAge(1)[0][43] == 0, "sample rate change retains mislabelled history");
            history.clear();
            check(history.atAge(0)[0][96] == 0 && render(600, 0).getPixelAt(300, 0) == freeqy::ink,
                  "spectrogram clear");
        }
        // Faster speed gives identical tap spacing more rows, with true sub-GUI-frame detail.
        {
            spectral::SpectrogramHistory history;
            spectral::StereoSpectrum tap{}, empty{};
            tap[0][43] = 1;
            int previousSpacing = 0;
            for (int speed : {3, 2, 1, 0})
            {
                history.setSpeed(speed);
                history.clear();
                const double row = history.secondsPerRow();
                history.append(12.001, 48000, &tap);
                history.append(12.101, 48000, &empty);
                const int spacing = int(std::floor(12.101 / row) - std::floor(12.001 / row));
                check(spacing > previousSpacing && history.atAge(spacing)[0][43] == 1 &&
                          history.atAge(0)[0][43] == 0,
                      "scroll speed must increase temporal row resolution");
                previousSpacing = spacing;
            }
            history.setSpeed(2);
            check(history.atAge(0)[0][43] == 0 && history.atAge(6)[0][43] == 0,
                  "speed change clears incompatible visual time scale");
        }
        if constexpr (!SPECTRAL_DIAGNOSTIC)
        {
            Processor probe;
            auto set = [&](const char *id, float value)
            {
                auto *parameter = probe.state.getParameter(id);
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            };
            set("mix", 0);
            set("outputTrimDb", -6);
            probe.prepareToPlay(48000, 512);
            juce::AudioBuffer<float> signal(2, 512);
            juce::MidiBuffer midi;
            Processor::Spectrum measured{};
            auto processTone = [&](int blocks, bool silence)
            {
                for (int b = 0; b < blocks; ++b)
                {
                    for (int n = 0; n < 512; ++n)
                    {
                        float sample = silence ? 0.f
                                               : float(.2 * std::sin(6.283185307179586 * 43 *
                                                                     (n + b * 512) / spectral::N));
                        signal.setSample(0, n, sample);
                        signal.setSample(1, n, -sample);
                    }
                    probe.processBlock(signal, midi);
                    probe.spectrum(measured);
                }
            };
            auto returnBanks = probe.getBanks();
            for (auto &bank : returnBanks.banks)
                for (auto &channel : bank.values)
                {
                    channel[0].fill(0);
                    channel[1].fill(0);
                    channel[2].fill(.5f);
                }
            probe.updateBanks(returnBanks);
            probe.prepareToPlay(48000, 512);
            processTone(12, false);
            check(std::abs(measured.input[0][43] - .2f) < .002f &&
                      std::abs(measured.input[1][43] - .2f) < .002f,
                  "input spectrum gain or stereo cancellation");
            check(std::abs(measured.delay[0][43] - .1f) < .002f &&
                      std::abs(measured.delay[1][43] - .1f) < .002f,
                  "delay spectrum must be independent of dry/wet and output trim");
            auto silent = probe.getBanks();
            for (auto &bank : silent.banks)
                for (auto &channel : bank.values)
                {
                    channel[0].fill(0);
                    channel[1].fill(0);
                    channel[2].fill(0);
                }
            set("mix", 1);
            probe.updateBanks(silent);
            probe.prepareToPlay(48000, 512);
            processTone(12, false);
            for (const auto &channel : measured.delay)
                for (float value : channel)
                    check(value == 0, "delay meter displays input when wet level muted");
            for (auto &bank : silent.banks)
                for (auto &channel : bank.values)
                {
                    channel[0].fill(1024.f / 48000);
                    channel[1].fill(.8f);
                    channel[2].fill(1);
                }
            probe.updateBanks(silent);
            probe.prepareToPlay(48000, 512);
            processTone(8, false);
            processTone(16, true);
            check(measured.delay[0][43] > .001f && measured.delay[1][43] > .001f &&
                      measured.input[0][43] == 0,
                  "dual spectra do not separate incoming silence from stored tail");
        }
        {
            Processor probe;
            probe.prepareToPlay(48000, 8192);
            juce::AudioBuffer<float> block(2, 8192);
            block.clear();
            juce::MidiBuffer midi;
            probe.processBlock(block, midi);
            Processor::Spectrum frame;
            int count = 0;
            double previous = 0;
            while (probe.nextSpectrum(frame))
            {
                check(std::abs(frame.seconds - previous - 256. / 48000) < 1e-9,
                      "analyzer sample timestamps / lost hops in large block");
                previous = frame.seconds;
                ++count;
            }
            check(count == 32, "spectrogram must retain frames between GUI paints");
            // A stalled GUI must remain bounded and resume without blocking audio.
            for (int i = 0; i < 8; ++i)
                probe.processBlock(block, midi);
            count = 0;
            while (probe.nextSpectrum(frame))
                ++count;
            check(count == 127, "bounded spectrogram queue overflow");
            probe.processBlock(block, midi);
            probe.discardSpectra();
            check(!probe.nextSpectrum(frame), "display reset retained queued analyzer frames");
            probe.processBlock(block, midi);
            check(probe.nextSpectrum(frame), "display reset stopped subsequent analyzer frames");
            count = 0;
            while (probe.nextSpectrum(frame))
                ++count;
            check(count == 31, "analyzer queue did not resume in timestamp order");
        }
        Processor p;
        auto banks = p.getBanks();
        banks.count = 3;
        for (int b = 0; b < 4; ++b)
            banks.banks[b].values[1][0][123] = float(b) * .2f;
        for (auto &bank : banks.banks)
            bank.values[0][1][16] = 1.f;
        p.updateBanks(banks);
        p.selectedBank = 2;
        auto *morph = p.state.getParameter("curveMorph");
        check(morph && morph->isAutomatable(), "missing automation");
        morph->setValueNotifyingHost(.67f);
        p.state.getParameter("stereoMorph")->setValueNotifyingHost(1);
        p.state.getParameter("morphDepth")->setValueNotifyingHost(.3f);
        p.state.getParameter("morphRateHz")
            ->setValueNotifyingHost(p.state.getParameter("morphRateHz")->convertTo0to1(1.5f));
        p.state.getParameter("shiftHz")->setValueNotifyingHost(
            p.state.getParameter("shiftHz")->convertTo0to1(123));
        p.state.getParameter("shiftRateHz")
            ->setValueNotifyingHost(p.state.getParameter("shiftRateHz")->convertTo0to1(1.7f));
        p.state.getParameter("stftSize")
            ->setValueNotifyingHost(p.state.getParameter("stftSize")->convertTo0to1(3));
        check(p.state.getParameter("copies") == nullptr, "copy engine still exposed");
        p.prepareToPlay(48000, 512);
        juce::AudioBuffer<float> meterInput(2, 512);
        juce::MidiBuffer meterMidi;
        for (int block = 0; block < 8; ++block)
        {
            for (int n = 0; n < 512; ++n)
            {
                const float sample =
                    float(.2 * std::sin(6.283185307179586 * 43 * (n + block * 512) / spectral::N));
                meterInput.setSample(0, n, sample);
                meterInput.setSample(1, n, -sample);
            }
            p.processBlock(meterInput, meterMidi);
        }
        Processor::Spectrum spectrum;
        check(p.spectrum(spectrum) && spectrum.input[0][43] > .19f && spectrum.input[1][43] > .19f,
              "stereo input meter cancelled anti-phase audio");
        if (argc > 1)
        {
            for (int block = 0; block < 64; ++block)
            {
                for (int n = 0; n < 512; ++n)
                {
                    float sample = float(.2 * std::sin(6.283185307179586 * (block < 48 ? 43 : 96) *
                                                       (n + block * 512) / spectral::N));
                    meterInput.setSample(0, n, sample);
                    meterInput.setSample(1, n, -sample);
                }
                p.processBlock(meterInput, meterMidi);
                if (block < 63)
                {
                    Processor::Spectrum discarded;
                    p.spectrum(discarded);
                }
            }
            if (argc > 4)
                p.state.getParameter("stftSize")
                    ->setValueNotifyingHost(
                        juce::jlimit(0, 7, juce::String(argv[4]).getIntValue()) / 7.f);
            const auto originalBanks = p.getBanks();
            auto demo = originalBanks;
            for (int b = 0; b < 4; ++b)
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < spectral::B; ++i)
                    {
                        float x = float(i) / (spectral::B - 1);
                        demo.banks[b].values[ch][0][i] =
                            .12f + .65f * std::pow(std::sin(x * 11 + b * .2f), 2);
                        demo.banks[b].values[ch][1][i] =
                            .1f + .45f * std::pow(std::cos(x * 8 + b * .15f), 2);
                        demo.banks[b].values[ch][2][i] =
                            .45f + .5f * std::pow(std::sin(x * 5 + b * .3f), 2);
                    }
            p.updateBanks(demo);
            if (argc > 5)
                p.spectrogramSpeed = juce::jlimit(0, 3, juce::String(argv[5]).getIntValue());
            std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
            if (argc > 3)
                editor->setSize(juce::String(argv[2]).getIntValue(),
                                juce::String(argv[3]).getIntValue());
            if constexpr (!SPECTRAL_DIAGNOSTIC)
            {
                auto *view = dynamic_cast<Editor *>(editor.get());
                check(view != nullptr, "editor preview type");
                const double start = juce::Time::getMillisecondCounterHiRes() * .001;
                for (int block = 0; block < 600; ++block)
                {
                    const int bin = 20 + int(85 * (.5 + .5 * std::sin(block * .016)));
                    for (int n = 0; n < 512; ++n)
                    {
                        float sample = float(.12 * std::sin(6.283185307179586 * bin *
                                                            (n + block * 512) / spectral::N));
                        meterInput.setSample(0, n, sample);
                        meterInput.setSample(1, n, -sample);
                    }
                    p.processBlock(meterInput, meterMidi);
                    if (block % 3 == 2)
                        view->refreshDisplay(start + (block + 1) * 512. / 48000);
                }
            }
            juce::PNGImageFormat png;
            auto output = juce::File(argv[1]).createOutputStream();
            check(output != nullptr, "snapshot output");
            check(png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),
                                         *output),
                  "snapshot render");
            p.editorBeingDeleted(editor.get());
            p.updateBanks(originalBanks);
            p.state.getParameter("stftSize")
                ->setValueNotifyingHost(p.state.getParameter("stftSize")->convertTo0to1(3));
        }
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditorAndMakeActive());
            auto button = [&](const juce::String &name) -> juce::TextButton *
            {
                for (auto *child : editor->getChildren())
                    if (auto *candidate = dynamic_cast<juce::TextButton *>(child))
                        if (candidate->getButtonText() == name)
                            return candidate;
                return nullptr;
            };
            juce::ComboBox *speedMenu = nullptr;
            for (auto *child : editor->getChildren())
                if (child->getName() == "Spectrogram Scroll Speed")
                    speedMenu = dynamic_cast<juce::ComboBox *>(child);
            check(speedMenu && speedMenu->getNumItems() == 4, "scroll speed menu missing");
            p.processBlock(meterInput, meterMidi);
            speedMenu->setSelectedId(1, juce::sendNotificationSync);
            check(p.spectrogramSpeed == 0, "scroll speed menu callback");
            check(!p.nextSpectrum(spectrum), "scroll speed change retained queued visual frames");
            speedMenu->setSelectedId(3, juce::sendNotificationSync);
            check(button("Delay") && button("Feedback") && button("Level"),
                  "parameter view buttons missing");
            check(button("Bank 4") && !button("Bank 4")->isEnabled(),
                  "inactive bank button enabled");
            juce::TextButton *addBank = nullptr, *removeBank = nullptr;
            for (auto *child : editor->getChildren())
            {
                if (child->getName() == "Add Bank")
                    addBank = dynamic_cast<juce::TextButton *>(child);
                if (child->getName() == "Remove Bank")
                    removeBank = dynamic_cast<juce::TextButton *>(child);
            }
            check(addBank && removeBank, "bank count steppers missing");
            const auto retained = p.getBanks().banks[3].values[1][0][123];
            addBank->onClick();
            check(p.getBanks().count == 4 && button("Bank 4")->isEnabled() && !addBank->isEnabled(),
                  "add bank callback / upper bound");
            removeBank->onClick();
            check(p.getBanks().count == 3 && !button("Bank 4")->isEnabled() &&
                      p.getBanks().banks[3].values[1][0][123] == retained,
                  "remove bank callback must retain stored curves");
            check(button("Clear Buffer") && !button("Undo") && !button("Redo") && !button("L > R"),
                  "sketch toolbar controls");
            p.processBlock(meterInput, meterMidi);
            button("Clear Buffer")->onClick();
            check(!p.nextSpectrum(spectrum), "Clear Buffer retained queued visual frames");
            button("Feedback")->onClick();
            check(p.selectedCurve == 1, "parameter button not selecting");
            button("Delay")->onClick();
            button("Channel: L")->onClick();
            check(p.selectedChannel == 1 && button("Channel: R"), "channel button not toggling");
            button("Channel: R")->onClick();
            button("Bank 1")->onClick();
            check(p.selectedBank == 0, "bank button not selecting");
            button("Bank 3")->onClick();
            p.editorBeingDeleted(editor.get());
        }
        p.spectrogramSpeed = 0;
        juce::MemoryBlock bytes;
        p.getStateInformation(bytes);
        p.prepareToPlay(48000, 512);
        p.requestClearBuffer();
        juce::AudioBuffer<float> audio(2, 512);
        audio.clear();
        juce::MidiBuffer midi;
        p.processBlock(audio, midi);
        juce::MemoryBlock afterClear;
        p.getStateInformation(afterClear);
        check(bytes == afterClear, "clear changed stored settings");
        Processor restored;
        restored.setStateInformation(bytes.getData(), int(bytes.getSize()));
        check(restored.spectrogramSpeed == 0, "spectrogram speed state recall");
        auto saved = restored.getBanks();
        check(saved.count == 3 && restored.selectedBank == 2, "bank settings recall");
        for (int b = 0; b < 4; ++b)
            check(saved.banks[b].values[1][0][123] == banks.banks[b].values[1][0][123],
                  "bank recall");
        check(std::abs(restored.state.getRawParameterValue("curveMorph")->load() - 67) < .001,
              "morph recall");
        check(std::abs(restored.state.getRawParameterValue("shiftHz")->load() - 123) < .001,
              "shift recall");
        check(std::abs(restored.state.getRawParameterValue("shiftRateHz")->load() - 1.7f) < .0001,
              "rate recall");
        check(restored.state.getRawParameterValue("stftSize")->load() == 3, "STFT size recall");
        check(p.getLatencySamples() == spectral::Latency, "fixed host latency");
        check(restored.state.getRawParameterValue("stereoMorph")->load() == 1,
              "stereo morph recall");
        check(std::abs(restored.state.getRawParameterValue("morphDepth")->load() - 30) < .001,
              "morph depth recall");
        check(std::abs(restored.state.getRawParameterValue("morphRateHz")->load() - 1.5f) < .001,
              "morph rate recall");
        check(restored.getBanks().banks[0].values[0][1][16] == 1.f, "100 percent feedback recall");
        check(std::isinf(restored.getTailLengthSeconds()), "unity feedback tail not reported");
        p.state.getParameter("stftSize")->setValueNotifyingHost(1);
        juce::MemoryBlock highest;
        p.getStateInformation(highest);
        restored.setStateInformation(highest.getData(), int(highest.getSize()));
        check(restored.state.getRawParameterValue("stftSize")->load() == 7,
              "2048 STFT state recall");
        p.state.getParameter("stftSize")
            ->setValueNotifyingHost(p.state.getParameter("stftSize")->convertTo0to1(3));
        restored.setStateInformation(bytes.getData(), int(bytes.getSize()));
        auto tree = juce::ValueTree::readFromData(bytes.getData(), bytes.getSize());
        tree.setProperty("version", 3, nullptr);
        juce::MemoryBlock version3;
        {
            juce::MemoryOutputStream out(version3, false);
            tree.writeToStream(out);
        }
        restored.setStateInformation(version3.getData(), int(version3.getSize()));
        check(restored.state.getRawParameterValue("stereoMorph")->load() == 0 &&
                  std::abs(restored.state.getRawParameterValue("shiftHz")->load() - 123) < .001,
              "v3 migration");
        tree.setProperty("version", 1, nullptr);
        tree.removeProperty("banks", nullptr);
        spectral::Curves legacy;
        legacy.values[0][0][17] = .83f;
        tree.setProperty("curves", juce::MemoryBlock(&legacy, sizeof(legacy)), nullptr);
        juce::MemoryBlock old;
        {
            juce::MemoryOutputStream out(old, false);
            tree.writeToStream(out);
        }
        restored.setStateInformation(old.getData(), int(old.getSize()));
        saved = restored.getBanks();
        for (auto &bank : saved.banks)
            check(bank.values[0][0][17] == .83f, "legacy curve migration");
        check(restored.selectedBank == 0 &&
                  restored.state.getRawParameterValue("curveMorph")->load() == 0,
              "legacy morph reset");
        check(restored.state.getRawParameterValue("shiftHz")->load() == 0 &&
                  restored.state.getRawParameterValue("stftSize")->load() == 6,
              "legacy effect defaults");
        // Reject nonfinite stored curves and out-of-range counts.
        tree.setProperty("version", 2, nullptr);
        juce::ValueTree retired("PARAM");
        retired.setProperty("id", "copies", nullptr);
        retired.setProperty("value", 16, nullptr);
        tree.addChild(retired, -1, nullptr);
        saved.banks[0].values[0][0][0] = std::numeric_limits<float>::quiet_NaN();
        tree.setProperty("banks", juce::MemoryBlock(saved.banks.data(), sizeof(saved.banks)),
                         nullptr);
        tree.setProperty("bankCount", 99, nullptr);
        old.reset();
        {
            juce::MemoryOutputStream out(old, false);
            tree.writeToStream(out);
        }
        restored.setStateInformation(old.getData(), int(old.getSize()));
        check(restored.getBanks().count == 4 &&
                  std::isfinite(restored.getBanks().banks[0].values[0][0][0]),
              "state sanitization");
        juce::MemoryBlock migrated;
        restored.getStateInformation(migrated);
        auto modern = juce::ValueTree::readFromData(migrated.getData(), migrated.getSize());
        check(int(modern.getProperty("version")) == 4 &&
                  !modern.getChildWithProperty("id", "copies").isValid(),
              "v2 migration cleanup");
        std::cout << "Bank state, automation, legacy migration and sanitization passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
