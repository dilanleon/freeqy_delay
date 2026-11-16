#include "PointCurves.h"
#pragma once
#include "Engine.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cstdint>
#include <mutex>
class Processor final : public juce::AudioProcessor, private juce::Timer
{
  public:
    // Initialize the host processor and its versioned APVTS root.
    Processor();
    // Stop model publishing before processor destruction.
    ~Processor() override
    {
        stopTimer();
    }
    // Return the stable host-facing product name.
    const juce::String getName() const override
    {
        return "FREEQY DELAY";
    }
    // Allocate DSP state and establish the fixed host latency.
    void prepareToPlay(double, int) override;
    // Leave preallocated resources ready for the next prepare.
    void releaseResources() override {}
    // Process normal audio through the spectral engine.
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    // Process bypass with the same dry-path latency.
    void processBlockBypassed(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    // Accept matching mono or stereo input and output layouts.
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    // Declare that this effect has no MIDI input.
    bool acceptsMidi() const override
    {
        return false;
    }
    // Declare that this effect has no MIDI output.
    bool producesMidi() const override
    {
        return false;
    }
    // Declare this processor as an audio effect.
    bool isMidiEffect() const override
    {
        return false;
    }
    // Report infinite tail when an active bank can sustain feedback.
    double getTailLengthSeconds() const override
    {
        return infiniteFeedback.load(std::memory_order_relaxed)
                   ? std::numeric_limits<double>::infinity()
                   : 9400;
    }
    // Construct the native JUCE editor.
    juce::AudioProcessorEditor *createEditor() override;
    // Advertise the native editor to hosts.
    bool hasEditor() const override
    {
        return true;
    }
    // Expose the single host program slot.
    int getNumPrograms() override
    {
        return 1;
    }
    // Return the only host program index.
    int getCurrentProgram() override
    {
        return 0;
    }
    // Ignore host program changes beyond the single slot.
    void setCurrentProgram(int) override {}
    // Name the host program without loading a UI preset.
    const juce::String getProgramName(int) override
    {
        return "Default";
    }
    // Keep the host program name fixed.
    void changeProgramName(int, const juce::String &) override {}
    // Serialize version 4 parameters, banks, and editor state.
    void getStateInformation(juce::MemoryBlock &) override;
    // Validate and restore a version 4 state payload.
    void setStateInformation(const void *, int) override;
    juce::AudioProcessorValueTreeState state;
    // Read the selected edit-bank curves under the model lock.
    spectral::Curves getCurves();
    // Publish an edited bank through bounded snapshot messaging.
    void updateCurves(const spectral::Curves &);
    // Copy all stored banks under the model lock.
    spectral::CurveBanks getBanks();
    // Replace stored banks and schedule an audio-thread snapshot.
    void updateBanks(const spectral::CurveBanks &);
    // Retry bounded curve publication off the audio thread.
    void publishPending();
    // Copy GUI point metadata under the model lock.
    spectral::PointCurves getPointCurves();
    // Replace GUI point metadata without touching DSP state.
    void setPointCurves(const spectral::PointCurves &);
    std::atomic<bool> pointMode{false};
    // Request a transient clear and invalidate visual frames.
    void requestClearBuffer()
    {
        // Publish the clear command before invalidating display frames. An audio
        // block racing this call must not acquire a fresh epoch without the clear.
        clearRequested.store(true, std::memory_order_release);
        spectrumEpoch.fetch_add(1, std::memory_order_acq_rel);
    }
    struct Spectrum
    {
        spectral::StereoSpectrum input{}, delay{};
        double seconds = 0;
        uint64_t epoch = 0;
    };
    // Read the newest available analyzer frame.
    bool spectrum(Spectrum &);
    // Read the next non-stale analyzer frame in timestamp order.
    bool nextSpectrum(Spectrum &);
    // Bound GUI work and optionally discard stalled frames.
    int drainSpectra(Spectrum *, int capacity, double maxAgeSeconds);
    // Drain queued analyzer frames without resetting the producer.
    void discardSpectra();
    // Invalidate in-flight frames and empty visual transfer.
    void restartSpectra();
    // Read a published live shift offset for GUI preview.
    float liveShiftOffset(int ch) const
    {
        return liveShift[std::clamp(ch, 0, 1)].load(std::memory_order_relaxed);
    }
    // Read a published live morph position for GUI preview.
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
    spectral::PointCurves pointModel;
    juce::AbstractFifo curveFifo{4}, spectrumFifo{128};
    std::array<spectral::CurveBanks, 4> curveQueue;
    std::vector<Spectrum> spectrumQueue = std::vector<Spectrum>(128);
    std::atomic<uint64_t> spectrumEpoch{1};
    std::atomic<double> latestMeterSeconds{0};
    bool pending = false;
    std::atomic<bool> infiniteFeedback{false};
    // Track whether any active bank can sustain an infinite tail.
    void updateTailFlag();
    std::atomic<bool> clearRequested{false};
    spectral::SpectrumAnalyzer inputAnalyzer;
    bool spectrumReady = false;
    int64_t meterSamples = 0;
    double meterSampleRate = 48000;
    double sinePhase = 0;
    // Bridge host buffers, control snapshots, DSP, and analyzer transfer.
    void process(juce::AudioBuffer<float> &, bool);
    // Feed pre-effect samples to the stereo input analyzer.
    void analyze(const juce::AudioBuffer<float> &, int offset, int count);
    // Snapshot atomic host parameters for one audio block.
    spectral::Settings readSettings() const;
    // Retry pending curve publication on the message thread.
    void timerCallback() override
    {
        publishPending();
    }
    // Define stable automatable host parameters and defaults.
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
};
