#pragma once
#include "PluginProcessor.h"
class Editor final : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit Editor(Processor &);
    void paint(juce::Graphics &) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    void mouseMove(const juce::MouseEvent &) override;

  private:
    Processor &p;
    std::array<juce::Slider, 6> knobs;
    std::array<juce::Label, 6> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    juce::ComboBox curve, channel, stftSize, bank, bankCount;
    juce::Slider morph;
    juce::Label morphLabel;
    juce::TextButton copyBank{"Copy to next bank"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttachment;
    juce::ToggleButton link{"Link"};
    juce::TextButton copyLR{"L > R"}, copyRL{"R > L"}, reset{"Reset"}, undo{"Undo"}, redo{"Redo"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sizeAttachment;
    std::array<float, spectral::B> magnitudes{};
    spectral::Curves drawing;
    spectral::CurveBanks beforeBanks;
    std::vector<spectral::CurveBanks> undoStack, redoStack;
    juce::Rectangle<int> plot, curvePanel, delayPanel, shiftPanel, gainPanel;
    juce::TextButton clearBuffer{"Clear Buffer"};
    int lastBin = -1;
    float lastValue = 0;
    juce::String readout;
    void timerCallback() override;
    int bin(float) const;
    float xFor(int) const;
    void drawAt(juce::Point<float>);
    void commit(const spectral::Curves &);
};
