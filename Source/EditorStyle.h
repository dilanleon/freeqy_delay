#pragma once
#include "FreeqyPalette.h"
#include <juce_gui_basics/juce_gui_basics.h>

class EditorStyle final : public juce::LookAndFeel_V4
{
  public:
    // Install the FREEQY DELAY control palette.
    EditorStyle()
    {
        setColour(juce::Slider::textBoxTextColourId, freeqy::cyan);
        setColour(juce::Slider::textBoxBackgroundColourId, freeqy::well);
        setColour(juce::Slider::textBoxOutlineColourId, freeqy::blueViolet);
        setColour(juce::Slider::textBoxHighlightColourId, freeqy::violet);
        setColour(juce::ComboBox::backgroundColourId, freeqy::panel);
        setColour(juce::ComboBox::outlineColourId, freeqy::blueViolet);
        setColour(juce::ComboBox::textColourId, freeqy::cyan);
        setColour(juce::ComboBox::arrowColourId, freeqy::cyan);
        setColour(juce::PopupMenu::backgroundColourId, freeqy::panel);
        setColour(juce::PopupMenu::textColourId, freeqy::cyan);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, freeqy::deepViolet);
        setColour(juce::PopupMenu::highlightedTextColourId, freeqy::cyan);
    }
    // Paint button surfaces and the active top marker.
    void drawButtonBackground(juce::Graphics &g, juce::Button &button, const juce::Colour &,
                              bool over, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(.5f);
        const bool on = button.getToggleState();
        g.setColour(on ? freeqy::deepViolet : over || down ? freeqy::royalPurple : freeqy::panel);
        g.fillRect(bounds);
        g.setColour(
            (on ? freeqy::cyan : freeqy::blueViolet).withAlpha(button.isEnabled() ? 1.f : .35f));
        g.drawRect(bounds, 1);
        if (on)
        {
            g.setColour(freeqy::pink);
            g.fillRect(bounds.removeFromTop(2));
        }
    }
    // Paint tab and channel-selector text.
    void drawButtonText(juce::Graphics &g, juce::TextButton &button, bool, bool) override
    {
        g.setFont(button.getComponentID() == "bankTab" ? 18.f : 14.f);
        g.setColour(freeqy::cyan.withAlpha(button.isEnabled() ? 1.f : .23f));
        if (button.getComponentID() == "channelSelector")
        {
            const bool left = button.getButtonText().endsWith("L");
            auto box = button.getLocalBounds();
            for (int side = 0; side < 2; ++side)
            {
                auto cell = box.removeFromLeft(button.getWidth() / (side == 0 ? 2 : 1));
                g.setColour((side == 0) == left ? freeqy::cyan : freeqy::blueViolet);
                g.drawText(side == 0 ? "L" : "R", cell, juce::Justification::centred);
            }
            return;
        }
        g.drawText(button.getButtonText(), button.getLocalBounds().reduced(3, 0),
                   juce::Justification::centred);
    }
    // Paint the link and stereo-morph direction icons.
    void drawToggleButton(juce::Graphics &g, juce::ToggleButton &button, bool over, bool) override
    {
        const auto box = button.getLocalBounds().toFloat().reduced(.5f);
        g.setColour(button.getToggleState() ? freeqy::deepViolet
                    : over                  ? freeqy::royalPurple
                                            : freeqy::panel);
        g.fillRect(box);
        g.setColour(button.getToggleState() ? freeqy::cyan : freeqy::blueViolet);
        g.drawRect(box, 1);
        const float x = box.getCentreX(), y = box.getCentreY();
        if (button.getComponentID() == "channelLink")
        {
            if (button.getToggleState())
            {
                g.drawEllipse(x - 12, y - 6, 15, 12, 1.5f);
                g.drawEllipse(x - 3, y - 6, 15, 12, 1.5f);
                g.drawLine(x - 3, y, x + 3, y, 2);
            }
            else
            {
                g.drawEllipse(x - 15, y - 5, 10, 10, 1.5f);
                g.drawEllipse(x + 5, y - 5, 10, 10, 1.5f);
            }
        }
        else
        {
            for (int side = 0; side < 2; ++side)
            {
                const float xx = x + (side == 0 ? -6 : 6);
                const bool opposite = button.getToggleState();
                const float direction = side == 1 && opposite ? -1.f : 1.f;
                g.drawLine(xx, y + direction * 9, xx, y - direction * 9, 1.5f);
                g.drawLine(xx, y - direction * 9, xx - 4, y - direction * 4, 1.5f);
                g.drawLine(xx, y - direction * 9, xx + 4, y - direction * 4, 1.5f);
            }
        }
    }
    // Paint rotary controls or a body-free digital morph value.
    void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height, float position,
                          float start, float end, juce::Slider &slider) override
    {
        const auto name = slider.getName();
        if (name == "curveMorph" || name == "morphDepth" || name == "morphRateHz")
        {
            // Keep the knob's vertical drag model, but render only its digital value.
            drawSegmentDisplay(g, {x + 2, y + 4, width - 4, height - 8}, sliderText(slider, {}));
            return;
        }
        auto box =
            juce::Rectangle<float>(float(x), float(y), float(width), float(height)).reduced(3);
        const float radius = std::min(box.getWidth(), box.getHeight()) * .5f;
        auto center = box.getCentre();
        const float angle = start + position * (end - start);
        // A recessed outer well, a lit bevel and a subtly shaded cap keep the
        // controls readable without looking like the stock JUCE rotary knob.
        g.setColour(freeqy::ink);
        g.fillEllipse(center.x - radius - 2, center.y - radius - 2, (radius + 2) * 2,
                      (radius + 2) * 2);
        juce::ColourGradient bevel(freeqy::sky, center.x - radius, center.y - radius,
                                   freeqy::royalPurple, center.x + radius, center.y + radius,
                                   false);
        g.setGradientFill(bevel);
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2, radius * 2);
        g.setColour(freeqy::well);
        g.fillEllipse(center.x - radius + 3, center.y - radius + 3, (radius - 3) * 2,
                      (radius - 3) * 2);
        juce::ColourGradient cap(freeqy::violet, center.x - radius * .35f, center.y - radius * .45f,
                                 freeqy::ink, center.x, center.y + radius, false);
        g.setGradientFill(cap);
        g.fillEllipse(center.x - radius + 5, center.y - radius + 5, (radius - 5) * 2,
                      (radius - 5) * 2);
        g.setColour(freeqy::cyan.withAlpha(.35f));
        g.drawEllipse(center.x - radius + 5, center.y - radius + 5, (radius - 5) * 2,
                      (radius - 5) * 2, 1);
        for (bool active : {false, true})
        {
            juce::Path arc;
            arc.addCentredArc(center.x, center.y, radius - 2, radius - 2, 0, start,
                              active ? angle : end, true);
            g.setColour(active ? freeqy::pink : freeqy::blueViolet);
            g.strokePath(arc, juce::PathStrokeType(2, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }
        g.setColour(freeqy::cyan);
        g.drawLine(center.x + std::sin(angle) * radius * .35f,
                   center.y - std::cos(angle) * radius * .35f,
                   center.x + std::sin(angle) * radius * .8f,
                   center.y - std::cos(angle) * radius * .8f, 2);
    }
    // Paint the custom vertical trim rail and marker.
    void drawLinearSlider(juce::Graphics &g, int x, int y, int width, int height, float sliderPos,
                          float, float, const juce::Slider::SliderStyle style,
                          juce::Slider &slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0, 0, style,
                                             slider);
            return;
        }
        const auto bounds = juce::Rectangle<float>(float(x), float(y), float(width), float(height));
        const float railWidth = std::min(12.f, bounds.getWidth() * .22f);
        auto rail = juce::Rectangle<float>(bounds.getCentreX() - railWidth * .5f, bounds.getY() + 3,
                                           railWidth, std::max(8.f, bounds.getHeight() - 6));
        g.setColour(freeqy::well);
        g.fillRoundedRectangle(rail, railWidth * .5f);
        g.setColour(freeqy::blueViolet);
        g.drawRoundedRectangle(rail, railWidth * .5f, 1);
        const float markerY = juce::jlimit(rail.getY() + 3, rail.getBottom() - 3, sliderPos);
        auto fill = rail.withTop(markerY);
        g.setColour(freeqy::violet);
        g.fillRoundedRectangle(fill, railWidth * .5f);
        g.setColour(freeqy::pink);
        g.drawLine(rail.getX() - 6, markerY, rail.getRight() + 6, markerY, 2.2f);
        g.setColour(freeqy::cyan);
        g.fillEllipse(rail.getCentreX() - 3, markerY - 3, 6, 6);
    }
    // Route slider value labels through the seven-segment renderer.
    void drawLabel(juce::Graphics &g, juce::Label &label) override
    {
        // Slider text boxes are the plugin's numerical readouts. Other labels
        // stay typographic so headings remain quiet and legible.
        if (dynamic_cast<juce::Slider *>(label.getParentComponent()) == nullptr ||
            label.isBeingEdited())
        {
            LookAndFeel_V4::drawLabel(g, label);
            return;
        }
        auto *slider = dynamic_cast<juce::Slider *>(label.getParentComponent());
        drawSegmentDisplay(g, label.getLocalBounds(), sliderText(*slider, label.getText()));
    }

  public:
    // Define logarithmic knob travel without changing host normalization.
    static juce::NormalisableRange<double> shiftDepthRange()
    {
        // Offset-log mapping gives zero an exact position while spacing small shifts finely.
        constexpr double offsetHz = 20.;
        return {0., 1000.,
                [](double start, double end, double position)
                { return start + offsetHz * std::expm1(position * std::log1p((end - start) / offsetHz)); },
                [](double start, double end, double value)
                { return std::log1p((value - start) / offsetHz) / std::log1p((end - start) / offsetHz); }};
    }
    // Paint a seven-segment value with a separate glowing unit.
    static void drawSegmentDisplay(juce::Graphics &g, juce::Rectangle<int> bounds,
                                   const juce::String &text)
    {
        bounds = bounds.reduced(1);
        g.setColour(freeqy::well);
        g.fillRect(bounds);
        g.setColour(freeqy::blueViolet);
        g.drawRect(bounds);

        juce::String digits, unit;
        bool unitStarted = false;
        for (auto character : text)
        {
            const bool numeric = juce::CharacterFunctions::isDigit(character) || character == '.' ||
                                 character == '-';
            if (!unitStarted && numeric)
                digits += character;
            else if (character != ' ')
            {
                unitStarted = true;
                unit += character;
            }
        }
        if (digits.isEmpty())
            digits = "0";
        const bool hasUnit = unit.isNotEmpty();
        const int bulbWidth = hasUnit ? 52 : 0;
        auto display = bounds.reduced(5, 4).withTrimmedRight(hasUnit ? bulbWidth + 4 : 0);
        const float glyphWidth = std::min(float(display.getWidth()) / std::max(1, digits.length()),
                                          float(display.getHeight()) * .58f);
        const float allWidth = glyphWidth * digits.length();
        float x = display.getCentreX() - allWidth * .5f;
        for (auto character : digits)
        {
            drawDigit(g, character,
                      {x, float(display.getY()), glyphWidth, float(display.getHeight())});
            x += glyphWidth;
        }
        if (hasUnit)
        {
            auto bulb =
                bounds.withTrimmedLeft(bounds.getRight() - bulbWidth).reduced(2, 2).toFloat();
            g.setFont(juce::FontOptions(25.f).withStyle("Bold"));
            const int unitX = int(bulb.getX()) + 7;
            const int baseline = int(bulb.getCentreY()) + 7;
            // Draw the bloom from the same positioned glyphs as the foreground so
            // the larger unit text keeps one baseline at every control size.
            juce::GlyphArrangement glyphs;
            glyphs.addLineOfText(g.getCurrentFont(), unit, float(unitX), float(baseline));
            g.setColour(freeqy::pink.withAlpha(.16f));
            for (const auto offset : {juce::Point<int>{-3, 0},
                                      {3, 0},
                                      {0, -3},
                                      {0, 3},
                                      {-1, -1},
                                      {1, -1},
                                      {-1, 1},
                                      {1, 1}})
            {
                auto glow = glyphs;
                glow.moveRangeOfGlyphs(0, -1, float(offset.x), float(offset.y));
                glow.draw(g);
            }
            g.setColour(freeqy::pink);
            glyphs.draw(g);
        }
    }
    // Format a slider value using its displayed unit.
    static juce::String sliderText(const juce::Slider &slider, const juce::String &fallback)
    {
        const auto name = slider.getName();
        const double value = slider.getValue();
        if (name == "mix")
            return juce::String(value * 100.0, 1) + " %";
        if (name == "maxDelayMs")
            return delayUsesSeconds(value) ? juce::String(value / 1000.0, 2) + " s"
                                           : juce::String(value, 1) + " ms";
        if (name == "shiftHz")
            return juce::String(value, 2) + " Hz";
        if (name == "shiftRateHz" || name == "morphRateHz")
            return juce::String(value, 2) + " Hz";
        if (name == "inputTrimDb" || name == "outputTrimDb")
            return juce::String(value, 2) + " dB";
        if (name == "curveMorph" || name == "morphDepth")
            return juce::String(value, 2) + " %";
        return fallback;
    }
    // Choose the Max Delay unit at the one-second boundary.
    static bool delayUsesSeconds(double milliseconds)
    {
        // APVTS normalization can return the 1000 ms default a few ulps below 1000.
        return milliseconds >= 999.999;
    }
    // Parse Max Delay text using its displayed or explicit unit.
    static double delayValueFromText(const juce::String &text, double displayedMilliseconds)
    {
        const auto unitText = text.trim().toLowerCase();
        const bool explicitMilliseconds = unitText.endsWith("ms");
        const bool explicitSeconds = !explicitMilliseconds && unitText.endsWith("s");
        const double value = unitText.getDoubleValue();
        return explicitSeconds || (!explicitMilliseconds && delayUsesSeconds(displayedMilliseconds))
                   ? value * 1000.0
                   : value;
    }
    // Paint one seven-segment numeral or punctuation glyph.
    static void drawDigit(juce::Graphics &g, juce::juce_wchar character, juce::Rectangle<float> box)
    {
        if (character == '.')
        {
            g.setColour(freeqy::cyan);
            g.fillEllipse(box.getCentreX() - 1.5f, box.getBottom() - 4, 3, 3);
            return;
        }
        if (character == '-')
        {
            g.setColour(freeqy::cyan);
            g.fillRoundedRectangle(box.getCentreX() - box.getWidth() * .27f, box.getCentreY() - 1,
                                   box.getWidth() * .54f, 2, 1);
            return;
        }
        const int digit = character - '0';
        constexpr unsigned masks[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
        const unsigned mask = digit >= 0 && digit <= 9 ? masks[digit] : 0;
        const float thickness = std::max(1.5f, box.getWidth() * .15f);
        const float left = box.getX() + thickness, right = box.getRight() - thickness;
        const float top = box.getY() + thickness, middle = box.getCentreY(),
                    bottom = box.getBottom() - thickness;
        const std::array<juce::Line<float>, 7> segments = {{{left, top, right, top},
                                                            {right, top, right, middle},
                                                            {right, middle, right, bottom},
                                                            {left, bottom, right, bottom},
                                                            {left, middle, left, bottom},
                                                            {left, top, left, middle},
                                                            {left, middle, right, middle}}};
        for (int i = 0; i < 7; ++i)
        {
            const bool lit = (mask & (1u << i)) != 0;
            g.setColour(lit ? freeqy::cyan : freeqy::blueViolet.darker(.7f));
            g.drawLine(segments[size_t(i)], thickness);
        }
    }
};

// Buttons step existing choices; the hidden ComboBox retains its host attachment.
class ChoiceStepper final : public juce::Component, public juce::SettableTooltipClient
{
  public:
    // Bind compact step buttons to an existing choice control.
    ChoiceStepper(juce::ComboBox &model, juce::String caption)
        : choice(model), title(std::move(caption))
    {
        addAndMakeVisible(less);
        addAndMakeVisible(more);
        less.onClick = [this] { step(-1); };
        more.onClick = [this] { step(1); };
        less.setTooltip(title + ": previous setting");
        more.setTooltip(title + ": next setting");
        setName(title);
    }
    // Apply one help message to the stepper and its buttons.
    void setHelpText(const juce::String &text)
    {
        setTooltip(text);
        less.setTooltip(text);
        more.setTooltip(text);
    }
    // Update stepper availability from the selected choice.
    void refresh()
    {
        const bool reverse = title == "Scroll";
        less.setEnabled(reverse ? choice.getSelectedId() < choice.getNumItems()
                                : choice.getSelectedId() > 1);
        more.setEnabled(reverse ? choice.getSelectedId() > 1
                                : choice.getSelectedId() < choice.getNumItems());
        repaint();
    }
    // Paint the stepper caption and selected value.
    void paint(juce::Graphics &g) override
    {
        g.setColour(freeqy::panel);
        g.fillRect(getLocalBounds());
        g.setColour(freeqy::blueViolet);
        g.drawRect(getLocalBounds());
        auto text = choice.getText();
        if (text.startsWith("Scroll: "))
        {
            const auto open = text.indexOfChar('('), close = text.indexOfChar(')');
            text = open >= 0 && close > open ? text.substring(open + 1, close) : text.substring(8);
        }
        else
            text = text.upToFirstOccurrenceOf(" samples", false, false) + " N";
        g.setColour(freeqy::sky);
        g.setFont(9.f);
        g.drawText(title.toUpperCase(), 25, 1, getWidth() - 50, 10, juce::Justification::centred);
        EditorStyle::drawSegmentDisplay(
            g,
            getLocalBounds().withTrimmedLeft(25).withTrimmedRight(25).withTrimmedTop(11).reduced(0,
                                                                                                 1),
            text);
    }
    // Place the stepper buttons at opposite ends.
    void resized() override
    {
        less.setBounds(0, 0, 23, getHeight());
        more.setBounds(getWidth() - 23, 0, 23, getHeight());
    }

  private:
    juce::ComboBox &choice;
    juce::String title;
    juce::TextButton less{"-"}, more{"+"};
    // Move one choice while respecting the Scroll ordering.
    void step(int direction)
    {
        // Scroll choices are ordered fastest first.
        const int delta = title == "Scroll" ? -direction : direction;
        choice.setSelectedId(juce::jlimit(1, choice.getNumItems(), choice.getSelectedId() + delta),
                             juce::sendNotificationSync);
        refresh();
    }
};
