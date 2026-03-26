#include "SequencerEngine.h"

SequencerEngine::SequencerEngine() {}

void SequencerEngine::play()
{
    playing.store(true);
}

void SequencerEngine::stop()
{
    playing.store(false);
    recording.store(false);
    // Don't reset position — double-click stop does that
}

void SequencerEngine::toggleRecord()
{
    recording.store(!recording.load());
}

void SequencerEngine::setBpm(double newBpm)
{
    bpm.store(juce::jlimit(20.0, 300.0, newBpm));
}

double SequencerEngine::advancePosition(int numSamples, double sampleRate)
{
    if (!playing.load())
        return 0.0;

    double currentBpm = bpm.load();
    double beatsPerSecond = currentBpm / 60.0;
    double beatsThisBlock = beatsPerSecond * (static_cast<double>(numSamples) / sampleRate);

    double oldPos = positionInBeats.load();
    double newPos = oldPos + beatsThisBlock;

    // Check if we crossed a beat boundary — trigger metronome click
    if (metronomeEnabled.load())
    {
        int oldBeat = static_cast<int>(std::floor(oldPos));
        int newBeat = static_cast<int>(std::floor(newPos));

        if (newBeat > oldBeat)
        {
            // Crossed a beat — start a click
            bool isDownbeat = (newBeat % 4) == 0;
            clickFrequency = isDownbeat ? 1500.0 : 1000.0;
            clickSamplesRemaining = static_cast<int>(sampleRate * 0.02); // 20ms click
            clickPhase = 0.0;
        }
    }

    positionInBeats.store(newPos);
    return beatsThisBlock;
}

void SequencerEngine::toggleMetronome()
{
    metronomeEnabled.store(!metronomeEnabled.load());
}

void SequencerEngine::renderMetronome(juce::AudioBuffer<float>& buffer, int numSamples, double sampleRate)
{
    if (clickSamplesRemaining <= 0) return;

    int samplesToRender = juce::jmin(clickSamplesRemaining, numSamples);
    double phaseInc = juce::MathConstants<double>::twoPi * clickFrequency / sampleRate;

    for (int s = 0; s < samplesToRender; ++s)
    {
        // Sine click with fast envelope decay
        double envelope = static_cast<double>(clickSamplesRemaining - s) /
                          static_cast<double>(clickSamplesRemaining + samplesToRender);
        float sample = static_cast<float>(std::sin(clickPhase) * envelope * 0.4);
        clickPhase += phaseInc;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addSample(ch, s, sample);
    }

    clickSamplesRemaining -= samplesToRender;
}

void SequencerEngine::resetPosition()
{
    positionInBeats.store(0.0);
    clickSamplesRemaining = 0;
}
