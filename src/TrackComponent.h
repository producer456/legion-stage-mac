#pragma once

#include <JuceHeader.h>
#include <functional>

class TrackComponent : public juce::Component
{
public:
    TrackComponent(int trackIndex);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    void setSelected(bool selected);
    void setPluginName(const juce::String& name);
    float getVolume() const;
    bool isMuted() const;
    bool isSoloed() const;

    // Callbacks — set by MainComponent
    std::function<void(int)> onSelected;           // track index
    std::function<void(int, float)> onVolumeChanged; // track index, value
    std::function<void(int, bool)> onMuteChanged;    // track index, state
    std::function<void(int, bool)> onSoloChanged;    // track index, state

private:
    int index;
    bool selected = false;

    juce::Label trackLabel;
    juce::Label pluginLabel;
    juce::Slider volumeSlider;
    juce::TextButton muteButton { "M" };
    juce::TextButton soloButton { "S" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackComponent)
};
