/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "filterCalc/FilterCalc.h"
#include "stk-filters/BiQuad.h"
#include <math.h> // pow
#include <algorithm> // min, max
#include <iostream>

#define PI 3.14159265
#define NUM_PARAMETERS 7
#define NUM_CHANNELS 2
#define GET_IN_RANGE(sample) (sample += (sample < 0) ? buffer_length : 0)

const float max_lfo_rate = 10.0;
const float max_pitch_shift = 4.0 * 12.0;
const int smoothing_window = 1000;
// over how many samples do we fade from the near to the far sound on
// the sawtooth delay?

struct ParameterVals {
    juce::AudioParameterFloat* u_param;
    float a_param;
    
    // Interpolation parameters
    float curr_val;
    float prev_val;
    int param_code;
    std::string name;
    
    ParameterVals(): a_param(0), curr_val(0), prev_val(0) {}
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
    ParameterVals* min_delay;
    ParameterVals* lo_cut;
    ParameterVals* hi_cut;
    
    ParameterVals* params[NUM_PARAMETERS];
    
    
private:
    int fs; // Sample frequency
    juce::AudioBuffer<float> delay_buffer;
    float buffer_read_pos;
    long buffer_write_pos;
    float delay_samples;
    int samples_since_reset;
    
    int buffer_length;
    
    float write_step;
    float lfo_len;
    float max_delay;
    
    float old_write_step;
    float old_lfo_len;
    float old_max_delay;
    
    float lo_freq, hi_freq;
    stk::BiQuad filter_lo_L, filter_lo_R, filter_hi_L, filter_hi_R;
    
    float semitones_to_ratio(float interval);
    void resizeBuffer();
    void calculateParameters();
    float getInBetween(const float* buffer, const float index);
    float linInterpolation(float start, float end, float fract);
    float getWetSaw(const int s, const float w_ptr, const float* delay_channel);
    float getRPointer(int s, float w_ptr, float step, float max, bool is_secondary);
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchDelayAudioProcessor)
};
