#include "PluginEditor.h"
#include "PresetFiles.h"
#include <limits>
// Build the native editor and bind it to processor state.
Editor::Editor(Processor &processor)
    : AudioProcessorEditor(processor), p(processor), resolutionControl(stftSize, "Resolution"),
      scrollControl(scrollSpeed, "Scroll")
{
    setLookAndFeel(&style);
    seenRevision = p.editorRevision();
    for (auto *button : {&undoButton, &redoButton}) addAndMakeVisible(button);
    undoButton.setTooltip("Undo the last curve or bank edit (Command-Z).");
    redoButton.setTooltip("Redo the last undone edit (Shift-Command-Z).");
    undoButton.onClick = [this] { stepHistory(true); };
    redoButton.onClick = [this] { stepHistory(false); };
    updateHistoryButtons();
    addAndMakeVisible(presetMenu);
    presetMenu.setName("Preset menu");
    presetMenu.setTooltip("Load or save plug-in and selected-bank presets.");
    presetMenu.onClick = [this]
    {
        juce::PopupMenu pluginPresets, bankPresets, presets;
        pluginPresets.addItem(1, "Default");
        bankPresets.addItem(2, "Default");
        pluginPresets.addSeparator();
        pluginPresets.addItem(3, "Save...");
        pluginPresets.addItem(4, "Load...");
        bankPresets.addSeparator();
        bankPresets.addItem(5, "Save...");
        bankPresets.addItem(6, "Load...");
        presets.addSubMenu("Plug-in", pluginPresets);
        presets.addSubMenu("Selected Bank", bankPresets);
        // The popup completes asynchronously and may outlive this editor.
        juce::Component::SafePointer<Editor> safeThis(this);
        presets.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetMenu),
                              [safeThis](int result)
        {
            if (safeThis == nullptr)
                return;
            if (result == 1)
                safeThis->loadDefaultPreset();
            else if (result == 2)
                safeThis->loadSelectedBankDefaultPreset();
            else if (result >= 3 && result <= 6)
                safeThis->choosePresetFile(result == 3 || result == 5, result >= 5);
        });
    };
    for (auto *button : {&drawMode, &pointsMode})
    {
        addAndMakeVisible(button);
        button->setClickingTogglesState(true);
        button->setRadioGroupId(300);
    }
    drawMode.setTooltip("Draw values directly into frequency buckets.");
    pointsMode.setTooltip("Add and drag points; right-click a point to remove it. Drag a solid midpoint handle vertically to bend its segment.");
    drawMode.onClick = [this] { finishGesture(); p.pointMode = false; selectedPoint = -1; repaint(); };
    pointsMode.onClick = [this] { finishGesture(); p.pointMode = true; selectedPoint = -1; repaint(); };
    setWantsKeyboardFocus(true);
    // knobs[] order is Wet, Max Delay, Curve Shift Depth/Rate, then IN/OUT trims.
    // resized() uses these same indices to place and size each control.
    const char *ids[] = {"mix",         "maxDelayMs",  "shiftHz",
                         "shiftRateHz", "inputTrimDb", "outputTrimDb"};
    const char *names[] = {"Wet", "Max Delay", "Depth Hz", "Rate Hz", "IN", "OUT"};
    for (int i = 0; i < 6; ++i)
    {
        addAndMakeVisible(knobs[i]);
        knobs[i].setName(ids[i]);
        knobs[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        // The 33 px value box sits below each rotary body; trims override its dimensions later.
        knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 96, 33);
        addAndMakeVisible(labels[i]);
        labels[i].setText(names[i], juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred);
        attachments.push_back(
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, ids[i],
                                                                                   knobs[i]));
    }
    // Keep host automation's existing mapping; only the knob's travel is logarithmic.
    knobs[2].setNormalisableRange(EditorStyle::shiftDepthRange());
    knobs[0].setTextValueSuffix(" %");
    knobs[2].setTextValueSuffix(" Hz");
    knobs[3].setTextValueSuffix(" Hz");
    knobs[4].setTextValueSuffix(" dB");
    knobs[5].setTextValueSuffix(" dB");
    knobs[1].textFromValueFunction = [](double milliseconds)
    {
        return EditorStyle::delayUsesSeconds(milliseconds)
                   ? juce::String(milliseconds / 1000.0, 2) + " s"
                   : juce::String(milliseconds, 1) + " ms";
    };
    knobs[1].valueFromTextFunction = [this](const juce::String &text)
    { return EditorStyle::delayValueFromText(text, knobs[1].getValue()); };
    knobs[2].setTooltip(
        "Move delay, feedback and level shapes along the frequency axis in opposite L/R "
        "directions. Amount is in Hz; audio bins keep their frequencies.");
    knobs[3].setTooltip("Modulation rate; zero holds a fixed left-up/right-down offset.");
    addChildComponent(scrollSpeed);
    addChildComponent(stftSize);
    scrollSpeed.setVisible(false);
    addAndMakeVisible(resolutionControl);
    addAndMakeVisible(scrollControl);
    addAndMakeVisible(bankLess);
    addAndMakeVisible(bankMore);
    bankLess.setName("Remove Bank");
    bankMore.setName("Add Bank");
    bankLess.setTooltip("Remove an active bank; its shapes are retained.");
    bankMore.setTooltip("Add an active bank (up to four).");
    bankLess.onClick = [this] { changeBankCount(-1); };
    bankMore.onClick = [this] { changeBankCount(1); };
    scrollSpeed.setName("Spectrogram Scroll Speed");
    scrollSpeed.addItemList(
        {"Scroll: 4x (1.5 s)", "Scroll: 2x (3 s)", "Scroll: 1x (6 s)", "Scroll: 0.5x (12 s)"}, 1);
    scrollSpeed.setSelectedId(p.spectrogramSpeed.load() + 1, juce::dontSendNotification);
    scrollSpeed.setTooltip(
        "Faster scrolling shows a shorter time span and more vertical detail per tap. Changing "
        "speed restarts visual history only. Saved with the project.");
    scrollSpeed.onChange = [this]
    {
        p.spectrogramSpeed = scrollSpeed.getSelectedId() - 1;
        activeScrollSpeed = p.spectrogramSpeed.load();
        delayHistory.setSpeed(p.spectrogramSpeed.load());
        resetDisplayHistory();
        scrollControl.repaint();
        repaint();
    };
    addAndMakeVisible(clearBuffer);
    clearBuffer.onClick = [this]
    {
        p.requestClearBuffer();
        resetDisplayHistory();
    };
    clearBuffer.setTooltip("Fade out and clear delay, feedback and wet tails. Curves "
                           "and dry audio are preserved.");
    const char *parameterNames[] = {"Delay", "Feedback", "Level"};
    const juce::Colour parameterColors[] = {freeqy::cyan, freeqy::pink, freeqy::violet};
    for (int i = 0; i < 3; ++i)
    {
        auto &button = parameterButtons[i];
        addAndMakeVisible(button);
        button.setButtonText(parameterNames[i]);
        button.setClickingTogglesState(true);
        button.setRadioGroupId(100);
        button.setColour(juce::TextButton::buttonOnColourId, parameterColors[i].darker(.35f));
        button.onClick = [this, i]
        {
            finishGesture();
            selectedPoint = -1;
            p.selectedCurve = i;
            timerCallback();
        };
    }
    for (int i = 0; i < 4; ++i)
    {
        auto &button = bankButtons[i];
        addAndMakeVisible(button);
        button.setComponentID("bankTab");
        button.setButtonText("Bank " + juce::String(i + 1));
        button.setClickingTogglesState(true);
        button.setRadioGroupId(200);
        button.setColour(juce::TextButton::buttonOnColourId, freeqy::violet);
        button.onClick = [this, i]
        {
            finishGesture();
            selectedPoint = -1;
            p.selectedBank = i;
            timerCallback();
        };
    }
    channelButton.setComponentID("channelSelector");
    channelButton.setTooltip("Choose the left or right channel to edit.");
    link.setComponentID("channelLink");
    link.setTooltip("Link channel edits. Enabling copies this channel's shapes to the other.");
    stereoMorph.setComponentID("stereoMorph");
    addAndMakeVisible(channelButton);
    channelButton.onClick = [this]
    {
        finishGesture();
        selectedPoint = -1;
        p.selectedChannel = 1 - p.selectedChannel.load();
        timerCallback();
    };
    morph.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    morph.setName("curveMorph");
    morph.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    morph.setTextValueSuffix(" %");
    morphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.state, "curveMorph", morph);
    morphLabel.setText("Morph", juce::dontSendNotification);
    morph.setTooltip(
        "Automatable bank-morph center. Stereo Morph offsets the two channels around this value.");
    stereoMorphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.state, "stereoMorph", stereoMorph);
    morphDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.state, "morphDepth", morphDepth);
    morphRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.state, "morphRateHz", morphRate);
    for (auto *slider : {&morphDepth, &morphRate})
    {
        slider->setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    }
    morphDepth.setName("morphDepth");
    morphRate.setName("morphRateHz");
    morphDepth.setTextValueSuffix(" %");
    morphRate.setTextValueSuffix(" Hz");
    morphDepthLabel.setText("Depth", juce::dontSendNotification);
    morphRateLabel.setText("Rate", juce::dontSendNotification);
    stereoMorph.setTooltip("Choose same or opposite L/R Morph modulation. Set Depth to zero to turn modulation off.");
    for (juce::Component *control : std::initializer_list<juce::Component *>{
             &stereoMorph, &morphDepth, &morphRate, &morphDepthLabel, &morphRateLabel})
        addAndMakeVisible(control);
    for (juce::Component *control : std::initializer_list<juce::Component *>{&morph, &morphLabel})
        addAndMakeVisible(control);
    stftSize.addItemList({"16 samples", "32 samples", "64 samples", "128 samples", "256 samples",
                          "512 samples", "1024 samples", "2048 samples"},
                         1);
    const juce::String resolutionTip =
        "Larger values improve frequency resolution but smooth transients. Changing resolution "
        "fades and clears the wet tail; curve banks are preserved.";
    stftSize.setTooltip(resolutionTip);
    resolutionControl.setHelpText(resolutionTip);
    link.setToggleState(p.linked, juce::dontSendNotification);
    for (juce::Component *c : std::initializer_list<juce::Component *>{&link, &reset})
        addAndMakeVisible(c);
    link.onClick = [this]
    {
        p.linked = link.getToggleState();
        if (p.linked)
        {
            auto c = p.getCurves();
            c.values[1 - p.selectedChannel.load()] = c.values[p.selectedChannel.load()];
            commit(c);
            auto points = p.getPointCurves();
            points.banks[p.selectedBank][1 - p.selectedChannel.load()] = points.banks[p.selectedBank][p.selectedChannel];
            p.setPointCurves(points);
        }
    };
    reset.onClick = [this]
    {
        auto c = p.getCurves();
        int t = p.selectedCurve;
        c.values[p.selectedChannel.load()][t].fill(t == 0 ? .25f : t == 1 ? .35f : 1.f);
        if (p.linked)
            c.values[1 - p.selectedChannel.load()][t] = c.values[p.selectedChannel.load()][t];
        commit(c);
        auto points = p.getPointCurves();
        points.banks[p.selectedBank][p.selectedChannel][t].count = 0;
        if (p.linked) points.banks[p.selectedBank][1 - p.selectedChannel][t].count = 0;
        p.setPointCurves(points);
        selectedPoint = -1;
    };
    sizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.state, "stftSize", stftSize);
    setResizable(true, true);
    // The minimum still fits the graph, toolbar, and five bottom control panels.
    setResizeLimits(960, 700, 2000, 1400);
    setSize(1120, 800);
    resetDisplayHistory();
    timerCallback();
    startTimerHz(30);
}
// Open a native file picker for a plug-in or selected-bank preset.
void Editor::choosePresetFile(bool save, bool selectedBank)
{
    const juce::String extension = selectedBank ? ".freeqybank" : ".freeqy";
    const auto documents = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    const auto start = save ? documents.getChildFile("FREEQY DELAY" + extension) : documents;
    presetChooser = std::make_unique<juce::FileChooser>(
        save ? "Save preset" : "Load preset", start, "*" + extension);
    const int flags = juce::FileBrowserComponent::canSelectFiles |
                      (save ? juce::FileBrowserComponent::saveMode |
                                  juce::FileBrowserComponent::warnAboutOverwriting
                            : juce::FileBrowserComponent::openMode);
    juce::Component::SafePointer<Editor> safeThis(this);
    presetChooser->launchAsync(flags, [safeThis, save, selectedBank, extension](const juce::FileChooser &chooser)
    {
        if (safeThis == nullptr) return;
        auto file = chooser.getResult();
        if (file == juce::File()) return;
        if (save) file = file.withFileExtension(extension);
        auto &editor = *safeThis;
        if (!save) editor.finishGesture();
        const auto before = !save && selectedBank ? editor.snapshot() : Snapshot{};
        const auto result = save ? (selectedBank ? spectral::presets::saveBank(editor.p, file)
                                               : spectral::presets::savePlugin(editor.p, file))
                                 : (selectedBank ? spectral::presets::loadBank(editor.p, file)
                                                 : spectral::presets::loadPlugin(editor.p, file));
        if (result.failed())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Preset", result.getErrorMessage());
            return;
        }
        if (!save)
        {
            editor.selectedPoint = editor.selectedSegment = -1;
            if (selectedBank)
            {
                editor.undoStack.push_back(before);
                if (editor.undoStack.size() > 100)
                    editor.undoStack.erase(editor.undoStack.begin());
            }
            else
                editor.undoStack.clear();
            editor.redoStack.clear();
            editor.timerCallback();
        }
    });
}
// Reset the whole plug-in to the documented Default preset.
void Editor::loadDefaultPreset()
{
    finishGesture();
    // Preset recall updates host parameters through their normal automation interfaces.
    for (const char *id : {"mix", "maxDelayMs", "shiftHz", "shiftRateHz", "inputTrimDb",
                           "outputTrimDb", "stftSize", "curveMorph", "stereoMorph",
                           "morphDepth", "morphRateHz"})
    {
        auto *parameter = p.state.getParameter(id);
        const auto &range = parameter->getNormalisableRange();
        const juce::String parameterId(id);
        const float requested = parameterId == "mix" ? .5f
                                : parameterId == "maxDelayMs" ? 1000.f
                                : parameterId == "stftSize" ? 6.f : 0.f;
        const float value = juce::jlimit(range.start, range.end, requested);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
    auto banks = p.getBanks();
    banks.count = 1;
    for (auto &bank : banks.banks)
        for (auto &channel : bank.values)
        {
            channel[0].fill(.25f);
            channel[1].fill(.35f);
            channel[2].fill(1.f);
        }
    p.setPointCurves({});
    p.selectedBank = 0;
    p.selectedCurve = 0;
    p.selectedChannel = 0;
    p.linked = false;
    selectedPoint = selectedSegment = -1;
    undoStack.clear();
    redoStack.clear();
    p.updateBanks(banks);
    timerCallback();
}
// Reset only the selected bank and its point metadata.
void Editor::loadSelectedBankDefaultPreset()
{
    finishGesture();
    selectedPoint = selectedSegment = -1;
    const int bankIndex = juce::jlimit(0, 3, p.selectedBank.load());
    auto banks = p.getBanks();
    auto &bank = banks.banks[size_t(bankIndex)];
    for (auto &channel : bank.values)
    {
        channel[0].fill(.25f);
        channel[1].fill(.35f);
        channel[2].fill(1.f);
    }
    auto points = p.getPointCurves();
    // Clear the bank's point metadata too; otherwise a later point edit could
    // render its old shape over the newly reset grid curves.
    points.banks[size_t(bankIndex)] = {};
    undoStack.push_back(snapshot());
    if (undoStack.size() > 100)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
    p.updateBanks(banks);
    p.setPointCurves(points);
    timerCallback();
}
// Invalidate queued frames and clear GUI-only history.
void Editor::resetDisplayHistory()
{
    // Invalidate in-flight analyzer blocks before clearing the GUI-owned ring.
    p.restartSpectra();
    delayHistory.clear();
    for (auto &channel : magnitudes)
        channel.fill(0);
    displayClockOffset = 0;
    lastDisplayAudio = lastDisplayArrival = -1;
}
// Change the active bank count while retaining stored shapes.
void Editor::changeBankCount(int delta)
{
    finishGesture();
    selectedPoint = -1;
    auto banks = p.getBanks();
    const int count = juce::jlimit(1, 4, banks.count + delta);
    if (count == banks.count)
        return;
    undoStack.push_back(snapshot());
    if (undoStack.size() > 100)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
    banks.count = count;
    p.selectedBank = std::min(p.selectedBank.load(), count - 1);
    p.updateBanks(banks);
    timerCallback();
}
// Discard snapshots and partial gestures from an earlier processor state.
void Editor::syncRevision()
{
    const auto revision = p.editorRevision();
    if (revision == seenRevision) return;
    seenRevision = revision;
    undoStack.clear();
    redoStack.clear();
    updateHistoryButtons();
    lastBin = -1;
    pointGesture = false;
    selectedPoint = selectedSegment = -1;
    updateHistoryButtons();
}
// Reflect the availability of undo and redo actions.
void Editor::updateHistoryButtons()
{
    undoButton.setEnabled(!undoStack.empty());
    redoButton.setEnabled(!redoStack.empty());
}
// Restore a bank and point snapshot for undo or redo.
void Editor::stepHistory(bool backwards)
{
    syncRevision();
    finishGesture();
    auto &source = backwards ? undoStack : redoStack;
    auto &destination = backwards ? redoStack : undoStack;
    if (source.empty())
        return;
    destination.push_back(snapshot());
    p.selectedBank = std::min(p.selectedBank.load(), source.back().banks.count - 1);
    p.updateBanks(source.back().banks);
    p.setPointCurves(source.back().points);
    selectedPoint = -1;
    source.pop_back();
    timerCallback();
}
// Handle gesture undo and redo shortcuts.
bool Editor::keyPressed(const juce::KeyPress &key)
{
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Z')
    {
        stepHistory(!key.getModifiers().isShiftDown());
        return true;
    }
    return false;
}
// Save an undo snapshot and publish a curve edit.
void Editor::commit(const spectral::Curves &c)
{
    undoStack.push_back(snapshot());
    if (undoStack.size() > 100)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
    updateHistoryButtons();
    p.updateCurves(c);
    repaint();
}
// Refresh the display on the GUI timer.
void Editor::timerCallback()
{
    refreshDisplay(juce::Time::getMillisecondCounterHiRes() * .001);
}
// Drain analyzer frames and refresh GUI-owned visual state.
void Editor::refreshDisplay(double nowSeconds)
{
    syncRevision();
    updateHistoryButtons();
    if ((lastBin >= 0 || pointGesture) && gestureSize != buckets().size)
        cancelGesture();
    auto banks = p.getBanks();
    if (p.pointMode) ensurePoints();
    drawMode.setToggleState(!p.pointMode.load(), juce::dontSendNotification);
    pointsMode.setToggleState(p.pointMode.load(), juce::dontSendNotification);
    bankLess.setEnabled(banks.count > 1);
    bankMore.setEnabled(banks.count < 4);
    for (int i = 0; i < 4; ++i)
    {
        bankButtons[i].setEnabled(i < banks.count);
        bankButtons[i].setToggleState(i == p.selectedBank.load(), juce::dontSendNotification);
    }
    for (int i = 0; i < 3; ++i)
        parameterButtons[i].setToggleState(i == p.selectedCurve.load(), juce::dontSendNotification);
    channelButton.setButtonText(p.selectedChannel.load() == 0 ? "Channel: L" : "Channel: R");
    p.publishPending();
    const int speed = p.spectrogramSpeed.load();
    if (speed != activeScrollSpeed)
    {
        activeScrollSpeed = speed;
        resetDisplayHistory();
    }
    scrollSpeed.setSelectedId(speed + 1, juce::dontSendNotification);
    delayHistory.setSpeed(speed);
    scrollControl.refresh();
    resolutionControl.refresh();
    const bool stalled = lastDisplayArrival >= 0 && nowSeconds - lastDisplayArrival > .15;
    const int receivedCount = p.drainSpectra(
        displayFrames.data(), int(displayFrames.size()),
        stalled ? .15 : std::numeric_limits<double>::infinity());
    const bool received = receivedCount > 0;
    const auto &fresh = displayFrames[size_t(std::max(0, receivedCount - 1))];
    {
        const double sampleRate = p.getSampleRate() > 0 ? p.getSampleRate() : 48000.;
        if (received)
        {
            if (lastDisplayArrival < 0 || nowSeconds - lastDisplayArrival > .15 ||
                fresh.seconds <= lastDisplayAudio ||
                std::abs(nowSeconds - (fresh.seconds + displayClockOffset)) > .25)
                displayClockOffset = nowSeconds - fresh.seconds;
            for (int i = 0; i < receivedCount; ++i)
                delayHistory.append(displayFrames[size_t(i)].seconds + displayClockOffset,
                                    sampleRate, &displayFrames[size_t(i)].delay);
            lastDisplayAudio = fresh.seconds;
            lastDisplayArrival = nowSeconds;
        }
        else if (lastDisplayArrival < 0 || nowSeconds - lastDisplayArrival > .15)
            delayHistory.append(nowSeconds, sampleRate, nullptr);
    }
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < spectral::B; ++i)
        {
            magnitudes[ch][i] =
                std::max(received ? fresh.input[ch][i] : 0.f, magnitudes[ch][i] * .83f);
        }
    link.setToggleState(p.linked, juce::dontSendNotification);
    repaint();
}
// Lay out the graph, bank tools, toolbar, and bottom controls.
void Editor::resized()
{
    // Top row: preset button at right, then Draw and Points mode buttons at left.
    auto header = getLocalBounds().removeFromTop(40).withTrimmedLeft(200).withTrimmedRight(24).reduced(0, 5);
    presetMenu.setBounds(header.removeFromRight(140));
    redoButton.setBounds(header.removeFromRight(70));
    undoButton.setBounds(header.removeFromRight(70));
    drawMode.setBounds(header.removeFromLeft(70));
    pointsMode.setBounds(header.removeFromLeft(70));
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(25);
    curvePanel = area;
    // The 42 px bank row has four equal tabs and a 106 px +/- area on the right.
    auto tabs = area.removeFromTop(42);
    bankTools = tabs.removeFromRight(106);
    const int tabWidth = tabs.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
        bankButtons[i].setBounds(i == 3 ? tabs : tabs.removeFromLeft(tabWidth));
    bankMore.setBounds(bankTools.getRight() - 31, bankTools.getY(), 31, 21);
    bankLess.setBounds(bankTools.getRight() - 31, bankTools.getY() + 21, 31, 21);
    // The strip reserves enough height for Max Delay's larger rotary body and
    // two stacked trims while keeping their value boxes inside the panel.
    constexpr int bottomStripHeight = 170;
    auto bottom = area.removeFromBottom(bottomStripHeight);
    // The 49 px editing toolbar sits directly above the bottom controls.
    toolbar = area.removeFromBottom(49);
    // Axis and pointer readout belong to the display, above the editing toolbar.
    plot = area.withTrimmedBottom(40).reduced(1, 0);
    auto tools = toolbar.reduced(8, 6);
    // Nominal widths, including a residual Clear Buffer slot, scale to the toolbar width.
    // parameterButtons are indexed Delay=0, Feedback=1, Level=2, hence the 0/2/1 order.
    const float scale = float(tools.getWidth() - 8 * 5) / 888.f;
    auto placeTool = [&](juce::Component &control, int width)
    {
        control.setBounds(tools.removeFromLeft(int(std::round(width * scale))));
        tools.removeFromLeft(5);
    };
    placeTool(parameterButtons[0], 84);
    placeTool(parameterButtons[2], 76);
    placeTool(parameterButtons[1], 96);
    placeTool(scrollControl, 164);
    placeTool(resolutionControl, 180);
    placeTool(link, 42);
    placeTool(channelButton, 64);
    placeTool(reset, 70);
    clearBuffer.setBounds(tools);
    const int width = bottom.getWidth();
    // Left to right: Bank Morph 37%, Max Delay 17%, Curve Shift 23%, Wet 12%, trims remainder.
    morphPanel = bottom.removeFromLeft(int(width * .37f));
    delayPanel = bottom.removeFromLeft(int(width * .17f));
    shiftPanel = bottom.removeFromLeft(int(width * .23f));
    mixPanel = bottom.removeFromLeft(int(width * .12f));
    gainPanel = bottom;
    auto morphArea = morphPanel.reduced(8, 0);
    // Reserve 38 px for the direction arrows, then split Morph/Depth/Rate evenly.
    morphArea.removeFromLeft(38);
    // Arrow button: 30 x 58, centered vertically beside the three numeric controls.
    stereoMorph.setBounds(morphPanel.getX() + 7, bottom.getY() + 55, 30, 58);
    const int numberWidth = morphArea.getWidth() / 3;
    for (auto pair : {std::pair{&morph, &morphLabel}, std::pair{&morphDepth, &morphDepthLabel},
                      std::pair{&morphRate, &morphRateLabel}})
    {
        auto cell = morphArea.removeFromLeft(numberWidth);
        // The heading is 18 px high at y+29; its digital value occupies y+52 through y+116.
        pair.second->setBounds(cell.withY(bottom.getY() + 29).withHeight(18));
        pair.first->setBounds(cell.withY(bottom.getY() + 52).withHeight(64));
    }
    // Indices: 1=Max Delay, 2/3=Curve Shift Depth/Rate, 0=Wet.
    // Max Delay gets the taller 110 px slider; the other three use 98 px.
    auto rotary = [this, bottom](int index, juce::Rectangle<int> cell)
    {
        cell = cell.reduced(3, 0);
        labels[index].setBounds(cell.withY(bottom.getY() + 29).withHeight(18));
        // Rotary bounds include the value box; its extra 33 px sits below the cap.
        const bool large = index == 1;
        knobs[index].setBounds(cell.withY(bottom.getY() + (large ? 46 : 52))
                                    .withHeight(large ? 110 : 98));
    };
    rotary(1, delayPanel);
    auto shift = shiftPanel;
    const int half = shift.getWidth() / 2;
    rotary(2, shift.removeFromLeft(half));
    rotary(3, shift);
    rotary(0, mixPanel);
    // IN (index 4) and OUT (index 5) are stacked in the rightmost panel.
    // Each 77 px slider uses panel width minus 20; its label hugs the panel's right edge.
    for (int i : {4, 5})
    {
        const int rowY = bottom.getY() + (i == 4 ? 2 : 81);
        knobs[i].setBounds(gainPanel.getX() + 2, rowY, gainPanel.getWidth() - 20, 77);
        knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                 gainPanel.getWidth() - 20, 29);
        labels[i].setBounds(gainPanel.getRight() - 37, rowY + 13, 34, 22);
    }
}
// Construct the sample-rate-aware visible bucket mapping.
spectral::BucketDisplay Editor::buckets() const
{
    return {16 << juce::jlimit(0, 7, int(p.state.getRawParameterValue("stftSize")->load())),
            p.getSampleRate() > 0 ? p.getSampleRate() : 48000., float(plot.getX()),
            float(plot.getWidth())};
}
// Hit-test a graph position against the current bucket layout.
int Editor::bin(float x) const
{
    return buckets().binAt(x);
}
// Render the graph, activity, curves, and panel surfaces.
void Editor::paint(juce::Graphics &g)
{
    g.fillAll(freeqy::ink);
    g.setColour(freeqy::cyan);
    g.setFont(12.f);
    g.drawText("FREEQY DELAY", 24, 12,
               getWidth() - 48, 20, juce::Justification::centredLeft);
    {
        g.setColour(freeqy::panel);
        g.fillRect(curvePanel);
        g.setColour(freeqy::blueViolet);
        g.drawRect(curvePanel);
        g.drawRect(toolbar);
        for (const auto &panel : {morphPanel, delayPanel, shiftPanel, mixPanel, gainPanel})
            g.drawRect(panel);
        g.setFont(12.f);
        g.setColour(freeqy::sky);
        g.drawText("BANKS", bankTools.withTrimmedRight(31), juce::Justification::centred);
        g.drawText("BANK MORPH", morphPanel.reduced(12, 8).removeFromTop(19),
                   juce::Justification::centred);
        g.drawText("CURVE SHIFT", shiftPanel.reduced(12, 8).removeFromTop(19),
                   juce::Justification::centred);
    }
    g.setColour(freeqy::well);
    g.fillRoundedRectangle(plot.toFloat(), 6);
    {
        g.setFont(12.f);
        for (int tick = 0; tick <= 4; ++tick)
        {
            int y = plot.getBottom() - tick * plot.getHeight() / 4;
            g.setColour(freeqy::deepViolet.withAlpha(.65f));
            g.drawHorizontalLine(y, float(plot.getX()), float(plot.getRight()));
        }
    }
    g.saveState();
    g.reduceClipRegion(plot);
    delayHistory.draw(g, plot, p.selectedChannel.load());
    const auto layout = buckets();
    const int ch = p.selectedChannel.load(), selected = p.selectedCurve.load();
    const auto stored = p.getCurves();
    const auto banks = p.getBanks();
    const float morph = p.liveMorphPosition(ch);
    const float offset = p.liveShiftOffset(ch);
    const juce::Colour colors[] = {freeqy::cyan, freeqy::pink, freeqy::violet};
    // Snap every band to device-independent pixels. Shared pixels keep maxima, never faint subpixel
    // caps.
    // Columns 8 and 9 hold the opposite channel's selected curve and input activity.
    std::vector<std::array<float, 10>> columns(size_t(plot.getWidth()), std::array<float, 10>{});
    for (int k = 0; k <= layout.size / 2; ++k)
    {
        const auto [left, right] = layout.edges(k);
        if (right <= left)
            continue;
        const float index = layout.curvePosition(k);
        const float inputActivity = juce::jlimit(
            0.f, 1.f,
            (juce::Decibels::gainToDecibels(layout.magnitude(magnitudes[ch], k), -90.f) + 90) / 90);
        const float otherValue = layout.curveValue(stored.values[1 - ch][selected], k);
        const float otherActivity = juce::jlimit(
            0.f, 1.f,
            (juce::Decibels::gainToDecibels(layout.magnitude(magnitudes[1 - ch], k), -90.f) + 90) / 90);
        const int first = juce::jlimit(0, plot.getWidth() - 1, int(std::floor(left)) - plot.getX());
        const int last =
            juce::jlimit(0, plot.getWidth() - 1, int(std::ceil(right)) - 1 - plot.getX());
        for (int x = first; x <= last; ++x)
        {
            auto &pixel = columns[size_t(x)];
            pixel[7] = 1;
            for (int t = 0; t < 3; ++t)
            {
                pixel[t] = std::max(pixel[t], layout.curveValue(stored.values[ch][t], k));
                pixel[t + 3] = std::max(pixel[t + 3], banks.shiftedSample(
                    ch, t, index, offset, p.getSampleRate() > 0 ? p.getSampleRate() : 48000., morph));
            }
            pixel[6] = std::max(pixel[6], inputActivity);
            pixel[8] = std::max(pixel[8], otherValue);
            pixel[9] = std::max(pixel[9], otherActivity);
        }
    }
    const bool showOtherChannel = std::any_of(columns.begin(), columns.end(),
        [selected](const auto &pixel) { return pixel[7] != 0 && std::abs(pixel[selected] - pixel[8]) > .00001f; });
    {
        for (int x = 0; x < plot.getWidth(); ++x)
        {
            const auto &pixel = columns[size_t(x)];
            if (pixel[7] == 0)
                continue;
            if (showOtherChannel)
            {
                const int otherHeight = int(std::round(pixel[8] * (plot.getHeight() - 1)));
                g.setColour(colors[selected].withAlpha(.04f + .07f * pixel[9]));
                g.fillRect(plot.getX() + x, plot.getBottom() - otherHeight, 1, otherHeight);
            }
            const int height = int(std::round(pixel[selected] * (plot.getHeight() - 1)));
            g.setColour(colors[selected].withAlpha(.10f + .20f * pixel[6]));
            g.fillRect(plot.getX() + x, plot.getBottom() - height, 1, height);
        }
    }
    for (int x = 0; x < plot.getWidth(); ++x)
    {
        const auto &pixel = columns[size_t(x)];
        if (pixel[7] == 0)
            continue;
        const int inputHeight = int(std::round(pixel[6] * plot.getHeight()));
        g.setColour(freeqy::sky.withAlpha(.55f));
        g.fillRect(plot.getX() + x, plot.getBottom() - inputHeight, 1, inputHeight);
    }
    {
        if (showOtherChannel)
        {
            g.setColour(colors[selected].withAlpha(.45f));
            for (int x = 0; x < plot.getWidth(); ++x)
            {
                const auto &pixel = columns[size_t(x)];
                if (pixel[7] == 0) continue;
                const int y = plot.getBottom() - 1 - int(std::round(pixel[8] * (plot.getHeight() - 1)));
                g.fillRect(plot.getX() + x, y, 1, 1);
            }
        }
    }
    for (int pass = 0; pass < 3; ++pass)
    {
        int t = (selected + 1 + pass) % 3;
        for (int x = 0; x < plot.getWidth(); ++x)
        {
            const auto &pixel = columns[size_t(x)];
            if (pixel[7] == 0)
                continue;
            {
                const int y =
                    plot.getBottom() - 1 - int(std::round(pixel[t] * (plot.getHeight() - 1)));
                g.setColour(colors[t].withAlpha(t == selected ? 1.f : .9f));
                g.fillRect(plot.getX() + x, y, 1, t == selected ? 2 : 1);
                if (std::abs(pixel[t] - pixel[t + 3]) > .002f && x % 6 < 3)
                {
                    const int target = plot.getBottom() - 1 -
                                       int(std::round(pixel[t + 3] * (plot.getHeight() - 1)));
                    g.setColour(colors[t].withAlpha(.75f));
                    g.fillRect(plot.getX() + x, target, 1, 1);
                }
            }
        }
    }
    if (p.pointMode)
    {
        const auto points = p.getPointCurves();
        const auto &curve = points.banks[p.selectedBank][ch][selected];
        auto position = [this](float x, float value)
        {
            return juce::Point<float>{juce::jlimit(float(plot.getX() + 4), float(plot.getRight() - 5),
                                                  plot.getX() + x * plot.getWidth()),
                                      juce::jlimit(float(plot.getY() + 4), float(plot.getBottom() - 5),
                                                  plot.getBottom() - value * plot.getHeight())};
        };
        for (int i = 0; i + 1 < curve.count; ++i)
        {
            const float x = (curve.points[i].x + curve.points[i + 1].x) * .5f;
            const auto pos = position(x, curve.valueAt(x));
            g.setColour(i == selectedSegment ? freeqy::pink : colors[selected]);
            g.fillEllipse(pos.x - 3.5f, pos.y - 3.5f, 7.f, 7.f);
        }
        for (int i = 0; i < curve.count; ++i)
        {
            const auto pos = position(curve.points[i].x, curve.points[i].value);
            g.setColour(freeqy::well);
            g.fillEllipse(pos.x - 5, pos.y - 5, 10, 10);
            g.setColour(i == selectedPoint ? freeqy::pink : colors[selected]);
            g.drawEllipse(pos.x - 5, pos.y - 5, 10, 10, 2.f);
        }
    }
    g.restoreState();
    g.setFont(13.f);
    g.setColour(freeqy::sky);
    for (int frequency : {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000})
    {
        const int x = plot.getX() + int(std::log(double(frequency) / 20.) / std::log(20000. / 20.) *
                                        plot.getWidth());
        const int labelX = juce::jlimit(plot.getX(), plot.getRight() - 60, x - 30);
        const auto label = frequency < 1000 ? juce::String(frequency) + " Hz"
                                            : juce::String(frequency / 1000) + " kHz";
        g.drawText(label, labelX, plot.getBottom() + 3, 60, 18, juce::Justification::centred);
    }
    g.drawText(readout, 24, plot.getBottom() + 20, getWidth() - 48, 18,
               juce::Justification::centred);
}
// Paint all saved samples covered by dragged display cells.
void Editor::drawAt(juce::Point<float> point)
{
    const auto layout = buckets();
    if (lastBin >= 0 && gestureSize != layout.size)
    {
        cancelGesture();
        return;
    }
    int k = layout.binAt(point.x), t = p.selectedCurve, ch = p.selectedChannel;
    if (lastBin < 0)
    {
        auto points = p.getPointCurves();
        points.banks[p.selectedBank][ch][t].count = 0;
        if (p.linked) points.banks[p.selectedBank][1 - ch][t].count = 0;
        p.setPointCurves(points);
    }
    float value = juce::jlimit(0.f, 1.f, (plot.getBottom() - point.y) / plot.getHeight());
    if (lastBin < 0)
    {
        lastBin = k;
        lastValue = value;
    }
    int lo = std::min(k, lastBin), hi = std::max(k, lastBin);
    for (int bucket = lo; bucket <= hi; ++bucket)
    {
        const float fraction = k == lastBin ? 1.f : float(bucket - lastBin) / float(k - lastBin);
        const float v = lastValue + (value - lastValue) * fraction;
        const auto [first, last] = layout.cell(bucket);
        for (int i = first; i <= last; ++i)
            drawing.values[ch][t][i] = v;
    }
    if (p.linked)
        drawing.values[1 - ch][t] = drawing.values[ch][t];
    lastBin = k;
    lastValue = value;
    p.updateCurves(drawing);
    repaint();
}
// Begin a bucket or point-edit gesture from the graph.
void Editor::mouseDown(const juce::MouseEvent &e)
{
    syncRevision();
    if (!plot.contains(e.getPosition()))
        return;
    gestureSize = buckets().size;
    beforeBanks = snapshot();
    if (p.pointMode)
    {
        ensurePoints();
        auto points = p.getPointCurves();
        auto &curve = points.banks[p.selectedBank][p.selectedChannel][p.selectedCurve];
        const float x = juce::jlimit(0.f, 1.f, (e.position.x - plot.getX()) / plot.getWidth());
        const float y = juce::jlimit(0.f, 1.f, (plot.getBottom() - e.position.y) / plot.getHeight());
        selectedPoint = -1;
        selectedSegment = -1;
        float best = 10.f;
        for (int i = 0; i < curve.count; ++i)
        {
            const float px = juce::jlimit(float(plot.getX() + 4), float(plot.getRight() - 5), plot.getX() + curve.points[i].x * plot.getWidth());
            const float py = juce::jlimit(float(plot.getY() + 4), float(plot.getBottom() - 5), plot.getBottom() - curve.points[i].value * plot.getHeight());
            const float dx = px - e.position.x;
            const float dy = py - e.position.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance < best) { best = distance; selectedPoint = i; }
        }
        if (e.mods.isPopupMenu())
        {
            if (selectedPoint > 0 && selectedPoint < curve.count - 1)
            {
                for (int i = selectedPoint; i + 1 < curve.count; ++i) curve.points[i] = curve.points[i + 1];
                --curve.count;
                if (p.linked) points.banks[p.selectedBank][1 - p.selectedChannel][p.selectedCurve] = curve;
                p.setPointCurves(points); applyPointCurve(); pointGesture = true; finishGesture();
            }
            selectedPoint = -1; repaint(); return;
        }
        if (selectedPoint < 0)
        {
            best = 9.f;
            for (int i = 0; i + 1 < curve.count; ++i)
            {
                const float mid = (curve.points[i].x + curve.points[i + 1].x) * .5f;
                const float px = juce::jlimit(float(plot.getX() + 4), float(plot.getRight() - 5), plot.getX() + mid * plot.getWidth());
                const float py = juce::jlimit(float(plot.getY() + 4), float(plot.getBottom() - 5), plot.getBottom() - curve.valueAt(mid) * plot.getHeight());
                const float distance = std::hypot(px - e.position.x, py - e.position.y);
                if (distance < best) { best = distance; selectedSegment = i; handleDragOffset = curve.valueAt(mid) - y; }
            }
            if (selectedSegment >= 0)
            {
                pointGesture = std::abs(curve.points[selectedSegment + 1].value -
                                        curve.points[selectedSegment].value) >= 1.e-6f;
                repaint(); return;
            }
        }
        if (selectedPoint < 0 && curve.count < spectral::PointCurve::capacity)
        {
            int i = 1; while (i < curve.count && curve.points[i].x < x) ++i;
            if (i < curve.count && x > curve.points[i-1].x + .0001f && x < curve.points[i].x - .0001f)
            {
                for (int j = curve.count; j > i; --j) curve.points[j] = curve.points[j-1];
                curve.points[i] = {x, y, 0}; ++curve.count; selectedPoint = i;
                if (p.linked) points.banks[p.selectedBank][1 - p.selectedChannel][p.selectedCurve] = curve;
                p.setPointCurves(points); applyPointCurve();
            }
        }
        if (selectedPoint >= 0)
            pointGesture = true;
        repaint(); return;
    }
    drawing = p.getCurves();
    lastBin = -1;
    drawAt(e.position);
}
// Update the active bucket, anchor, or curvature edit.
void Editor::mouseDrag(const juce::MouseEvent &e)
{
    syncRevision();
    if (pointGesture && selectedSegment >= 0)
    {
        if (gestureSize != buckets().size) { cancelGesture(); return; }
        auto points = p.getPointCurves();
        auto &curve = points.banks[p.selectedBank][p.selectedChannel][p.selectedCurve];
        const float value = (plot.getBottom() - e.position.y) / plot.getHeight() + handleDragOffset;
        if (!curve.setMidpointValue(selectedSegment, value)) return;
        if (p.linked) points.banks[p.selectedBank][1 - p.selectedChannel][p.selectedCurve] = curve;
        p.setPointCurves(points); applyPointCurve();
    }
    else if (pointGesture && selectedPoint >= 0)
    {
        if (gestureSize != buckets().size) { cancelGesture(); return; }
        auto points = p.getPointCurves();
        auto &curve = points.banks[p.selectedBank][p.selectedChannel][p.selectedCurve];
        if (selectedPoint >= curve.count) return;
        auto &pt = curve.points[selectedPoint];
        if (selectedPoint > 0 && selectedPoint < curve.count - 1)
            pt.x = juce::jlimit(curve.points[selectedPoint-1].x + .0001f, curve.points[selectedPoint+1].x - .0001f,
                               (e.position.x - plot.getX()) / plot.getWidth());
        pt.value = juce::jlimit(0.f, 1.f, (plot.getBottom() - e.position.y) / plot.getHeight());
        if (p.linked) points.banks[p.selectedBank][1 - p.selectedChannel][p.selectedCurve] = curve;
        p.setPointCurves(points); applyPointCurve();
    }
    else if (lastBin >= 0) drawAt(e.position);
}
// Finish or cancel a gesture after a resolution change.
void Editor::mouseUp(const juce::MouseEvent &)
{
    syncRevision();
    if ((lastBin >= 0 || pointGesture) && gestureSize != buckets().size)
        cancelGesture();
    else
        finishGesture();
}
// Restore the pre-gesture snapshot after STFT automation.
void Editor::cancelGesture()
{
    if (lastBin < 0 && !pointGesture)
        return;
    // Automated resolution changes invalidate bucket coordinates mid-drag. Restore
    // both representations without adding a partial edit to undo history.
    p.updateBanks(beforeBanks.banks);
    p.setPointCurves(beforeBanks.points);
    lastBin = -1;
    pointGesture = false;
    selectedPoint = selectedSegment = -1;
    repaint();
}
// Commit one completed drawing gesture to undo history.
void Editor::finishGesture()
{
    syncRevision();
    if (lastBin < 0 && !pointGesture)
        return;
    undoStack.push_back(beforeBanks);
    if (undoStack.size() > 100)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
    updateHistoryButtons();
    lastBin = -1;
    pointGesture = false;
    selectedSegment = -1;
}
// Update the hovered cell and physical-value readout.
void Editor::mouseMove(const juce::MouseEvent &e)
{
    int k = bin(e.position.x), t = p.selectedCurve;
    const auto layout = buckets();
    float value = layout.curveValue(p.getCurves().values[p.selectedChannel.load()][t], k);
    float physical =
        t == 0 ? value * p.state.getRawParameterValue("maxDelayMs")->load() : value * 100;
    readout = juce::String(layout.boundaryFrequency(k), 1) + "–" +
              juce::String(layout.boundaryFrequency(k + 1), 1) + " Hz  |  bucket " +
              juce::String(k) + "  |  " + juce::String(physical, 2) + (t == 0 ? " ms" : " %");
}

// Capture banks and point metadata for one undo step.
Editor::Snapshot Editor::snapshot() { return {p.getBanks(), p.getPointCurves()}; }
// Create point anchors from a drawn curve on first use.
void Editor::ensurePoints()
{
    auto points = p.getPointCurves();
    auto &curve = points.banks[p.selectedBank][p.selectedChannel][p.selectedCurve];
    if (curve.count) return;
    const auto source = p.getCurves().values[p.selectedChannel][p.selectedCurve];
    const double sr = p.pointSampleRate();
    // Conversion is adaptive so narrow drawn peaks survive the first point edit.
    curve = spectral::PointCurve::fromCurve(source, sr);
    p.setPointCurves(points, sr);
}
// Render the selected point curve onto saved-grid values.
void Editor::applyPointCurve()
{
    const auto points = p.getPointCurves();
    auto curves = p.getCurves();
    const int ch = p.selectedChannel, type = p.selectedCurve;
    points.banks[p.selectedBank][ch][type].render(curves.values[ch][type], p.pointSampleRate());
    if (p.linked) curves.values[1-ch][type] = curves.values[ch][type];
    p.updateCurves(curves); repaint();
}
