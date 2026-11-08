#include "PluginEditor.h"

#include "BinaryData.h"

namespace
{
juce::Image loadImage (const void* data, int size)
{
    return juce::ImageCache::getFromMemory (data, size);
}
}

JuneEditor::JuneEditor (JuneProcessor& p)
    : AudioProcessorEditor (&p),
      chorus1Attachment (*p.apvts.getParameter ("chorus1"), [this] (float v) { chorus1  = v > 0.5f; repaint(); }),
      chorus2Attachment (*p.apvts.getParameter ("chorus2"), [this] (float v) { chorus2  = v > 0.5f; repaint(); }),
      outputAttachment  (*p.apvts.getParameter ("output"),  [this] (float v) { stereo   = v > 0.5f; repaint(); }),
      bypassAttachment  (*p.apvts.getParameter ("bypass"),  [this] (float v) { bypassed = v > 0.5f; repaint(); })
{
    background   = loadImage (BinaryData::bg_png,            BinaryData::bg_pngSize);
    sliderStereo = loadImage (BinaryData::slider_stereo_png, BinaryData::slider_stereo_pngSize);
    led1On       = loadImage (BinaryData::led1_on_png,       BinaryData::led1_on_pngSize);
    led2On       = loadImage (BinaryData::led2_on_png,       BinaryData::led2_on_pngSize);
    button1      = loadImage (BinaryData::btn_1_png,         BinaryData::btn_1_pngSize);
    button2      = loadImage (BinaryData::btn_2_png,         BinaryData::btn_2_pngSize);
    button12     = loadImage (BinaryData::btn_12_png,        BinaryData::btn_12_pngSize);

    // 16:9 and resizable. The artwork is 1920x1080, so the default 960x540 is
    // crisp on a 2x screen.
    setResizable (true, true);
    setResizeLimits (640, 360, 1920, 1080);
    getConstrainer()->setFixedAspectRatio ((double) layout::kImageW / (double) layout::kImageH);
    setSize (960, 540);
    setBufferedToImage (true);

    chorus1Attachment.sendInitialUpdate();
    chorus2Attachment.sendInitialUpdate();
    outputAttachment.sendInitialUpdate();
    bypassAttachment.sendInitialUpdate();
}

float JuneEditor::scale() const noexcept
{
    return (float) getWidth() / (float) layout::kImageW;
}

void JuneEditor::drawSprite (juce::Graphics& g, const juce::Image& sprite, const layout::Rect& r) const
{
    const float s = scale();
    g.drawImage (sprite, juce::Rectangle<float> ((float) r.x * s, (float) r.y * s,
                                                 (float) r.w * s, (float) r.h * s));
}

void JuneEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, getLocalBounds().toFloat());

    if (stereo)
        drawSprite (g, sliderStereo, layout::kSlider);

    // The LEDs follow the buttons, but go dark while bypassed.
    if (chorus1 && ! bypassed)
        drawSprite (g, led1On, layout::kLed1);
    if (chorus2 && ! bypassed)
        drawSprite (g, led2On, layout::kLed2);

    // The buttons keep their position while bypassed, like a latching switch.
    if (chorus1 && chorus2)
        drawSprite (g, button12, layout::kButtons);
    else if (chorus1)
        drawSprite (g, button1, layout::kButtons);
    else if (chorus2)
        drawSprite (g, button2, layout::kButtons);
}

JuneEditor::Hit JuneEditor::hitTest (juce::Point<float> p) const noexcept
{
    const float dx = p.x - layout::kFootX;
    const float dy = p.y - layout::kFootY;

    if (dx * dx + dy * dy <= layout::kFootR * layout::kFootR)
        return Hit::footswitch;
    if (layout::kHitButton1.contains (p.x, p.y))
        return Hit::button1;
    if (layout::kHitButton2.contains (p.x, p.y))
        return Hit::button2;
    if (layout::kHitSlider.contains (p.x, p.y))
        return Hit::slider;

    return Hit::none;
}

void JuneEditor::setBool (juce::ParameterAttachment& attachment, bool& state, bool newValue)
{
    // The attachment ignores changes it makes itself, so update our mirror here.
    state = newValue;
    attachment.setValueAsCompleteGesture (newValue ? 1.0f : 0.0f);
    repaint();
}

void JuneEditor::mouseDown (const juce::MouseEvent& e)
{
    const float s = scale();
    const juce::Point<float> p (e.position.x / s, e.position.y / s);

    switch (hitTest (p))
    {
        case Hit::slider:     setBool (outputAttachment,  stereo,   p.x >= layout::kSliderCentreX); break;
        case Hit::button1:    setBool (chorus1Attachment, chorus1,  ! chorus1);                     break;
        case Hit::button2:    setBool (chorus2Attachment, chorus2,  ! chorus2);                     break;
        case Hit::footswitch: setBool (bypassAttachment,  bypassed, ! bypassed);                    break;
        case Hit::none:       break;
    }
}

void JuneEditor::mouseMove (const juce::MouseEvent& e)
{
    const float s = scale();
    const bool overControl = hitTest ({ e.position.x / s, e.position.y / s }) != Hit::none;

    setMouseCursor (overControl ? juce::MouseCursor::PointingHandCursor
                                : juce::MouseCursor::NormalCursor);
}
