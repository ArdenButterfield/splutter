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
    
    
    for (int i = 0; i < NUM_PARAMETERS; i++) {
        params[i]->prev_val = params[i]->curr_val;
    }
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
    pitch_shift->a_param = semitones_to_ratio(*(pitch_shift->u_param)) - 1;
    lfo_rate->a_param = *(lfo_rate->u_param) * fs;
    // std::cout << "pitch shift ratio: "<<  pitch_shift->a_param << "\n";
}

float PitchDelayAudioProcessor::getInBetween(float* buffer, float index)
{
    // Currently linear interpolation, maybe i should do more.
    int lower_index = index;
    float offset = index - lower_index;
    return buffer[lower_index] * (1 - offset) + (offset) * buffer[lower_index + 1];
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
    long w_ptr;
    float dry, wet, out, in, smoosh, r_ptr, d_samp, smelsh;
    for (int channel = 0; channel < fmin(NUM_CHANNELS, totalNumInputChannels); ++channel)
    {
        float* delay_channel = delay_buffer.getWritePointer(channel);
        
        w_ptr = buffer_write_pos;
        d_samp = delay_samples;
        
        auto* channelData = buffer.getWritePointer (channel);
        
        for (int sample = 0; sample < numSamples; ++sample) {
            r_ptr = w_ptr - d_samp;
            if (r_ptr < 0) {
                r_ptr += buffer_length;
            }
            in = channelData[sample];
            if (d_samp < smoothing_window) {
                smelsh = r_ptr - lfo_rate->a_param;
                if (smelsh < 0) {
                    smelsh += buffer_length;
                }
                smoosh = ((float) d_samp) / smoothing_window;
                // The read pointer jumps in position when d_samp gets to zero,
                // wheter d_samp is increasing or decreasing. So, the closer
                // we are to zero, the more we want the smelshed sound instead
                // of the original sound.
                wet = getInBetween(delay_channel, r_ptr) * (smoosh) + getInBetween(delay_channel, smelsh) * (1 - smoosh);
            } else {
                wet = getInBetween(delay_channel, r_ptr);
                if (channel == 0) {
                    std::cout << r_ptr << " r ptr\n";
                }
            }
            dry = in;
            out = wet * (dry_wet->a_param) + dry * (1 - dry_wet->a_param);
            delay_channel[w_ptr] = (getInBetween(delay_channel, r_ptr) * feedback_level->a_param) + in;
            
            channelData[sample] = out;
            
            w_ptr += 1;
            d_samp -= pitch_shift->a_param;
            if (w_ptr >= buffer_length) {
                w_ptr = 0;
            }
            if (d_samp < 0) {
                d_samp = lfo_rate->a_param;
            } else if (d_samp > lfo_rate->a_param) {
                d_samp = 0;
            }
        }
    }
    buffer_read_pos = r_ptr;
    buffer_write_pos = w_ptr;
    delay_samples = d_samp;
    //std::cout << "r: " << buffer_read_pos << " w: " << buffer_write_pos << " diff " << buffer_write_pos - buffer_read_pos << "\n";
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
