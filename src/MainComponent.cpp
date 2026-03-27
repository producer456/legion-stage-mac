#include "MainComponent.h"

MainComponent::MainComponent()
{
    themeManager.setTheme(ThemeManager::Ice, this);

    auto result = deviceManager.initialiseWithDefaultDevices(0, 2);
    if (result.isNotEmpty())
        DBG("Audio device init error: " + result);

    audioPlayer.setProcessor(&pluginHost);
    deviceManager.addAudioCallback(&audioPlayer);

    addAndMakeVisible(spectrumDisplay);
    pluginHost.spectrumDisplay = &spectrumDisplay;

    addAndMakeVisible(lissajousDisplay);
    pluginHost.lissajousDisplay = &lissajousDisplay;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        pluginHost.setAudioParams(device->getCurrentSampleRate(),
                                  device->getCurrentBufferSizeSamples());
        pluginHost.prepareToPlay(device->getCurrentSampleRate(),
                                 device->getCurrentBufferSizeSamples());
    }

    // ── Top Bar: Transport + Track Select ──
    addAndMakeVisible(prevTrackButton);
    prevTrackButton.onClick = [this] { selectTrack(juce::jmax(0, selectedTrackIndex - 1)); };

    addAndMakeVisible(nextTrackButton);
    nextTrackButton.onClick = [this] { selectTrack(juce::jmin(PluginHost::NUM_TRACKS - 1, selectedTrackIndex + 1)); };

    addAndMakeVisible(trackNameLabel);
    trackNameLabel.setJustificationType(juce::Justification::centred);
    trackNameLabel.setFont(juce::Font("Consolas", 16.0f, juce::Font::bold));
    trackNameLabel.setColour(juce::Label::textColourId, juce::Colour(themeManager.getColors().amber));

    addAndMakeVisible(recordButton);
    recordButton.setClickingTogglesState(true);
    recordButton.onClick = [this] { pluginHost.getEngine().toggleRecord(); };

    addAndMakeVisible(playButton);
    playButton.onClick = [this] { pluginHost.getEngine().play(); };

    addAndMakeVisible(stopButton);
    stopButton.onClick = [this] {
        auto& eng = pluginHost.getEngine();
        if (!eng.isPlaying())
        {
            eng.resetPosition();
            for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
            {
                auto* cp = pluginHost.getTrack(t).clipPlayer;
                if (cp) cp->stopAllSlots();
            }
        }
        else
        {
            eng.stop();
            for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
            {
                auto* cp = pluginHost.getTrack(t).clipPlayer;
                if (cp) cp->sendAllNotesOff.store(true);
            }
        }
        if (timelineComponent) timelineComponent->repaint();
    };

    addAndMakeVisible(metronomeButton);
    metronomeButton.setClickingTogglesState(true);
    metronomeButton.onClick = [this] { pluginHost.getEngine().toggleMetronome(); };

    addAndMakeVisible(bpmDownButton);
    bpmDownButton.onClick = [this] {
        double bpm = juce::jmax(20.0, pluginHost.getEngine().getBpm() - 1.0);
        pluginHost.getEngine().setBpm(bpm);
        bpmLabel.setText(juce::String(static_cast<int>(bpm)) + " BPM", juce::dontSendNotification);
    };

    addAndMakeVisible(bpmLabel);
    bpmLabel.setText("120 BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(bpmUpButton);
    bpmUpButton.onClick = [this] {
        double bpm = juce::jmin(300.0, pluginHost.getEngine().getBpm() + 1.0);
        pluginHost.getEngine().setBpm(bpm);
        bpmLabel.setText(juce::String(static_cast<int>(bpm)) + " BPM", juce::dontSendNotification);
    };

    addAndMakeVisible(beatLabel);

    // ── Edit Toolbar ──
    addAndMakeVisible(newClipButton);
    newClipButton.onClick = [this] {
        takeSnapshot();
        if (timelineComponent) timelineComponent->createClipAtPlayhead();
    };

    addAndMakeVisible(deleteClipButton);
    deleteClipButton.onClick = [this] {
        takeSnapshot();
        if (timelineComponent) timelineComponent->deleteSelected();
    };

    addAndMakeVisible(duplicateClipButton);
    duplicateClipButton.onClick = [this] {
        takeSnapshot();
        if (timelineComponent) timelineComponent->duplicateSelected();
    };

    addAndMakeVisible(splitClipButton);
    splitClipButton.onClick = [this] {
        takeSnapshot();
        if (timelineComponent) timelineComponent->splitSelected();
    };

    addAndMakeVisible(quantizeButton);
    quantizeButton.onClick = [this] {
        takeSnapshot();
        if (timelineComponent) timelineComponent->quantizeSelectedClip();
    };

    addAndMakeVisible(gridSelector);
    gridSelector.addItem("1/4", 1);
    gridSelector.addItem("1/8", 2);
    gridSelector.addItem("1/16", 3);
    gridSelector.addItem("1/32", 4);
    gridSelector.setSelectedId(3, juce::dontSendNotification); // default 1/16
    gridSelector.onChange = [this] {
        if (timelineComponent)
        {
            double res = 1.0;
            switch (gridSelector.getSelectedId())
            {
                case 1: res = 1.0; break;    // 1/4
                case 2: res = 0.5; break;    // 1/8
                case 3: res = 0.25; break;   // 1/16
                case 4: res = 0.125; break;  // 1/32
            }
            timelineComponent->setGridResolution(res);
        }
    };

    addAndMakeVisible(countInButton);
    countInButton.setClickingTogglesState(true);
    countInButton.onClick = [this] { pluginHost.getEngine().toggleCountIn(); };

    addAndMakeVisible(loopButton);
    loopButton.setClickingTogglesState(true);
    loopButton.onClick = [this] { pluginHost.getEngine().toggleLoop(); };

    addAndMakeVisible(panicButton);
    panicButton.onClick = [this] {
        for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
        {
            auto& track = pluginHost.getTrack(t);
            if (track.plugin != nullptr)
            {
                juce::MidiBuffer panic;
                for (int ch = 1; ch <= 16; ++ch)
                {
                    panic.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
                    panic.addEvent(juce::MidiMessage::allSoundOff(ch), 0);
                    panic.addEvent(juce::MidiMessage::allControllersOff(ch), 0);
                }
                juce::AudioBuffer<float> dummy(2, 64);
                dummy.clear();
                track.plugin->processBlock(dummy, panic);
            }
            if (track.clipPlayer)
                track.clipPlayer->sendAllNotesOff.store(true);
        }
        statusLabel.setText("MIDI Panic — all notes off", juce::dontSendNotification);
    };

    addAndMakeVisible(zoomInButton);
    zoomInButton.onClick = [this] { if (timelineComponent) timelineComponent->zoomIn(); };

    addAndMakeVisible(zoomOutButton);
    zoomOutButton.onClick = [this] { if (timelineComponent) timelineComponent->zoomOut(); };

    addAndMakeVisible(scrollLeftButton);
    scrollLeftButton.onClick = [this] { if (timelineComponent) timelineComponent->scrollLeft(); };

    addAndMakeVisible(scrollRightButton);
    scrollRightButton.onClick = [this] { if (timelineComponent) timelineComponent->scrollRight(); };

    addAndMakeVisible(editClipButton);
    editClipButton.onClick = [this] {
        if (timelineComponent)
        {
            auto* clip = timelineComponent->getSelectedClip();
            if (clip != nullptr)
                new PianoRollWindow("Piano Roll", *clip, pluginHost.getEngine());
        }
    };

    // ── Right Panel ──
    addAndMakeVisible(pluginSelector);
    pluginSelector.onChange = [this] { loadSelectedPlugin(); };

    addAndMakeVisible(openEditorButton);
    openEditorButton.onClick = [this] { openPluginEditor(); };
    openEditorButton.setEnabled(false);

    // Preset browser
    addAndMakeVisible(presetPrevButton);
    presetPrevButton.onClick = [this] { changePreset(-1); };

    addAndMakeVisible(presetNameLabel);
    presetNameLabel.setText("No Preset", juce::dontSendNotification);
    presetNameLabel.setJustificationType(juce::Justification::centred);
    presetNameLabel.setFont(juce::Font(11.0f));

    addAndMakeVisible(presetNextButton);
    presetNextButton.onClick = [this] { changePreset(1); };

    addAndMakeVisible(midiInputSelector);
    midiInputSelector.onChange = [this] { selectMidiDevice(); };

    addAndMakeVisible(midiRefreshButton);
    midiRefreshButton.onClick = [this] { scanMidiDevices(); };

    addAndMakeVisible(audioSettingsButton);
    audioSettingsButton.onClick = [this] { showAudioSettings(); };

    addAndMakeVisible(midi2Button);
    midi2Button.setClickingTogglesState(true);
    midi2Button.onClick = [this] {
        midi2Enabled = midi2Button.getToggleState();
        if (midi2Enabled)
        {
            auto& track = pluginHost.getTrack(selectedTrackIndex);
            midi2Handler.setPlugin(track.plugin);

            // Find matching MIDI output for the selected input
            auto midiOutputs = juce::MidiOutput::getAvailableDevices();
            juce::String outputId;

            // Try to find output with matching name
            for (auto& out : midiOutputs)
            {
                for (auto& in : midiDevices)
                {
                    if (in.identifier == currentMidiDeviceId && out.name == in.name)
                    {
                        outputId = out.identifier;
                        break;
                    }
                }
                if (outputId.isNotEmpty()) break;
            }

            // Fallback: try partial name match
            if (outputId.isEmpty())
            {
                for (auto& in : midiDevices)
                {
                    if (in.identifier == currentMidiDeviceId)
                    {
                        for (auto& out : midiOutputs)
                        {
                            if (out.name.containsIgnoreCase("keystage") ||
                                in.name.containsIgnoreCase(out.name.substring(0, 8)))
                            {
                                outputId = out.identifier;
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            // Send Discovery broadcast
            midi2Handler.sendDiscovery();

            // Open MIDI output and keep it open
            auto useId = outputId.isNotEmpty() ? outputId : currentMidiDeviceId;
            midiOutput = juce::MidiOutput::openDevice(useId);
            midiOutputId = useId;

            auto& outgoing = midi2Handler.getOutgoing();
            if (!outgoing.isEmpty() && midiOutput)
            {
                for (const auto metadata : outgoing)
                    midiOutput->sendMessageNow(metadata.getMessage());

                statusLabel.setText("MIDI 2.0: Discovery sent via " + midiOutput->getName(),
                    juce::dontSendNotification);
            }
            else if (!midiOutput)
            {
                statusLabel.setText("MIDI 2.0: No MIDI output found!", juce::dontSendNotification);
            }
            midi2Handler.clearOutgoing();
        }
        else
        {
            midiOutput = nullptr;
            statusLabel.setText("MIDI 2.0 disabled", juce::dontSendNotification);
        }
    };

    testNoteButton.setVisible(false);

    // ── Bottom Bar: Mix Controls ──
    addAndMakeVisible(volumeSlider);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(0.8, juce::dontSendNotification);
    volumeSlider.setSliderStyle(juce::Slider::LinearVertical);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
    volumeSlider.onValueChange = [this] {
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.gainProcessor) track.gainProcessor->volume.store(static_cast<float>(volumeSlider.getValue()));
    };

    addAndMakeVisible(volumeLabel);
    volumeLabel.setJustificationType(juce::Justification::centred);
    volumeLabel.setFont(juce::Font(12.0f));

    addAndMakeVisible(panSlider);
    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setValue(0.0, juce::dontSendNotification);
    panSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    panSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
    panSlider.onValueChange = [this] {
        auto& track = pluginHost.getTrack(selectedTrackIndex);
        if (track.gainProcessor) track.gainProcessor->pan.store(static_cast<float>(panSlider.getValue()));
    };

    addAndMakeVisible(panLabel);
    panLabel.setJustificationType(juce::Justification::centred);
    panLabel.setFont(juce::Font(12.0f));

    addAndMakeVisible(saveButton);
    saveButton.onClick = [this] { saveProject(); };

    addAndMakeVisible(loadButton);
    loadButton.onClick = [this] { loadProject(); };

    addAndMakeVisible(undoButton);
    undoButton.onClick = [this] {
        if (undoIndex > 0)
        {
            undoIndex--;
            restoreSnapshot(undoHistory[undoIndex]);
        }
    };

    addAndMakeVisible(redoButton);
    redoButton.onClick = [this] {
        if (undoIndex < undoHistory.size() - 1)
        {
            undoIndex++;
            restoreSnapshot(undoHistory[undoIndex]);
        }
    };

    // ── Theme Selector ──
    addAndMakeVisible(themeSelector);
    for (int i = 0; i < ThemeManager::NumThemes; ++i)
        themeSelector.addItem(ThemeManager::getThemeName(static_cast<ThemeManager::Theme>(i)), i + 1);
    themeSelector.setSelectedId(ThemeManager::Ice + 1, juce::dontSendNotification);
    themeSelector.onChange = [this] {
        auto idx = themeSelector.getSelectedId() - 1;
        if (idx >= 0 && idx < ThemeManager::NumThemes)
        {
            themeManager.setTheme(static_cast<ThemeManager::Theme>(idx), this);
            applyThemeToControls();
        }
    };

    trackInfoLabel.setVisible(false);

    // Plugin parameter sliders
    for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
    {
        auto* slider = new juce::Slider();
        slider->setRange(0.0, 1.0, 0.001);
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 14);
        slider->setEnabled(false);

        int paramIdx = i; // capture for lambda
        slider->onValueChange = [this, slider] {
            auto& track = pluginHost.getTrack(selectedTrackIndex);
            if (track.plugin == nullptr) return;

            int realIdx = static_cast<int>(slider->getProperties().getWithDefault("paramIndex", -1));
            auto& params = track.plugin->getParameters();
            if (realIdx < 0 || realIdx >= params.size()) return;

            params[realIdx]->setValue(static_cast<float>(slider->getValue()));

            // Record automation if transport is playing + recording
            auto& eng = pluginHost.getEngine();
            if (eng.isPlaying() && eng.isRecording() && !eng.isInCountIn())
            {
                AutomationLane* lane = nullptr;
                for (auto* l : track.automationLanes)
                {
                    if (l->parameterIndex == realIdx) { lane = l; break; }
                }
                if (lane == nullptr)
                {
                    lane = new AutomationLane();
                    lane->parameterIndex = realIdx;
                    lane->parameterName = params[realIdx]->getName(20);
                    track.automationLanes.add(lane);
                }

                AutomationPoint pt;
                pt.beat = eng.getPositionInBeats();
                pt.value = static_cast<float>(slider->getValue());
                lane->points.add(pt);
            }
        };

        addAndMakeVisible(slider);
        paramSliders.add(slider);

        auto* label = new juce::Label();
        label->setJustificationType(juce::Justification::centred);
        label->setFont(juce::Font(9.0f));
        label->setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
        addAndMakeVisible(label);
        paramLabels.add(label);
    }

    addAndMakeVisible(statusLabel);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setFont(juce::Font(12.0f));

    // ── Timeline (arrangement view — always visible) ──
    timelineComponent = std::make_unique<TimelineComponent>(pluginHost);
    timelineComponent->onBeforeEdit = [this] { takeSnapshot(); };
    addAndMakeVisible(*timelineComponent);

    setSize(1280, 800);
    setWantsKeyboardFocus(true);

    scanPlugins();
    scanMidiDevices();
    selectTrack(0);
    updateStatusLabel();

    // Initial undo snapshot
    takeSnapshot();

    // Apply initial theme colors to all controls
    applyThemeToControls();

    startTimerHz(15);
}

MainComponent::~MainComponent()
{
    pluginHost.spectrumDisplay = nullptr;
    pluginHost.lissajousDisplay = nullptr;
    setLookAndFeel(nullptr);  // clear before ThemeManager destructs
    stopTimer();
    disableCurrentMidiDevice();
    closePluginEditor();
    audioPlayer.setProcessor(nullptr);
    deviceManager.removeAudioCallback(&audioPlayer);
}

// ── Timer ────────────────────────────────────────────────────────────────────

void MainComponent::timerCallback()
{
    auto& eng = pluginHost.getEngine();

    if (eng.isInCountIn())
    {
        int barsLeft = static_cast<int>(std::ceil(eng.getCountInBeatsRemaining() / 4.0));
        beatLabel.setText("Count: -" + juce::String(barsLeft), juce::dontSendNotification);
    }
    else
    {
        double beat = eng.getPositionInBeats();
        beatLabel.setText("Beat: " + juce::String(beat, 1), juce::dontSendNotification);
    }

    // Auto-snapshot when recording stops (detect transition)
    static bool wasRecording = false;
    bool isRec = false;
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto* cp = pluginHost.getTrack(t).clipPlayer;
        if (cp != nullptr)
            for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
                if (cp->getSlot(s).state.load() == ClipSlot::Recording)
                    isRec = true;
    }
    if (wasRecording && !isRec)
        takeSnapshot();
    wasRecording = isRec;

    // Sync if timeline changed the selected track or arm state
    int currentSelected = pluginHost.getSelectedTrack();
    if (currentSelected != selectedTrackIndex)
    {
        selectedTrackIndex = currentSelected;
        updateTrackDisplay();
        updateStatusLabel();
    }

}

// ── Track Selection ──────────────────────────────────────────────────────────

void MainComponent::selectTrack(int index)
{
    selectedTrackIndex = juce::jlimit(0, PluginHost::NUM_TRACKS - 1, index);
    pluginHost.setSelectedTrack(selectedTrackIndex);
    closePluginEditor();
    updateTrackDisplay();
    updateStatusLabel();
    pluginSelector.setSelectedId(1, juce::dontSendNotification);
}

void MainComponent::updateTrackDisplay()
{
    auto& track = pluginHost.getTrack(selectedTrackIndex);

    juce::String name = "Track " + juce::String(selectedTrackIndex + 1);
    if (track.plugin != nullptr)
        name += ": " + track.plugin->getName();
    trackNameLabel.setText(name, juce::dontSendNotification);

    openEditorButton.setEnabled(track.plugin != nullptr);
    testNoteButton.setEnabled(track.plugin != nullptr);
    loadPresetList();

    if (track.gainProcessor)
    {
        volumeSlider.setValue(track.gainProcessor->volume.load(), juce::dontSendNotification);
        panSlider.setValue(track.gainProcessor->pan.load(), juce::dontSendNotification);
    }

    // Track info
    juce::String info;
    if (track.plugin != nullptr)
        info += "Plugin: " + track.plugin->getName() + "\n";
    else
        info += "Plugin: (none)\n";

    if (track.clipPlayer != nullptr)
    {
        int clipCount = 0;
        int totalNotes = 0;
        for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
        {
            auto& slot = track.clipPlayer->getSlot(s);
            if (slot.hasContent())
            {
                clipCount++;
                totalNotes += slot.clip->events.getNumEvents() / 2; // note on+off pairs
            }
        }
        info += "Clips: " + juce::String(clipCount) + "\n";
        info += "Notes: " + juce::String(totalNotes) + "\n";
    }

    info += "Armed: " + juce::String(track.clipPlayer && track.clipPlayer->armed.load() ? "Yes" : "No");
    trackInfoLabel.setText(info, juce::dontSendNotification);

    updateParamSliders();

    // Update MIDI 2.0 handler
    if (midi2Enabled)
        midi2Handler.setPlugin(track.plugin);

}

// ── Plugin ───────────────────────────────────────────────────────────────────

void MainComponent::scanPlugins()
{
    statusLabel.setText("Scanning plugins...", juce::dontSendNotification);
    repaint();

    pluginHost.scanForPlugins();

    pluginSelector.clear(juce::dontSendNotification);
    pluginDescriptions.clear();
    pluginSelector.addItem("-- Plugin --", 1);

    int id = 2;
    for (const auto& desc : pluginHost.getPluginList().getTypes())
    {
        if (desc.isInstrument)
        {
            pluginSelector.addItem(desc.name, id);
            pluginDescriptions.add(desc);
            id++;
        }
    }
    pluginSelector.setSelectedId(1, juce::dontSendNotification);
}

void MainComponent::loadSelectedPlugin()
{
    int idx = pluginSelector.getSelectedId() - 2;
    if (idx < 0 || idx >= pluginDescriptions.size()) return;

    closePluginEditor();
    audioPlayer.setProcessor(nullptr);

    juce::String err;
    bool ok = pluginHost.loadPlugin(selectedTrackIndex, pluginDescriptions[idx], err);

    audioPlayer.setProcessor(&pluginHost);

    if (ok)
        updateTrackDisplay();
    else
        statusLabel.setText("Failed: " + err, juce::dontSendNotification);

    updateStatusLabel();
}

void MainComponent::openPluginEditor()
{
    auto& track = pluginHost.getTrack(selectedTrackIndex);
    if (track.plugin == nullptr) return;
    closePluginEditor();
    currentEditor.reset(track.plugin->createEditorIfNeeded());
    if (currentEditor == nullptr) return;
    editorWindow = std::make_unique<PluginEditorWindow>(track.plugin->getName(), currentEditor.get(),
        [this] { closePluginEditor(); });
}

void MainComponent::closePluginEditor()
{
    editorWindow = nullptr;
    currentEditor = nullptr;
}

void MainComponent::playTestNote()
{
    pluginHost.sendTestNoteOn(60, 0.78f);
    juce::Timer::callAfterDelay(500, [this] { pluginHost.sendTestNoteOff(60); });
}

// ── MIDI ─────────────────────────────────────────────────────────────────────

void MainComponent::scanMidiDevices()
{
    midiInputSelector.clear(juce::dontSendNotification);
    midiDevices = juce::MidiInput::getAvailableDevices();
    midiInputSelector.addItem("-- No MIDI --", 1);
    midiInputSelector.addItem("Computer Keyboard", 2);
    int id = 3;
    for (const auto& d : midiDevices) midiInputSelector.addItem(d.name, id++);
    midiInputSelector.setSelectedId(1, juce::dontSendNotification);
}

void MainComponent::selectMidiDevice()
{
    disableCurrentMidiDevice();
    useComputerKeyboard = false;

    int selectedId = midiInputSelector.getSelectedId();

    if (selectedId == 2)
    {
        useComputerKeyboard = true;
        setWantsKeyboardFocus(true);
        grabKeyboardFocus();
        updateStatusLabel();
        return;
    }

    int idx = selectedId - 3;
    if (idx < 0 || idx >= midiDevices.size()) { updateStatusLabel(); return; }
    auto& d = midiDevices[idx];
    deviceManager.setMidiInputDeviceEnabled(d.identifier, true);
    // Route through our callback so we can intercept CI SysEx
    deviceManager.addMidiInputDeviceCallback(d.identifier, this);
    currentMidiDeviceId = d.identifier;
    updateStatusLabel();
}

void MainComponent::handleIncomingMidiMessage(juce::MidiInput* /*source*/, const juce::MidiMessage& msg)
{
    if (midi2Enabled)
    {
        // Route CI SysEx to the handler
        if (midi2Handler.processIncoming(msg))
        {
            // Count and send CI responses back to the Keystage
            auto& outgoing = midi2Handler.getOutgoing();
            int outCount = outgoing.getNumEvents();

            if (!outgoing.isEmpty() && midiOutput)
            {
                for (const auto metadata : outgoing)
                    midiOutput->sendMessageNow(metadata.getMessage());
                midi2Handler.clearOutgoing();
            }

            // Show what CI message was received and how many responses we sent
            juce::String ciInfo;
            {
                auto sdata = msg.getSysExData();
                int ssize = msg.getSysExDataSize();
                int subId = (ssize > 3) ? sdata[3] : 0;
                ciInfo = "CI:0x" + juce::String::toHexString(subId);

                if (subId == 0x34 && ssize > 16)
                {
                    int hdrLen = sdata[14] | (sdata[15] << 7);
                    juce::String hdr;
                    for (int i = 0; i < hdrLen && (16 + i) < ssize; ++i)
                        hdr += juce::String::charToString(static_cast<char>(sdata[16 + i]));
                    ciInfo += " " + hdr;
                }
            }

            juce::MessageManager::callAsync([this, ciInfo, outCount] {
                trackNameLabel.setText(ciInfo + " sent:" + juce::String(outCount),
                    juce::dontSendNotification);
            });

            return; // Don't forward CI SysEx to the audio engine
        }

        // Handle CCs from Keystage knobs (24-31)
        if (msg.isController())
        {
            int cc = msg.getControllerNumber();
            int val = msg.getControllerValue();

            if (cc >= 0 && cc <= 7)
            {
                midi2Handler.handleCC(cc, val);

                // Send OLED updates
                auto& ciOut = midi2Handler.getOutgoing();
                if (!ciOut.isEmpty() && midiOutput)
                {
                    for (const auto metadata : ciOut)
                        midiOutput->sendMessageNow(metadata.getMessage());
                    midi2Handler.clearOutgoing();
                }
            }
        }

        // Handle Keystage transport/nav buttons
        // Log ALL CCs for debugging
        if (msg.isController())
        {
            int tcc = msg.getControllerNumber();
            int tval = msg.getControllerValue();
            int tch = msg.getChannel();

            juce::MessageManager::callAsync([this, tcc, tval, tch] {
                statusLabel.setText("CC" + juce::String(tcc) + "=" + juce::String(tval) + " ch" + juce::String(tch),
                    juce::dontSendNotification);
            });
        }

        // Transport/nav buttons — trigger on any non-zero value (button press)
        if (msg.isController() && msg.getControllerValue() > 0)
        {
            int tcc = msg.getControllerNumber();
            if (tcc == 0x29 || tcc == 41)      // PLAY
            {
                pluginHost.getEngine().play();
                juce::MessageManager::callAsync([this] { playButton.setToggleState(true, juce::dontSendNotification); });
            }
            else if (tcc == 0x2A || tcc == 42) // STOP
            {
                auto& eng = pluginHost.getEngine();
                if (!eng.isPlaying())
                {
                    eng.resetPosition();
                    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
                    {
                        auto* cp = pluginHost.getTrack(t).clipPlayer;
                        if (cp) cp->stopAllSlots();
                    }
                }
                else
                {
                    eng.stop();
                    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
                    {
                        auto* cp = pluginHost.getTrack(t).clipPlayer;
                        if (cp) cp->sendAllNotesOff.store(true);
                    }
                }
                juce::MessageManager::callAsync([this] {
                    if (timelineComponent) timelineComponent->repaint();
                });
            }
            else if (tcc == 0x2D || tcc == 45) // REC
            {
                pluginHost.getEngine().toggleRecord();
                juce::MessageManager::callAsync([this] {
                    recordButton.setToggleState(pluginHost.getEngine().isRecording(), juce::dontSendNotification);
                });
            }
            else if (tcc == 0x2B || tcc == 43) // REW / SHIFT+VALUE LEFT → move playhead back
            {
                auto& eng = pluginHost.getEngine();
                double pos = eng.getPositionInBeats();
                double grid = timelineComponent ? timelineComponent->getGridResolution() : 1.0;
                eng.setPosition(juce::jmax(0.0, pos - grid));
                if (timelineComponent) timelineComponent->repaint();
            }
            else if (tcc == 0x2C || tcc == 44) // FF / SHIFT+VALUE RIGHT → move playhead forward
            {
                auto& eng = pluginHost.getEngine();
                double pos = eng.getPositionInBeats();
                double grid = timelineComponent ? timelineComponent->getGridResolution() : 1.0;
                eng.setPosition(pos + grid);
                if (timelineComponent) timelineComponent->repaint();
            }
            else if (tcc == 0x2E || tcc == 46) // LOOP → (reserved for future)
            {
                // Could toggle loop mode
            }
            else if (tcc == 0x2F || tcc == 47) // TEMPO — toggle metronome
            {
                pluginHost.getEngine().toggleMetronome();
                juce::MessageManager::callAsync([this] {
                    metronomeButton.setToggleState(pluginHost.getEngine().isMetronomeOn(), juce::dontSendNotification);
                });
            }
            else if (tcc == 58 || tcc == 0x3A) // NEXT TRACK
                selectTrack(juce::jmin(PluginHost::NUM_TRACKS - 1, selectedTrackIndex + 1));
            else if (tcc == 59 || tcc == 0x3B) // PREV TRACK
                selectTrack(juce::jmax(0, selectedTrackIndex - 1));
            else if (tcc == 32) // CC32 — Page/Value button → cycle parameter page
            {
                midi2Handler.nextPage();
                auto& ciOut = midi2Handler.getOutgoing();
                if (!ciOut.isEmpty() && midiOutput) { for (const auto metadata : ciOut) midiOutput->sendMessageNow(metadata.getMessage()); midi2Handler.clearOutgoing(); }
                juce::MessageManager::callAsync([this] {
                    trackNameLabel.setText("Page " + juce::String(midi2Handler.getCurrentPage() + 1)
                        + "/" + juce::String(midi2Handler.getNumPages()), juce::dontSendNotification);
                    updateParamSliders();
                });
            }
            else if (tcc == 60 || tcc == 0x3C) // VALUE DOWN → prev preset
            {
                midi2Handler.prevPreset();
                juce::MessageManager::callAsync([this] {
                    auto& trk = pluginHost.getTrack(selectedTrackIndex);
                    if (trk.plugin) trackNameLabel.setText("Preset: " + trk.plugin->getProgramName(trk.plugin->getCurrentProgram()), juce::dontSendNotification);
                });
            }
            else if (tcc == 61 || tcc == 0x3D) // VALUE UP → next preset
            {
                midi2Handler.nextPreset();
                juce::MessageManager::callAsync([this] {
                    auto& trk = pluginHost.getTrack(selectedTrackIndex);
                    if (trk.plugin) trackNameLabel.setText("Preset: " + trk.plugin->getProgramName(trk.plugin->getCurrentProgram()), juce::dontSendNotification);
                });
            }
            else if (tcc == 62 || tcc == 0x3E) // VALUE KNOB LEFT → prev page
            {
                midi2Handler.prevPage();
                auto& ciOut = midi2Handler.getOutgoing();
                if (!ciOut.isEmpty() && midiOutput) { for (const auto metadata : ciOut) midiOutput->sendMessageNow(metadata.getMessage()); midi2Handler.clearOutgoing(); }
                juce::MessageManager::callAsync([this] { trackNameLabel.setText("Page " + juce::String(midi2Handler.getCurrentPage() + 1), juce::dontSendNotification); });
            }
            else if (tcc == 63 || tcc == 0x3F) // VALUE KNOB RIGHT → next page
            {
                midi2Handler.nextPage();
                auto& ciOut = midi2Handler.getOutgoing();
                if (!ciOut.isEmpty() && midiOutput) { for (const auto metadata : ciOut) midiOutput->sendMessageNow(metadata.getMessage()); midi2Handler.clearOutgoing(); }
                juce::MessageManager::callAsync([this] { trackNameLabel.setText("Page " + juce::String(midi2Handler.getCurrentPage() + 1), juce::dontSendNotification); });
            }
        }

        // Auto-reconnect if connection was lost
        if (!midi2Handler.isConnected() && msg.isController())
        {
            midi2Handler.sendDiscovery();
            auto& ciOut = midi2Handler.getOutgoing();
            if (!ciOut.isEmpty() && midiOutput)
            {
                for (const auto metadata : ciOut)
                    midiOutput->sendMessageNow(metadata.getMessage());
                midi2Handler.clearOutgoing();
            }
        }
    }

    // Forward all MIDI to the collector for audio processing
    pluginHost.getMidiCollector().addMessageToQueue(msg);
}

void MainComponent::disableCurrentMidiDevice()
{
    if (currentMidiDeviceId.isNotEmpty())
    {
        deviceManager.removeMidiInputDeviceCallback(currentMidiDeviceId, this);
        deviceManager.setMidiInputDeviceEnabled(currentMidiDeviceId, false);
        currentMidiDeviceId.clear();
    }
}

void MainComponent::showAudioSettings()
{
    auto* sel = new juce::AudioDeviceSelectorComponent(deviceManager, 0, 0, 1, 2, false, false, false, false);
    sel->setSize(500, 400);
    juce::DialogWindow::LaunchOptions opt;
    opt.content.setOwned(sel);
    opt.dialogTitle = "Audio Settings";
    opt.componentToCentreAround = this;
    opt.dialogBackgroundColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    opt.escapeKeyTriggersCloseButton = true;
    opt.useNativeTitleBar = true;
    opt.resizable = false;
    opt.launchAsync();
    juce::Timer::callAfterDelay(500, [this] {
        if (auto* dev = deviceManager.getCurrentAudioDevice())
            pluginHost.setAudioParams(dev->getCurrentSampleRate(), dev->getCurrentBufferSizeSamples());
        updateStatusLabel();
    });
}

void MainComponent::updateStatusLabel()
{
    juce::String text;
    if (useComputerKeyboard)
        text += "KB Oct " + juce::String(computerKeyboardOctave) + " | ";
    else if (currentMidiDeviceId.isNotEmpty())
        for (const auto& d : midiDevices)
            if (d.identifier == currentMidiDeviceId) { text += d.name + " | "; break; }

    if (auto* dev = deviceManager.getCurrentAudioDevice())
        text += dev->getName() + " | " + juce::String(dev->getCurrentSampleRate(), 0) + " Hz";
    statusLabel.setText(text, juce::dontSendNotification);
}

// ── Plugin Parameters ─────────────────────────────────────────────────────────

void MainComponent::updateParamSliders()
{
    auto& track = pluginHost.getTrack(selectedTrackIndex);

    if (track.plugin == nullptr)
    {
        for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
        {
            paramSliders[i]->setEnabled(false);
            paramSliders[i]->setValue(0.0, juce::dontSendNotification);
            paramLabels[i]->setText("", juce::dontSendNotification);
        }
        return;
    }

    auto& allParams = track.plugin->getParameters();
    juce::String pluginName = track.plugin->getName().toLowerCase();

    juce::Array<juce::AudioProcessorParameter*> selectedParams;

    // ── Plugin-specific parameter mappings ──

    // u-he Diva: filter, oscillators, envelope
    if (pluginName.contains("diva"))
    {
        juce::StringArray wanted = { "cutoff", "resonance", "hpf", "vco mix",
                                      "env2 att", "env2 dec" };
        for (auto& w : wanted)
        {
            for (auto* param : allParams)
            {
                if (param->getName(30).toLowerCase().contains(w))
                { selectedParams.add(param); break; }
            }
            if (selectedParams.size() >= NUM_PARAM_SLIDERS) break;
        }
    }
    // u-he Hive: macros then filter
    else if (pluginName.contains("hive"))
    {
        juce::StringArray wanted = { "macro 1", "macro 2", "macro 3", "macro 4",
                                      "cutoff", "resonance" };
        for (auto& w : wanted)
        {
            for (auto* param : allParams)
            {
                if (param->getName(30).toLowerCase().contains(w))
                { selectedParams.add(param); break; }
            }
            if (selectedParams.size() >= NUM_PARAM_SLIDERS) break;
        }
    }
    // Arturia Pigments: macros
    else if (pluginName.contains("pigments"))
    {
        juce::StringArray wanted = { "macro 1", "macro 2", "macro 3",
                                      "macro 4", "macro 5", "macro 6" };
        for (auto& w : wanted)
        {
            for (auto* param : allParams)
            {
                if (param->getName(30).toLowerCase().contains(w))
                { selectedParams.add(param); break; }
            }
            if (selectedParams.size() >= NUM_PARAM_SLIDERS) break;
        }
    }
    // Arturia Analog Lab / any Arturia — look for macros first
    else if (pluginName.contains("analog lab") || pluginName.contains("arturia") ||
             pluginName.contains("jun-6") || pluginName.contains("jup-8") ||
             pluginName.contains("mini v") || pluginName.contains("cs-80"))
    {
        for (auto* param : allParams)
        {
            juce::String name = param->getName(30).toLowerCase();
            if (name.contains("macro") || name.contains("mcr") || name.contains("assign"))
                selectedParams.add(param);
            if (selectedParams.size() >= NUM_PARAM_SLIDERS) break;
        }
    }

    // Generic: try macros, then common synth params
    if (selectedParams.isEmpty())
    {
        for (auto* param : allParams)
        {
            juce::String name = param->getName(30).toLowerCase();
            if (name.contains("macro") || name.contains("mcr") || name.contains("assign"))
                selectedParams.add(param);
            if (selectedParams.size() >= NUM_PARAM_SLIDERS) break;
        }
    }

    if (selectedParams.isEmpty())
    {
        juce::StringArray commonNames = { "cutoff", "filter", "resonance",
                                           "attack", "release", "drive", "mix", "volume" };
        for (auto& cn : commonNames)
        {
            for (auto* param : allParams)
            {
                if (param->getName(30).toLowerCase().contains(cn))
                { selectedParams.add(param); break; }
            }
            if (selectedParams.size() >= NUM_PARAM_SLIDERS) break;
        }
    }

    // Fallback: first N parameters
    if (selectedParams.isEmpty())
    {
        for (int i = 0; i < juce::jmin(NUM_PARAM_SLIDERS, allParams.size()); ++i)
            selectedParams.add(allParams[i]);
    }

    for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
    {
        if (i < selectedParams.size())
        {
            auto* param = selectedParams[i];
            paramSliders[i]->setEnabled(true);
            paramSliders[i]->setValue(param->getValue(), juce::dontSendNotification);
            paramLabels[i]->setText(param->getName(12), juce::dontSendNotification);

            // Store the actual parameter index for the slider callback
            paramSliders[i]->getProperties().set("paramIndex", allParams.indexOf(param));
        }
        else
        {
            paramSliders[i]->setEnabled(false);
            paramSliders[i]->setValue(0.0, juce::dontSendNotification);
            paramLabels[i]->setText("", juce::dontSendNotification);
            paramSliders[i]->getProperties().set("paramIndex", -1);
        }
    }
}

// ── Save/Load/Undo ───────────────────────────────────────────────────────────

void MainComponent::takeSnapshot()
{
    // Trim future history if we undid something
    while (undoHistory.size() > undoIndex + 1)
        undoHistory.removeLast();

    ProjectSnapshot snap;
    snap.bpm = pluginHost.getEngine().getBpm();

    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto* cp = pluginHost.getTrack(t).clipPlayer;
        if (cp == nullptr) continue;

        for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
        {
            auto& slot = cp->getSlot(s);
            if (slot.clip != nullptr && slot.hasContent())
            {
                ProjectSnapshot::ClipData cd;
                cd.trackIndex = t;
                cd.slotIndex = s;
                cd.lengthInBeats = slot.clip->lengthInBeats;
                cd.timelinePosition = slot.clip->timelinePosition;

                for (int e = 0; e < slot.clip->events.getNumEvents(); ++e)
                    cd.events.addEvent(slot.clip->events.getEventPointer(e)->message);
                cd.events.updateMatchedPairs();

                snap.clips.add(std::move(cd));
            }
        }
    }

    undoHistory.add(std::move(snap));
    undoIndex = undoHistory.size() - 1;

    // Limit history
    if (undoHistory.size() > 50)
    {
        undoHistory.remove(0);
        undoIndex--;
    }
}

void MainComponent::restoreSnapshot(const ProjectSnapshot& snap)
{
    // Clear all clips
    for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
    {
        auto* cp = pluginHost.getTrack(t).clipPlayer;
        if (cp == nullptr) continue;

        for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
        {
            auto& slot = cp->getSlot(s);
            slot.clip = nullptr;
            slot.state.store(ClipSlot::Empty);
        }
    }

    pluginHost.getEngine().setBpm(snap.bpm);
    bpmLabel.setText(juce::String(static_cast<int>(snap.bpm)) + " BPM", juce::dontSendNotification);

    // Restore clips
    for (auto& cd : snap.clips)
    {
        auto* cp = pluginHost.getTrack(cd.trackIndex).clipPlayer;
        if (cp == nullptr) continue;

        auto& slot = cp->getSlot(cd.slotIndex);
        slot.clip = std::make_unique<MidiClip>();
        slot.clip->lengthInBeats = cd.lengthInBeats;
        slot.clip->timelinePosition = cd.timelinePosition;

        for (int e = 0; e < cd.events.getNumEvents(); ++e)
            slot.clip->events.addEvent(cd.events.getEventPointer(e)->message);
        slot.clip->events.updateMatchedPairs();

        slot.state.store(ClipSlot::Playing);
    }

    updateTrackDisplay();
    if (timelineComponent) timelineComponent->repaint();
}

void MainComponent::saveProject()
{
    auto chooser = std::make_shared<juce::FileChooser>("Save Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.seqproj");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File()) return;
        auto xml = std::make_unique<juce::XmlElement>("SequencerProject");
        xml->setAttribute("bpm", pluginHost.getEngine().getBpm());

        for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
        {
            auto& track = pluginHost.getTrack(t);

            auto* trackXml = xml->createNewChildElement("Track");
            trackXml->setAttribute("index", t);

            if (track.gainProcessor)
            {
                trackXml->setAttribute("volume", static_cast<double>(track.gainProcessor->volume.load()));
                trackXml->setAttribute("pan", static_cast<double>(track.gainProcessor->pan.load()));
                trackXml->setAttribute("muted", track.gainProcessor->muted.load());
                trackXml->setAttribute("soloed", track.gainProcessor->soloed.load());
            }

            if (track.plugin)
                trackXml->setAttribute("pluginName", track.plugin->getName());

            auto* cp = track.clipPlayer;
            if (cp == nullptr) continue;

            for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
            {
                auto& slot = cp->getSlot(s);
                if (slot.clip == nullptr || !slot.hasContent()) continue;

                auto* clipXml = trackXml->createNewChildElement("Clip");
                clipXml->setAttribute("slot", s);
                clipXml->setAttribute("length", slot.clip->lengthInBeats);
                clipXml->setAttribute("position", slot.clip->timelinePosition);

                for (int e = 0; e < slot.clip->events.getNumEvents(); ++e)
                {
                    auto* event = slot.clip->events.getEventPointer(e);
                    auto* noteXml = clipXml->createNewChildElement("Event");
                    noteXml->setAttribute("time", event->message.getTimeStamp());
                    noteXml->setAttribute("data", juce::String::toHexString(
                        event->message.getRawData(), event->message.getRawDataSize()));
                }
            }
        }

        xml->writeTo(file);
        statusLabel.setText("Saved: " + file.getFileName(), juce::dontSendNotification);
    });
}

void MainComponent::loadProject()
{
    auto chooser = std::make_shared<juce::FileChooser>("Load Project",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.seqproj");

    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File()) return;

        auto xml = juce::parseXML(file);

        if (xml == nullptr || !xml->hasTagName("SequencerProject"))
        {
            statusLabel.setText("Invalid project file", juce::dontSendNotification);
            return;
        }

        // Clear all clips first
        for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
        {
            auto* cp = pluginHost.getTrack(t).clipPlayer;
            if (cp == nullptr) continue;
            for (int s = 0; s < ClipPlayerNode::NUM_SLOTS; ++s)
            {
                cp->getSlot(s).clip = nullptr;
                cp->getSlot(s).state.store(ClipSlot::Empty);
            }
        }

        double bpm = xml->getDoubleAttribute("bpm", 120.0);
        pluginHost.getEngine().setBpm(bpm);
        bpmLabel.setText(juce::String(static_cast<int>(bpm)) + " BPM", juce::dontSendNotification);

        for (auto* trackXml : xml->getChildWithTagNameIterator("Track"))
        {
            int t = trackXml->getIntAttribute("index", -1);
            if (t < 0 || t >= PluginHost::NUM_TRACKS) continue;

            auto& track = pluginHost.getTrack(t);

            if (track.gainProcessor)
            {
                track.gainProcessor->volume.store(static_cast<float>(trackXml->getDoubleAttribute("volume", 0.8)));
                track.gainProcessor->pan.store(static_cast<float>(trackXml->getDoubleAttribute("pan", 0.0)));
                track.gainProcessor->muted.store(trackXml->getBoolAttribute("muted", false));
                track.gainProcessor->soloed.store(trackXml->getBoolAttribute("soloed", false));
            }

            auto* cp = track.clipPlayer;
            if (cp == nullptr) continue;

            for (auto* clipXml : trackXml->getChildWithTagNameIterator("Clip"))
            {
                int s = clipXml->getIntAttribute("slot", -1);
                if (s < 0 || s >= ClipPlayerNode::NUM_SLOTS) continue;

                auto& slot = cp->getSlot(s);
                slot.clip = std::make_unique<MidiClip>();
                slot.clip->lengthInBeats = clipXml->getDoubleAttribute("length", 4.0);
                slot.clip->timelinePosition = clipXml->getDoubleAttribute("position", 0.0);

                for (auto* noteXml : clipXml->getChildWithTagNameIterator("Event"))
                {
                    double time = noteXml->getDoubleAttribute("time", 0.0);
                    auto hexData = noteXml->getStringAttribute("data");

                    juce::MemoryBlock mb;
                    mb.loadFromHexString(hexData);

                    if (mb.getSize() > 0)
                    {
                        auto msg = juce::MidiMessage(mb.getData(), static_cast<int>(mb.getSize()));
                        msg.setTimeStamp(time);
                        slot.clip->events.addEvent(msg);
                    }
                }

                slot.clip->events.updateMatchedPairs();
                slot.state.store(ClipSlot::Playing);
            }
        }

        updateTrackDisplay();
        if (timelineComponent) timelineComponent->repaint();
        statusLabel.setText("Loaded: " + file.getFileName(), juce::dontSendNotification);

        // Take snapshot for undo
        takeSnapshot();
    });
}

// ── Layout ───────────────────────────────────────────────────────────────────

void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
}

void MainComponent::paint(juce::Graphics& g)
{
    auto& c = themeManager.getColors();

    // Main body
    g.fillAll(juce::Colour(c.body));

    // Top bar background
    g.setColour(juce::Colour(c.bodyLight));
    g.fillRect(0, 0, getWidth(), 50);

    // Toolbar background
    g.setColour(juce::Colour(c.bodyDark));
    g.fillRect(0, 50, getWidth(), 40);

    // Panel dividers
    g.setColour(juce::Colour(c.border));
    g.drawHorizontalLine(50, 0, static_cast<float>(getWidth()));
    g.drawHorizontalLine(90, 0, static_cast<float>(getWidth()));

    // Bottom bar
    auto bottomY = getHeight() - 45;
    g.setColour(juce::Colour(c.bodyLight));
    g.fillRect(0, bottomY, getWidth(), 45);
    g.setColour(juce::Colour(c.border));
    g.drawHorizontalLine(bottomY, 0, static_cast<float>(getWidth()));

    // Accent stripe at top
    g.setColour(juce::Colour(c.accentStripe));
    g.fillRect(0, 0, getWidth(), 2);

    // Right panel border
    int rightPanelX = getWidth() - 200;
    g.setColour(juce::Colour(c.border));
    g.drawVerticalLine(rightPanelX, 50, static_cast<float>(bottomY));
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    int topBarH = 50;
    int bottomBarH = 45;
    int rightPanelW = 200;

    // ── Top Bar ──
    auto topBar = area.removeFromTop(topBarH).reduced(6, 6);

    prevTrackButton.setBounds(topBar.removeFromLeft(40));
    topBar.removeFromLeft(4);
    nextTrackButton.setBounds(topBar.removeFromLeft(40));
    topBar.removeFromLeft(8);
    trackNameLabel.setBounds(topBar.removeFromLeft(200));
    topBar.removeFromLeft(8);

    recordButton.setBounds(topBar.removeFromLeft(55));
    topBar.removeFromLeft(4);
    playButton.setBounds(topBar.removeFromLeft(60));
    topBar.removeFromLeft(4);
    stopButton.setBounds(topBar.removeFromLeft(55));
    topBar.removeFromLeft(4);
    metronomeButton.setBounds(topBar.removeFromLeft(45));
    topBar.removeFromLeft(4);
    countInButton.setBounds(topBar.removeFromLeft(75));
    topBar.removeFromLeft(4);
    loopButton.setBounds(topBar.removeFromLeft(55));
    topBar.removeFromLeft(4);
    panicButton.setBounds(topBar.removeFromLeft(55));
    topBar.removeFromLeft(8);

    beatLabel.setBounds(topBar.removeFromRight(90));
    topBar.removeFromRight(4);
    bpmUpButton.setBounds(topBar.removeFromRight(38));
    topBar.removeFromRight(2);
    bpmLabel.setBounds(topBar.removeFromRight(70));
    topBar.removeFromRight(2);
    bpmDownButton.setBounds(topBar.removeFromRight(38));
    topBar.removeFromRight(4);

    // Spectrum analyzer fills remaining top bar space
    if (topBar.getWidth() > 40)
        spectrumDisplay.setBounds(topBar);

    // ── Bottom Bar ──
    auto bottomBar = area.removeFromBottom(bottomBarH).reduced(8, 6);

    scrollLeftButton.setBounds(bottomBar.removeFromLeft(40));
    bottomBar.removeFromLeft(2);
    scrollRightButton.setBounds(bottomBar.removeFromLeft(40));
    bottomBar.removeFromLeft(4);
    zoomOutButton.setBounds(bottomBar.removeFromLeft(60));
    bottomBar.removeFromLeft(2);
    zoomInButton.setBounds(bottomBar.removeFromLeft(60));
    bottomBar.removeFromLeft(8);
    statusLabel.setBounds(bottomBar);

    // ── Edit Toolbar ──
    auto toolbar = area.removeFromTop(40).reduced(6, 4);
    newClipButton.setBounds(toolbar.removeFromLeft(90));
    toolbar.removeFromLeft(4);
    deleteClipButton.setBounds(toolbar.removeFromLeft(75));
    toolbar.removeFromLeft(4);
    duplicateClipButton.setBounds(toolbar.removeFromLeft(90));
    toolbar.removeFromLeft(4);
    splitClipButton.setBounds(toolbar.removeFromLeft(65));
    toolbar.removeFromLeft(4);
    editClipButton.setBounds(toolbar.removeFromLeft(90));
    toolbar.removeFromLeft(4);
    quantizeButton.setBounds(toolbar.removeFromLeft(80));
    toolbar.removeFromLeft(8);
    gridSelector.setBounds(toolbar.removeFromLeft(70));
    toolbar.removeFromLeft(8);
    saveButton.setBounds(toolbar.removeFromLeft(55));
    toolbar.removeFromLeft(4);
    loadButton.setBounds(toolbar.removeFromLeft(55));
    toolbar.removeFromLeft(4);
    undoButton.setBounds(toolbar.removeFromLeft(55));
    toolbar.removeFromLeft(4);
    redoButton.setBounds(toolbar.removeFromLeft(55));
    toolbar.removeFromLeft(8);
    themeSelector.setBounds(toolbar.removeFromLeft(90));

    // ── Right Panel ──
    auto rightPanel = area.removeFromRight(rightPanelW).reduced(8, 4);

    // Lissajous stereo field at top
    lissajousDisplay.setBounds(rightPanel.removeFromTop(100));
    rightPanel.removeFromTop(4);

    pluginSelector.setBounds(rightPanel.removeFromTop(30));
    rightPanel.removeFromTop(4);
    openEditorButton.setBounds(rightPanel.removeFromTop(28));
    rightPanel.removeFromTop(4);
    auto midiRow = rightPanel.removeFromTop(26);
    midiRefreshButton.setBounds(midiRow.removeFromRight(55));
    midiRow.removeFromRight(4);
    midiInputSelector.setBounds(midiRow);
    rightPanel.removeFromTop(4);

    // Preset browser row
    auto presetRow = rightPanel.removeFromTop(28);
    presetPrevButton.setBounds(presetRow.removeFromLeft(28));
    presetRow.removeFromLeft(2);
    presetNextButton.setBounds(presetRow.removeFromRight(28));
    presetRow.removeFromRight(2);
    presetNameLabel.setBounds(presetRow);
    rightPanel.removeFromTop(4);

    // Plugin parameter knobs — 3x2 grid, compact
    if (paramSliders.size() > 0)
    {
        int knobSize = juce::jmin(54, (rightPanel.getWidth() - 8) / 3);
        int knobAreaH = 2 * (knobSize + 14) + 4;
        auto knobArea = rightPanel.removeFromTop(knobAreaH);
        rightPanel.removeFromTop(4);

        for (int i = 0; i < NUM_PARAM_SLIDERS; ++i)
        {
            int col = i % 3;
            int row = i / 3;
            int kx = knobArea.getX() + col * (knobSize + 4);
            int ky = knobArea.getY() + row * (knobSize + 14 + 2);

            paramLabels[i]->setBounds(kx, ky, knobSize, 12);
            paramSliders[i]->setBounds(kx, ky + 12, knobSize, knobSize);
        }
    }

    // Volume/Pan — compact side by side
    auto mixArea = rightPanel.removeFromTop(juce::jmin(100, rightPanel.getHeight() / 3));
    auto volArea = mixArea.removeFromLeft(mixArea.getWidth() / 2);
    auto panArea = mixArea;

    volumeLabel.setBounds(volArea.removeFromTop(14));
    volumeSlider.setBounds(volArea);

    panLabel.setBounds(panArea.removeFromTop(14));
    panSlider.setBounds(panArea.reduced(8, 0));
    rightPanel.removeFromTop(4);

    audioSettingsButton.setBounds(rightPanel.removeFromTop(28));
    rightPanel.removeFromTop(4);
    midi2Button.setBounds(rightPanel.removeFromTop(28));

    // ── Timeline fills the rest ──
    area.reduce(2, 2);
    if (timelineComponent)
        timelineComponent->setBounds(area);
}

// ── Keyboard ─────────────────────────────────────────────────────────────────

int MainComponent::keyToNote(int keyCode) const
{
    switch (keyCode)
    {
        case 'A': return 0;  case 'W': return 1;  case 'S': return 2;  case 'E': return 3;
        case 'D': return 4;  case 'F': return 5;  case 'T': return 6;  case 'G': return 7;
        case 'Y': return 8;  case 'H': return 9;  case 'U': return 10; case 'J': return 11;
        case 'K': return 12; case 'O': return 13; case 'L': return 14; case 'P': return 15;
        default: return -1;
    }
}

void MainComponent::sendNoteOn(int note)
{
    auto msg = juce::MidiMessage::noteOn(1, note, 0.8f);
    msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    pluginHost.getMidiCollector().addMessageToQueue(msg);
}

void MainComponent::sendNoteOff(int note)
{
    auto msg = juce::MidiMessage::noteOff(1, note);
    msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    pluginHost.getMidiCollector().addMessageToQueue(msg);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    // Spacebar = play/stop
    if (key == juce::KeyPress::spaceKey)
    {
        auto& eng = pluginHost.getEngine();
        if (eng.isPlaying())
        {
            eng.stop();
            for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
            {
                auto* cp = pluginHost.getTrack(t).clipPlayer;
                if (cp) cp->sendAllNotesOff.store(true);
            }
        }
        else
        {
            double timeSinceLastStop = juce::Time::getMillisecondCounterHiRes() - lastSpaceStopTime;
            if (timeSinceLastStop < 400.0)
            {
                eng.resetPosition();
                for (int t = 0; t < PluginHost::NUM_TRACKS; ++t)
                {
                    auto* cp = pluginHost.getTrack(t).clipPlayer;
                    if (cp) cp->stopAllSlots();
                }
                if (timelineComponent) timelineComponent->repaint();
            }
            else
            {
                eng.play();
            }
        }
        lastSpaceStopTime = eng.isPlaying() ? 0.0 : juce::Time::getMillisecondCounterHiRes();
        return true;
    }

    // Arrow keys = switch tracks
    if (key == juce::KeyPress::leftKey) { selectTrack(selectedTrackIndex - 1); return true; }
    if (key == juce::KeyPress::rightKey) { selectTrack(selectedTrackIndex + 1); return true; }

    if (!useComputerKeyboard) return false;

    int keyCode = key.getTextCharacter();
    if (keyCode >= 'a' && keyCode <= 'z') keyCode -= 32;

    if (keyCode == 'Z') { computerKeyboardOctave = juce::jmax(0, computerKeyboardOctave - 1); updateStatusLabel(); return true; }
    if (keyCode == 'X') { computerKeyboardOctave = juce::jmin(8, computerKeyboardOctave + 1); updateStatusLabel(); return true; }

    return false;
}

bool MainComponent::keyStateChanged(bool /*isKeyDown*/)
{
    if (!useComputerKeyboard) return false;

    const int mappedKeys[] = { 'A','W','S','E','D','F','T','G','Y','H','U','J','K','O','L','P' };

    for (int keyCode : mappedKeys)
    {
        bool isDown = juce::KeyPress::isKeyCurrentlyDown(keyCode);
        int semitone = keyToNote(keyCode);
        if (semitone < 0) continue;

        int midiNote = (computerKeyboardOctave * 12) + semitone;
        if (midiNote < 0 || midiNote > 127) continue;

        bool wasDown = keysCurrentlyDown.count(keyCode) > 0;

        if (isDown && !wasDown) { keysCurrentlyDown.insert(keyCode); sendNoteOn(midiNote); }
        else if (!isDown && wasDown) { keysCurrentlyDown.erase(keyCode); sendNoteOff(midiNote); }
    }
    return true;
}

void MainComponent::applyThemeToControls()
{
    auto& c = themeManager.getColors();
    auto fontName = themeManager.getLookAndFeel()->getUIFontName();

    // Labels
    trackNameLabel.setColour(juce::Label::textColourId, juce::Colour(c.amber));
    trackNameLabel.setFont(juce::Font(fontName, 16.0f, juce::Font::bold));
    beatLabel.setFont(juce::Font(fontName, 16.0f, juce::Font::bold));
    beatLabel.setColour(juce::Label::textColourId, juce::Colour(c.lcdText));
    beatLabel.setColour(juce::Label::backgroundColourId, juce::Colour(c.lcdBg));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(c.textSecondary));
    trackInfoLabel.setColour(juce::Label::textColourId, juce::Colour(c.textSecondary));

    // BPM controls
    bpmLabel.setFont(juce::Font(fontName, 14.0f, juce::Font::bold));
    bpmLabel.setColour(juce::Label::textColourId, juce::Colour(c.lcdText));
    bpmLabel.setColour(juce::Label::backgroundColourId, juce::Colour(c.lcdBg));
    bpmDownButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    bpmUpButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));

    // Transport
    recordButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.redDark));
    recordButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.red));
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.greenDark));
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnStop));
    metronomeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnMetronome));
    metronomeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.btnMetronomeOn));
    countInButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnCountIn));
    countInButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.btnCountInOn));
    loopButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnLoop));
    loopButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.btnLoopOn));
    panicButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffdd6600));

    // Edit toolbar
    newClipButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNewClip));
    deleteClipButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnDeleteClip));
    duplicateClipButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnDuplicate));
    splitClipButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnSplit));
    quantizeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnQuantize));
    editClipButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnEditNotes));

    // Navigation
    zoomInButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    zoomOutButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    scrollLeftButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    scrollRightButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));

    // Right panel
    midiRefreshButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    midi2Button.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnMidi2));
    midi2Button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(c.btnMidi2On));
    saveButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnSave));
    loadButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnLoad));
    undoButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnUndoRedo));
    redoButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnUndoRedo));

    // Preset browser
    presetPrevButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    presetNextButton.setColour(juce::TextButton::buttonColourId, juce::Colour(c.btnNav));
    presetNameLabel.setColour(juce::Label::textColourId, juce::Colour(c.textPrimary));
    presetNameLabel.setFont(juce::Font(fontName, 11.0f, juce::Font::plain));

    repaint();
}

void MainComponent::loadPresetList()
{
    presetNames.clear();
    currentPresetIndex = 0;
    presetParamIndex = -1;

    auto& track = pluginHost.getTrack(selectedTrackIndex);
    if (track.plugin == nullptr)
    {
        presetNameLabel.setText("No Plugin", juce::dontSendNotification);
        return;
    }

    auto& params = track.plugin->getParameters();

    // Strategy 1: look for a discrete "Preset" / "Program" parameter
    // Arturia and many VST3 plugins expose presets this way
    for (int i = 0; i < params.size(); ++i)
    {
        juce::String pname = params[i]->getName(50).toLowerCase();
        if (pname.contains("preset") || pname == "program" || pname == "patch" ||
            pname.contains("browser") || pname.contains("prog"))
        {
            int numSteps = params[i]->getNumSteps();
            if (numSteps > 1 && numSteps < 10000)
            {
                presetParamIndex = i;
                for (int s = 0; s < numSteps; ++s)
                {
                    float normVal = static_cast<float>(s) / static_cast<float>(numSteps - 1);
                    auto name = params[i]->getText(normVal, 80);
                    if (name.isEmpty()) name = "Preset " + juce::String(s + 1);
                    presetNames.add(name);
                }
                float curVal = params[i]->getValue();
                currentPresetIndex = juce::roundToInt(curVal * (numSteps - 1));
                break;
            }
        }
    }

    // Strategy 2: check if getNumPrograms() returns real presets
    if (presetNames.isEmpty())
    {
        int numPrograms = track.plugin->getNumPrograms();
        if (numPrograms > 1)
        {
            for (int i = 0; i < numPrograms; ++i)
            {
                auto name = track.plugin->getProgramName(i);
                if (name.isEmpty()) name = "Preset " + juce::String(i + 1);
                presetNames.add(name);
            }
            currentPresetIndex = track.plugin->getCurrentProgram();
        }
    }

    // Strategy 3: if nothing found, log all param names to help debug
    if (presetNames.isEmpty())
    {
        DBG("=== No presets found. Plugin params for: " + track.plugin->getName() + " ===");
        for (int i = 0; i < juce::jmin(params.size(), 30); ++i)
            DBG("  [" + juce::String(i) + "] " + params[i]->getName(60) +
                " (steps=" + juce::String(params[i]->getNumSteps()) + ")");

        // Last resort: use program API even if only 1 program
        int numPrograms = track.plugin->getNumPrograms();
        if (numPrograms >= 1)
        {
            for (int i = 0; i < numPrograms; ++i)
            {
                auto name = track.plugin->getProgramName(i);
                if (name.isEmpty()) name = "Preset " + juce::String(i + 1);
                presetNames.add(name);
            }
            currentPresetIndex = track.plugin->getCurrentProgram();
        }
    }

    if (presetNames.isEmpty())
        presetNameLabel.setText("No Presets", juce::dontSendNotification);
    else
    {
        currentPresetIndex = juce::jlimit(0, presetNames.size() - 1, currentPresetIndex);
        presetNameLabel.setText(presetNames[currentPresetIndex], juce::dontSendNotification);
    }
}

void MainComponent::changePreset(int delta)
{
    auto& track = pluginHost.getTrack(selectedTrackIndex);
    if (track.plugin == nullptr || presetNames.isEmpty()) return;

    currentPresetIndex += delta;
    if (currentPresetIndex < 0) currentPresetIndex = presetNames.size() - 1;
    if (currentPresetIndex >= presetNames.size()) currentPresetIndex = 0;

    if (presetParamIndex >= 0)
    {
        // Change via the preset parameter
        auto& params = track.plugin->getParameters();
        if (presetParamIndex < params.size())
        {
            int numSteps = params[presetParamIndex]->getNumSteps();
            if (numSteps > 1)
            {
                float normVal = static_cast<float>(currentPresetIndex) / static_cast<float>(numSteps - 1);
                params[presetParamIndex]->setValue(normVal);

                // Update the label from the parameter's text (may differ from our stored name)
                auto newName = params[presetParamIndex]->getText(normVal, 80);
                if (newName.isNotEmpty())
                    presetNameLabel.setText(newName, juce::dontSendNotification);
                else
                    presetNameLabel.setText(presetNames[currentPresetIndex], juce::dontSendNotification);
            }
        }
    }
    else
    {
        // Change via program API + send MIDI program change
        track.plugin->setCurrentProgram(currentPresetIndex);

        // Also send MIDI program change in case the plugin responds to that
        juce::MidiBuffer pc;
        pc.addEvent(juce::MidiMessage::programChange(1, currentPresetIndex % 128), 0);
        juce::AudioBuffer<float> dummy(2, 64);
        dummy.clear();
        track.plugin->processBlock(dummy, pc);

        presetNameLabel.setText(presetNames[currentPresetIndex], juce::dontSendNotification);
    }

    updateParamSliders();
}
