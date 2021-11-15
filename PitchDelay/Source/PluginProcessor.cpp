/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PitchDelayAudioProcessor::PitchDelayAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    feedback_level = new parameter_vals;
    dry_wet = new parameter_vals;
    pitch_shift = new parameter_vals;
    lfo_rate = new parameter_vals;
    
    feedback_level->name = "feedback";
    dry_wet->name = "dry/wet";
    pitch_shift->name = "pitch shift";
    lfo_rate->name = "rate";
    
    params[0] = feedback_level;
    params[1] = dry_wet;
    params[2] = pitch_shift;
    params[3] = lfo_rate;
    
    for (int i = 0; i < NUM_PARAMETERS; ++i) {
        params[i]->param_code = i;
    }
    
    auto feedback_range = juce::NormalisableRange<float> (0.0f, 0.95f);
    auto dry_wet_range = juce::NormalisableRange<float> (0.0f, 1.0f);
    auto pitch_shift_range = juce::NormalisableRange<float> (-max_pitch_shift, max_pitch_shift);
    auto lfo_range = juce::NormalisableRange<float> (0.1f, max_lfo_rate);
    
    feedback_level->u_param = new juce::AudioParameterFloat("feedback level", "feedback", feedback_range, 0.0f);
    dry_wet->u_param = new juce::AudioParameterFloat("dry/wet", "dry/wet", dry_wet_range, 0.5);
    pitch_shift->u_param = new juce::AudioParameterFloat("pitch shift", "pitch shift", pitch_shift_range, 0.0);
    lfo_rate->u_param = new juce::AudioParameterFloat("LFO rate", "rate", lfo_range, 1.0);
    for (int i = 0; i < NUM_PARAMETERS; ++i) {
        addParameter(params[i]->u_param);
    }
    
    delay_buffer = juce::AudioBuffer<float>();
    
    sine_mode = true;
    
    samples_since_reset = 0;
}

PitchDelayAudioProcessor::~PitchDelayAudioProcessor()
{
}

//==============================================================================
const juce::String PitchDelayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PitchDelayAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PitchDelayAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PitchDelayAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PitchDelayAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PitchDelayAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PitchDelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PitchDelayAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String PitchDelayAudioProcessor::getProgramName (int index)
{
    return {};
}

void PitchDelayAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================

void PitchDelayAudioProcessor::resizeBuffer()
{
    buffer_length = (int)(semitones_to_ratio(max_pitch_shift) * fs * max_lfo_rate);
    delay_buffer.setSize(NUM_CHANNELS, buffer_length, true, true, true);
}

void PitchDelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    fs = sampleRate;
    
    calculateParameters();
    resizeBuffer();
    delay_buffer.clear();
    
    
    delay_samples = lfo_rate->a_param;
    buffer_write_pos = delay_samples;
}

void PitchDelayAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PitchDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

float PitchDelayAudioProcessor::semitones_to_ratio(float interval)
{
    return pow(2.0, interval / 12.0);
}

void PitchDelayAudioProcessor::calculateParameters()
{
    feedback_level->a_param = *(feedback_level->u_param);
    dry_wet->a_param = *(dry_wet->u_param);
        
    // lfo rate a param: number of samples per saw
    lfo_rate->a_param = *(lfo_rate->u_param) * fs;
    
    // how much will the read pointer move per sample?
    pitch_shift->a_param = semitones_to_ratio(*(pitch_shift->u_param));
    
    // max delay: maximum number of samples between read pointer and write pointer
    if (pitch_shift->a_param == 1) {
        // no pitch up or down.
        max_delay = lfo_rate->a_param;
    } else {
        max_delay = abs(pitch_shift->a_param - 1) * lfo_rate->a_param;
    }
    for (int i = 0; i < NUM_PARAMETERS; ++i) {
        params[i]->curr_val = params[i]->a_param;
    }
}

float PitchDelayAudioProcessor::getInBetween(const float* buffer, const float index)
{
    // Currently linear interpolation, maybe i should do more.
    int lower_index = index;
    float offset = index - lower_index;
    return buffer[lower_index] * (1 - offset) + (offset) * buffer[lower_index + 1];
}


float PitchDelayAudioProcessor::linInterpolation(const float start, const float end, const float fract)
{
    return start + (fract * (end - start));
}

float PitchDelayAudioProcessor::getWetSaw(const int s, const float w_ptr, const float* delay_channel)
{
    float r_ptr;
    if (pitch_shift->a_param < 1) {
        r_ptr = w_ptr + ((float)s) * (pitch_shift->a_param - 1);
    } else if (pitch_shift->a_param > 1) {
        // we need to buffer the read pointer away from the write pointer by the smoothing window,
        // so that the secondary read pointer in the smoothing window doesn't go past the write pointer
        r_ptr = w_ptr - max_delay + ((float)s) * (pitch_shift->a_param - 1) - (float)smoothing_window;
    } else {
        r_ptr = w_ptr - max_delay;
    }
    if (r_ptr < 0) {
        r_ptr += buffer_length;
    }
    if (s < smoothing_window) {
        // For the smallest values of s, we use a "smoothing window": we calculate the values from
        // where the read pointer would be if it had continued its trajectory, and fade from the old
        // values to the new values.
        float secondary_r_ptr;
        
        
        float r_scale = (pitch_shift->a_param == 0) ? 1 : ((float)s) / (float)smoothing_window;
        float secondary_scale = (pitch_shift->a_param == 0) ? 0 : 1 - r_scale;
        
        if (pitch_shift->a_param < 1) {
            // Pitch down means we are increasing the distance between the read and write pointers
            // as s increases, so when we jump s back to 0, the old values will be farther away from
            // the new values by the max delay amount.
            secondary_r_ptr = r_ptr - max_delay;

            
        } else if (pitch_shift->a_param > 1) {
            // vice versa from above
            secondary_r_ptr = r_ptr + max_delay;
        } else {
            // special case for no pitch shift
            secondary_r_ptr = 0.0;
        }
        if (secondary_r_ptr < 0) {
            secondary_r_ptr += buffer_length;
        }
        return r_scale * getInBetween(delay_channel, r_ptr) +
            secondary_scale * getInBetween(delay_channel, secondary_r_ptr);
    } else {
        return getInBetween(delay_channel, r_ptr);
    }
}

float PitchDelayAudioProcessor::getWetSine(float d_samp, float lo, float hi, const float* delay_channel) {
    // TODO: make this not be broken
    if (lo == hi) {
        return getInBetween(delay_channel, lo);
    }
    if (lo > hi) {
        lo -= buffer_length;
    }
    float phase = 2 * PI * (d_samp - lo) / (hi - lo);
    float amp = (hi - lo) / 2;
    float center = (hi + lo) / 2;
    float index = sin(phase) * amp + center;
    if (index < 0) {
        index += buffer_length;
    }
    // std::cout << "offset " << hi - index << " index: " << index << "\n";
    return getInBetween(delay_channel, index);
    
}

void PitchDelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    calculateParameters();
    
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    float fract;
    long w_ptr;
    int s;
    float dry, wet, out, in, d_samp;
    for (int channel = 0; channel < fmin(NUM_CHANNELS, totalNumInputChannels); ++channel)
    {
        float* delay_channel = delay_buffer.getWritePointer(channel);
        
        w_ptr = buffer_write_pos;
        d_samp = delay_samples;
        s = samples_since_reset;
        
        auto* channelData = buffer.getWritePointer (channel);
        
        for (int sample = 0; sample < numSamples; ++sample) {
            
            fract = ((float) sample / (float) numSamples);
            for (int i = 0; i < NUM_PARAMETERS; ++i) {
                if (params[i]->curr_val != params[i]->prev_val) {
                    params[i]->a_param = linInterpolation(params[i]->prev_val, params[i]->curr_val, fract);
                }
            }

            in = channelData[sample];
            
            wet = getWetSaw(s, w_ptr, delay_channel);
            dry = in;
            out = wet * (dry_wet->a_param) + dry * (1 - dry_wet->a_param);
            delay_channel[w_ptr] = wet * feedback_level->a_param + in;
            
            channelData[sample] = out;
            
            // every sample, the write position in the delay array steps forward one
            w_ptr++;
            if (w_ptr >= buffer_length) {
                w_ptr = 0;
            }
            s++;
            if (s >= lfo_rate->a_param) {
                s = 0;
            }
        }
        for (int i = 0; i < NUM_PARAMETERS; ++i) {
            params[i]->prev_val = params[i]->curr_val;
        }
    }
    samples_since_reset = s;
    buffer_write_pos = w_ptr;
    delay_samples = d_samp;
}

//==============================================================================
bool PitchDelayAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PitchDelayAudioProcessor::createEditor()
{
    return new PitchDelayAudioProcessorEditor (*this);
}

//==============================================================================
void PitchDelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    /*std::unique_ptr<juce::XmlElement> xml (new juce::XmlElement ("sliderParams"));
    for (int i = 0; i < NUM_PARAMETERS; ++i) {
        xml->setAttribute(juce::Identifier(params[i]->name),
                          (double) *(params[i]->u_param));
    }
    copyXmlToBinary (*xml, destData);*/
}

void PitchDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    /*std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary(data, sizeInBytes));
    
    if ((xmlState != nullptr) && (xmlState->hasTagName("sliderParams"))) {
        for (int i = 0; i < NUM_PARAMETERS; ++i) {
            *(params[i]->u_param) = xmlState->getDoubleAttribute(params[i]->name,0.0);
        }
    }*/
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchDelayAudioProcessor();
}
