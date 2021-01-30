/*
  ==============================================================================

   This file is part of the JUCE tutorials.
   Copyright (c) 2020 - Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             SynthUsingMidiInputTutorial
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Synthesiser with midi input.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_processors, juce_audio_utils, juce_core,
                   juce_data_structures, juce_events, juce_graphics,
                   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2019, linux_make

 type:             Component
 mainClass:        MainContentComponent

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/
#include <string>

#pragma once
//==============================================================================
class WavetableOscillator
{
public:
	WavetableOscillator (const juce::AudioSampleBuffer& wavetableToUse)
		: wavetable (wavetableToUse),
		tableSize (wavetable.getNumSamples() - 1)
	{
		jassert (wavetable.getNumChannels() == 1);
	}
	
	void setFrequency (float frequency, float sampleRate)
	{
		auto tableSizeOverSampleRate = (float) tableSize / sampleRate;
		tableDelta = frequency * tableSizeOverSampleRate;
	}
	
	forcedinline float getNextSample() noexcept
	{
		auto index0 = (unsigned int) currentIndex;
		auto index1 = index0 + 1;
		
		//auto frac = currentIndex - (float) index0;
		
		auto* table = wavetable.getReadPointer (0);
		auto value0 = table[index0];
		auto value1 = table[index1];
		
		auto currentSample = value0 + 1 * (value1 - value0);
		
		if ((currentIndex += tableDelta) > (float) tableSize)
			currentIndex -= (float) tableSize;
		
		return currentSample;
	}
	
private:
	const juce::AudioSampleBuffer& wavetable;
	const int tableSize;
	float currentIndex = 0.0f, tableDelta = 0.0f;
};

//==============================================================================
struct SineWaveSound   : public juce::SynthesiserSound
{

    SineWaveSound() {
		createWavetable();
	}

    bool appliesToNote    (int) override        { return true; }
    bool appliesToChannel (int) override        { return true; }
	juce::AudioSampleBuffer *getWaveTable() { return &waveTable; }
	
private:
	void createWavetable()
	{
		waveTable.setSize (1, (int) tableSize + 1);
		waveTable.clear();
	
		auto* samples = waveTable.getWritePointer (0);

		int harmonics[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
		//float harmonicWeights[] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
		
		//jassert (juce::numElementsInArray (harmonics) == juce::numElementsInArray (harmonicWeights));
		
		for (auto harmonic = 0; harmonic < juce::numElementsInArray (harmonics); ++harmonic)
		{
			auto angleDelta = juce::MathConstants<double>::twoPi / (double) (tableSize - 1) * harmonics[harmonic];
			auto currentAngle = 0.0;

			for (unsigned int i = 0; i < tableSize; ++i)
			{
				auto sample = std::sin (currentAngle);
				samples[i] += (float) sample / (harmonic + 1);
				currentAngle += angleDelta;
			}
			samples[tableSize] = samples[0];
			}
	}

	juce::AudioSampleBuffer waveTable;
	const unsigned int tableSize = 1 << 24;
	
};

//==============================================================================
struct SineWaveVoice   : public juce::SynthesiserVoice
{
    SineWaveVoice() {}

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SineWaveSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound* sound, int /*currentPitchWheelPosition*/) override
    {
        level = velocity * 0.025;
        tailOff = 0.0;
		
		auto sineWaveSound = dynamic_cast<SineWaveSound*> (sound);
		juce::AudioSampleBuffer *waveTable = sineWaveSound->getWaveTable();
		
		osc = new WavetableOscillator(*waveTable);

        auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);

		osc->setFrequency ((float) cyclesPerSecond, (float) getSampleRate());
		notePlaying = true;
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            if (tailOff == 0.0)
                tailOff = 1.0;
        }
        else
        {
            clearCurrentNote();
			notePlaying = false;
        }
    }

    void pitchWheelMoved (int) override      {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override
    {
		if (notePlaying) {
			if (tailOff > 0.0)
			{
				while (--numSamples >= 0)
				{
					auto currentSample = osc->getNextSample() * level * tailOff;
	
					for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
						outputBuffer.addSample (i, startSample, currentSample);

					++startSample;

					tailOff *= 0.99;
					if (tailOff <= 0.005)
					{
						clearCurrentNote();
						notePlaying = false;
						break;
					}
				}
			}
			else
			{
				while (--numSamples >= 0)
				{
					auto currentSample = osc->getNextSample() * level;
					for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
						outputBuffer.addSample (i, startSample, currentSample);
					++startSample;
				}
			}
		}
    }

private:
    double level = 0.0, tailOff = 0.0;
	bool notePlaying = false;
	WavetableOscillator *osc;
};

//==============================================================================
class SynthAudioSource   : public juce::AudioSource
{
public:
    SynthAudioSource (juce::MidiKeyboardState& keyState)
        : keyboardState (keyState)
    {
        for (auto i = 0; i < 4; ++i)
            synth.addVoice (new SineWaveVoice());

        synth.addSound (new SineWaveSound());
    }

    void setUsingSineWaveSound()
    {
        synth.clearSounds();
    }

    void prepareToPlay (int /*samplesPerBlockExpected*/, double sampleRate) override
    {
        synth.setCurrentPlaybackSampleRate (sampleRate);
    }

    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();

        juce::MidiBuffer incomingMidi;
        keyboardState.processNextMidiBuffer (incomingMidi, bufferToFill.startSample,
                                             bufferToFill.numSamples, true);

        synth.renderNextBlock (*bufferToFill.buffer, incomingMidi,
                               bufferToFill.startSample, bufferToFill.numSamples);
    }

private:
    juce::MidiKeyboardState& keyboardState;
    juce::Synthesiser synth;
};

//==============================================================================
class MainContentComponent   : public juce::AudioAppComponent,
                               private juce::Timer
{
public:
    MainContentComponent()
        : synthAudioSource  (keyboardState),
          keyboardComponent (keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        addAndMakeVisible (keyboardComponent);
        setAudioChannels (0, 2);

        setSize (600, 160);
        startTimer (400);
    }

    ~MainContentComponent() override
    {
        shutdownAudio();
    }

    void resized() override
    {
        keyboardComponent.setBounds (10, 10, getWidth() - 20, getHeight() - 20);
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        synthAudioSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        synthAudioSource.getNextAudioBlock (bufferToFill);
    }

    void releaseResources() override
    {
        synthAudioSource.releaseResources();
    }

private:
    void timerCallback() override
    {
		keyboardComponent.setKeyPressBaseOctave(4);
        keyboardComponent.grabKeyboardFocus();
        stopTimer();
    }

    //==========================================================================
    juce::MidiKeyboardState keyboardState;
    SynthAudioSource synthAudioSource;
    juce::MidiKeyboardComponent keyboardComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};
