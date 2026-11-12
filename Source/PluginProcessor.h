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
        return "Spectral Delay";
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
        return 9400;
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
    bool spectrum(std::array<float, spectral::B> &);
    std::atomic<int> selectedCurve{0}, selectedChannel{0}, selectedBank{0};
    std::atomic<bool> linked{false};

  private:
    spectral::Engine engine;
    std::mutex modelMutex;
    spectral::CurveBanks model;
    juce::AbstractFifo curveFifo{4}, spectrumFifo{4};
    std::array<spectral::CurveBanks, 4> curveQueue;
    std::array<std::array<float, spectral::B>, 4> spectrumQueue{};
    bool pending = false;
    std::atomic<bool> clearRequested{false};
    juce::dsp::FFT analyzer{11};
    std::array<float, spectral::N> input{};
    std::array<float, 2 * spectral::N> fft{};
    int inputPos = 0, analyzerHop = 0;
    double sinePhase = 0;
    void process(juce::AudioBuffer<float> &, bool);
    void analyze(const juce::AudioBuffer<float> &);
    spectral::Settings readSettings() const;
    void timerCallback() override
    {
        publishPending();
    }
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
};
