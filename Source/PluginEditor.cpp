#include "PluginEditor.h"

using namespace juce;

namespace vgui
{
Font uiFont (float size, bool bold, float kerning)
{
    Font f { FontOptions (size, bold ? Font::bold : Font::plain) };
    return kerning > 0.0f ? f.withExtraKerningFactor (kerning) : f;
}

void embossed (Graphics& g, const String& t, Rectangle<float> r, Font f, Colour c, Justification j)
{
    g.setFont (f);
    if (c.getPerceivedBrightness() < 0.5f)          // dark ink on the silver plate: engraved look
    {
        g.setColour (Colours::white.withAlpha (0.75f));
        g.drawText (t, r.translated (0, 1.2f), j, false);
    }
    else
    {
        g.setColour (Colours::black.withAlpha (0.8f));
        g.drawText (t, r.translated (0, 1.3f), j, false);
    }
    g.setColour (c);
    g.drawText (t, r, j, false);
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (PopupMenu::backgroundColourId, Colour (0xfff0f1f3));
    setColour (PopupMenu::textColourId, col::text);
    setColour (PopupMenu::highlightedBackgroundColourId, col::red);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (ComboBox::textColourId, Colours::white);
}

/** Chrome knob: scalloped polished skirt, brushed chrome face, black pointer. */
static void chromeKnob (Graphics& g, Point<float> c, float r, float angle)
{
    // shadow
    Path sh; sh.addEllipse (c.x - r, c.y - r, r * 2, r * 2);
    DropShadow (Colours::black.withAlpha (0.55f), (int) (r * 0.35f), { (int) (r * 0.10f), (int) (r * 0.16f) }).drawForPath (g, sh);
    // scalloped skirt
    Path skirt;
    const int lobes = 9;
    for (int i = 0; i <= 180; ++i)
    {
        const float a = (float) i / 180.0f * MathConstants<float>::twoPi;
        const float rr = r * (0.94f + 0.06f * std::cos (a * (float) lobes + angle * (float) lobes));
        const auto p = c.getPointOnCircumference (rr, a);
        if (i == 0) skirt.startNewSubPath (p); else skirt.lineTo (p);
    }
    skirt.closeSubPath();
    ColourGradient sk (Colour (0xffffffff), c.x - r * 0.6f, c.y - r * 0.8f, Colour (0xff6d727a), c.x + r * 0.6f, c.y + r, false);
    sk.addColour (0.45, Colour (0xffd9dce0));
    sk.addColour (0.7, Colour (0xff9aa0a8));
    g.setGradientFill (sk);
    g.fillPath (skirt);
    g.setColour (Colour (0xff4a4e55));
    g.strokePath (skirt, PathStrokeType (1.0f));
    // face
    const float fr = r * 0.70f;
    g.setGradientFill (ColourGradient (Colour (0xfff4f5f7), c.x, c.y - fr, Colour (0xffa5abb3), c.x, c.y + fr, false));
    g.fillEllipse (c.x - fr, c.y - fr, fr * 2, fr * 2);
    for (int i = 0; i < 36; ++i)        // spun-metal rings
    {
        const float rr = fr * (0.2f + 0.8f * (float) i / 36.0f);
        g.setColour ((i % 2 ? Colours::white : Colours::black).withAlpha (0.045f));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 1.0f);
    }
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.65f), c.x - fr * 0.4f, c.y - fr, Colours::transparentWhite, c.x, c.y, false));
    g.fillEllipse (c.x - fr * 0.85f, c.y - fr * 0.95f, fr * 1.5f, fr * 1.1f);
    g.setColour (Colour (0xff7a8088));
    g.drawEllipse (c.x - fr, c.y - fr, fr * 2, fr * 2, 1.0f);
    // pointer
    const auto p1 = c.getPointOnCircumference (fr * 0.15f, angle), p2 = c.getPointOnCircumference (fr * 0.95f, angle);
    g.setColour (Colour (0xff101113));
    g.drawLine ({ p1, p2 }, jmax (3.0f, r * 0.09f));
}

void LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float sa, float ea, Slider& s)
{
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float ringR = size * 0.5f - 4.0f;
    const float angle = sa + pos * (ea - sa);
    // scale: dark ticks engraved into the plate, thin red value arc
    for (int i = 0; i <= 10; ++i)
    {
        const float a = sa + (float) i / 10.0f * (ea - sa);
        const bool major = i % 5 == 0;
        const auto p1 = c.getPointOnCircumference (ringR - (major ? 7.0f : 5.0f), a), p2 = c.getPointOnCircumference (ringR, a);
        g.setColour (Colours::white.withAlpha (0.8f));
        g.drawLine ({ p1.translated (0, 1), p2.translated (0, 1) }, major ? 2.2f : 1.5f);
        g.setColour (Colour (0xff2a2c30));
        g.drawLine ({ p1, p2 }, major ? 2.2f : 1.5f);
    }
    {
        const float from = bipolar ? (sa + ea) * 0.5f : sa;
        const float ar = ringR - 10.0f;
        Path arc; arc.addCentredArc (c.x, c.y, ar, ar, 0, jmin (from, angle), jmax (from, angle), true);
        g.setColour (col::red);
        g.strokePath (arc, PathStrokeType (2.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    chromeKnob (g, c, ringR - 13.0f, angle);
}

void LookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider& s)
{
    const bool redCap = (bool) s.getProperties().getWithDefault ("redCap", false);
    const float cx = (float) x + (float) w * 0.5f;
    const float top = (float) y + 14.0f, bot = (float) (y + h) - 14.0f;
    // scale
    for (int i = 0; i <= 20; ++i)
    {
        const float yy = bot - (bot - top) * (float) i / 20.0f;
        const bool major = i % 2 == 0;
        g.setColour (Colour (0xff3b3e44).withAlpha (major ? 0.9f : 0.5f));
        g.fillRect (cx - 25.0f, yy - 0.5f, major ? 8.0f : 5.0f, 1.0f);
        g.fillRect (cx + (major ? 17.0f : 20.0f), yy - 0.5f, major ? 8.0f : 5.0f, 1.0f);
        if (major && i % 4 == 0)
        {
            g.setFont (uiFont (10.5f));
            g.drawText (String (i / 2), Rectangle<float> (cx - 42.0f, yy - 7.0f, 15.0f, 14.0f), Justification::centredRight, false);
        }
    }
    // chrome-rimmed slot
    auto slot = Rectangle<float> (cx - 4.5f, top - 6, 9.0f, bot - top + 12);
    g.setGradientFill (ColourGradient (Colours::white, cx - 8, 0, Colour (0xff8d939b), cx + 8, 0, false));
    g.fillRoundedRectangle (slot.expanded (3.5f), 7.0f);
    g.setColour (Colour (0xff0b0c0e));
    g.fillRoundedRectangle (slot, 4.5f);
    // cap
    const float py = jlimit (top, bot, (float) pos);
    auto cap = Rectangle<float> (30.0f, 38.0f).withCentre ({ cx, py });
    DropShadow (Colours::black.withAlpha (0.55f), 8, { 4, 6 }).drawForRectangle (g, cap.toNearestInt());
    ColourGradient cg = redCap ? ColourGradient (Colour (0xffff6b6b), 0, cap.getY(), Colour (0xff7a0a10), 0, cap.getBottom(), false)
                               : ColourGradient (Colour (0xffffffff), 0, cap.getY(), Colour (0xff767c84), 0, cap.getBottom(), false);
    cg.addColour (0.45, redCap ? Colour (0xffe0262f) : Colour (0xffd6d9dd));
    cg.addColour (0.55, redCap ? Colour (0xffa8121a) : Colour (0xffaab0b7));
    g.setGradientFill (cg);
    g.fillRoundedRectangle (cap, 4.0f);
    g.setColour (Colours::white.withAlpha (0.6f));
    g.drawLine (cap.getX() + 3, cap.getY() + 1.5f, cap.getRight() - 3, cap.getY() + 1.5f, 1.0f);
    for (float k : { -8.0f, -4.0f, 4.0f, 8.0f })     // grip ridges
    {
        g.setColour (Colours::black.withAlpha (0.28f));
        g.fillRect (cap.getX() + 3, py + k, cap.getWidth() - 6, 1.0f);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.fillRect (cap.getX() + 3, py + k + 1, cap.getWidth() - 6, 1.0f);
    }
    g.setColour (Colours::black.withAlpha (0.8f));
    g.fillRect (cap.getX() + 2, py - 1.0f, cap.getWidth() - 4, 2.0f);
    g.setColour (Colour (0xff3a3d42));
    g.drawRoundedRectangle (cap, 4.0f, 1.0f);
}

void LookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const auto led = b.findColour (TextButton::buttonOnColourId);
    const bool on = b.getToggleState();
    DropShadow (Colours::black.withAlpha (0.45f), 4, { 0, down ? 0 : 2 }).drawForRectangle (g, r.toNearestInt());
    if (on)
        g.setGradientFill (ColourGradient (Colour (0xff35383e), 0, r.getY(), Colour (0xff0e0f11), 0, r.getBottom(), false));
    else
        g.setGradientFill (ColourGradient (Colour (hi ? 0xffffffff : 0xfff3f4f6), 0, r.getY(), Colour (down ? 0xffa4a9b0 : 0xffb9bdc3), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colour (0xff2a2c30));
    g.drawRoundedRectangle (r, 4.0f, 1.2f);
    if (on)
    {
        auto strip = Rectangle<float> (r.getX() + 6, r.getBottom() - 5, r.getWidth() - 12, 2.5f);
        g.setColour (led.withAlpha (0.4f)); g.fillRoundedRectangle (strip.expanded (2, 1.5f), 2);
        g.setColour (led); g.fillRoundedRectangle (strip, 1.2f);
    }
}

void LookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    auto r = b.getLocalBounds().reduced (3);
    g.setFont (uiFont (jmin (15.0f, (float) b.getHeight() * 0.46f), true, 0.06f));
    g.setColour (b.getToggleState() ? Colours::white : Colour (0xff23252a));
    g.drawFittedText (b.getButtonText(), r, Justification::centred, 1, 0.8f);
}

void LookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setGradientFill (ColourGradient (Colour (0xff2c2f34), 0, 0, Colour (0xff0d0e10), 0, (float) h, false));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colour (0xff6b7078));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.2f);
    Path a;
    const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
    a.addTriangle (ax - 5, ay - 2.5f, ax + 5, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (col::red);
    g.fillPath (a);
}
Font LookAndFeel::getComboBoxFont (ComboBox&) { return uiFont (16.0f); }
Font LookAndFeel::getPopupMenuFont() { return uiFont (15.5f, false); }
void LookAndFeel::positionComboBoxText (ComboBox& b, Label& l) { l.setBounds (12, 1, b.getWidth() - 36, b.getHeight() - 2); l.setFont (getComboBoxFont (b)); }

//==============================================================================
static void drawLcd (Graphics& g, Rectangle<float> lcd, const String& text, Colour c, float fontSize)
{
    g.setColour (col::lcd);
    g.fillRoundedRectangle (lcd, 5.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (lcd, 5.0f, 1.2f);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawLine (lcd.getX() + 4, lcd.getBottom() + 0.8f, lcd.getRight() - 4, lcd.getBottom() + 0.8f, 1.0f);
    g.setColour (c);
    g.setFont (uiFont (fontSize));
    g.drawText (text, lcd, Justification::centred, false);
}

Knob::Knob (APVTS& s, const String& id, const String& t, Colour accent, bool b, bool bipolar) : title (t), big (b)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (big ? 260 : 210);
    slider.setColour (Slider::rotarySliderFillColourId, accent);
    slider.getProperties().set ("bipolar", bipolar);
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id)) slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}
void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (26);
    r.removeFromBottom (subtitle.isEmpty() ? 32 : 50);
    const int s = jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (s, s));
}
void Knob::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    embossed (g, title, r.removeFromTop (26), uiFont (big ? 16.5f : 15.0f, true, 0.12f), col::text, Justification::centred);
    if (subtitle.isNotEmpty())
    {
        auto st = r.removeFromBottom (18);
        g.setColour (col::red);
        g.setFont (uiFont (12.5f, true, 0.06f));
        g.drawText (subtitle, st, Justification::centred, false);
    }
    auto lcd = r.removeFromBottom (32).withSizeKeepingCentre (jmin ((float) getWidth() - 8, big ? 100.0f : 90.0f), 25.0f);
    drawLcd (g, lcd, slider.getTextFromValue (slider.getValue()), col::redHi, big ? 15.5f : 14.5f);
}

//==============================================================================
Fader::Fader (APVTS& s, const String& id, const String& t, const String& h, Colour accent) : title (t), hint (h)
{
    slider.setSliderStyle (Slider::LinearVertical);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setColour (Slider::trackColourId, accent);
    slider.getProperties().set ("redCap", accent == col::red);
    slider.setSliderSnapsToMousePosition (false);
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id)) slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}
void Fader::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (44);
    r.removeFromBottom (36);
    slider.setBounds (r);
}
void Fader::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto accent = slider.findColour (Slider::trackColourId);
    auto head = r.removeFromTop (44);
    const auto titleR = head.withHeight (20);
    g.setFont (uiFont (13.5f, true, 0.0f));
    g.setColour (Colours::white.withAlpha (0.75f));
    g.drawFittedText (title, titleR.translated (0, 1.2f).toNearestInt(), Justification::centred, 1, 0.7f);
    g.setColour (col::text);
    g.drawFittedText (title, titleR.toNearestInt(), Justification::centred, 1, 0.7f);
    // activity LED + hint
    {
        const float lit = jlimit (0.0f, 1.0f, (activity + 50.0f) / 40.0f);
        auto led = Rectangle<float> (7, 7).withCentre ({ head.getCentreX(), head.getY() + 39 });
        if (lit > 0.02f) { g.setColour (col::redHi.withAlpha (0.35f * lit)); g.fillEllipse (led.expanded (4)); }
        g.setColour (lit > 0.02f ? col::redHi.withAlpha (0.35f + 0.65f * lit) : Colour (0xff2a2c30));
        g.fillEllipse (led);
        g.setColour (Colours::black);
        g.drawEllipse (led, 0.8f);
        g.setColour (accent == col::red ? col::red : col::textDim);
        g.setFont (uiFont (10.0f, true, 0.04f));
        g.drawFittedText (hint, Rectangle<float> (head.getX(), head.getY() + 20, head.getWidth(), 13).toNearestInt(), Justification::centred, 1, 0.7f);
    }
    auto lcd = r.removeFromBottom (36).withSizeKeepingCentre (jmin ((float) getWidth() - 10, 58.0f), 25.0f);
    drawLcd (g, lcd, String (slider.getValue(), 1), accent == col::red ? col::redHi : Colours::white, 15.0f);
}

//==============================================================================
ChoiceButtons::ChoiceButtons (RangedAudioParameter& param, const StringArray& labels, Colour led, bool v) : vertical (v)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new TextButton (labels[i]));
        b->setColour (TextButton::buttonOnColourId, led);
        b->onClick = [this, i] { if (att) att->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    att = std::make_unique<ParameterAttachment> (param, [this] (float val) { update (roundToInt (val)); });
    att->sendInitialUpdate();
}
void ChoiceButtons::update (int index)
{
    selected = index;
    for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == index, dontSendNotification);
    if (onChange) onChange (index);
}
void ChoiceButtons::resized()
{
    const int n = buttons.size();
    const float step = (float) (vertical ? getHeight() : getWidth()) / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const int a = roundToInt ((float) i * step), b = roundToInt ((float) (i + 1) * step);
        if (vertical) buttons[i]->setBounds (0, a + 2, getWidth(), b - a - 4);
        else          buttons[i]->setBounds (a + 2, 0, b - a - 4, getHeight());
    }
}

//==============================================================================
namespace
{
    float biquadMagDb (const std::array<float, 6>& c, double f, double fs)
    {
        const double w = MathConstants<double>::twoPi * f / fs;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        const auto num = (double) c[0] + (double) c[1] * z1 + (double) c[2] * z2;
        const auto den = (double) c[3] + (double) c[4] * z1 + (double) c[5] * z2;
        return (float) (20.0 * std::log10 (std::max (1.0e-9, std::abs (num / den))));
    }
    constexpr float fMin = 20.0f, fMax = 20000.0f, dbRange = 15.0f;
}

EqDisplay::EqDisplay (VocalFeelProcessor& p) : proc (p)
{
    fftData.assign ((size_t) fftSize * 2, 0.0f);
    specIn.assign (512, -120.0f);
    specOut.assign (512, -120.0f);
    setOpaque (false);
}
float EqDisplay::xForFreq (float f) const { return (float) getWidth() * std::log (f / fMin) / std::log (fMax / fMin); }
float EqDisplay::yForDb (float db) const
{
    const float top = 34.0f, bot = (float) getHeight() - 30.0f;
    return jmap (db, dbRange, -dbRange, top, bot);
}
float EqDisplay::specY (float db) const
{
    const float top = 34.0f, bot = (float) getHeight() - 30.0f;
    return jmap (jlimit (-96.0f, 0.0f, db), 0.0f, -96.0f, top, bot);
}

void EqDisplay::computeSpectrum (const ScopeRing& ring, std::vector<float>& smooth)
{
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    ring.copyLatest (fftData.data(), fftSize);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    const double fs = proc.getFs();
    const int points = (int) smooth.size();
    for (int i = 0; i < points; ++i)
    {
        const float f0 = fMin * std::pow (fMax / fMin, (float) i / (float) points);
        const float f1 = fMin * std::pow (fMax / fMin, (float) (i + 1) / (float) points);
        int b0 = (int) (f0 / fs * fftSize), b1 = jmax (b0, (int) (f1 / fs * fftSize));
        b0 = jlimit (1, fftSize / 2 - 1, b0); b1 = jlimit (1, fftSize / 2 - 1, b1);
        float m = 0; for (int b = b0; b <= b1; ++b) m = jmax (m, fftData[(size_t) b]);
        const float fc = std::sqrt (f0 * f1);
        float db = Decibels::gainToDecibels (m * 4.0f / (float) fftSize, -140.0f) + 3.0f * std::log2 (fc / 1000.0f);   // +3 dB/oct tilt for voice
        float& s = smooth[(size_t) i];
        s = db > s ? s + 0.6f * (db - s) : s + 0.18f * (db - s);
    }
}

void EqDisplay::refresh()
{
    computeSpectrum (proc.scopeIn, specIn);
    computeSpectrum (proc.scopeOut, specOut);
    deessGr += (proc.deessGr.load() - deessGr) * 0.35f;
    repaint();
}

void EqDisplay::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float W = b.getWidth(), H = b.getHeight();
    const float top = 34.0f, bot = H - 30.0f;
    // screen
    g.setGradientFill (ColourGradient (Colour (0xff0b1013), 0, 0, Colour (0xff06080a), 0, H, false));
    g.fillRoundedRectangle (b, 8.0f);
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (b.reduced (1).toNearestInt());

    // grid
    g.setFont (uiFont (11.5f));
    for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
    {
        const float x = xForFreq (f);
        g.setColour (Colours::white.withAlpha (0.08f));
        g.drawVerticalLine (roundToInt (x), top, bot);
        g.setColour (col::screenDim.withAlpha (0.85f));
        g.drawText (f >= 1000 ? String ((int) (f / 1000)) + "k" : String ((int) f), Rectangle<float> (x - 20, bot + 6, 40, 16), Justification::centred, false);
    }
    for (float db : { -12.f, -6.f, 0.f, 6.f, 12.f })
    {
        const float y = yForDb (db);
        g.setColour (Colours::white.withAlpha (db == 0 ? 0.22f : 0.07f));
        g.drawHorizontalLine (roundToInt (y), 0, W);
        g.setColour (col::screenDim.withAlpha (0.75f));
        g.drawText ((db > 0 ? "+" : "") + String ((int) db), Rectangle<float> (W - 36, y - 8, 30, 16), Justification::centredRight, false);
    }

    // spectrum: input (grey) and output (mint)
    auto specPath = [&] (const std::vector<float>& s, bool closed)
    {
        Path p;
        const int n = (int) s.size();
        for (int i = 0; i < n; ++i)
        {
            const float x = W * ((float) i + 0.5f) / (float) n, y = specY (s[(size_t) i]);
            if (i == 0) p.startNewSubPath (0, y); else p.lineTo (x, y);
        }
        if (closed) { p.lineTo (W, bot + 30); p.lineTo (0, bot + 30); p.closeSubPath(); }
        return p;
    };
    g.setColour (Colours::white.withAlpha (0.10f));
    g.fillPath (specPath (specIn, true));
    g.setColour (Colours::white.withAlpha (0.35f));
    g.strokePath (specPath (specIn, false), PathStrokeType (1.2f));
    g.setGradientFill (ColourGradient (col::mint.withAlpha (0.38f), 0, top, col::mint.withAlpha (0.04f), 0, bot, false));
    g.fillPath (specPath (specOut, true));
    g.setColour (col::mint.withAlpha (0.8f));
    g.strokePath (specPath (specOut, false), PathStrokeType (1.4f));

    // ---- processing curve ---------------------------------------------------
    const double fs = proc.getFs();
    const auto& vc = vf::voices()[(size_t) jlimit (0, 2, (int) proc.value ("voice"))];
    const float lo = proc.value ("low"), mi = proc.value ("mid"), hi = proc.value ("high");
    const float ny = (float) fs * 0.45f;
    const auto cLow = vf::AC::makeLowShelf (fs, vc.lowF, 0.7f, vf::dbToGain (lo));
    const auto cMid = vf::AC::makePeakFilter (fs, vc.midF, 0.9f, vf::dbToGain (mi));
    const auto cHigh = vf::AC::makeHighShelf (fs, jmin (vc.highF, ny), 0.7f, vf::dbToGain (hi));
    auto amt = [&] (const char* id) { return VocalFeelProcessor::chainAmount (proc.value (id)); };
    const float aMagic = amt ("magic"), aPres = amt ("presence"), aAtti = amt ("attitude"), aAtt = amt ("attack"), aSpace = amt ("space");
    const auto cMagic = vf::AC::makePeakFilter (fs, vc.magicF, 0.6f, vf::dbToGain (Decibels::gainToDecibels (1.0f + 0.9f * aMagic)));
    const auto cPres = vf::AC::makePeakFilter (fs, vc.presenceF, 1.0f, vf::dbToGain (Decibels::gainToDecibels (1.0f + 1.1f * aPres)));
    const auto cAtti = vf::AC::makePeakFilter (fs, std::sqrt (vc.attitudeLo * vc.attitudeHi), 0.35f, vf::dbToGain (Decibels::gainToDecibels (1.0f + 0.7f * aAtti)));
    const auto cAtt = vf::AC::makePeakFilter (fs, 2500.0f, 0.6f, vf::dbToGain (Decibels::gainToDecibels (1.0f + 0.5f * aAtt)));
    const auto cSpace = vf::AC::makePeakFilter (fs, 1100.0f, 0.4f, vf::dbToGain (Decibels::gainToDecibels (1.0f + 0.35f * aSpace)));
    const float dsF = proc.value ("deess_freq");
    const auto cDs = vf::AC::makeHighShelf (fs, jmin (dsF * 0.8f, ny), 0.7f, vf::dbToGain (-deessGr));

    Path curve, fillP, dsCurve;
    const float y0 = yForDb (0);
    fillP.startNewSubPath (0, y0);
    for (float x = 0; x <= W; x += 2.0f)
    {
        const float f = fMin * std::pow (fMax / fMin, x / W);
        if (f >= fs * 0.5) break;
        float tone = biquadMagDb (cLow, f, fs) + biquadMagDb (cMid, f, fs) + biquadMagDb (cHigh, f, fs);
        float chains = biquadMagDb (cMagic, f, fs) + biquadMagDb (cPres, f, fs) + biquadMagDb (cAtti, f, fs) + biquadMagDb (cAtt, f, fs) + biquadMagDb (cSpace, f, fs);
        float ds = biquadMagDb (cDs, f, fs);
        const float y = yForDb (jlimit (-dbRange - 2, dbRange + 2, tone + chains + ds));
        if (x == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        fillP.lineTo (x, y);
        const float yd = yForDb (jlimit (-dbRange, dbRange, ds));
        if (x == 0) dsCurve.startNewSubPath (x, yd); else dsCurve.lineTo (x, yd);
    }
    fillP.lineTo (W, y0); fillP.closeSubPath();
    g.setColour (col::copper.withAlpha (0.16f));
    g.fillPath (fillP);
    // de-ess band
    {
        const float xd = xForFreq (dsF * 0.8f);
        const float amount = proc.value ("deess") * 0.01f;
        g.setGradientFill (ColourGradient (col::violet.withAlpha (0.05f + 0.16f * amount), xd, 0, col::violet.withAlpha (0.0f), W, 0, false));
        g.fillRect (xd, top, W - xd, bot - top);
        g.setColour (col::violet.withAlpha (0.8f));
        const float dashes[] = { 4.0f, 4.0f };
        g.drawDashedLine ({ xd, top, xd, bot }, dashes, 2, 1.2f);
        if (deessGr > 0.05f)
        {
            g.setColour (col::violet);
            g.strokePath (dsCurve, PathStrokeType (1.6f));
        }
        g.setFont (uiFont (12.0f));
        g.setColour (col::violet.brighter (0.3f));
        g.drawText ("DE-ESS  -" + String (deessGr, 1) + " dB", Rectangle<float> (jmin (xd + 6, W - 185), top + 4, 140, 16), Justification::centredLeft, false);
    }
    g.setColour (col::copper.withAlpha (0.35f));
    g.strokePath (curve, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (col::copper.brighter (0.25f));
    g.strokePath (curve, PathStrokeType (2.4f, PathStrokeType::curved, PathStrokeType::rounded));

    // EQ nodes
    auto node = [&] (float f, float db, const String& lbl)
    {
        const float x = xForFreq (f), y = yForDb (jlimit (-dbRange, dbRange, db));
        g.setColour (Colours::black.withAlpha (0.6f)); g.fillEllipse (x - 8, y - 8, 16, 16);
        g.setColour (col::copper); g.fillEllipse (x - 6, y - 6, 12, 12);
        g.setColour (Colours::white); g.drawEllipse (x - 6, y - 6, 12, 12, 1.4f);
        g.setFont (uiFont (12.0f));
        g.drawText (lbl, Rectangle<float> (x - 30, y + (db >= 0 ? -26.0f : 9.0f), 60, 16), Justification::centred, false);
    };
    node (vc.lowF, lo, "LOW"); node (vc.midF, mi, "MID"); node (jmin (vc.highF, ny), hi, "HIGH");

    // chain tags along the bottom
    struct Tag { float f; const char* name; float a; Colour c; };
    const Tag tags[] = { { vc.attitudeLo * 2.2f, "ATTITUDE", aAtti, col::rose }, { vc.magicF, "MAGIC", aMagic, col::mint },
                         { vc.presenceF * 1.35f, "PRESENCE", aPres, col::gold }, { 1300.0f, "SPACE", aSpace, Colour (0xff6fb6ff) } };
    g.setFont (uiFont (11.0f, true, 0.06f));
    int row = 0;
    for (auto& t : tags)
    {
        if (t.a < 0.005f) continue;
        const float x = xForFreq (t.f);
        auto r = Rectangle<float> (74, 18).withCentre ({ x, bot - 14 - 22.0f * (float) row++ });
        g.setColour (t.c.withAlpha (0.18f + 0.5f * t.a)); g.fillRoundedRectangle (r, 9);
        g.setColour (t.c); g.drawRoundedRectangle (r, 9, 1.0f);
        g.setColour (Colours::white); g.drawText (t.name, r, Justification::centred, false);
    }

    // legend
    {
        float x = 12;
        auto item = [&] (const String& s, Colour c)
        {
            g.setColour (c); g.fillRoundedRectangle (x, 13, 16, 5, 2);
            g.setColour (col::screenText); g.setFont (uiFont (12.0f));
            g.drawText (s, Rectangle<float> (x + 22, 6, 160, 18), Justification::centredLeft, false);
            x += 22 + GlyphArrangement::getStringWidth (g.getCurrentFont(), s) + 18;
        };
        item ("IN", Colours::white.withAlpha (0.5f));
        item ("OUT", col::mint);
        item ("YOUR CHANGES", col::copper);
        item ("DE-ESS", col::violet);
    }
    g.setColour (Colours::black);
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.5f);
}

//==============================================================================
void Meters::set (float in, float out, float comp, float deess)
{
    const float t[4] = { in, out, comp, deess };
    for (int i = 0; i < 4; ++i)
    {
        const bool gr = i >= 2;
        v[i] = gr ? v[i] + (t[i] - v[i]) * 0.35f : (t[i] > v[i] ? t[i] : v[i] + (t[i] - v[i]) * 0.15f);
        if (v[i] > hold[i] || ++holdT[i] > 45) { hold[i] = v[i]; holdT[i] = 0; }
    }
    repaint();
}
void Meters::bar (Graphics& g, Rectangle<float> r, const String& name, float value, float hl, bool gr, Colour c)
{
    auto label = r.removeFromBottom (22);
    auto num = r.removeFromBottom (26);
    g.setFont (uiFont (13.0f, true, 0.04f));
    g.setColour (col::screenText);
    g.drawFittedText (name, label.toNearestInt(), Justification::centred, 1, 0.7f);
    drawLcd (g, num.reduced (2, 2), gr ? (value > 0.05f ? "-" + String (value, 1) : String ("0.0")) : (value < -60 ? String ("-inf") : String (value, 1)), c.brighter (0.3f), 13.0f);
    r.removeFromBottom (6);
    g.setColour (Colour (0xff050608));
    g.fillRoundedRectangle (r, 4);
    auto in = r.reduced (4);
    const int segs = 30;
    const float sh = in.getHeight() / (float) segs;
    float frac, hfrac;
    if (gr) { frac = jlimit (0.0f, 1.0f, value / 20.0f); hfrac = jlimit (0.0f, 1.0f, hl / 20.0f); }
    else    { frac = jlimit (0.0f, 1.0f, (value + 48.0f) / 54.0f); hfrac = jlimit (0.0f, 1.0f, (hl + 48.0f) / 54.0f); }
    for (int s = 0; s < segs; ++s)
    {
        const float pos = ((float) s + 0.5f) / (float) segs;
        auto seg = gr ? Rectangle<float> (in.getX(), in.getY() + s * sh, in.getWidth(), sh - 2)
                      : Rectangle<float> (in.getX(), in.getBottom() - (s + 1) * sh + 2, in.getWidth(), sh - 2);
        Colour sc = c;
        if (! gr) { const float db = -48.0f + pos * 54.0f; sc = db > 0 ? col::rose : (db > -9 ? col::gold : col::mint); }
        const bool lit = pos <= frac;
        const bool isHold = std::abs (pos - hfrac) < 0.5f / segs && hfrac > 0.02f;
        g.setColour (lit || isHold ? sc : sc.withAlpha (0.10f));
        g.fillRoundedRectangle (seg, 1.5f);
    }
}
void Meters::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (Colour (0xff15181c), 0, 0, Colour (0xff07080a), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.5f);
    r = r.reduced (4, 8);
    const float w = r.getWidth() / 4.0f;
    // dB scale strip drawn inside each IN/OUT bar label area is enough; keep it clean
    bar (g, r.removeFromLeft (w).reduced (6, 0), "IN", v[0], hold[0], false, col::mint);
    bar (g, r.removeFromLeft (w).reduced (6, 0), "OUT", v[1], hold[1], false, col::mint);
    bar (g, r.removeFromLeft (w).reduced (6, 0), "COMP", v[2], hold[2], true, col::copper);
    bar (g, r.removeFromLeft (w).reduced (6, 0), "DE-ESS", v[3], hold[3], true, col::violet);
}

//==============================================================================
namespace
{
    const Rectangle<int> toneW   { 24, 92, 380, 300 };
    const Rectangle<int> eqW     { 416, 92, 552, 300 };
    const Rectangle<int> meterW  { 980, 92, 276, 300 };
    const Rectangle<int> sensW   { 24, 404, 160, 332 };
    const Rectangle<int> deessW  { 196, 404, 254, 332 };
    const Rectangle<int> compW   { 462, 404, 254, 332 };
    const Rectangle<int> chainW  { 728, 404, 528, 332 };
    const char* colourInfo[] = { "Clean, transparent", "Thick low end, round", "Triode glow, 2nd harm." };
}

MainPanel::MainPanel (VocalFeelProcessor& p)
    : proc (p), eq (p),
      voice (*p.apvts.getParameter ("voice"), { "MALE", "MALE 2", "FEMALE" }, col::redHi),
      colour (*p.apvts.getParameter ("comp_colour"), { "FLAT", "WARM", "TUBE" }, col::redHi, true),
      sens (p.apvts, "sens", "SENS", col::red, true, true),
      low (p.apvts, "low", "LOW", col::red, false, true),
      mid (p.apvts, "mid", "MID", col::red, false, true),
      high (p.apvts, "high", "HIGH", col::red, false, true),
      deess (p.apvts, "deess", "AMOUNT", col::red), deessFreq (p.apvts, "deess_freq", "FREQ", col::red),
      comp (p.apvts, "comp", "COMP", col::red, true),
      main (p.apvts, "main", "MAIN", "DRY LEVEL", col::red),
      magic (p.apvts, "magic", "MAGIC", "SHINE+DBL", col::textDim),
      space (p.apvts, "space", "SPACE", "SLAP+PLATE", col::textDim),
      attack (p.apvts, "attack", "ATTACK", "CONSONANTS", col::textDim),
      attitude (p.apvts, "attitude", "ATTITUDE", "DRIVE+POWER", col::textDim),
      presence (p.apvts, "presence", "PRESENCE", "CLARITY", col::textDim),
      master (p.apvts, "master", "MASTER", "OUTPUT", col::red)
{
    setLookAndFeel (&lnf);
    setOpaque (true);
    for (Component* c : std::initializer_list<Component*> { &eq, &meters, &voice, &colour, &sens, &low, &mid, &high, &deess, &deessFreq, &comp,
                                                            &main, &magic, &space, &attack, &attitude, &presence, &master, &presetBox })
        addAndMakeVisible (c);
    colour.onChange = [this] (int) { repaint (compW); };
    deessFreq.subtitle = " ";
    deess.subtitle = " ";

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id > 0 && id - 1 != proc.getCurrentProgram()) proc.setCurrentProgram (id - 1); };
    updateSubtitles();
    startTimerHz (30);
}

MainPanel::~MainPanel() { stopTimer(); setLookAndFeel (nullptr); }

void MainPanel::updateSubtitles()
{
    const int v = jlimit (0, 2, (int) proc.value ("voice"));
    lastVoice = v;
    const auto& vc = vf::voices()[(size_t) v];
    auto hz = [] (float f) { return f >= 1000 ? String (f / 1000.0f, f >= 10000 ? 0 : 1) + " kHz" : String (roundToInt (f)) + " Hz"; };
    low.subtitle = "@ " + hz (vc.lowF);
    mid.subtitle = "@ " + hz (vc.midF);
    high.subtitle = "@ " + hz (vc.highF);
    for (auto* k : { &low, &mid, &high }) { k->resized(); k->repaint(); }
}

void MainPanel::resized()
{
    renderTexture();
    voice.setBounds (520, 24, 330, 40);
    presetBox.setBounds (W - 24 - 270, 26, 270, 36);

    // tone EQ
    {
        const int kw = (toneW.getWidth() - 28) / 3;
        Knob* k[] = { &low, &mid, &high };
        for (int i = 0; i < 3; ++i) k[i]->setBounds (toneW.getX() + 14 + i * kw, toneW.getY() + 46, kw, 214);
    }
    eq.setBounds (eqW.reduced (12).withTrimmedTop (36));
    meters.setBounds (meterW.reduced (12).withTrimmedTop (38));

    sens.setBounds (sensW.getX() + 8, sensW.getY() + 50, sensW.getWidth() - 16, 200);

    deess.setBounds     (deessW.getX() + 10,  deessW.getY() + 46, 117, 214);
    deessFreq.setBounds (deessW.getX() + 127, deessW.getY() + 46, 117, 214);

    comp.setBounds   (compW.getX() + 8, compW.getY() + 46, 140, 214);
    colour.setBounds (compW.getX() + 152, compW.getY() + 64, 90, 150);

    {
        Fader* f[] = { &main, &magic, &space, &attack, &attitude, &presence, &master };
        const float fw = (float) (chainW.getWidth() - 20) / 7.0f;
        for (int i = 0; i < 7; ++i)
            f[i]->setBounds (chainW.getX() + 10 + roundToInt ((float) i * fw), chainW.getY() + 42, roundToInt (fw), chainW.getHeight() - 50);
    }
}

void MainPanel::renderTexture()
{
    texture = Image (Image::RGB, W, H, true);
    Graphics g (texture);
    // brushed aluminium plate
    g.setGradientFill (ColourGradient (col::bgTop, 0, 0, col::bgBot, 0, (float) H, false));
    g.fillAll();
    Random rnd (77);
    for (int y = 0; y < H; ++y)
        for (int k = 0; k < 7; ++k)
        {
            const float x0 = rnd.nextFloat() * W, len = 60.0f + rnd.nextFloat() * 320.0f;
            g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (0.03f + rnd.nextFloat() * 0.05f));
            g.fillRect (x0, (float) y, len, 1.0f);
        }
    // polished sheen bands
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.0f), 0, 0, Colours::white.withAlpha (0.0f), (float) W, 0, false));
    {
        ColourGradient sh (Colours::white.withAlpha (0.0f), 0, 0, Colours::white.withAlpha (0.0f), (float) W, (float) H * 0.4f, false);
        sh.addColour (0.35, Colours::white.withAlpha (0.35f));
        sh.addColour (0.5, Colours::white.withAlpha (0.0f));
        sh.addColour (0.72, Colours::white.withAlpha (0.18f));
        g.setGradientFill (sh);
        g.fillAll();
    }
    g.setGradientFill (ColourGradient (Colours::transparentBlack, W * 0.5f, H * 0.45f, Colours::black.withAlpha (0.18f), 0, 0, true));
    g.fillAll();
    // header band (black, like a hardware nameplate)
    g.setGradientFill (ColourGradient (Colour (0xff26282c), 0, 0, Colour (0xff0d0e10), 0, 80, false));
    g.fillRect (0, 0, W, 80);
    g.setColour (col::red);
    g.fillRect (0, 80, W, 3);
    g.setColour (Colours::white.withAlpha (0.6f));
    g.fillRect (0, 83, W, 1);
    g.setColour (col::red);
    g.fillRect (0, H - 4, W, 4);
    // screws
    for (auto pt : { Point<float> (12, (float) H - 18), Point<float> ((float) W - 12, (float) H - 18), Point<float> (12, 98), Point<float> ((float) W - 12, 98) })
    {
        g.setGradientFill (ColourGradient (Colours::white, pt.x - 5, pt.y - 5, Colour (0xff6d727a), pt.x + 5, pt.y + 5, false));
        g.fillEllipse (pt.x - 6, pt.y - 6, 12, 12);
        g.setColour (Colour (0xff3a3d42));
        g.drawEllipse (pt.x - 6, pt.y - 6, 12, 12, 0.8f);
        g.drawLine (pt.x - 4, pt.y + 2, pt.x + 4, pt.y - 2, 1.6f);
    }
}

void MainPanel::drawWell (Graphics& g, Rectangle<int> r, const String& title, Colour accent)
{
    auto rf = r.toFloat();
    // raised silver panel with an engraved title
    DropShadow (Colours::black.withAlpha (0.28f), 12, { 2, 5 }).drawForRectangle (g, r);
    g.setGradientFill (ColourGradient (Colour (0xfff7f8f9), 0, rf.getY(), Colour (0xffd3d6da), 0, rf.getBottom(), false));
    g.fillRoundedRectangle (rf, 10.0f);
    g.setColour (Colours::white);
    g.drawRoundedRectangle (rf.reduced (1.0f), 10.0f, 1.2f);
    g.setColour (Colour (0xff8d929a));
    g.drawRoundedRectangle (rf.reduced (0.3f), 10.0f, 1.0f);
    g.setColour (accent);
    g.fillRoundedRectangle (rf.getX() + 14, rf.getY() + 11, 5, 18, 2.5f);
    embossed (g, title, Rectangle<float> (rf.getX() + 28, rf.getY() + 4, rf.getWidth() - 40, 30), uiFont (16.5f, true, 0.14f), col::text, Justification::centredLeft);
    g.setColour (Colour (0xffb4b8be));
    g.fillRect (rf.getX() + 12, rf.getY() + 37, rf.getWidth() - 24, 1.0f);
    g.setColour (Colours::white);
    g.fillRect (rf.getX() + 12, rf.getY() + 38, rf.getWidth() - 24, 1.0f);
}

void MainPanel::miniGr (Graphics& g, Rectangle<float> r, float gr, Colour c)
{
    embossed (g, "GR", r.removeFromLeft (30), uiFont (13.0f), col::textDim, Justification::centredLeft);
    auto lcd = r.removeFromRight (74);
    drawLcd (g, lcd.reduced (0, 2), "-" + String (gr, 1) + " dB", c.brighter (0.3f), 13.5f);
    r.removeFromRight (8);
    auto track = r.withSizeKeepingCentre (r.getWidth(), 10);
    g.setColour (Colour (0xff050608));
    g.fillRoundedRectangle (track, 5);
    const float frac = jlimit (0.0f, 1.0f, gr / 15.0f);
    if (frac > 0.003f)
    {
        g.setColour (c.withAlpha (0.35f));
        g.fillRoundedRectangle (track.withWidth (track.getWidth() * frac).expanded (0, 3), 5);
        g.setColour (c);
        g.fillRoundedRectangle (track.withWidth (track.getWidth() * frac).reduced (0, 2), 3);
    }
}

void MainPanel::paint (Graphics& g)
{
    g.drawImage (texture, getLocalBounds().toFloat());

    // header
    {
        auto logo = Rectangle<float> (26, 10, 440, 44);
        g.setFont (uiFont (40.0f, true, 0.14f));
        g.setColour (Colours::black.withAlpha (0.8f));
        g.drawText ("VOCAL FEEL", logo.translated (0, 2), Justification::centredLeft, false);
        g.setGradientFill (ColourGradient (Colours::white, logo.getX(), logo.getY(), Colour (0xffaeb4bc), logo.getX(), logo.getBottom(), false));
        g.drawText ("VOCAL", logo, Justification::centredLeft, false);
        const float vw = GlyphArrangement::getStringWidth (g.getCurrentFont(), "VOCAL ");
        g.setColour (col::redHi);
        g.drawText ("FEEL", logo.withTrimmedLeft (vw), Justification::centredLeft, false);
        embossed (g, "ROHIT JI   |   VOCAL MULTI-CHAIN PROCESSOR", Rectangle<float> (28, 52, 480, 20), uiFont (13.5f, true, 0.18f), col::screenDim, Justification::centredLeft);
        embossed (g, "VOICE", Rectangle<float> (440, 24, 72, 40), uiFont (14.0f, true, 0.18f), col::screenDim, Justification::centredRight);
        embossed (g, "PRESET", Rectangle<float> ((float) W - 24 - 270 - 90, 26, 82, 36), uiFont (14.0f, true, 0.18f), col::screenDim, Justification::centredRight);
    }

    drawWell (g, toneW, "TONE EQ", col::red);
    drawWell (g, eqW, "VISUAL EQ  /  ANALYZER", col::red);
    drawWell (g, meterW, "METERS", col::red);
    drawWell (g, sensW, "INPUT", col::red);
    drawWell (g, deessW, "DE-ESSER  (DYNAMIC)", col::red);
    drawWell (g, compW, "OPTO COMP", col::red);
    drawWell (g, chainW, "VOCAL CHAINS", col::red);

    // tone EQ footer
    {
        auto r = Rectangle<float> ((float) toneW.getX() + 14, (float) toneW.getBottom() - 34, (float) toneW.getWidth() - 28, 20);
        g.setColour (col::textDim);
        g.setFont (uiFont (12.5f, false));
        g.drawText ("Centres follow the VOICE mode  |  double-click = reset", r, Justification::centred, false);
    }
    // sens signal LED
    {
        const float lvl = inLvl;
        const Colour c = lvl > -1.0f ? col::rose : (lvl > -12.0f ? col::gold : (lvl > -50.0f ? col::mint : Colour (0xff2a2c30)));
        auto led = Rectangle<float> (14, 14).withCentre ({ (float) sensW.getCentreX() - 36, (float) sensW.getY() + 276 });
        if (lvl > -50.0f) { g.setColour (c.withAlpha (0.35f)); g.fillEllipse (led.expanded (5)); }
        g.setColour (c); g.fillEllipse (led);
        g.setColour (Colours::black); g.drawEllipse (led, 1.0f);
        embossed (g, "SIGNAL", Rectangle<float> (led.getRight() + 8, led.getY() - 3, 80, 20), uiFont (13.0f), col::text, Justification::centredLeft);
        g.setColour (col::textDim);
        g.setFont (uiFont (11.5f, false));
        g.drawText ("aim for yellow", Rectangle<float> ((float) sensW.getX(), (float) sensW.getY() + 292, (float) sensW.getWidth(), 18), Justification::centred, false);
    }
    // de-ess / comp gain-reduction strips
    miniGr (g, Rectangle<float> ((float) deessW.getX() + 16, (float) deessW.getY() + 272, (float) deessW.getWidth() - 32, 30), dGr, col::violet);
    {
        g.setColour (col::textDim);
        g.setFont (uiFont (11.5f, false));
        g.drawText ("Only the band above FREQ is reduced", Rectangle<float> ((float) deessW.getX() + 8, (float) deessW.getY() + 304, (float) deessW.getWidth() - 16, 18), Justification::centred, false);
    }
    miniGr (g, Rectangle<float> ((float) compW.getX() + 16, (float) compW.getY() + 272, (float) compW.getWidth() - 32, 30), cGr, col::redHi);
    {
        embossed (g, "COLOUR", Rectangle<float> ((float) compW.getX() + 152, (float) compW.getY() + 42, 90, 20), uiFont (12.5f, true, 0.14f), col::textDim, Justification::centred);
        g.setColour (col::red);
        g.setFont (uiFont (11.5f, false));
        g.drawText (colourInfo[jlimit (0, 2, colour.getSelected())], Rectangle<float> ((float) compW.getX() + 8, (float) compW.getY() + 304, (float) compW.getWidth() - 16, 18), Justification::centred, false);
    }
    // fader group separators
    {
        const float fw = (float) (chainW.getWidth() - 20) / 7.0f;
        g.setColour (Colour (0xff9a9fa6));
        for (int i : { 1, 6 })
        {
            const float x = (float) chainW.getX() + 10 + (float) i * fw;
            g.fillRect (x - 0.5f, (float) chainW.getY() + 48, 1.0f, (float) chainW.getHeight() - 62);
        }
    }
    embossed (g, "v1.0  |  ROHIT JI", Rectangle<float> ((float) W - 24 - 300, (float) H - 20, 300, 14), uiFont (11.0f, true, 0.2f), col::textDim, Justification::centredRight);
}

void MainPanel::timerCallback()
{
    auto sm = [] (float& v, float t) { v = t > v ? t : v + (t - v) * 0.2f; };
    sm (inLvl, proc.inLevel.load()); sm (outLvl, proc.outLevel.load());
    cGr += (proc.compGr.load() - cGr) * 0.35f;
    dGr += (proc.deessGr.load() - dGr) * 0.35f;
    meters.set (inLvl, outLvl, cGr, dGr);
    eq.refresh();
    Fader* f[] = { &magic, &space, &attack, &attitude, &presence };
    for (int k = 0; k < 5; ++k)
    {
        const float a = proc.chainLevel[k].load();
        if (std::abs (a - f[k]->activity) > 0.5f) { f[k]->activity = a; f[k]->repaint(); }
    }
    main.activity = proc.value ("main") > 0.05f ? outLvl : -100.0f;
    repaint (sensW); repaint (deessW.withTrimmedTop (260)); repaint (compW.withTrimmedTop (260));
    if ((int) proc.value ("voice") != lastVoice) updateSubtitles();
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1) presetBox.setSelectedId (proc.getCurrentProgram() + 1, dontSendNotification);
}
} // namespace vgui

//==============================================================================
VocalFeelEditor::VocalFeelEditor (VocalFeelProcessor& p) : AudioProcessorEditor (&p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (vgui::MainPanel::W * 3 / 5, vgui::MainPanel::H * 3 / 5, vgui::MainPanel::W * 2, vgui::MainPanel::H * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) vgui::MainPanel::W / (double) vgui::MainPanel::H);
    setSize (vgui::MainPanel::W, vgui::MainPanel::H);
}
void VocalFeelEditor::resized()
{
    panel.setBounds (0, 0, vgui::MainPanel::W, vgui::MainPanel::H);
    panel.setTransform (AffineTransform::scale ((float) getWidth() / (float) vgui::MainPanel::W));
}
