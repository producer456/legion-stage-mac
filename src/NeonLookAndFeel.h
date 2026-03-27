#pragma once

#include "DawLookAndFeel.h"

// "Neon" theme — deep navy with bright cyan/electric blue HUD elements.
// Inspired by futuristic dashboard with glowing progress rings and data panels.
class NeonLookAndFeel : public DawLookAndFeel
{
public:
    NeonLookAndFeel()
    {
        // ── Surfaces — deep navy ──
        theme.body        = 0xff0a1035;
        theme.bodyLight   = 0xff101840;
        theme.bodyDark    = 0xff060a28;
        theme.border      = 0xff1a2860;
        theme.borderLight = 0xff2a3870;

        // ── Text ──
        theme.textPrimary   = 0xffbbccee;
        theme.textSecondary = 0xff6678aa;
        theme.textBright    = 0xffeef0ff;

        // ── Accent — bright cyan ──
        theme.red       = 0xffee5555;
        theme.redDark   = 0xff882222;
        theme.amber     = 0xff55ccee;  // bright cyan
        theme.amberDark = 0xff338899;
        theme.green     = 0xff55ee88;
        theme.greenDark = 0xff228855;

        // ── LCD — dark with cyan ──
        theme.lcdBg    = 0xff060a28;
        theme.lcdText  = 0xff55ccee;
        theme.lcdAmber = 0xff55ccee;

        // ── Buttons ──
        theme.buttonFace  = 0xff0e1440;
        theme.buttonHover = 0xff141c50;
        theme.buttonDown  = 0xff080e30;

        theme.btnStop       = 0xff0e1440;
        theme.btnMetronome  = 0xff0e1440;
        theme.btnMetronomeOn = 0xff0e2050;
        theme.btnCountIn    = 0xff0e1440;
        theme.btnCountInOn  = 0xff0e2050;
        theme.btnNewClip    = 0xff0e2240;
        theme.btnDeleteClip = 0xff280e1a;
        theme.btnDuplicate  = 0xff0e1848;
        theme.btnSplit      = 0xff1a1c0e;
        theme.btnQuantize   = 0xff101440;
        theme.btnEditNotes  = 0xff0e1848;
        theme.btnNav        = 0xff0e1440;
        theme.btnSave       = 0xff0e2230;
        theme.btnLoad       = 0xff101448;
        theme.btnUndoRedo   = 0xff101440;
        theme.btnMidi2      = 0xff0e1440;
        theme.btnMidi2On    = 0xff0e2860;
        theme.btnLoop       = 0xff0e1440;
        theme.btnLoopOn     = 0xff0e2860;
        theme.loopRegion    = 0x2255ccee;
        theme.loopBorder    = 0xff44aacc;

        // ── Timeline ──
        theme.timelineBg         = 0xff080c30;
        theme.timelineAltRow     = 0xff0c1038;
        theme.timelineSelectedRow = 0xff102050;
        theme.timelineGridMajor  = 0xff7788cc;
        theme.timelineGridMinor  = 0xff2a3866;
        theme.timelineGridFaint  = 0xff101840;
        theme.timelineGridBeat   = 0xff1a2860;

        // ── Clips ──
        theme.clipDefault   = 0xff182858;
        theme.clipRecording = 0xff552222;
        theme.clipQueued    = 0xff1a3858;
        theme.clipPlaying   = 0xff1a5838;

        // ── Playhead ──
        theme.playhead     = 0xdd44dd66;
        theme.playheadGlow = 0x3344dd66;

        theme.accentStripe = 0xff55ccee;

        theme.trackSelected = 0xff102050;
        theme.trackArmed    = 0xff441122;
        theme.trackMuteOn   = 0xffee5555;
        theme.trackSoloOn   = 0xff55ccee;
        theme.trackSoloText = 0xff060a28;

        applyThemeColors();
    }

    float getButtonRadius() const override { return 3.0f; }

    // Neon: glowing border buttons
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto baseColour = backgroundColour;

        if (shouldDrawButtonAsDown)
            baseColour = baseColour.darker(0.25f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter(0.1f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 3.0f);

        // Bright cyan border
        auto borderAlpha = shouldDrawButtonAsHighlighted ? 0.35f : 0.15f;
        g.setColour(juce::Colour(theme.amber).withAlpha(borderAlpha));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeonLookAndFeel)
};
