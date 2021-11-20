/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "BinaryData.h"
#include "slider_gui.h"

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
    const int window_width = 640;
    const int window_height = 400;
    
    juce::Image background;
    SliderLookFeel sliderLookFeel;
    
    PitchDelayAudioProcessor& audioProcessor;
    juce::Slider feedback_level;
    juce::Slider dry_wet;
    juce::Slider pitch_shift;
    juce::Slider lfo_rate;
    juce::Slider min_delay;
    juce::Slider lo_cut;
    juce::Slider hi_cut;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchDelayAudioProcessorEditor)
};
