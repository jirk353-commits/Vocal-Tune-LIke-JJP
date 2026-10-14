#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

namespace vgui
{
using APVTS = juce::AudioProcessorValueTreeState;

namespace col
{
    const juce::Colour bgTop    { 0xffeef0f2 };
    const juce::Colour bgBot    { 0xffc3c7cd };
    const juce::Colour text     { 0xff1d1f23 };
    const juce::Colour textDim  { 0xff4b5058 };
    const juce::Colour screenText { 0xfff4f1ea };
    const juce::Colour screenDim  { 0xffc3c8cf };
    const juce::Colour red      { 0xffd8222c };
    const juce::Colour redHi    { 0xffff5a5f };
    const juce::Colour copper   { 0xffe8935a };
    const juce::Colour mint     { 0xff3fe6c8 };
    const juce::Colour violet   { 0xffa98bff };
    const juce::Colour rose     { 0xffff6b8a };
    const juce::Colour gold     { 0xffffc85a };
    const juce::Colour lcd      { 0xff090b0e };
}

juce::Font uiFont (float size, bool bold = true, float kerning = 0.0f);
void embossed (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Font, juce::Colour, juce::Justification);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
};

class Knob : public juce::Component
{
public:
    Knob (APVTS&, const juce::String& id, const juce::String& title, juce::Colour accent, bool big = false, bool bipolar = false);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
    juce::String subtitle;
private:
    juce::String title; bool big;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

class Fader : public juce::Component
{
public:
    Fader (APVTS&, const juce::String& id, const juce::String& title, const juce::String& hint, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
    float activity = -100.0f;    // live chain level (dB) for the LED
private:
    juce::String title, hint;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

class ChoiceButtons : public juce::Component
{
public:
    ChoiceButtons (juce::RangedAudioParameter&, const juce::StringArray& labels, juce::Colour led, bool vertical = false);
    void resized() override;
    std::function<void (int)> onChange;
    int getSelected() const { return selected; }
private:
    void update (int);
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> att;
    int selected = -1; bool vertical;
};

/** Live tone curve + IN/OUT spectrum: shows exactly what the plugin is changing. */
class EqDisplay : public juce::Component
{
public:
    explicit EqDisplay (VocalFeelProcessor&);
    void paint (juce::Graphics&) override;
    void refresh();
private:
    float xForFreq (float f) const;
    float yForDb (float db) const;
    float specY (float db) const;
    void computeSpectrum (const ScopeRing&, std::vector<float>& smooth);
    VocalFeelProcessor& proc;
    static constexpr int order = 12, fftSize = 1 << order;
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fftData, specIn, specOut;
    float deessGr = 0;
};

class Meters : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void set (float in, float out, float comp, float deess);
private:
    void bar (juce::Graphics&, juce::Rectangle<float>, const juce::String&, float value, float hold, bool gr, juce::Colour);
    float v[4] { -100, -100, 0, 0 }, hold[4] { -100, -100, 0, 0 };
    int holdT[4] {};
};

class MainPanel : public juce::Component, private juce::Timer
{
public:
    explicit MainPanel (VocalFeelProcessor&);
    ~MainPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int W = 1280, H = 760;
private:
    void timerCallback() override;
    void renderTexture();
    void drawWell (juce::Graphics&, juce::Rectangle<int>, const juce::String& title, juce::Colour accent);
    void miniGr (juce::Graphics&, juce::Rectangle<float>, float gr, juce::Colour);
    void updateSubtitles();

    VocalFeelProcessor& proc;
    LookAndFeel lnf;
    juce::Image texture;
    EqDisplay eq;
    Meters meters;
    ChoiceButtons voice, colour;
    Knob sens, low, mid, high, deess, deessFreq, comp;
    Fader main, magic, space, attack, attitude, presence, master;
    juce::ComboBox presetBox;
    float inLvl = -100, outLvl = -100, cGr = 0, dGr = 0;
    int lastVoice = -1;
};
} // namespace vgui

class VocalFeelEditor : public juce::AudioProcessorEditor
{
public:
    explicit VocalFeelEditor (VocalFeelProcessor&);
    void resized() override;
private:
    vgui::MainPanel panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalFeelEditor)
};
