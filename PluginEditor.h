#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "EditorLayout.h"
#include "PluginProcessor.h"

// The pedal-shaped editor: one background render, plus small sprites drawn on
// top for the slider, the two LEDs and the two buttons. The sprites and click
// areas come from tools/make_sprites.py (see EditorLayout.h).
class JuneEditor : public juce::AudioProcessorEditor
{
public:
    explicit JuneEditor (JuneProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    enum class Hit { none, slider, button1, button2, footswitch };

    float scale() const noexcept;   // editor pixels per artwork pixel
    Hit hitTest (juce::Point<float> artworkPos) const noexcept;
    void drawSprite (juce::Graphics&, const juce::Image&, const layout::Rect&) const;
    void setBool (juce::ParameterAttachment&, bool& state, bool newValue);

    juce::Image background, sliderStereo, led1On, led2On, button1, button2, button12;

    // Mirrors of the parameters, kept in step by the attachments below.
    bool chorus1 = false, chorus2 = false, stereo = true, bypassed = false;

    juce::ParameterAttachment chorus1Attachment, chorus2Attachment, outputAttachment, bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JuneEditor)
};
