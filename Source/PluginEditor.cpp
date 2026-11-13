#include "PluginEditor.h"
Editor::Editor(Processor &processor)
    : AudioProcessorEditor(processor), p(processor), resolutionControl(stftSize, "Resolution"),
      scrollControl(scrollSpeed, "Scroll")
{
    setLookAndFeel(&style);
    setWantsKeyboardFocus(true);
    const char *ids[] = {"mix",         "maxDelayMs",  "shiftHz",
                         "shiftRateHz", "inputTrimDb", "outputTrimDb"};
    const char *names[] = {"Wet", "Max Delay", "Depth Hz", "Rate Hz", "IN", "OUT"};
    for (int i = 0; i < 6; ++i)
    {
        addAndMakeVisible(knobs[i]);
        knobs[i].setName(ids[i]);
        knobs[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 22);
        addAndMakeVisible(labels[i]);
        labels[i].setText(names[i], juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred);
        attachments.push_back(
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, ids[i],
                                                                                   knobs[i]));
    }
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
    stereoMorph.setTooltip(
        "Sweep L/R bank positions oppositely around Morph, using the depth and rate below.");
    for (juce::Component *control : std::initializer_list<juce::Component *>{
             &stereoMorph, &morphDepth, &morphRate, &morphDepthLabel, &morphRateLabel})
        addAndMakeVisible(control);
    for (juce::Component *control : std::initializer_list<juce::Component *>{&morph, &morphLabel})
        addAndMakeVisible(control);
    stftSize.addItemList({"16 samples", "32 samples", "64 samples", "128 samples", "256 samples",
                          "512 samples", "1024 samples", "2048 samples"},
                         1);
    stftSize.setTooltip(
        "Analysis window size. Changes fade and clear the wet tail; curve banks are preserved.");
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
    };
    sizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.state, "stftSize", stftSize);
    if constexpr (SPECTRAL_DIAGNOSTIC)
        for (auto *control : getChildren())
            control->setVisible(false);
    setResizable(true, true);
    setResizeLimits(960, 700, 2000, 1400);
    setSize(1120, 800);
    resetDisplayHistory();
    timerCallback();
    startTimerHz(30);
}
void Editor::resetDisplayHistory()
{
    p.discardSpectra();
    delayHistory.clear();
    for (auto &channel : magnitudes)
        channel.fill(0);
    displayClockOffset = 0;
    lastDisplayAudio = lastDisplayArrival = -1;
}
void Editor::changeBankCount(int delta)
{
    finishGesture();
    auto banks = p.getBanks();
    const int count = juce::jlimit(1, 4, banks.count + delta);
    if (count == banks.count)
        return;
    undoStack.push_back(banks);
    if (undoStack.size() > 100)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
    banks.count = count;
    p.selectedBank = std::min(p.selectedBank.load(), count - 1);
    p.updateBanks(banks);
    timerCallback();
}
void Editor::stepHistory(bool backwards)
{
    finishGesture();
    auto &source = backwards ? undoStack : redoStack;
    auto &destination = backwards ? redoStack : undoStack;
    if (source.empty())
        return;
    destination.push_back(p.getBanks());
    p.selectedBank = std::min(p.selectedBank.load(), source.back().count - 1);
    p.updateBanks(source.back());
    source.pop_back();
    timerCallback();
}
bool Editor::keyPressed(const juce::KeyPress &key)
{
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Z')
    {
        stepHistory(!key.getModifiers().isShiftDown());
        return true;
    }
    return false;
}
void Editor::commit(const spectral::Curves &c)
{
    undoStack.push_back(p.getBanks());
    if (undoStack.size() > 100)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
    p.updateCurves(c);
    repaint();
}
void Editor::timerCallback()
{
    refreshDisplay(juce::Time::getMillisecondCounterHiRes() * .001);
}
void Editor::refreshDisplay(double nowSeconds)
{
    auto banks = p.getBanks();
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
    const bool stereo = p.state.getRawParameterValue("stereoMorph")->load() > .5f;
    morphDepth.setEnabled(stereo);
    morphRate.setEnabled(stereo);
    p.publishPending();
    const int speed = p.spectrogramSpeed.load();
    scrollSpeed.setSelectedId(speed + 1, juce::dontSendNotification);
    delayHistory.setSpeed(speed);
    scrollControl.refresh();
    resolutionControl.refresh();
    int receivedCount = 0;
    while (receivedCount < int(displayFrames.size()) &&
           p.nextSpectrum(displayFrames[size_t(receivedCount)]))
        ++receivedCount;
    const bool received = receivedCount > 0;
    const auto &fresh = displayFrames[size_t(std::max(0, receivedCount - 1))];
    if constexpr (!SPECTRAL_DIAGNOSTIC)
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
void Editor::resized()
{
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(25);
    curvePanel = area;
    auto tabs = area.removeFromTop(42);
    bankTools = tabs.removeFromRight(106);
    const int tabWidth = tabs.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
        bankButtons[i].setBounds(i == 3 ? tabs : tabs.removeFromLeft(tabWidth));
    bankMore.setBounds(bankTools.getRight() - 31, bankTools.getY(), 31, 21);
    bankLess.setBounds(bankTools.getRight() - 31, bankTools.getY() + 21, 31, 21);
    auto bottom = area.removeFromBottom(186);
    toolbar = area.removeFromBottom(49);
    // Axis and pointer readout belong to the display, above the editing toolbar.
    plot = area.withTrimmedBottom(40).reduced(1, 0);
    auto tools = toolbar.reduced(8, 6);
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
    morphPanel = bottom.removeFromLeft(int(width * .37f));
    delayPanel = bottom.removeFromLeft(int(width * .17f));
    shiftPanel = bottom.removeFromLeft(int(width * .23f));
    mixPanel = bottom.removeFromLeft(int(width * .12f));
    gainPanel = bottom;
    auto morphArea = morphPanel.reduced(12, 18);
    morphArea.removeFromTop(19);
    stereoMorph.setBounds(morphArea.removeFromLeft(34).withHeight(58).translated(0, 26));
    morphArea.removeFromLeft(8);
    const int numberWidth = (morphArea.getWidth() - 12) / 3;
    for (auto pair : {std::pair{&morph, &morphLabel}, std::pair{&morphDepth, &morphDepthLabel},
                      std::pair{&morphRate, &morphRateLabel}})
    {
        auto cell = morphArea.removeFromLeft(numberWidth);
        pair.second->setBounds(cell.removeFromTop(23));
        pair.first->setBounds(cell.removeFromTop(62));
        morphArea.removeFromLeft(6);
    }
    auto rotary = [this](int index, juce::Rectangle<int> cell, int heading)
    {
        cell = cell.reduced(6, 8);
        labels[index].setBounds(cell.removeFromTop(heading));
        knobs[index].setBounds(cell);
    };
    rotary(1, delayPanel.withTrimmedTop(14), 23);
    auto shift = shiftPanel.withTrimmedTop(42);
    const int half = shift.getWidth() / 2;
    rotary(2, shift.removeFromLeft(half), 20);
    rotary(3, shift, 20);
    rotary(0, mixPanel.withTrimmedTop(42), 20);
    auto gains = gainPanel;
    rotary(4, gains.removeFromTop(93), 17);
    rotary(5, gains, 17);
    for (int i : {4, 5})
        knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 19);
    if constexpr (SPECTRAL_DIAGNOSTIC)
        plot = getLocalBounds().reduced(24).withTrimmedTop(55).withTrimmedBottom(45);
}
spectral::BucketDisplay Editor::buckets() const
{
    return {16 << juce::jlimit(0, 7, int(p.state.getRawParameterValue("stftSize")->load())),
            p.getSampleRate() > 0 ? p.getSampleRate() : 48000., float(plot.getX()),
            float(plot.getWidth())};
}
int Editor::bin(float x) const
{
    return buckets().binAt(x);
}
void Editor::paint(juce::Graphics &g)
{
    g.fillAll(freeqy::ink);
    g.setColour(freeqy::cyan);
    g.setFont(12.f);
    g.drawText(SPECTRAL_DIAGNOSTIC ? "FFT / SINE — development diagnostic" : "FREEQY DELAY", 24, 12,
               getWidth() - 48, 20, juce::Justification::centredLeft);
    if constexpr (!SPECTRAL_DIAGNOSTIC)
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
        g.drawText("BANK MORPH", morphPanel.reduced(12, 12).removeFromTop(19),
                   juce::Justification::centredLeft);
        g.drawText("CURVE SHIFT", shiftPanel.reduced(12, 12).removeFromTop(19),
                   juce::Justification::centred);
    }
    g.setColour(freeqy::well);
    g.fillRoundedRectangle(plot.toFloat(), 6);
    if constexpr (!SPECTRAL_DIAGNOSTIC)
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
    if constexpr (!SPECTRAL_DIAGNOSTIC)
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
    std::vector<std::array<float, 8>> columns(size_t(plot.getWidth()), std::array<float, 8>{});
    for (int k = 0; k <= layout.size / 2; ++k)
    {
        const auto [left, right] = layout.edges(k);
        if (right <= left)
            continue;
        const float index = layout.curvePosition(k);
        const float inputActivity = juce::jlimit(
            0.f, 1.f,
            (juce::Decibels::gainToDecibels(layout.magnitude(magnitudes[ch], k), -90.f) + 90) / 90);
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
                const float source = spectral::CurveBanks::shiftedIndex(
                    index, offset, p.getSampleRate() > 0 ? p.getSampleRate() : 48000.);
                pixel[t + 3] = std::max(pixel[t + 3], banks.sample(ch, t, source, morph));
            }
            pixel[6] = std::max(pixel[6], inputActivity);
        }
    }
    for (int x = 0; x < plot.getWidth(); ++x)
    {
        const auto &pixel = columns[size_t(x)];
        if (pixel[7] == 0)
            continue;
        const int inputHeight = int(std::round(pixel[6] * plot.getHeight()));
        g.setColour(freeqy::sky.withAlpha(SPECTRAL_DIAGNOSTIC ? .7f : .55f));
        g.fillRect(plot.getX() + x, plot.getBottom() - inputHeight, 1, inputHeight);
    }
    for (int pass = 0; pass < 3; ++pass)
    {
        int t = (selected + 1 + pass) % 3;
        for (int x = 0; x < plot.getWidth(); ++x)
        {
            const auto &pixel = columns[size_t(x)];
            if (pixel[7] == 0)
                continue;
            if constexpr (!SPECTRAL_DIAGNOSTIC)
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
void Editor::drawAt(juce::Point<float> point)
{
    if (SPECTRAL_DIAGNOSTIC)
        return;
    const auto layout = buckets();
    if (lastBin >= 0 && gestureSize != layout.size)
    {
        finishGesture();
        return;
    }
    int k = layout.binAt(point.x), t = p.selectedCurve, ch = p.selectedChannel;
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
void Editor::mouseDown(const juce::MouseEvent &e)
{
    if (!plot.contains(e.getPosition()) || SPECTRAL_DIAGNOSTIC)
        return;
    gestureSize = buckets().size;
    beforeBanks = p.getBanks();
    drawing = p.getCurves();
    lastBin = -1;
    drawAt(e.position);
}
void Editor::mouseDrag(const juce::MouseEvent &e)
{
    if (lastBin >= 0)
        drawAt(e.position);
}
void Editor::mouseUp(const juce::MouseEvent &)
{
    finishGesture();
}
void Editor::finishGesture()
{
    if (lastBin < 0)
        return;
    undoStack.push_back(beforeBanks);
    if (undoStack.size() > 100)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
    lastBin = -1;
}
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
