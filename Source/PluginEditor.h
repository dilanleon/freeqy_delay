#pragma once
#include "BucketDisplay.h"
#include "EditorStyle.h"
#include "PluginProcessor.h"
#include "SpectrogramHistory.h"
class Editor final : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    // Build the native editor and bind it to processor state.
    explicit Editor(Processor &);
    // Stop GUI refresh before releasing the custom look and feel.
    ~Editor() override
    {
        stopTimer();
        setLookAndFeel(nullptr);
    }
    // Render the graph, activity, curves, and panel surfaces.
    void paint(juce::Graphics &) override;
    // Lay out the graph, bank tools, toolbar, and bottom controls.
    void resized() override;
    // Begin a bucket or point-edit gesture from the graph.
    void mouseDown(const juce::MouseEvent &) override;
    // Update the active bucket, anchor, or curvature edit.
    void mouseDrag(const juce::MouseEvent &) override;
    // Finish or cancel a gesture after a resolution change.
    void mouseUp(const juce::MouseEvent &) override;
    // Update the hovered cell and physical-value readout.
    void mouseMove(const juce::MouseEvent &) override;
    // Drain analyzer frames and refresh GUI-owned visual state.
    void refreshDisplay(double nowSeconds);
    // Handle gesture undo and redo shortcuts.
    bool keyPressed(const juce::KeyPress &) override;
    // Reset the whole plug-in to the documented Default preset.
    void loadDefaultPreset();
    // Reset only the selected bank and its point metadata.
    void loadSelectedBankDefaultPreset();

  private:
    EditorStyle style;
    Processor &p;
    juce::TooltipWindow tooltips{this, 600};
    std::array<juce::Slider, 6> knobs;
    std::array<juce::Label, 6> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    juce::ComboBox stftSize, scrollSpeed;
    juce::TextButton presetMenu{"Presets"};
    ChoiceStepper resolutionControl, scrollControl;
    juce::TextButton drawMode{"Draw"}, pointsMode{"Points"};
    int selectedPoint = -1, selectedSegment = -1;
    float handleDragOffset = 0;
    bool pointGesture = false;
    struct Snapshot { spectral::CurveBanks banks; spectral::PointCurves points; };
    // Capture banks and point metadata for one undo step.
    Snapshot snapshot();
    // Render the selected point curve onto saved-grid values.
    void applyPointCurve();
    // Create point anchors from a drawn curve on first use.
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
    // Refresh the display on the GUI timer.
    void timerCallback() override;
    // Hit-test a graph position against the current bucket layout.
    int bin(float) const;
    // Construct the sample-rate-aware visible bucket mapping.
    spectral::BucketDisplay buckets() const;
    // Paint all saved samples covered by dragged display cells.
    void drawAt(juce::Point<float>);
    // Save an undo snapshot and publish a curve edit.
    void commit(const spectral::Curves &);
    // Commit one completed drawing gesture to undo history.
    void finishGesture();
    // Restore the pre-gesture snapshot after STFT automation.
    void cancelGesture();
    // Change the active bank count while retaining stored shapes.
    void changeBankCount(int delta);
    // Restore a bank and point snapshot for undo or redo.
    void stepHistory(bool backwards);
    // Invalidate queued frames and clear GUI-only history.
    void resetDisplayHistory();
};
