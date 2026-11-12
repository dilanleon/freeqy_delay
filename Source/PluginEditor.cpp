#include "PluginEditor.h"
Editor::Editor(Processor &processor) : AudioProcessorEditor(processor), p(processor)
{
    const char *ids[] = {"mix",         "maxDelayMs",  "shiftHz",
                         "shiftRateHz", "inputTrimDb", "outputTrimDb"};
    const char *names[] = {"Dry / Wet", "Max Delay ms", "Amount Hz",
                           "Rate Hz",   "Input dB",     "Output dB"};
    for (int i = 0; i < 6; ++i)
    {
        addAndMakeVisible(knobs[i]);
        knobs[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 22);
        addAndMakeVisible(labels[i]);
        labels[i].setText(names[i], juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred);
        attachments.push_back(
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, ids[i],
                                                                                   knobs[i]));
    }
    knobs[2].setTooltip(
        "Peak additive frequency shift. Left and right move in opposite directions.");
    knobs[3].setTooltip("Modulation rate; zero holds a fixed left-up/right-down offset.");
    addAndMakeVisible(clearBuffer);
    clearBuffer.onClick = [this] { p.requestClearBuffer(); };
    clearBuffer.setTooltip("Fade out and clear delay, feedback and frequency-shifter tails. Curves "
                           "and dry audio are preserved.");
    bank.addItemList({"Edit bank 1", "Edit bank 2", "Edit bank 3", "Edit bank 4"}, 1);
    bankCount.addItemList({"1 bank", "2 banks", "3 banks", "4 banks"}, 1);
    bank.setSelectedId(p.selectedBank.load() + 1, juce::dontSendNotification);
    bankCount.setSelectedId(p.getBanks().count, juce::dontSendNotification);
    copyBank.setButtonText("Copy to bank " +
                           juce::String((p.selectedBank.load() + 1) % p.getBanks().count + 1));
    bank.onChange = [this]
    {
        p.selectedBank = bank.getSelectedId() - 1;
        repaint();
    };
    bankCount.onChange = [this]
    {
        auto banks = p.getBanks();
        undoStack.push_back(banks);
        if (undoStack.size() > 100)
            undoStack.erase(undoStack.begin());
        redoStack.clear();
        banks.count = bankCount.getSelectedId();
        p.selectedBank = std::min(p.selectedBank.load(), banks.count - 1);
        p.updateBanks(banks);
        repaint();
    };
    copyBank.onClick = [this]
    {
        auto banks = p.getBanks();
        undoStack.push_back(banks);
        if (undoStack.size() > 100)
            undoStack.erase(undoStack.begin());
        redoStack.clear();
        int selected = p.selectedBank;
        banks.banks[(selected + 1) % banks.count] = banks.banks[selected];
        p.updateBanks(banks);
        repaint();
    };
    morph.setSliderStyle(juce::Slider::LinearHorizontal);
    morph.setTextBoxStyle(juce::Slider::TextBoxRight, false, 75, 22);
    morph.setTextValueSuffix(" %");
    morphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        p.state, "curveMorph", morph);
    morphLabel.setText("Morph", juce::dontSendNotification);
    morph.setTooltip("Automatable curve-bank blend. Solid: edited bank. Dashed: current morph.");
    bank.setTooltip("Select a stored bank to edit; this does not move the morph control.");
    for (juce::Component *control : std::initializer_list<juce::Component *>{
             &bank, &bankCount, &copyBank, &morph, &morphLabel})
        addAndMakeVisible(control);
    curve.addItemList({"Delay", "Feedback", "Level"}, 1);
    channel.addItemList({"Left", "Right"}, 1);
    stftSize.addItemList({"16 samples", "32 samples", "64 samples", "128 samples", "256 samples",
                          "512 samples", "1024 samples"},
                         1);
    stftSize.setTooltip(
        "Analysis window size. Changes fade and clear the wet tail; curve banks are preserved.");
    curve.setSelectedId(p.selectedCurve + 1);
    channel.setSelectedId(p.selectedChannel + 1);
    link.setToggleState(p.linked, juce::dontSendNotification);
    for (juce::Component *c : std::initializer_list<juce::Component *>{
             &curve, &channel, &stftSize, &link, &copyLR, &copyRL, &reset, &undo, &redo})
        addAndMakeVisible(c);
    curve.onChange = [this]
    {
        p.selectedCurve = curve.getSelectedId() - 1;
        repaint();
    };
    channel.onChange = [this]
    {
        p.selectedChannel = channel.getSelectedId() - 1;
        repaint();
    };
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
    copyLR.onClick = [this]
    {
        auto c = p.getCurves();
        c.values[1] = c.values[0];
        commit(c);
    };
    copyRL.onClick = [this]
    {
        auto c = p.getCurves();
        c.values[0] = c.values[1];
        commit(c);
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
    undo.onClick = [this]
    {
        if (undoStack.empty())
            return;
        redoStack.push_back(p.getBanks());
        p.selectedBank = std::min(p.selectedBank.load(), undoStack.back().count - 1);
        p.updateBanks(undoStack.back());
        undoStack.pop_back();
        repaint();
    };
    redo.onClick = [this]
    {
        if (redoStack.empty())
            return;
        undoStack.push_back(p.getBanks());
        p.selectedBank = std::min(p.selectedBank.load(), redoStack.back().count - 1);
        p.updateBanks(redoStack.back());
        redoStack.pop_back();
        repaint();
    };
    sizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        p.state, "stftSize", stftSize);
    if constexpr (SPECTRAL_DIAGNOSTIC)
    {
        for (juce::Component *control : std::initializer_list<juce::Component *>{
                 &bank, &bankCount, &copyBank, &morph, &morphLabel})
            control->setVisible(false);
        clearBuffer.setVisible(false);
        for (auto &k : knobs)
            k.setVisible(false);
        for (auto &l : labels)
            l.setVisible(false);
        for (juce::Component *c : std::initializer_list<juce::Component *>{
                 &curve, &channel, &stftSize, &link, &copyLR, &copyRL, &reset, &undo, &redo})
            c->setVisible(false);
    }
    setResizable(true, true);
    setResizeLimits(900, 680, 1800, 1200);
    setSize(1080, 760);
    startTimerHz(30);
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
    auto banks = p.getBanks();
    bank.setSelectedId(p.selectedBank.load() + 1, juce::dontSendNotification);
    bankCount.setSelectedId(banks.count, juce::dontSendNotification);
    for (int i = 1; i <= 4; ++i)
        bank.setItemEnabled(i, i <= banks.count);
    copyBank.setEnabled(banks.count > 1);
    copyBank.setButtonText("Copy to bank " +
                           juce::String((p.selectedBank.load() + 1) % banks.count + 1));
    p.publishPending();
    p.spectrum(magnitudes);
    curve.setSelectedId(p.selectedCurve + 1, juce::dontSendNotification);
    channel.setSelectedId(p.selectedChannel + 1, juce::dontSendNotification);
    link.setToggleState(p.linked, juce::dontSendNotification);
    repaint();
}
void Editor::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto header = area.removeFromTop(42);
    clearBuffer.setBounds(header.removeFromRight(138).reduced(0, 5));
    area.removeFromTop(10);
    curvePanel = area.removeFromTop(112);
    auto controls = curvePanel.reduced(12);
    controls.removeFromTop(22);
    auto edit = controls.removeFromTop(28);
    curve.setBounds(edit.removeFromLeft(112));
    channel.setBounds(edit.removeFromLeft(90).reduced(3, 0));
    link.setBounds(edit.removeFromLeft(65));
    copyLR.setBounds(edit.removeFromLeft(70).reduced(2, 0));
    copyRL.setBounds(edit.removeFromLeft(70).reduced(2, 0));
    reset.setButtonText("Reset curve");
    reset.setBounds(edit.removeFromLeft(100).reduced(2, 0));
    undo.setBounds(edit.removeFromLeft(65).reduced(2, 0));
    redo.setBounds(edit.removeFromLeft(65).reduced(2, 0));
    controls.removeFromTop(9);
    auto banks = controls.removeFromTop(28);
    bank.setBounds(banks.removeFromLeft(130));
    bankCount.setBounds(banks.removeFromLeft(95).reduced(3, 0));
    copyBank.setBounds(banks.removeFromLeft(145).reduced(3, 0));
    morphLabel.setBounds(banks.removeFromLeft(55));
    morph.setBounds(banks);

    auto bottom = area.removeFromBottom(190);
    const int delayWidth = int(bottom.getWidth() * .36f);
    const int shiftWidth = int(bottom.getWidth() * .34f);
    delayPanel = bottom.removeFromLeft(delayWidth);
    bottom.removeFromLeft(12);
    shiftPanel = bottom.removeFromLeft(shiftWidth - 12);
    bottom.removeFromLeft(12);
    gainPanel = bottom;
    auto place = [this](juce::Rectangle<int> panel, std::initializer_list<int> indices)
    {
        auto body = panel.reduced(10);
        body.removeFromTop(32);
        const int width = body.getWidth() / int(indices.size());
        for (int i : indices)
        {
            auto cell = body.removeFromLeft(width);
            labels[i].setBounds(cell.removeFromTop(22));
            knobs[i].setBounds(cell.reduced(2, 0));
        }
    };
    place(delayPanel, {1, 0});
    place(shiftPanel, {2, 3});
    place(gainPanel, {4, 5});
    stftSize.setBounds(delayPanel.getRight() - 145, delayPanel.getY() + 8, 133, 25);
    area.removeFromBottom(48);
    area.removeFromTop(30);
    plot = area.withTrimmedLeft(80);
    if constexpr (SPECTRAL_DIAGNOSTIC)
        plot = getLocalBounds().reduced(24).withTrimmedTop(55).withTrimmedBottom(45);
}
int Editor::bin(float x) const
{
    const double sampleRate = p.getSampleRate() > 0 ? p.getSampleRate() : 48000.;
    const double fraction = std::clamp((x - plot.getX()) / plot.getWidth(), 0.f, 1.f);
    const double frequency = 20. * std::pow(21000. / 20., fraction);
    const int firstVisibleBin = std::max(1, int(std::ceil(20. * spectral::N / sampleRate)));
    const int lastVisibleBin = std::min(spectral::B - 1, int(21000. * spectral::N / sampleRate));
    return juce::jlimit(firstVisibleBin, lastVisibleBin,
                        int(std::round(frequency * spectral::N / sampleRate)));
}
float Editor::xFor(int k) const
{
    const double sampleRate = p.getSampleRate() > 0 ? p.getSampleRate() : 48000.;
    const double frequency = std::max(1., k * sampleRate / spectral::N);
    return plot.getX() +
           float(std::log(frequency / 20.) / std::log(21000. / 20.)) * plot.getWidth();
}
void Editor::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colour(0xff141a24));
    g.setColour(juce::Colours::whitesmoke);
    g.setFont(24.f);
    g.drawText(SPECTRAL_DIAGNOSTIC ? "FFT / SINE — development diagnostic" : "SPECTRAL DELAY", 24,
               20, getWidth() - 190, 32, juce::Justification::centredLeft);
    if constexpr (!SPECTRAL_DIAGNOSTIC)
    {
        auto card = [&](juce::Rectangle<int> bounds, const juce::String &title)
        {
            g.setColour(juce::Colour(0xff1b2431));
            g.fillRoundedRectangle(bounds.toFloat(), 9);
            g.setColour(juce::Colour(0xff304052));
            g.drawRoundedRectangle(bounds.toFloat(), 9, 1);
            g.setColour(juce::Colour(0xff9fb5cb));
            g.setFont(12.f);
            g.drawText(title, bounds.getX() + 12, bounds.getY() + 9, bounds.getWidth() - 24, 18,
                       juce::Justification::centredLeft);
        };
        card(curvePanel, "CURVES & BANKS");
        card(delayPanel, "DELAY / STFT");
        card(shiftPanel, "STEREO FREQUENCY SHIFT");
        card(gainPanel, "INPUT / OUTPUT");
        g.setColour(juce::Colours::lightgrey);
        g.setFont(11.f);
        g.drawText("Solid: edit bank     Dashed: morph", plot.getX(), plot.getY() - 22, 350, 18,
                   juce::Justification::centredLeft);
    }
    g.setColour(juce::Colour(0xff0b1018));
    g.fillRoundedRectangle(plot.toFloat(), 6);
    if constexpr (!SPECTRAL_DIAGNOSTIC)
    {
        g.setFont(12.f);
        const juce::Colour colors[] = {juce::Colour(0xff64d9ef), juce::Colour(0xffffb866),
                                       juce::Colour(0xffc19aff)};
        const char *names[] = {"Delay", "Feedback", "Level"};
        for (int t = 0; t < 3; ++t)
        {
            g.setColour(colors[t]);
            g.drawText(names[t], plot.getRight() - 240 + t * 80, plot.getY() - 22, 80, 18,
                       juce::Justification::centredLeft);
        }
        int selected = p.selectedCurve.load();
        float maximum = selected == 0   ? p.state.getRawParameterValue("maxDelayMs")->load()
                        : selected == 1 ? 99.f
                                        : 100.f;
        for (int tick = 0; tick <= 4; ++tick)
        {
            int y = plot.getBottom() - tick * plot.getHeight() / 4;
            g.setColour(juce::Colour(0xff26313e));
            g.drawHorizontalLine(y, float(plot.getX()), float(plot.getRight()));
            g.setColour(juce::Colours::lightgrey);
            g.drawText(juce::String(maximum * tick / 4, 1) + (selected == 0 ? " ms" : " %"),
                       plot.getX() - 80, juce::jlimit(plot.getY(), plot.getBottom() - 16, y - 8),
                       76, 16, juce::Justification::centredLeft);
        }
    }
    g.saveState();
    g.reduceClipRegion(plot);
    juce::Path spectrum;
    for (int k = 0; k < spectral::B; ++k)
    {
        float db = juce::Decibels::gainToDecibels(magnitudes[k], -90.f);
        float y = plot.getBottom() - juce::jlimit(0.f, 1.f, (db + 90) / 90) * plot.getHeight();
        if (k == 0)
            spectrum.startNewSubPath(xFor(k), y);
        else
            spectrum.lineTo(xFor(k), y);
    }
    g.setColour(juce::Colour(0xff364656));
    g.strokePath(spectrum, juce::PathStrokeType(1));
    if constexpr (!SPECTRAL_DIAGNOSTIC)
    {
        auto c = p.getCurves();
        spectral::Curves blended;
        p.getBanks().blend(p.state.getRawParameterValue("curveMorph")->load(), blended);
        juce::Colour colors[] = {juce::Colour(0xff64d9ef), juce::Colour(0xffffb866),
                                 juce::Colour(0xffc19aff)};
        for (int t = 0; t < 3; ++t)
        {
            juce::Path path;
            for (int k = 0; k < spectral::B; ++k)
            {
                float value = c.values[p.selectedChannel.load()][t][k] / (t == 1 ? .99f : 1.f);
                float y = plot.getBottom() - value * plot.getHeight();
                if (k == 0)
                    path.startNewSubPath(xFor(k), y);
                else
                    path.lineTo(xFor(k), y);
            }
            g.setColour(colors[t].withAlpha(t == p.selectedCurve ? 1.f : .5f));
            g.strokePath(path, juce::PathStrokeType(t == p.selectedCurve ? 2.5f : 1.3f));
            juce::Path preview;
            for (int k = 0; k < spectral::B; ++k)
            {
                float y = plot.getBottom() - blended.values[p.selectedChannel.load()][t][k] /
                                                 (t == 1 ? .99f : 1.f) * plot.getHeight();
                if (k == 0)
                    preview.startNewSubPath(xFor(k), y);
                else
                    preview.lineTo(xFor(k), y);
            }
            const float dashes[] = {4.f, 4.f};
            juce::Path dashed;
            juce::PathStrokeType(1.f).createDashedStroke(dashed, preview, dashes, 2);
            g.setColour(colors[t].withAlpha(.8f));
            g.fillPath(dashed);
        }
    }
    g.restoreState();
    g.setFont(13.f);
    g.setColour(juce::Colours::lightgrey);
    for (int frequency : {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 21000})
    {
        const int x = plot.getX() + int(std::log(double(frequency) / 20.) / std::log(21000. / 20.) *
                                        plot.getWidth());
        const int labelX = juce::jlimit(plot.getX(), plot.getRight() - 60, x - 30);
        const auto label = frequency < 1000 ? juce::String(frequency) + " Hz"
                                            : juce::String(frequency / 1000) + " kHz";
        g.drawText(label, labelX, plot.getBottom() + 3, 60, 18, juce::Justification::centred);
    }
    g.drawText(readout, 24, plot.getBottom() + 23, getWidth() - 48, 20,
               juce::Justification::centred);
}
void Editor::drawAt(juce::Point<float> point)
{
    if (SPECTRAL_DIAGNOSTIC)
        return;
    int k = bin(point.x), t = p.selectedCurve, ch = p.selectedChannel;
    float value = juce::jlimit(0.f, 1.f, (plot.getBottom() - point.y) / plot.getHeight()) *
                  (t == 1 ? .99f : 1.f);
    if (lastBin < 0)
    {
        lastBin = k;
        lastValue = value;
    }
    int lo = std::min(k, lastBin), hi = std::max(k, lastBin);
    for (int i = lo; i <= hi; ++i)
    {
        float f = k == lastBin ? 1.f : float(i - lastBin) / float(k - lastBin);
        drawing.values[ch][t][i] = lastValue + (value - lastValue) * f;
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
    float value = p.getCurves().values[p.selectedChannel.load()][t][k];
    float physical =
        t == 0 ? value * p.state.getRawParameterValue("maxDelayMs")->load() : value * 100;
    readout =
        juce::String(k * (p.getSampleRate() > 0 ? p.getSampleRate() : 48000.) / spectral::N, 1) +
        " Hz  |  bin " + juce::String(k) + "  |  " + juce::String(physical, 2) +
        (t == 0 ? " ms" : " %");
}
