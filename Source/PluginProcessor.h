#pragma once
#include "Engine.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <mutex>
class Processor final : public juce::AudioProcessor, private juce::Timer
{
  public:
    Processor();
    ~Processor() override
    {
        stopTimer();
    }
    const juce::String getName() const override
    {
        return "FREEQY DELAY";
    }
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    void processBlockBypassed(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    bool acceptsMidi() const override
    {
        return false;
    }
    bool producesMidi() const override
    {
        return false;
    }
    bool isMidiEffect() const override
    {
        return false;
    }
    double getTailLengthSeconds() const override
    {
        return infiniteFeedback.load(std::memory_order_relaxed)
                   ? std::numeric_limits<double>::infinity()
                   : 9400;
    }
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override
    {
        return true;
    }
    int getNumPrograms() override
    {
        return 1;
    }
    int getCurrentProgram() override
    {
        return 0;
    }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override
    {
        return "Default";
    }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;
    juce::AudioProcessorValueTreeState state;
    spectral::Curves getCurves();
    void updateCurves(const spectral::Curves &);
    spectral::CurveBanks getBanks();
    void updateBanks(const spectral::CurveBanks &);
    void publishPending();
    void requestClearBuffer()
    {
        clearRequested.store(true, std::memory_order_release);
    }
    struct Spectrum
    {
        spectral::StereoSpectrum input{}, delay{};
        double seconds = 0;
    };
    bool spectrum(Spectrum &);
    bool nextSpectrum(Spectrum &);
    void discardSpectra();
    float liveShiftOffset(int ch) const
    {
        return liveShift[std::clamp(ch, 0, 1)].load(std::memory_order_relaxed);
    }
    float liveMorphPosition(int ch) const
    {
        return liveMorph[std::clamp(ch, 0, 1)].load(std::memory_order_relaxed);
    }
    std::atomic<int> selectedCurve{0}, selectedChannel{0}, selectedBank{0};
    std::atomic<bool> linked{false};
    std::atomic<int> spectrogramSpeed{2}; // Display-only: 1.5, 3, 6, 12 seconds.

  private:
    spectral::Engine engine;
    std::array<std::atomic<float>, 2> liveMorph{}, liveShift{};
    std::mutex modelMutex;
    spectral::CurveBanks model;
    juce::AbstractFifo curveFifo{4}, spectrumFifo{128};
    std::array<spectral::CurveBanks, 4> curveQueue;
    std::vector<Spectrum> spectrumQueue = std::vector<Spectrum>(128);
    bool pending = false;
    std::atomic<bool> infiniteFeedback{false};
    void updateTailFlag();
    std::atomic<bool> clearRequested{false};
    spectral::SpectrumAnalyzer inputAnalyzer;
    bool spectrumReady = false;
    int64_t meterSamples = 0;
    double meterSampleRate = 48000;
    double sinePhase = 0;
    void process(juce::AudioBuffer<float> &, bool);
    void analyze(const juce::AudioBuffer<float> &, int offset, int count);
    spectral::Settings readSettings() const;
    void timerCallback() override
    {
        publishPending();
    }
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
};
