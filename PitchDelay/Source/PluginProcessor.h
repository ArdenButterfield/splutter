/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <math.h> // pow
#include <algorithm> // min, max
#include <iostream>

#define PI 3.14159265
#define NUM_PARAMETERS 4
#define NUM_CHANNELS 2

const float max_lfo_rate = 10.0;
const float max_pitch_shift = 4.0 * 12.0;
const float smoothing_window = 1000.0;
// over how many samples do we fade from the near to the far sound on
// the sawtooth delay?

struct ParameterVals {
    juce::AudioParameterFloat* u_param;
    float a_param;
    
    // Interpolation parameters
    float curr_val;
    float prev_val;
    ParameterVals(): a_param(0), curr_val(0), prev_val(0) {}
    int param_code;
    std::string name;
};



//==============================================================================
/**
*/


class PitchDelayAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    PitchDelayAudioProcessor();
    ~PitchDelayAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    ParameterVals* feedback_level;
    ParameterVals* dry_wet;
    ParameterVals* pitch_shift;
    ParameterVals* lfo_rate;
    
    ParameterVals* params[NUM_PARAMETERS];
    
    
private:
    int fs; // Sample frequency
    juce::AudioBuffer<float> delay_buffer;
    float buffer_read_pos;
    int buffer_write_pos;
    float delay_samples;
    
    int buffer_length;
    
    float read_ptr_step;
    float max_delay;
    
    float semitones_to_ratio(float interval);
    void resizeBuffer();
    void calculateParameters();
    float getInBetween(const float* buffer, const float index);
    float linInterpolation(float start, float end, float fract);
    float getWetSaw(const float d_samp, const float r_ptr, const float* delay_channel);
    int ctr; // TODO: temp test variable

    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchDelayAudioProcessor)
};
