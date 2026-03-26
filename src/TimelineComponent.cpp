#include "TimelineComponent.h"

TimelineComponent::TimelineComponent(PluginHost& host)
    : pluginHost(host)
{
    startTimerHz(30);
}

// ── Coordinate conversion ────────────────────────────────────────────────────

float TimelineComponent::beatToX(double beat) const
{
    return trackLabelWidth + static_cast<float>((beat - scrollX) * pixelsPerBeat);
}

double TimelineComponent::xToBeat(float x) const
{
    return scrollX + (x - trackLabelWidth) / pixelsPerBeat;
}

int TimelineComponent::yToTrack(float y) const
{
    return static_cast<int>((y - headerHeight) / trackHeight);
}

// ── Timer ────────────────────────────────────────────────────────────────────

void TimelineComponent::timerCallback()
{
    auto& engine = pluginHost.getEngine();
    if (engine.isPlaying())
    {
        // Auto-scroll to follow playhead
        double pos = engine.getPositionInBeats();
        float playheadX = beatToX(pos);
        float viewWidth = static_cast<float>(getWidth());

        if (playheadX > viewWidth * 0.8f)
        {
            scrollX = pos - (viewWidth * 0.2 - trackLabelWidth) / pixelsPerBeat;
        }
        else if (playheadX < static_cast<float>(trackLabelWidth))
        {
            scrollX = pos - 1.0;
            if (scrollX < 0.0) scrollX = 0.0;
        }

        repaint();
    }
}

// ── Mouse ────────────────────────────────────────────────────────────────────

void TimelineComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCtrlDown())
    {
        // Zoom
        double zoomFactor = 1.0 + w.deltaY * 0.3;
        pixelsPerBeat = juce::jlimit(10.0, 200.0, pixelsPerBeat * zoomFactor);
    }
    else
    {
        // Scroll horizontal
        scrollX -= w.deltaY * 4.0;
        if (scrollX < 0.0) scrollX = 0.0;
    }
    repaint();
}

void TimelineComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.x < trackLabelWidth) return;

    int trackIdx = yToTrack(static_cast<float>(e.y));
    if (trackIdx < 0 || trackIdx >= PluginHost::NUM_TRACKS) return;

    // Right-click a clip to open piano roll
    if (e.mods.isRightButtonDown())
    {
        double beatPos = xToBeat(static_cast<float>(e.x));
        auto* cp = pluginHost.getTrack(trackIdx).clipPlayer;
        if (cp == nullptr) return;

        // Find which clip slot is at this position
        for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
        {
            auto& slot = cp->getSlot(s);
            if (slot.clip != nullptr && slot.hasContent())
            {
                // For now, clips in the timeline are placed at slot_index * clip_length
                double clipStart = s * slot.clip->lengthInBeats;
                double clipEnd = clipStart + slot.clip->lengthInBeats;

                if (beatPos >= clipStart && beatPos < clipEnd)
                {
                    new PianoRollWindow("Piano Roll - Track " + juce::String(trackIdx + 1)
                        + " Slot " + juce::String(s + 1), *slot.clip,
                        pluginHost.getEngine());
                    return;
                }
            }
        }
    }
}

// ── Drawing ──────────────────────────────────────────────────────────────────

void TimelineComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
    drawHeader(g);
    drawTrackLanes(g);
    drawClips(g);
    drawPlayhead(g);
}

void TimelineComponent::resized() {}

void TimelineComponent::drawHeader(juce::Graphics& g)
{
    // Background
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(0, 0, getWidth(), headerHeight);

    // Beat/bar numbers
    double firstBeat = std::floor(scrollX);
    double lastBeat = scrollX + (getWidth() - trackLabelWidth) / pixelsPerBeat;

    for (double beat = firstBeat; beat <= lastBeat; beat += 1.0)
    {
        float x = beatToX(beat);
        if (x < trackLabelWidth) continue;

        bool isBar = std::abs(std::fmod(beat, 4.0)) < 0.001;

        if (isBar)
        {
            int barNum = static_cast<int>(beat / 4.0) + 1;
            g.setColour(juce::Colour(0xffcccccc));
            g.setFont(11.0f);
            g.drawText(juce::String(barNum), static_cast<int>(x) + 2, 0, 40, headerHeight,
                       juce::Justification::centredLeft);
        }

        g.setColour(isBar ? juce::Colour(0xff555555) : juce::Colour(0xff333333));
        g.drawVerticalLine(static_cast<int>(x), static_cast<float>(headerHeight),
                           static_cast<float>(getHeight()));
    }

    // Divider line
    g.setColour(juce::Colour(0xff444444));
    g.drawHorizontalLine(headerHeight - 1, 0, static_cast<float>(getWidth()));
}

void TimelineComponent::drawTrackLanes(juce::Graphics& g)
{
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        int y = headerHeight + t * trackHeight;

        // Alternating background
        g.setColour(t % 2 == 0 ? juce::Colour(0xff1e1e1e) : juce::Colour(0xff222222));
        g.fillRect(0, y, getWidth(), trackHeight);

        // Track label
        g.setColour(juce::Colour(0xffaaaaaa));
        g.setFont(11.0f);

        juce::String label = "Trk " + juce::String(t + 1);
        auto& track = pluginHost.getTrack(t);
        if (track.plugin != nullptr)
            label = track.plugin->getName().substring(0, 8);

        g.drawText(label, 4, y, trackLabelWidth - 8, trackHeight, juce::Justification::centredLeft);

        // Lane divider
        g.setColour(juce::Colour(0xff333333));
        g.drawHorizontalLine(y + trackHeight - 1, 0, static_cast<float>(getWidth()));
    }
}

void TimelineComponent::drawClips(juce::Graphics& g)
{
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto* cp = pluginHost.getTrack(t).clipPlayer;
        if (cp == nullptr) continue;

        int laneY = headerHeight + t * trackHeight;

        for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
        {
            auto& slot = cp->getSlot(s);
            if (slot.clip == nullptr || !slot.hasContent()) continue;

            // Place clips sequentially: slot 0 at beat 0, slot 1 at end of slot 0, etc.
            double clipStart = s * slot.clip->lengthInBeats;
            double clipEnd = clipStart + slot.clip->lengthInBeats;

            float x1 = beatToX(clipStart);
            float x2 = beatToX(clipEnd);

            // Skip if out of view
            if (x2 < trackLabelWidth || x1 > getWidth()) continue;

            // Clip block
            auto clipRect = juce::Rectangle<float>(x1, static_cast<float>(laneY + 2),
                                                    x2 - x1, static_cast<float>(trackHeight - 4));

            auto state = slot.state.load();
            juce::Colour clipColor;
            if (state == ClipSlot::Playing)
                clipColor = juce::Colour(0xff338844);
            else if (state == ClipSlot::Recording)
                clipColor = juce::Colour(0xff883333);
            else
                clipColor = juce::Colour(0xff445566);

            g.setColour(clipColor);
            g.fillRoundedRectangle(clipRect, 3.0f);

            // Border
            g.setColour(clipColor.brighter(0.3f));
            g.drawRoundedRectangle(clipRect, 3.0f, 1.0f);

            // Mini note preview inside the clip
            g.saveState();
            g.reduceClipRegion(clipRect.toNearestInt());
            drawMiniNotes(g, *slot.clip, clipRect);
            g.restoreState();

            // Slot label
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.setFont(9.0f);
            g.drawText("S" + juce::String(s + 1), clipRect.reduced(4, 0),
                       juce::Justification::topLeft);
        }
    }
}

void TimelineComponent::drawMiniNotes(juce::Graphics& g, const MidiClip& clip, juce::Rectangle<float> area)
{
    if (clip.events.getNumEvents() == 0 || clip.lengthInBeats <= 0.0) return;

    // Find note range for scaling
    int minNote = 127, maxNote = 0;
    for (int i = 0; i < clip.events.getNumEvents(); ++i)
    {
        auto& msg = clip.events.getEventPointer(i)->message;
        if (msg.isNoteOn())
        {
            minNote = juce::jmin(minNote, msg.getNoteNumber());
            maxNote = juce::jmax(maxNote, msg.getNoteNumber());
        }
    }

    if (minNote > maxNote) return;
    int noteRange = juce::jmax(1, maxNote - minNote + 1);

    float noteH = juce::jmax(1.0f, (area.getHeight() - 12.0f) / static_cast<float>(noteRange));
    float beatsToPixels = area.getWidth() / static_cast<float>(clip.lengthInBeats);

    g.setColour(juce::Colours::white.withAlpha(0.5f));

    for (int i = 0; i < clip.events.getNumEvents(); ++i)
    {
        auto* event = clip.events.getEventPointer(i);
        if (!event->message.isNoteOn()) continue;

        float nx = area.getX() + static_cast<float>(event->message.getTimeStamp()) * beatsToPixels;

        float noteLen = 0.25f;
        if (event->noteOffObject != nullptr)
        {
            noteLen = static_cast<float>(event->noteOffObject->message.getTimeStamp()
                                         - event->message.getTimeStamp());
            if (noteLen < 0.05f) noteLen = 0.25f;
        }
        float nw = noteLen * beatsToPixels;

        int noteRow = maxNote - event->message.getNoteNumber();
        float ny = area.getY() + 10.0f + noteRow * noteH;

        g.fillRect(nx, ny, juce::jmax(1.0f, nw), juce::jmax(1.0f, noteH - 1.0f));
    }
}

void TimelineComponent::drawPlayhead(juce::Graphics& g)
{
    auto& engine = pluginHost.getEngine();
    if (!engine.isPlaying()) return;

    double pos = engine.getPositionInBeats();
    float x = beatToX(pos);

    if (x < trackLabelWidth || x > getWidth()) return;

    // Playhead line
    g.setColour(juce::Colour(0xddffcc00));
    g.drawVerticalLine(static_cast<int>(x), static_cast<float>(headerHeight),
                       static_cast<float>(getHeight()));

    // Glow
    g.setColour(juce::Colour(0x33ffcc00));
    g.fillRect(x - 1.0f, static_cast<float>(headerHeight), 3.0f,
               static_cast<float>(getHeight() - headerHeight));

    // Playhead triangle at top
    g.setColour(juce::Colour(0xffee9900));
    juce::Path triangle;
    triangle.addTriangle(x - 5, static_cast<float>(headerHeight),
                         x + 5, static_cast<float>(headerHeight),
                         x, static_cast<float>(headerHeight + 8));
    g.fillPath(triangle);
}
