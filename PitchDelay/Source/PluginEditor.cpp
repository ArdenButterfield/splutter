/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
PitchDelayAudioProcessorEditor::PitchDelayAudioProcessorEditor (PitchDelayAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setLookAndFeel (&sliderLookFeel);
    
    setSize (window_width, window_height);
    
    startTimer(100); // milliseconds
    
    feedback_level.setRange(0.0, 0.95, 0.01);
    feedback_level.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    feedback_level.setTextBoxStyle(juce::Slider::NoTextBox, false, 60, 20);
    feedback_level.addListener(this);
    addAndMakeVisible(feedback_level);
    
    dry_wet.setRange(0.0, 1.0, 0.01);
    dry_wet.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    dry_wet.setTextBoxStyle(juce::Slider::NoTextBox, false, 60, 20);
    dry_wet.addListener(this);
    addAndMakeVisible(dry_wet);
     
    pitch_shift.setRange(-max_pitch_shift, max_pitch_shift); // Measured in semitones
    // max is defined in PluginProcessor.h
    pitch_shift.setSliderStyle(juce::Slider::LinearBarVertical);
    pitch_shift.setTextValueSuffix(" semitones");
    pitch_shift.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    pitch_shift.addListener(this);
    addAndMakeVisible(pitch_shift);
    
    lfo_rate.setRange(0.025, max_lfo_rate, 0.01); // Measured in seconds
    // max is defined in PluginProcessor.h
    lfo_rate.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    lfo_rate.setTextBoxStyle(juce::Slider::NoTextBox, false, 60, 20);
    lfo_rate.addListener(this);
    addAndMakeVisible(lfo_rate);
    
    min_delay.setRange(0.0, max_delay_slider_val, 0.01); // Measured in seconds
    min_delay.setSliderStyle(juce::Slider::LinearBar);
    min_delay.setTextValueSuffix(" seconds");
    min_delay.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    min_delay.addListener(this);
    addAndMakeVisible(min_delay);
    
    lo_cut.setRange(10.0,2000.0,1.0); //hz
    lo_cut.setSkewFactorFromMidPoint(200.0);
    lo_cut.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    lo_cut.setTextBoxStyle(juce::Slider::NoTextBox, false, 60, 20);
    lo_cut.addListener(this);
    addAndMakeVisible(lo_cut);
    
    hi_cut.setRange(200.0,20000.0,1.0); // hz
    hi_cut.setSkewFactorFromMidPoint(2000.0);
    hi_cut.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    hi_cut.setTextBoxStyle(juce::Slider::NoTextBox, false, 60, 20);
    hi_cut.addListener(this);
    addAndMakeVisible(hi_cut);
    
    background = juce::ImageCache::getFromMemory(BinaryData::splutter_png, BinaryData::splutter_pngSize);
    
}

PitchDelayAudioProcessorEditor::~PitchDelayAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void PitchDelayAudioProcessorEditor::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &feedback_level) {
        audioProcessor.feedback_level->u_param->beginChangeGesture();
        *(audioProcessor.feedback_level->u_param) = (float)feedback_level.getValue();
        audioProcessor.feedback_level->u_param->endChangeGesture();
    } else if (slider == &dry_wet) {
        audioProcessor.dry_wet->u_param->beginChangeGesture();
        *(audioProcessor.dry_wet->u_param) = (float)dry_wet.getValue();
        audioProcessor.dry_wet->u_param->endChangeGesture();
    } else if (slider == &pitch_shift) {
        audioProcessor.pitch_shift->u_param->beginChangeGesture();
        *(audioProcessor.pitch_shift->u_param) = (float)pitch_shift.getValue();
        audioProcessor.pitch_shift->u_param->endChangeGesture();
    } else if (slider == &lfo_rate) {
        audioProcessor.lfo_rate->u_param->beginChangeGesture();
        *(audioProcessor.lfo_rate->u_param) = (float)lfo_rate.getValue();
        audioProcessor.lfo_rate->u_param->endChangeGesture();
    } else if (slider == &min_delay) {
        audioProcessor.min_delay->u_param->beginChangeGesture();
        *(audioProcessor.min_delay->u_param) = (float)min_delay.getValue();
        audioProcessor.min_delay->u_param->endChangeGesture();
    } else if (slider == &lo_cut) {
        audioProcessor.lo_cut->u_param->beginChangeGesture();
        *(audioProcessor.lo_cut->u_param) = (float)lo_cut.getValue();
        audioProcessor.lo_cut->u_param->endChangeGesture();
    } else if (slider == &hi_cut) {
        audioProcessor.hi_cut->u_param->beginChangeGesture();
        *(audioProcessor.hi_cut->u_param) = (float)hi_cut.getValue();
        audioProcessor.hi_cut->u_param->endChangeGesture();
    }
}

void PitchDelayAudioProcessorEditor::timerCallback()
{
    feedback_level.setValue(
        *audioProcessor.feedback_level->u_param, juce::dontSendNotification);
    dry_wet.setValue(
        *audioProcessor.dry_wet->u_param, juce::dontSendNotification);
    pitch_shift.setValue(
        *audioProcessor.pitch_shift->u_param, juce::dontSendNotification);
    lfo_rate.setValue(
        *audioProcessor.lfo_rate->u_param, juce::dontSendNotification);
    min_delay.setValue(*audioProcessor.min_delay->u_param, juce::dontSendNotification);
    lo_cut.setValue(*audioProcessor.lo_cut->u_param, juce::dontSendNotification);
    hi_cut.setValue(*audioProcessor.hi_cut->u_param, juce::dontSendNotification);

}

//==============================================================================
void PitchDelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawImage(background, 0,0,window_width,window_height,0,0,2667,1667);
}

void PitchDelayAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    pitch_shift.setBounds(94-92/2,243-242/2,92,242);
    lfo_rate.setBounds(192.5-62/2,233.5-62/2,62,62);
    min_delay.setBounds(360-230/2,236-84/2,230,84);
    feedback_level.setBounds(377.5-66/2,108.5-66/2,66,66);
    dry_wet.setBounds(523.5-66/2,110.5-66/2,66,66);
    hi_cut.setBounds(517.5-80/2, 351.5-80/2, 80, 80);
    lo_cut.setBounds(350.5-80/2, 351.5-80/2, 80, 80);
    
}
