#pragma once
#include "BucketDisplay.h"
#include "EditorStyle.h"
#include "PluginProcessor.h"
#include "SpectrogramHistory.h"
class Editor final : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit Editor(Processor &);
    ~Editor() override
    {
        stopTimer();
        setLookAndFeel(nullptr);
    }
    void paint(juce::Graphics &) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    void mouseMove(const juce::MouseEvent &) override;
    void refreshDisplay(double nowSeconds);
    bool keyPressed(const juce::KeyPress &) override;

  private:
    EditorStyle style;
    Processor &p;
    juce::TooltipWindow tooltips{this, 600};
    std::array<juce::Slider, 6> knobs;
    std::array<juce::Label, 6> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    juce::ComboBox stftSize, scrollSpeed;
    ChoiceStepper resolutionControl, scrollControl;
    juce::TextButton bankLess{"-"}, bankMore{"+"};
    std::array<juce::TextButton, 3> parameterButtons;
    std::array<juce::TextButton, 4> bankButtons;
    juce::TextButton channelButton;
    juce::Slider morph, morphDepth, morphRate;
    juce::Label morphLabel, morphDepthLabel, morphRateLabel;
    juce::ToggleButton stereoMorph{"Stereo Morph"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphDepthAttachment,
        morphRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stereoMorphAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttachment;
    juce::ToggleButton link{"Link"};
    juce::TextButton reset{"Reset"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sizeAttachment;
    spectral::StereoSpectrum magnitudes{};
    spectral::SpectrogramHistory delayHistory;
    std::vector<Processor::Spectrum> displayFrames = std::vector<Processor::Spectrum>(128);
    double displayClockOffset = 0, lastDisplayAudio = -1, lastDisplayArrival = -1;
    spectral::Curves drawing;
    spectral::CurveBanks beforeBanks;
    std::vector<spectral::CurveBanks> undoStack, redoStack;
    juce::Rectangle<int> plot, curvePanel, delayPanel, shiftPanel, gainPanel, morphPanel, mixPanel,
        toolbar, bankTools;
    juce::TextButton clearBuffer{"Clear Buffer"};
    int lastBin = -1, gestureSize = 0;
    float lastValue = 0;
    juce::String readout;
    void timerCallback() override;
    int bin(float) const;
    spectral::BucketDisplay buckets() const;
    void drawAt(juce::Point<float>);
    void commit(const spectral::Curves &);
    void finishGesture();
    void changeBankCount(int delta);
    void stepHistory(bool backwards);
    void resetDisplayHistory();
};
