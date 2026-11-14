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
    juce::ComboBox stftSize, scrollSpeed, presetMenu;
    ChoiceStepper resolutionControl, scrollControl;
    juce::TextButton drawMode{"Draw"}, pointsMode{"Points"};
    int selectedPoint = -1, selectedSegment = -1;
    float handleDragOffset = 0;
    bool pointGesture = false;
    struct Snapshot { spectral::CurveBanks banks; spectral::PointCurves points; };
    Snapshot snapshot();
    void applyPointCurve();
    void ensurePoints();
    juce::TextButton bankLess{"-"}, bankMore{"+"};
    std::array<juce::TextButton, 3> parameterButtons;
    std::array<juce::TextButton, 4> bankButtons;
    juce::TextButton channelButton;
    juce::Slider morph, morphDepth, morphRate;
    juce::Label morphLabel, morphDepthLabel, morphRateLabel;
    juce::ToggleButton stereoMorph{"Morph direction"};
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
    int activeScrollSpeed = -1;
    spectral::Curves drawing;
    Snapshot beforeBanks;
    std::vector<Snapshot> undoStack, redoStack;
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
    void cancelGesture();
    void changeBankCount(int delta);
    void stepHistory(bool backwards);
    void resetDisplayHistory();
    void loadDefaultPreset();
};
