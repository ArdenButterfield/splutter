/*
  ==============================================================================

    slider_gui.h
    Created: 19 Nov 2021 3:51:31pm
    Author:  Arden Butterfield

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

const int KNOB_DARK = 0xff2d2d46;
//const int SLIDER_LIGHTER = 0xff4c2da1;
const int SLIDER_LIGHTER = 0xff49367d;

class SliderLookFeel: public juce::LookAndFeel_V4
{
public:
    SliderLookFeel()
    {
        setColour(juce::Slider::trackColourId, juce::Colour(SLIDER_LIGHTER));
    }
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider&) override
    {
        // Based on https://docs.juce.com/master/tutorial_look_and_feel_customisation.html
        auto radius = (float) juce::jmin(width / 2, height / 2);
        auto centerx = (float) x + (float) width  * 0.5f;
        auto centery = (float) y + (float) height * 0.5f;

        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        // fill
        g.setColour (juce::Colour(KNOB_DARK));

        
        juce::Path p;
        auto pointerLength = radius * 1.2;
        auto pointerThickness = 5.0f;
        p.addRectangle (-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
        p.applyTransform (juce::AffineTransform::rotation (angle).translated (centerx, centery));
        
        g.fillPath (p);

    }
    
};
