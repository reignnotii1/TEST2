#pragma once
#include "PluginProcessor.h"

namespace ui
{
    const juce::Colour pink { 0xffe0218a };

    struct Look : juce::LookAndFeel_V4
    {
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    };

    struct Lcd : juce::Component
    {
        juce::String text;
        void set (const juce::String& s) { text = s; repaint(); }
        void paint (juce::Graphics&) override;
    };

    struct PillSwitch : juce::ToggleButton
    {
        void paintButton (juce::Graphics&, bool, bool) override;
    };

    struct ChopDisplay : juce::Component
    {
        explicit ChopDisplay (ChopProcessor& p) : proc (p) {}
        void paint (juce::Graphics&) override;
        ChopProcessor& proc;
    };
}

class ChopEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChopEditor (ChopProcessor&);
    ~ChopEditor() override { setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { display.repaint(); }
    void updateLcds();

    ChopProcessor& proc;
    ui::Look look;
    juce::Slider chop, refresh, gate, fade, mix, output;
    ui::Lcd lChop, lRefresh, lGate, lFade, lMix, lOut;
    ui::PillSwitch freeze;
    ui::ChopDisplay display;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> aChop, aRefresh, aGate, aFade, aMix, aOut;
    std::unique_ptr<BA> aFreeze;
};
