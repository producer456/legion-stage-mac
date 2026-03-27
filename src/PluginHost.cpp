#include "PluginHost.h"
#include "SpectrumComponent.h"
#include "LissajousComponent.h"

PluginHost::PluginHost()
{
    formatManager.addFormat(new juce::VST3PluginFormat());

    for (int i = 0; i < NUM_TRACKS; ++i)
    {
        tracks[static_cast<size_t>(i)].index = i;
        tracks[static_cast<size_t>(i)].name = "Track " + juce::String(i + 1);
    }

    setupGraph();
}

PluginHost::~PluginHost()
{
    clear();
}

void PluginHost::setupGraph()
{
    setPlayConfigDetails(0, 2, storedSampleRate, storedBlockSize);

    midiInputNode = addNode(
        std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(
            AudioProcessorGraph::AudioGraphIOProcessor::midiInputNode));

    audioOutputNode = addNode(
        std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor>(
            AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));

    for (int i = 0; i < NUM_TRACKS; ++i)
    {
        auto& track = tracks[static_cast<size_t>(i)];

        // Gain node
        auto gainProc = std::make_unique<GainProcessor>();
        gainProc->soloCount = &soloCount;
        track.gainProcessor = gainProc.get();
        track.gainNode = addNode(std::move(gainProc));

        // ClipPlayer node
        auto clipProc = std::make_unique<ClipPlayerNode>(engine);
        track.clipPlayer = clipProc.get();
        track.clipPlayerNode = addNode(std::move(clipProc));

        // Gain -> audio output
        for (int ch = 0; ch < 2; ++ch)
        {
            addConnection({ { track.gainNode->nodeID, ch },
                            { audioOutputNode->nodeID, ch } });
        }
    }
}

void PluginHost::scanForPlugins()
{
    auto* format = formatManager.getFormat(0);
    if (format == nullptr) return;

    auto searchPaths = format->getDefaultLocationsToSearch();
    auto foundFiles = format->searchPathsForPlugins(searchPaths, true, false);

    for (const auto& file : foundFiles)
    {
        juce::OwnedArray<juce::PluginDescription> foundTypes;
        knownPluginList.scanAndAddFile(file, true, foundTypes, *format);
    }
}

void PluginHost::setAudioParams(double sampleRate, int blockSize)
{
    storedSampleRate = sampleRate;
    storedBlockSize = blockSize;
    midiCollector.reset(sampleRate);
}

bool PluginHost::loadPlugin(int trackIndex, const juce::PluginDescription& desc, juce::String& errorMsg)
{
    if (trackIndex < 0 || trackIndex >= NUM_TRACKS) return false;

    unloadPlugin(trackIndex);

    auto instance = formatManager.createPluginInstance(desc, storedSampleRate, storedBlockSize, errorMsg);
    if (instance == nullptr)
        return false;

    auto& track = tracks[static_cast<size_t>(trackIndex)];
    track.plugin = instance.get();
    track.pluginNode = addNode(std::move(instance));

    if (track.pluginNode == nullptr)
    {
        track.plugin = nullptr;
        errorMsg = "Failed to add plugin to graph";
        return false;
    }

    connectTrackAudio(trackIndex);
    updateMidiRouting();
    prepareToPlay(storedSampleRate, storedBlockSize);

    return true;
}

void PluginHost::unloadPlugin(int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= NUM_TRACKS) return;

    auto& track = tracks[static_cast<size_t>(trackIndex)];
    if (track.pluginNode == nullptr) return;

    // Send all-notes-off to prevent stuck notes
    if (track.clipPlayer != nullptr)
        track.clipPlayer->sendAllNotesOff.store(true);

    auto connections = getConnections();
    for (auto& conn : connections)
    {
        if (conn.source.nodeID == track.pluginNode->nodeID ||
            conn.destination.nodeID == track.pluginNode->nodeID)
        {
            removeConnection(conn);
        }
    }

    removeNode(track.pluginNode->nodeID);
    track.pluginNode = nullptr;
    track.plugin = nullptr;
}

void PluginHost::connectTrackAudio(int trackIndex)
{
    auto& track = tracks[static_cast<size_t>(trackIndex)];
    if (track.pluginNode == nullptr) return;

    // ClipPlayer MIDI -> Plugin MIDI
    addConnection({ { track.clipPlayerNode->nodeID, AudioProcessorGraph::midiChannelIndex },
                    { track.pluginNode->nodeID, AudioProcessorGraph::midiChannelIndex } });

    // Plugin audio -> Gain
    for (int ch = 0; ch < 2; ++ch)
    {
        addConnection({ { track.pluginNode->nodeID, ch },
                        { track.gainNode->nodeID, ch } });
    }
}

void PluginHost::setSelectedTrack(int index)
{
    if (index < 0 || index >= NUM_TRACKS) return;

    // Disarm previous track (unless it's lock-armed)
    if (selectedTrack != index)
    {
        auto& oldTrack = tracks[static_cast<size_t>(selectedTrack)];
        if (oldTrack.clipPlayer != nullptr && !oldTrack.clipPlayer->armLocked.load())
            oldTrack.clipPlayer->armed.store(false);
    }

    selectedTrack = index;

    // Auto-arm new track
    auto& newTrack = tracks[static_cast<size_t>(selectedTrack)];
    if (newTrack.clipPlayer != nullptr)
        newTrack.clipPlayer->armed.store(true);

    updateMidiRouting();
}

void PluginHost::updateMidiRouting()
{
    // Remove all MIDI connections from MIDI input node
    auto connections = getConnections();
    for (auto& conn : connections)
    {
        if (conn.source.nodeID == midiInputNode->nodeID &&
            conn.source.channelIndex == AudioProcessorGraph::midiChannelIndex)
        {
            removeConnection(conn);
        }
    }

    // Connect MIDI input to selected track's ClipPlayerNode
    auto& track = tracks[static_cast<size_t>(selectedTrack)];
    addConnection({ { midiInputNode->nodeID, AudioProcessorGraph::midiChannelIndex },
                    { track.clipPlayerNode->nodeID, AudioProcessorGraph::midiChannelIndex } });
}

void PluginHost::sendTestNoteOn(int noteNumber, float velocity)
{
    auto msg = juce::MidiMessage::noteOn(1, noteNumber, velocity);
    msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    midiCollector.addMessageToQueue(msg);
}

void PluginHost::sendTestNoteOff(int noteNumber)
{
    auto msg = juce::MidiMessage::noteOff(1, noteNumber);
    msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    midiCollector.addMessageToQueue(msg);
}

void PluginHost::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    midiCollector.removeNextBlockOfMessages(midiMessages, buffer.getNumSamples());

    // Advance transport
    engine.advancePosition(buffer.getNumSamples(), storedSampleRate);

    // ── MIDI Clock ──
    // Send start/stop messages on transport state changes
    bool isPlaying = engine.isPlaying() && !engine.isInCountIn();
    if (isPlaying && !midiClockWasPlaying)
    {
        // Send Song Position Pointer then Start
        double beatPos = engine.getPositionInBeats();
        int midiBeats = static_cast<int>(beatPos * 6.0); // MIDI beat = 1/16 note = 6 clocks
        midiMessages.addEvent(juce::MidiMessage::songPositionPointer(midiBeats), 0);
        midiMessages.addEvent(juce::MidiMessage(0xFA), 0); // Start
        midiClockPulseAccum = 0.0;
    }
    else if (!isPlaying && midiClockWasPlaying)
    {
        midiMessages.addEvent(juce::MidiMessage(0xFC), 0); // Stop
        midiClockPulseAccum = 0.0;
    }
    midiClockWasPlaying = isPlaying;

    // Send clock pulses (0xF8) at 24 PPQN while playing
    if (isPlaying)
    {
        double bpm = engine.getBpm();
        double pulsesPerSecond = (bpm / 60.0) * 24.0;
        double pulsesThisBlock = pulsesPerSecond * (static_cast<double>(buffer.getNumSamples()) / storedSampleRate);
        midiClockPulseAccum += pulsesThisBlock;

        int numPulses = static_cast<int>(midiClockPulseAccum);
        if (numPulses > 0)
        {
            midiClockPulseAccum -= numPulses;
            // Spread pulses evenly across the block
            double samplesPerPulse = static_cast<double>(buffer.getNumSamples()) / numPulses;
            for (int p = 0; p < numPulses; ++p)
            {
                int sampleOffset = static_cast<int>(p * samplesPerPulse);
                midiMessages.addEvent(juce::MidiMessage(0xF8), sampleOffset);
            }
        }
    }

    AudioProcessorGraph::processBlock(buffer, midiMessages);

    // Apply automation during playback
    if (engine.isPlaying() && !engine.isInCountIn())
    {
        double beat = engine.getPositionInBeats();

        for (int t = 0; t < NUM_TRACKS; ++t)
        {
            auto& track = tracks[static_cast<size_t>(t)];
            if (track.plugin == nullptr) continue;

            auto& params = track.plugin->getParameters();

            for (auto* lane : track.automationLanes)
            {
                if (lane->parameterIndex >= 0 && lane->parameterIndex < params.size()
                    && !lane->points.isEmpty())
                {
                    float val = lane->getValueAtBeat(beat);
                    if (val >= 0.0f)
                        params[lane->parameterIndex]->setValue(val);
                }
            }
        }
    }

    // Render metronome click on top of the output
    engine.renderMetronome(buffer, buffer.getNumSamples(), storedSampleRate);

    // Feed spectrum analyzer (mono mix of L+R)
    if (spectrumDisplay != nullptr && buffer.getNumChannels() >= 2)
    {
        int n = buffer.getNumSamples();
        const float* L = buffer.getReadPointer(0);
        const float* R = buffer.getReadPointer(1);

        // Stack-allocate a small mono buffer
        float mono[2048];
        int count = juce::jmin(n, 2048);
        for (int i = 0; i < count; ++i)
            mono[i] = (L[i] + R[i]) * 0.5f;

        spectrumDisplay->pushSamples(mono, count);
    }

    // Feed Lissajous display (stereo)
    if (lissajousDisplay != nullptr && buffer.getNumChannels() >= 2)
    {
        lissajousDisplay->pushSamples(buffer.getReadPointer(0),
                                       buffer.getReadPointer(1),
                                       buffer.getNumSamples());
    }
}
