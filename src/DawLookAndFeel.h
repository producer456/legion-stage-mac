#pragma once

#include <JuceHeader.h>
#include "DawTheme.h"

// Base class for all DAW themes.  Provides:
//   - A DawTheme color struct for paint() code
//   - Common custom drawing (buttons, sliders, combos)
//   - Subclasses only need to supply colors + optionally override drawing
class DawLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DawLookAndFeel() = default;
    virtual ~DawLookAndFeel() = default;

    const DawTheme& getTheme() const { return theme; }

    // The font name used for UI controls (monospace by default)
    virtual juce::String getUIFontName() const { return "Consolas"; }

    // The font name used for display/headings (can be serif for some themes)
    virtual juce::String getDisplayFontName() const { return getUIFontName(); }

protected:
    DawTheme theme {};

    // Call from subclass constructor after filling in `theme`
    void applyThemeColors()
    {
        setColour(juce::ResizableWindow::backgroundColourId,       juce::Colour(theme.body));
        setColour(juce::TextButton::buttonColourId,                juce::Colour(theme.buttonFace));
        setColour(juce::TextButton::buttonOnColourId,              juce::Colour(theme.amber));
        setColour(juce::TextButton::textColourOffId,               juce::Colour(theme.textPrimary));
        setColour(juce::TextButton::textColourOnId,                juce::Colour(theme.bodyDark));
        setColour(juce::ComboBox::backgroundColourId,              juce::Colour(theme.bodyDark));
        setColour(juce::ComboBox::textColourId,                    juce::Colour(theme.textPrimary));
        setColour(juce::ComboBox::outlineColourId,                 juce::Colour(theme.border));
        setColour(juce::PopupMenu::backgroundColourId,             juce::Colour(theme.bodyLight));
        setColour(juce::PopupMenu::textColourId,                   juce::Colour(theme.textPrimary));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,  juce::Colour(theme.amber));
        setColour(juce::PopupMenu::highlightedTextColourId,        juce::Colour(theme.bodyDark));
        setColour(juce::Label::textColourId,                       juce::Colour(theme.textPrimary));
        setColour(juce::Slider::thumbColourId,                     juce::Colour(theme.amber));
        setColour(juce::Slider::rotarySliderFillColourId,          juce::Colour(theme.amber));
        setColour(juce::Slider::rotarySliderOutlineColourId,       juce::Colour(theme.border));
        setColour(juce::Slider::trackColourId,                     juce::Colour(theme.bodyDark));
        setColour(juce::Slider::textBoxTextColourId,               juce::Colour(theme.lcdText));
        setColour(juce::Slider::textBoxBackgroundColourId,         juce::Colour(theme.lcdBg));
        setColour(juce::Slider::textBoxOutlineColourId,            juce::Colour(theme.border));

        setDefaultSansSerifTypefaceName(getUIFontName());
    }

    // ── Default custom button drawing ──
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto baseColour = backgroundColour;

        if (shouldDrawButtonAsDown)
            baseColour = baseColour.darker(0.2f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter(0.1f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, getButtonRadius());

        if (!shouldDrawButtonAsDown)
        {
            g.setColour(baseColour.brighter(0.15f));
            g.drawLine(bounds.getX() + 3, bounds.getY() + 1,
                       bounds.getRight() - 3, bounds.getY() + 1, 1.0f);
        }

        g.setColour(juce::Colour(theme.border));
        g.drawRoundedRectangle(bounds, getButtonRadius(), 1.0f);
    }

    virtual float getButtonRadius() const { return 3.0f; }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool, bool) override
    {
        auto font = juce::Font(getUIFontName(),
                               juce::jmin(14.0f, button.getHeight() * 0.55f),
                               juce::Font::bold);
        g.setFont(font);

        auto textColour = button.findColour(button.getToggleState()
            ? juce::TextButton::textColourOnId
            : juce::TextButton::textColourOffId);
        g.setColour(textColour);

        g.drawText(formatButtonText(button.getButtonText()),
                   button.getLocalBounds().reduced(4, 2),
                   juce::Justification::centred);
    }

    virtual juce::String formatButtonText(const juce::String& text) const
    {
        return text.toUpperCase();
    }

    // ── Rotary slider ──
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        auto radius = static_cast<float>(juce::jmin(width, height)) / 2.0f - 4.0f;
        auto centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        auto centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        g.setColour(juce::Colour(theme.bodyDark));
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

        juce::Path track;
        track.addArc(centreX - radius + 2, centreY - radius + 2,
                     (radius - 2) * 2.0f, (radius - 2) * 2.0f,
                     rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(theme.border));
        g.strokePath(track, juce::PathStrokeType(3.0f));

        juce::Path valueArc;
        valueArc.addArc(centreX - radius + 2, centreY - radius + 2,
                        (radius - 2) * 2.0f, (radius - 2) * 2.0f,
                        rotaryStartAngle, angle, true);
        g.setColour(juce::Colour(theme.amber));
        g.strokePath(valueArc, juce::PathStrokeType(3.0f));

        juce::Path pointer;
        auto pointerLength = radius * 0.6f;
        pointer.addRectangle(-1.5f, -pointerLength, 3.0f, pointerLength);
        pointer.applyTransform(juce::AffineTransform::rotation(angle)
                               .translated(centreX, centreY));
        g.setColour(juce::Colour(theme.textBright));
        g.fillPath(pointer);

        g.setColour(juce::Colour(theme.buttonFace));
        g.fillEllipse(centreX - 4, centreY - 4, 8.0f, 8.0f);
    }

    // ── Linear slider ──
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float, float,
                          const juce::Slider::SliderStyle style, juce::Slider&) override
    {
        if (style == juce::Slider::LinearVertical)
        {
            auto trackWidth = 6.0f;
            auto trackX = static_cast<float>(x) + static_cast<float>(width) * 0.5f - trackWidth * 0.5f;
            g.setColour(juce::Colour(theme.bodyDark));
            g.fillRoundedRectangle(trackX, static_cast<float>(y), trackWidth, static_cast<float>(height), 3.0f);

            g.setColour(juce::Colour(theme.amber));
            auto fillHeight = static_cast<float>(height) - (sliderPos - static_cast<float>(y));
            g.fillRoundedRectangle(trackX, sliderPos, trackWidth, fillHeight, 3.0f);

            g.setColour(juce::Colour(theme.textBright));
            g.fillRoundedRectangle(trackX - 4, sliderPos - 6, trackWidth + 8, 12.0f, 3.0f);
            g.setColour(juce::Colour(theme.border));
            g.drawRoundedRectangle(trackX - 4, sliderPos - 6, trackWidth + 8, 12.0f, 3.0f, 1.0f);
        }
        else
        {
            auto trackHeight = 6.0f;
            auto trackY = static_cast<float>(y) + static_cast<float>(height) * 0.5f - trackHeight * 0.5f;
            g.setColour(juce::Colour(theme.bodyDark));
            g.fillRoundedRectangle(static_cast<float>(x), trackY, static_cast<float>(width), trackHeight, 3.0f);

            g.setColour(juce::Colour(theme.amber));
            g.fillRoundedRectangle(static_cast<float>(x), trackY, sliderPos - static_cast<float>(x), trackHeight, 3.0f);

            g.setColour(juce::Colour(theme.textBright));
            g.fillRoundedRectangle(sliderPos - 6, trackY - 4, 12.0f, trackHeight + 8, 3.0f);
            g.setColour(juce::Colour(theme.border));
            g.drawRoundedRectangle(sliderPos - 6, trackY - 4, 12.0f, trackHeight + 8, 3.0f, 1.0f);
        }
    }

    // ── Combo box ──
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height));

        g.setColour(juce::Colour(theme.bodyDark));
        g.fillRoundedRectangle(bounds, getButtonRadius());
        g.setColour(juce::Colour(theme.border));
        g.drawRoundedRectangle(bounds.reduced(0.5f), getButtonRadius(), 1.0f);

        auto arrowZone = bounds.removeFromRight(20.0f).reduced(5.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowZone.getX(), arrowZone.getY(),
                          arrowZone.getRight(), arrowZone.getY(),
                          arrowZone.getCentreX(), arrowZone.getBottom());
        g.setColour(juce::Colour(theme.amber));
        g.fillPath(arrow);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DawLookAndFeel)
};
