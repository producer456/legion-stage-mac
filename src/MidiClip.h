#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

struct MidiClip
{
    juce::MidiMessageSequence events;  // timestamps in beats
    double lengthInBeats = 4.0;        // default 1 bar at 4/4
};

struct ClipSlot
{
    enum State { Empty, Stopped, Playing, Recording };

    std::unique_ptr<MidiClip> clip;
    std::atomic<State> state { Empty };

    bool hasContent() const { return clip != nullptr && clip->events.getNumEvents() > 0; }
};
