#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace brasa::ui
{
namespace col
{
    const juce::Colour bg1 { 0xff1c1613 }, bg2 { 0xff0e0b0a }, panel { 0xff221b17 }, panelEdge { 0xff3b2e27 },
        ash { 0xffe6dccf }, ashDim { 0xff9a8b80 }, ember { 0xffff7a2a }, emberHot { 0xffffb347 }, flame { 0xffe2412a },
        coal { 0xff0b0908 }, metal { 0xff3a332f };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
};

// Knob with caption and live value
class Knob : public juce::Component, private juce::Slider::Listener
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const char* paramId, juce::String caption, bool big = false);
    ~Knob() override { slider.removeListener (this); }
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
    std::function<juce::String (double)> valueText;   // optional override

private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }
    juce::String caption;
    bool big;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// Pill toggle with an ember LED
class Toggle : public juce::ToggleButton
{
public:
    Toggle (juce::AudioProcessorValueTreeState& s, const char* paramId, juce::String label, bool hot = false);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    bool hot;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// The five circuits
class StyleSelector : public juce::Component
{
public:
    explicit StyleSelector (juce::AudioProcessorValueTreeState& s);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

private:
    int segmentAt (juce::Point<float>) const;
    int index = 0, hover = -1;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> att;
};

// "Heat": how hard the circuit is being driven
class HeatMeter : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void setLevel (float db, float dt);

private:
    float shown = -40.0f, peak = -40.0f, peakHold = 0.0f;
};
}

class BrasaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BrasaEditor (BrasaProcessor&);
    ~BrasaEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void fillPresetMenu();
    void step (int delta);

    BrasaProcessor& proc;
    brasa::ui::LookAndFeel lnf;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    std::unique_ptr<brasa::ui::Toggle> hq, punish, thump, steep, autoGain;
    std::unique_ptr<brasa::ui::Knob> drive, output, mix, lowCut, tone, highCut;
    std::unique_ptr<brasa::ui::StyleSelector> style;
    brasa::ui::HeatMeter heat;

    juce::Rectangle<int> header, drivePanel, centrePanel, outPanel, filterPanel;
    int shownProgram = -1;
    double lastTick = 0;
    float shownAutoDb = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrasaEditor)
};
