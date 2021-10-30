/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class PitchDelayAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Slider::Listener, public juce::Timer
{
public:
    PitchDelayAudioProcessorEditor (PitchDelayAudioProcessor&);
    ~PitchDelayAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void sliderValueChanged(juce::Slider*) override;
    void timerCallback() override;
     

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    const int window_width = 800;
    const int window_height = 500;
    
    PitchDelayAudioProcessor& audioProcessor;
    juce::Slider feedback_level;
    juce::Slider dry_wet;
    juce::Slider pitch_shift;
    juce::Slider lfo_rate;
    // juce::ToggleButton sine_saw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchDelayAudioProcessorEditor)
};
