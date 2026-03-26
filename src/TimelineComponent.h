#pragma once

#include <JuceHeader.h>
#include "PluginHost.h"
#include "SequencerEngine.h"
#include "PianoRollComponent.h"

class TimelineComponent : public juce::Component, public juce::Timer
{
public:
    TimelineComponent(PluginHost& host);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    PluginHost& pluginHost;

    // View state
    double scrollX = 0.0;       // beats offset
    double pixelsPerBeat = 40.0;
    int trackHeight = 40;
    int headerHeight = 20;      // beat numbers at top
    int trackLabelWidth = 60;

    // Drawing
    void drawHeader(juce::Graphics& g);
    void drawTrackLanes(juce::Graphics& g);
    void drawClips(juce::Graphics& g);
    void drawPlayhead(juce::Graphics& g);
    void drawMiniNotes(juce::Graphics& g, const MidiClip& clip, juce::Rectangle<float> area);

    // Coordinate conversion
    float beatToX(double beat) const;
    double xToBeat(float x) const;
    int yToTrack(float y) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineComponent)
};
