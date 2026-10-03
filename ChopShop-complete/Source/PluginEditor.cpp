#include "PluginEditor.h"
using namespace juce;
using namespace ui;

void Look::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider&)
{
    const bool big = w > 80;
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float cx = b.getCentreX(), cy = b.getCentreY();
    const float r = (big ? 0.78f : 0.9f) * jmin (w, h) * 0.5f;
    const float ang = a0 + pos * (a1 - a0);

    if (big) // ring of dots, lit up to the current value
    {
        const int N = 21;
        for (int i = 0; i < N; ++i)
        {
            float f = i / (float) (N - 1), a = a0 + f * (a1 - a0), rr = r + 15.f;
            bool lit = f <= pos + 0.001f;
            float s = lit ? 3.4f : 2.6f;
            g.setColour (lit ? pink : Colour (0xffb9b9bd));
            g.fillEllipse (cx + rr * std::sin (a) - s / 2, cy - rr * std::cos (a) - s / 2, s, s);
        }
    }
    Path p; p.addEllipse (cx - r, cy - r, r * 2, r * 2);
    DropShadow (Colours::black.withAlpha (0.28f), big ? 12 : 7, { 0, big ? 6 : 3 }).drawForPath (g, p);
    g.setGradientFill (ColourGradient (Colours::white, cx, cy - r, Colour (0xffe6e6e9), cx, cy + r, false));
    g.fillPath (p);
    g.setColour (Colours::white.withAlpha (0.9f)); g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 1.f);
    g.setColour (pink);
    g.drawLine (cx + r * 0.45f * std::sin (ang), cy - r * 0.45f * std::cos (ang),
                cx + r * 0.82f * std::sin (ang), cy - r * 0.82f * std::cos (ang), big ? 3.2f : 2.4f);
}

void Lcd::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff111113)); g.fillRoundedRectangle (b, 6.f);
    g.setColour (pink);
    g.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), b.getHeight() * 0.52f, Font::bold)));
    g.drawText (text, getLocalBounds(), Justification::centred);
}

void PillSwitch::paintButton (Graphics& g, bool, bool)
{
    auto b = getLocalBounds().toFloat().reduced (2);
    g.setColour (Colour (0xffd8d8dc)); g.fillRoundedRectangle (b, b.getHeight() / 2);
    g.setColour (getToggleState() ? pink.withAlpha (0.85f) : Colour (0xfff4f4f6));
    g.fillRoundedRectangle (b.reduced (2), (b.getHeight() - 4) / 2);
    float d = b.getHeight() - 6;
    float x = getToggleState() ? b.getRight() - d - 3 : b.getX() + 3;
    DropShadow (Colours::black.withAlpha (0.25f), 4, { 0, 2 }).drawForPath (g, [&] { Path p; p.addEllipse (x, b.getY() + 3, d, d); return p; }());
    g.setColour (Colours::white); g.fillEllipse (x, b.getY() + 3, d, d);
}

void ChopDisplay::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0e0e10)); g.fillRoundedRectangle (b, 8.f);
    const double cb = ChopProcessor::chopBeats[jlimit (0, 9, (int) proc.apvts.getRawParameterValue ("chop")->load())];
    const double rb = ChopProcessor::refreshBeats[jlimit (0, 7, (int) proc.apvts.getRawParameterValue ("refresh")->load())];
    const float gate = proc.apvts.getRawParameterValue ("gate")->load() / 100.f;
    const int cells = jlimit (1, 64, (int) std::round (rb / cb));
    const float cw = b.getWidth() / (float) cells;
    const float phase = proc.uiRefreshPhase.load();
    const bool frozen = proc.apvts.getRawParameterValue ("freeze")->load() > 0.5f;

    for (int i = 0; i < cells; ++i)
    {
        auto c = Rectangle<float> (b.getX() + i * cw, b.getY() + 10, cw * gate, b.getHeight() - 20);
        g.setColour (pink.withAlpha ((i == 0 && ! frozen) ? 0.55f : 0.22f));
        g.fillRoundedRectangle (c.reduced (0, 0), 2.f);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) (b.getX() + i * cw), b.getY(), b.getBottom());
    }
    g.setColour (Colours::white.withAlpha (0.08f)); g.drawHorizontalLine ((int) b.getCentreY(), b.getX(), b.getRight());
    const float px = b.getX() + phase * b.getWidth();
    g.setColour (pink.withAlpha (0.25f)); g.fillRect (px - 6, b.getY(), 12.f, b.getHeight());
    g.setColour (pink); g.fillRect (px - 1, b.getY(), 2.f, b.getHeight());
}

ChopEditor::ChopEditor (ChopProcessor& p) : AudioProcessorEditor (&p), proc (p), display (p)
{
    setLookAndFeel (&look);
    setSize (830, 360);

    for (auto* s : { &chop, &refresh, &gate, &fade, &mix, &output })
    {
        s->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        s->setMouseDragSensitivity (200);
        s->onValueChange = [this] { updateLcds(); };
        addAndMakeVisible (*s);
    }
    for (auto* l : { &lChop, &lRefresh, &lGate, &lFade, &lMix, &lOut }) addAndMakeVisible (*l);
    addAndMakeVisible (freeze); addAndMakeVisible (display);

    aChop    = std::make_unique<SA> (proc.apvts, "chop", chop);
    aRefresh = std::make_unique<SA> (proc.apvts, "refresh", refresh);
    aGate    = std::make_unique<SA> (proc.apvts, "gate", gate);
    aFade    = std::make_unique<SA> (proc.apvts, "fade", fade);
    aMix     = std::make_unique<SA> (proc.apvts, "mix", mix);
    aOut     = std::make_unique<SA> (proc.apvts, "output", output);
    aFreeze  = std::make_unique<BA> (proc.apvts, "freeze", freeze);
    updateLcds();
    startTimerHz (30);
}

void ChopEditor::updateLcds()
{
    auto pct = [] (double v) { return String (roundToInt (v)) + "%"; };
    lChop.set    (ChopProcessor::chopNames    [jlimit (0, 9, (int) chop.getValue())]);
    lRefresh.set (ChopProcessor::refreshNames [jlimit (0, 7, (int) refresh.getValue())]);
    lGate.set (pct (gate.getValue())); lFade.set (pct (fade.getValue())); lMix.set (pct (mix.getValue()));
    lOut.set (String (output.getValue() >= 0 ? "+" : "") + String (output.getValue(), 1) + " dB");
}

void ChopEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xffdedee1));
    Path panel; panel.addRoundedRectangle (15.f, 15.f, 800.f, 330.f, 26.f, 26.f, 26.f, 140.f, true, true, true, true);
    DropShadow (Colours::black.withAlpha (0.25f), 18, { 0, 8 }).drawForPath (g, panel);
    g.setGradientFill (ColourGradient (Colours::white, 0, 15, Colour (0xfff0f0f2), 0, 345, false)); g.fillPath (panel);

    g.setColour (Colour (0xff3a3a3e));
    g.setFont (Font (FontOptions (21.f, Font::bold))); g.drawText ("ChopShop", 38, 40, 200, 28, Justification::left);
    g.setFont (Font (FontOptions (13.f))); g.setColour (Colour (0xff8a8a90)); g.drawText ("vocal chopper", 38, 66, 200, 18, Justification::left);

    g.setColour (Colours::black); g.setFont (Font (FontOptions (14.f, Font::bold)));
    auto lab = [&] (const String& t, Component& c, int dy) { g.drawText (t, c.getBounds().getX() - 10, c.getBottom() + dy, c.getWidth() + 20, 18, Justification::centred); };
    lab ("CHOP", lChop, 4); lab ("REFRESH", lRefresh, 4); lab ("FREEZE", freeze, 6);
    lab ("GATE", lGate, 0); lab ("FADE", lFade, 0); lab ("MIX", lMix, 0); lab ("OUTPUT", lOut, 0);

    g.setColour (Colour (0xffe4e4e8)); g.drawRoundedRectangle (30.f, 262.f, 560.f, 62.f, 30.f, 1.5f);
}

void ChopEditor::resized()
{
    display.setBounds (265, 38, 520, 100);
    chop.setBounds    (45, 150, 100, 100);   lChop.setBounds    (160, 172, 80, 38);
    refresh.setBounds (290, 150, 100, 100);  lRefresh.setBounds (405, 172, 80, 38);
    freeze.setBounds  (580, 178, 70, 34);

    const int y = 268;
    gate.setBounds   (42,  y, 44, 44);  lGate.setBounds   (92,  y + 9, 66, 26);
    fade.setBounds   (180, y, 44, 44);  lFade.setBounds   (230, y + 9, 66, 26);
    mix.setBounds    (318, y, 44, 44);  lMix.setBounds    (368, y + 9, 66, 26);
    output.setBounds (456, y, 44, 44);  lOut.setBounds    (506, y + 9, 76, 26);
}
