#pragma once

#include "DawLookAndFeel.h"

// "Midnight" theme — dark navy background with cyan/teal gradient accents.
// Inspired by modern dark social media UI with mint highlights.
class MidnightLookAndFeel : public DawLookAndFeel
{
public:
    MidnightLookAndFeel()
    {
        // ── Surfaces — deep navy ──
        theme.body        = 0xff0a1628;
        theme.bodyLight   = 0xff122040;
        theme.bodyDark    = 0xff060e1c;
        theme.border      = 0xff1a3050;
        theme.borderLight = 0xff254060;

        // ── Text ──
        theme.textPrimary   = 0xffccdde8;
        theme.textSecondary = 0xff6688aa;
        theme.textBright    = 0xffeef4f8;

        // ── Accent — cyan/teal ──
        theme.red       = 0xffee5566;
        theme.redDark   = 0xff882233;
        theme.amber     = 0xff44dde8;  // cyan
        theme.amberDark = 0xff228899;
        theme.green     = 0xff44ddaa;
        theme.greenDark = 0xff228866;

        // ── LCD ──
        theme.lcdBg    = 0xff060e1c;
        theme.lcdText  = 0xff44dde8;
        theme.lcdAmber = 0xff44dde8;

        // ── Buttons ──
        theme.buttonFace  = 0xff0e1a30;
        theme.buttonHover = 0xff142440;
        theme.buttonDown  = 0xff081220;

        theme.btnStop       = 0xff0e1a30;
        theme.btnMetronome  = 0xff0e1a30;
        theme.btnMetronomeOn = 0xff0e2a3a;
        theme.btnCountIn    = 0xff0e1a30;
        theme.btnCountInOn  = 0xff0e2a3a;
        theme.btnNewClip    = 0xff0e2530;
        theme.btnDeleteClip = 0xff200e18;
        theme.btnDuplicate  = 0xff0e1830;
        theme.btnSplit      = 0xff181e0e;
        theme.btnQuantize   = 0xff101828;
        theme.btnEditNotes  = 0xff0e1830;
        theme.btnNav        = 0xff0e1a30;
        theme.btnSave       = 0xff0e2520;
        theme.btnLoad       = 0xff101830;
        theme.btnUndoRedo   = 0xff101828;
        theme.btnMidi2      = 0xff0e1a30;
        theme.btnMidi2On    = 0xff0e2a40;
        theme.btnLoop       = 0xff0e1a30;
        theme.btnLoopOn     = 0xff0e2a40;
        theme.loopRegion    = 0x2244dde8;
        theme.loopBorder    = 0xff44aabb;

        // ── Timeline ──
        theme.timelineBg         = 0xff081220;
        theme.timelineAltRow     = 0xff0c1628;
        theme.timelineSelectedRow = 0xff0e2a3a;
        theme.timelineGridMajor  = 0xff6688aa;
        theme.timelineGridMinor  = 0xff2a4466;
        theme.timelineGridFaint  = 0xff101e30;
        theme.timelineGridBeat   = 0xff1a3050;

        // ── Clips ──
        theme.clipDefault   = 0xff1a3050;
        theme.clipRecording = 0xff552233;
        theme.clipQueued    = 0xff1a3a4a;
        theme.clipPlaying   = 0xff1a4a3a;

        // ── Playhead ──
        theme.playhead     = 0xdd44dd66;
        theme.playheadGlow = 0x3344dd66;

        theme.accentStripe = 0xff44dde8;

        theme.trackSelected = 0xff0e2a3a;
        theme.trackArmed    = 0xff441122;
        theme.trackMuteOn   = 0xffee5566;
        theme.trackSoloOn   = 0xff44dde8;
        theme.trackSoloText = 0xff060e1c;

        applyThemeColors();
    }

    float getButtonRadius() const override { return 5.0f; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidnightLookAndFeel)
};
