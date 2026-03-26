#pragma once

#include <JuceHeader.h>

// Scans Arturia preset folders and provides a list of preset names.
// Loads presets by name by finding the matching file and loading it
// through the plugin's state system.
class ArturiaPresetScanner
{
public:
    struct PresetInfo {
        juce::String name;
        juce::File file;
    };

    // Scan presets for a given plugin name (e.g. "Pigments", "Jun-6 V")
    static juce::Array<PresetInfo> scanPresets(const juce::String& pluginName)
    {
        juce::Array<PresetInfo> presets;

        // Try common Arturia preset paths
        juce::File presetDir("C:/ProgramData/Arturia/Presets/" + pluginName + "/Factory/Factory");

        if (!presetDir.isDirectory())
        {
            // Try without double Factory
            presetDir = juce::File("C:/ProgramData/Arturia/Presets/" + pluginName + "/Factory");
        }

        if (!presetDir.isDirectory())
        {
            // Try the base preset dir
            presetDir = juce::File("C:/ProgramData/Arturia/Presets/" + pluginName);
        }

        if (presetDir.isDirectory())
        {
            auto files = presetDir.findChildFiles(juce::File::findFiles, false);
            files.sort();

            for (auto& f : files)
            {
                // Skip xml/config files
                if (f.getFileExtension().isNotEmpty()) continue;

                PresetInfo p;
                p.name = f.getFileName();
                p.file = f;
                presets.add(p);
            }
        }

        return presets;
    }

    // Load a preset file into a plugin via state
    static bool loadPreset(juce::AudioProcessor* plugin, const juce::File& presetFile)
    {
        if (plugin == nullptr || !presetFile.existsAsFile()) return false;

        juce::MemoryBlock data;
        if (!presetFile.loadFileAsData(data)) return false;

        // Try setting the state — Arturia plugins may accept their own format
        plugin->setStateInformation(data.getData(), static_cast<int>(data.getSize()));
        return true;
    }
};
