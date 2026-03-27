#pragma once

#include "DawLookAndFeel.h"

// "Steel" theme — dark blue with steel/teal blue accents.
// Inspired by sci-fi HUD with globe graphics, progress rings, panel borders.
class SteelLookAndFeel : public DawLookAndFeel
{
public:
    SteelLookAndFeel()
    {
        // ── Surfaces — dark blue-steel ──
        theme.body        = 0xff0a1540;
        theme.bodyLight   = 0xff101c4a;
        theme.bodyDark    = 0xff060e30;
        theme.border      = 0xff1e3070;
        theme.borderLight = 0xff2a4080;

        // ── Text ──
        theme.textPrimary   = 0xffaabbdd;
        theme.textSecondary = 0xff5568aa;
        theme.textBright    = 0xffdde4ff;

        // ── Accent — steel teal ──
        theme.red       = 0xffdd5555;
        theme.redDark   = 0xff882222;
        theme.amber     = 0xff66aacc;  // steel teal
        theme.amberDark = 0xff447799;
        theme.green     = 0xff55cc88;
        theme.greenDark = 0xff228855;

        // ── LCD ──
        theme.lcdBg    = 0xff060e30;
        theme.lcdText  = 0xff66aacc;
        theme.lcdAmber = 0xff66aacc;

        // ── Buttons ──
        theme.buttonFace  = 0xff0e1848;
        theme.buttonHover = 0xff142055;
        theme.buttonDown  = 0xff0a1238;

        theme.btnStop       = 0xff0e1848;
        theme.btnMetronome  = 0xff0e1848;
        theme.btnMetronomeOn = 0xff102858;
        theme.btnCountIn    = 0xff0e1848;
        theme.btnCountInOn  = 0xff102858;
        theme.btnNewClip    = 0xff0e2448;
        theme.btnDeleteClip = 0xff260e20;
        theme.btnDuplicate  = 0xff0e1850;
        theme.btnSplit      = 0xff1a1e0e;
        theme.btnQuantize   = 0xff101848;
        theme.btnEditNotes  = 0xff0e1850;
        theme.btnNav        = 0xff0e1848;
        theme.btnSave       = 0xff0e2438;
        theme.btnLoad       = 0xff101850;
        theme.btnUndoRedo   = 0xff101848;
        theme.btnMidi2      = 0xff0e1848;
        theme.btnMidi2On    = 0xff102860;
        theme.btnLoop       = 0xff0e1848;
        theme.btnLoopOn     = 0xff102860;
        theme.loopRegion    = 0x2266aacc;
        theme.loopBorder    = 0xff5588aa;

        // ── Timeline ──
        theme.timelineBg         = 0xff081038;
        theme.timelineAltRow     = 0xff0c1440;
        theme.timelineSelectedRow = 0xff122458;
        theme.timelineGridMajor  = 0xff7788bb;
        theme.timelineGridMinor  = 0xff2a3868;
        theme.timelineGridFaint  = 0xff101c48;
        theme.timelineGridBeat   = 0xff1e3068;

        // ── Clips ──
        theme.clipDefault   = 0xff1a2a60;
        theme.clipRecording = 0xff552222;
        theme.clipQueued    = 0xff1a3a60;
        theme.clipPlaying   = 0xff1a5a3a;

        // ── Playhead ──
        theme.playhead     = 0xdd44dd66;
        theme.playheadGlow = 0x3344dd66;

        theme.accentStripe = 0xff66aacc;

        theme.trackSelected = 0xff122458;
        theme.trackArmed    = 0xff441122;
        theme.trackMuteOn   = 0xffdd5555;
        theme.trackSoloOn   = 0xff66aacc;
        theme.trackSoloText = 0xff060e30;

        applyThemeColors();
    }

    float getButtonRadius() const override { return 2.0f; }

    // Steel: sharp military-style buttons with bright border
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
            baseColour = baseColour.brighter(0.08f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 2.0f);

        // Corner accents on hover
        if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(juce::Colour(theme.amber).withAlpha(0.5f));
            float s = 5.0f;
            // Top-left
            g.drawLine(bounds.getX(), bounds.getY(), bounds.getX() + s, bounds.getY(), 1.5f);
            g.drawLine(bounds.getX(), bounds.getY(), bounds.getX(), bounds.getY() + s, 1.5f);
            // Bottom-right
            g.drawLine(bounds.getRight(), bounds.getBottom(), bounds.getRight() - s, bounds.getBottom(), 1.5f);
            g.drawLine(bounds.getRight(), bounds.getBottom(), bounds.getRight(), bounds.getBottom() - s, 1.5f);
        }

        g.setColour(juce::Colour(theme.border));
        g.drawRoundedRectangle(bounds, 2.0f, 1.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SteelLookAndFeel)
};
