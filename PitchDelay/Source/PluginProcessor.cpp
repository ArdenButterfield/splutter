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
    feedback_level = new ParameterVals;
    dry_wet = new ParameterVals;
    pitch_shift = new ParameterVals;
    lfo_rate = new ParameterVals;
    min_delay = new ParameterVals;
    lo_cut = new ParameterVals;
    hi_cut = new ParameterVals;
    
    feedback_level->name = "feedback";
    dry_wet->name = "drywet";
    pitch_shift->name = "pitchshift";
    min_delay->name = "mindelay";
    lfo_rate->name = "rate";
    lo_cut->name = "locut";
    hi_cut->name = "hicut";
    
    params[0] = feedback_level;
    params[1] = dry_wet;
    params[2] = pitch_shift;
    params[3] = lfo_rate;
    params[4] = min_delay;
    params[5] = lo_cut;
    params[6] = hi_cut;
    
    for (int i = 0; i < NUM_PARAMETERS; ++i) {
        params[i]->param_code = i;
    }
    
    auto feedback_range = juce::NormalisableRange<float> (0.0f, 0.95f);
    auto dry_wet_range = juce::NormalisableRange<float> (0.0f, 1.0f);
    auto pitch_shift_range = juce::NormalisableRange<float> (-max_pitch_shift, max_pitch_shift);
    auto lfo_range = juce::NormalisableRange<float> (0.025f, max_lfo_rate);
    auto min_delay_range = juce::NormalisableRange<float> (0.0, max_delay_slider_val);
    auto lo_filter_range = juce::NormalisableRange<float> (10.0, 2000.0);
    auto hi_filter_range = juce::NormalisableRange<float> (200.0, 20000.0);
    
    feedback_level->u_param = new juce::AudioParameterFloat("feedback level", "feedback", feedback_range, 0.0f);
    dry_wet->u_param = new juce::AudioParameterFloat("dry/wet", "dry/wet", dry_wet_range, 0.5);
    pitch_shift->u_param = new juce::AudioParameterFloat("pitch shift", "pitch shift", pitch_shift_range, 0.0);
    lfo_rate->u_param = new juce::AudioParameterFloat("LFO rate", "rate", lfo_range, 1.0);
    min_delay->u_param = new juce::AudioParameterFloat("Min delay", "min delay", min_delay_range, 1.0);
    lo_cut->u_param = new juce::AudioParameterFloat("Low cut", "lo", lo_filter_range, 10.0);
    hi_cut->u_param = new juce::AudioParameterFloat("High cut", "hi", hi_filter_range, 20000.0);
    
    for (int i = 0; i < NUM_PARAMETERS; ++i) {
        addParameter(params[i]->u_param);
    }
    
    delay_buffer = juce::AudioBuffer<float>();
        
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
    buffer_length = (int)(semitones_to_ratio(max_pitch_shift) * (fs * max_lfo_rate + smoothing_window) + max_delay_slider_val * fs);
    std::cout<<"buffer length: "<<buffer_length<<"\n";
    delay_buffer.setSize(NUM_CHANNELS, buffer_length, true, true, true);
}

void PitchDelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    fs = sampleRate;
    samps_for_delay_move = fs * time_for_delay_move;
    min_delay->a_param = *(min_delay->u_param) * fs;
    min_delay_actual = min_delay->a_param;
    
    calculateParameters();

    
    old_max_delay = max_delay;
    old_write_step = write_step;
    old_lfo_len = lfo_len;
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

    min_delay->a_param = *(min_delay->u_param) * fs;
    if (min_delay_actual != min_delay->a_param) {
        float distance = min_delay->a_param - min_delay_actual;
        min_delay_step = distance / samps_for_delay_move;
    }
    
    // We don't want to change the shape of the sawtooth while we are in the
    // middle of transitioning, that will cause clicking.
    std::cout << samples_since_reset<<" samples since reset ";
    if (samples_since_reset > 1000 || samples_since_reset == 0) {
        lfo_len = lfo_rate->a_param;
        write_step = pitch_shift->a_param - 1;
        // max delay: maximum number of samples between read pointer and write pointer
        if (pitch_shift->a_param == 1) {
            // no pitch up or down.
            max_delay = lfo_rate->a_param;
        } else {
            max_delay = abs(pitch_shift->a_param - 1) * lfo_rate->a_param;
        }
    }
    std::cout<<write_step<<" "<<lfo_len<<"\n";
    for (int i = 0; i < NUM_PARAMETERS; ++i) {
        params[i]->curr_val = params[i]->a_param;
    }
    
    float coeffs[5];
    float fc = *(lo_cut->u_param);
    float Q = 1.0;
    FilterCalc::calcCoeffsHPF(coeffs, fc, Q, fs);
    filter_lo_L.setCoefficients(coeffs[0], coeffs[1], coeffs[2], coeffs[3], coeffs[4]);
    filter_lo_R.setCoefficients(coeffs[0], coeffs[1], coeffs[2], coeffs[3], coeffs[4]);
    
    fc = *(hi_cut->u_param);
    FilterCalc::calcCoeffsLPF(coeffs, fc, Q, fs);
    filter_hi_L.setCoefficients(coeffs[0], coeffs[1], coeffs[2], coeffs[3], coeffs[4]);
    filter_hi_R.setCoefficients(coeffs[0], coeffs[1], coeffs[2], coeffs[3], coeffs[4]);
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

float PitchDelayAudioProcessor::getRPointer(int s, float w_ptr, float step, float max, bool is_secondary)
{
    float secondary_shift;
    if (is_secondary) {
        secondary_shift = max;
    } else {
        secondary_shift = 0;
    }
    float r_ptr;
    if (step < 0) {
        r_ptr = w_ptr + ((float)s) * step - secondary_shift - min_delay_actual;
    } else if (step > 0) {
        // we need to buffer the read pointer away from the write pointer by the smoothing window,
        // so that the secondary read pointer in the smoothing window doesn't go past the write pointer
        r_ptr = w_ptr - max + ((float)s) * step - smoothing_window + secondary_shift - min_delay_actual;
    } else {
        r_ptr = w_ptr - min_delay_actual; // No secondary shift for constant delay.
    }
    return r_ptr;
}

float PitchDelayAudioProcessor::getWetSaw(const int s, const float w_ptr, const float* delay_channel)
{
    float r_ptr, secondary_r_ptr;
    if (s > smoothing_window || (write_step == 0 && old_write_step == 0)) {
        r_ptr = getRPointer(s, w_ptr, old_write_step, old_max_delay, false);
        //std::cout<<r_ptr<<"\n";
        if (r_ptr < 0) {
            r_ptr += buffer_length;
        }
        return getInBetween(delay_channel, r_ptr);
    } else {
        r_ptr = getRPointer(s, w_ptr, write_step, max_delay, false);
        secondary_r_ptr = getRPointer(s, w_ptr, old_write_step, old_max_delay, true);
        if (r_ptr < 0) {
            r_ptr += buffer_length;
        }
        if (secondary_r_ptr < 0) {
            secondary_r_ptr += buffer_length;
        }
        // For the smallest values of s, we use a "smoothing window": we calculate the values from
        // where the read pointer would be if it had continued its trajectory, and fade from the old
        // values to the new values.
        
        // Normally, we want to preserve power across the transiton. However, if we are
        // Not shifting the pitch up or down, we want to preserve
        float r_scale = sin( PI *(((float)s) / (float)smoothing_window) / 2.0);
        float secondary_scale = cos( PI *(((float)s) / (float)smoothing_window) / 2);
        //std::cout<<r_ptr<<" at "<<r_scale<<"; "<<secondary_r_ptr<<" at "<<secondary_scale<<"\n";
        return r_scale * getInBetween(delay_channel, r_ptr) +
            secondary_scale * getInBetween(delay_channel, secondary_r_ptr);
    }
}

void PitchDelayAudioProcessor::adjustMinDelayActual() {
    if (abs(min_delay_actual - min_delay->a_param) < abs(min_delay_step)) {
        min_delay_actual = min_delay->a_param;
    } else {
        min_delay_actual += min_delay_step;
    }
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
    float oll = old_lfo_len;
    float omd = old_max_delay;
    float ows = old_write_step;
    float mda = min_delay_actual;
    for (int channel = 0; channel < fmin(NUM_CHANNELS, totalNumInputChannels); ++channel)
    {
        min_delay_actual = mda;
        old_lfo_len = oll;
        old_max_delay = omd;
        old_write_step = ows;
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
            delay_channel[w_ptr] = in;
            // This is necessary for when the delay time is set to 0.
            
            wet = getWetSaw(s, w_ptr, delay_channel);
            dry = in;
            out = wet * (dry_wet->a_param) + dry * (1 - dry_wet->a_param);
            if (out != 0) {
                std::cout<<s<< " " <<out<<"\n";
            }
            float unfiltered = wet * feedback_level->a_param + in;
            if (channel == 0) {
                delay_channel[w_ptr] = filter_lo_L.tick(filter_hi_L.tick(unfiltered));
                //delay_channel[w_ptr] = unfiltered;
            } else if (channel == 1) {
                delay_channel[w_ptr] = filter_lo_R.tick(filter_hi_R.tick(unfiltered));
                //delay_channel[w_ptr] = unfiltered;
            }
            channelData[sample] = out;
            
            // every sample, the write position in the delay array steps forward one
            w_ptr++;
            if (w_ptr >= buffer_length) {
                w_ptr = 0;
            }
            s++;
            if (s == smoothing_window) {
                old_lfo_len = lfo_len;
                old_max_delay = max_delay;
                old_write_step = write_step;
            }
            if (s >= old_lfo_len) {
                s = 0;
            }
            adjustMinDelayActual();
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
    std::unique_ptr<juce::XmlElement> xml (new juce::XmlElement ("sliderParams"));
    for (int i = 0; i < NUM_PARAMETERS; ++i) {
        xml->setAttribute(juce::Identifier(params[i]->name),
                          (double) *(params[i]->u_param));
    }
    copyXmlToBinary (*xml, destData);
}

void PitchDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary(data, sizeInBytes));
    
    if ((xmlState != nullptr) && (xmlState->hasTagName("sliderParams"))) {
        for (int i = 0; i < NUM_PARAMETERS; ++i) {
            *(params[i]->u_param) = xmlState->getDoubleAttribute(params[i]->name,0.0);
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchDelayAudioProcessor();
}
