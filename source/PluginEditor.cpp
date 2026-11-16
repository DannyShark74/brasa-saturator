#include "PluginEditor.h"

using namespace brasa;
using namespace brasa::ui;

namespace
{
    juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
    constexpr int kW = 840, kH = 500;

    const char* styleBlurb (int s)
    {
        static const char* t[kNumStyles] = {
            "Tube tape amp: smooth, thick, a little dark",
            "Transistor console: full lows, airy top",
            "Germanium + iron: weight and grip on the lows",
            "Triode: warm 2nd harmonic, punch, squash",
            "Pentode: bite, grit and fizz, the most aggressive",
        };
        return (s >= 0 && s < kNumStyles) ? t[s] : "";
    }

    void panelBg (juce::Graphics& g, juce::Rectangle<int> rr, const char* title)
    {
        auto r = rr.toFloat();
        g.setGradientFill (juce::ColourGradient (col::panel.brighter (0.06f), r.getX(), r.getY(), col::panel.darker (0.25f),
                                                 r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        g.drawLine (r.getX() + 10, r.getY() + 1.5f, r.getRight() - 10, r.getY() + 1.5f, 1.0f);
        if (title != nullptr && *title != 0)
        {
            g.setColour (col::ember.withAlpha (0.9f));
            g.setFont (font (11.5f, true).withExtraKerningFactor (0.25f));
            g.drawText (title, r.reduced (14, 9).withHeight (16), juce::Justification::topLeft);
        }
    }
}

// =================================================================== LookAndFeel
LookAndFeel::LookAndFeel()
{
    setColour (juce::ComboBox::textColourId, col::ash);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::ash);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::ember);
    setColour (juce::PopupMenu::highlightedTextColourId, col::coal);
    setColour (juce::PopupMenu::headerTextColourId, col::ember);
    setColour (juce::TextButton::textColourOffId, col::ash);
    setColour (juce::TextButton::textColourOnId, col::ash);
    setColour (juce::ComboBox::outlineColourId, col::panelEdge);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const bool big = std::min (w, h) > 150;
    const float size = (float) std::min (w, h) - (big ? 52.0f : 22.0f);
    const float r = size * 0.5f;
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    const float ang = start + pos * (end - start);
    const float ringR = r + 4.0f, stroke = big ? 4.0f : 3.0f;

    // track
    juce::Path track;
    track.addCentredArc (cx, cy, ringR, ringR, 0.0f, start, end, true);
    g.setColour (col::coal);
    g.strokePath (track, juce::PathStrokeType (stroke + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc: glow + core
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (ang - from) > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (cx, cy, ringR, ringR, 0.0f, std::min (from, ang), std::max (from, ang), true);
        const float heat = bipolar ? 0.5f : pos;
        const auto c = col::ember.interpolatedWith (col::flame, juce::jlimit (0.0f, 1.0f, heat * 1.2f - 0.2f));
        g.setColour (c.withAlpha (0.22f));
        g.strokePath (val, juce::PathStrokeType (stroke + 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c);
        g.strokePath (val, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // body
    const float br = r * 0.86f;
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (cx - br + 1.5f, cy - br + 3.0f, br * 2, br * 2);
    juce::ColourGradient body (juce::Colour (0xff4d433d), cx - br * 0.5f, cy - br * 0.8f, juce::Colour (0xff15110f), cx + br * 0.6f, cy + br, true);
    g.setGradientFill (body);
    g.fillEllipse (cx - br, cy - br, br * 2, br * 2);
    // knurling
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    const int teeth = big ? 48 : 32;
    for (int i = 0; i < teeth; ++i)
    {
        const float a = (float) i / (float) teeth * juce::MathConstants<float>::twoPi;
        g.drawLine (cx + std::sin (a) * br * 0.88f, cy - std::cos (a) * br * 0.88f, cx + std::sin (a) * br, cy - std::cos (a) * br, 1.0f);
    }
    // cap
    const float cr = br * 0.74f;
    juce::ColourGradient cap (juce::Colour (0xff3b3430), cx, cy - cr, juce::Colour (0xff1b1715), cx, cy + cr, false);
    g.setGradientFill (cap);
    g.fillEllipse (cx - cr, cy - cr, cr * 2, cr * 2);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawEllipse (cx - cr, cy - cr, cr * 2, cr * 2, 1.0f);

    // pointer
    juce::Path p;
    p.addRoundedRectangle (-1.8f, -br * 0.95f, 3.6f, br * 0.5f, 1.8f);
    const auto tf = juce::AffineTransform::rotation (ang).translated (cx, cy);
    g.setColour (col::ember.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (3.0f), tf);
    g.setColour (col::emberHot);
    g.fillPath (p, tf);

    // scale numbers on the big knob
    if (big && s.getMaximum() == 10.0 && s.getMinimum() == 0.0)
    {
        g.setFont (font (11.0f, true));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = start + (float) i / 10.0f * (end - start);
            const float tr = ringR + 15.0f;
            const bool lit = (float) i / 10.0f <= pos + 1.0e-4f;
            g.setColour (lit ? col::ash : col::ashDim.withAlpha (0.6f));
            g.drawText (juce::String (i), juce::Rectangle<float> (cx + std::sin (a) * tr - 10, cy - std::cos (a) * tr - 7, 20, 14),
                        juce::Justification::centred);
        }
    }
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto b = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (col::coal);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (col::ember.withAlpha (0.55f));
    g.drawRoundedRectangle (b, 6.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 17.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 5, ay - 3, ax + 5, ay - 3, ax, ay + 4);
    g.setColour (col::ember);
    g.fillPath (arrow);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (15.0f, true); }

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 34, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (col::panel);
    g.setColour (col::panelEdge);
    g.drawRect (0, 0, w, h);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (down ? col::panelEdge : col::coal);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (over ? col::ember.withAlpha (0.8f) : col::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

// =================================================================== Knob
Knob::Knob (juce::AudioProcessorValueTreeState& s, const char* paramId, juce::String cap, bool isBig)
    : caption (std::move (cap)), big (isBig)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    slider.setMouseDragSensitivity (big ? 320 : 220);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
    param = s.getParameter (paramId);
    if (param != nullptr)
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.addListener (this);
}

void Knob::resized()
{
    auto b = getLocalBounds();
    b.removeFromBottom (big ? 38 : 32);
    const int d = std::min (b.getWidth(), b.getHeight());
    slider.setBounds (b.withSizeKeepingCentre (d, d));
}

void Knob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().removeFromBottom (big ? 38 : 32);
    g.setColour (col::ash);
    g.setFont (font (big ? 15.0f : 11.5f, true).withExtraKerningFactor (0.18f));
    g.drawText (caption.toUpperCase(), b.removeFromTop (big ? 20 : 16), juce::Justification::centred);
    juce::String v;
    if (valueText) v = valueText (slider.getValue());
    else if (param != nullptr) v = param->getText (param->convertTo0to1 ((float) slider.getValue()), 0);
    g.setColour (col::ashDim);
    g.setFont (font (big ? 12.0f : 10.5f));
    g.drawText (v, b, juce::Justification::centredTop);
}

// =================================================================== Toggle
Toggle::Toggle (juce::AudioProcessorValueTreeState& s, const char* paramId, juce::String label, bool isHot) : hot (isHot)
{
    setButtonText (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, paramId, *this);
}

void Toggle::paintButton (juce::Graphics& g, bool over, bool)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    const auto lit = hot ? col::flame : col::ember;
    g.setColour (on ? lit.withAlpha (0.16f) : col::coal);
    g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    g.setColour (on ? lit.withAlpha (0.9f) : (over ? col::ashDim : col::panelEdge.brighter (0.25f)));
    g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);

    const float d = b.getHeight() * 0.36f;
    auto led = juce::Rectangle<float> (b.getX() + b.getHeight() * 0.42f, b.getCentreY() - d * 0.5f, d, d);
    if (on)
    {
        g.setColour (lit.withAlpha (0.35f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (on ? (hot ? col::flame.brighter (0.3f) : col::emberHot) : juce::Colour (0xff3a2a22));
    g.fillEllipse (led);

    g.setColour (on ? col::ash : (over ? col::ash : col::ashDim));
    g.setFont (font (11.5f, true).withExtraKerningFactor (0.15f));
    g.drawText (getButtonText(), b.withTrimmedLeft (b.getHeight() * 0.5f + d), juce::Justification::centred);
}

// =================================================================== StyleSelector
StyleSelector::StyleSelector (juce::AudioProcessorValueTreeState& s)
{
    param = s.getParameter (pid::style);
    att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
    {
        index = juce::roundToInt (v);
        repaint();
    });
    att->sendInitialUpdate();
    setRepaintsOnMouseActivity (true);
}

int StyleSelector::segmentAt (juce::Point<float> p) const
{
    const float w = (float) getWidth() / (float) kNumStyles;
    return juce::jlimit (0, kNumStyles - 1, (int) (p.x / w));
}

void StyleSelector::mouseDown (const juce::MouseEvent& e)
{
    att->setValueAsCompleteGesture ((float) segmentAt (e.position));
}

void StyleSelector::mouseMove (const juce::MouseEvent& e)
{
    const int h = segmentAt (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void StyleSelector::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (col::coal);
    g.fillRoundedRectangle (b, 8.0f);
    const float w = b.getWidth() / (float) kNumStyles;
    static const char* letters[kNumStyles] = { "C", "K", "G", "T", "P" };
    for (int i = 0; i < kNumStyles; ++i)
    {
        auto seg = juce::Rectangle<float> (b.getX() + w * (float) i, b.getY(), w, b.getHeight()).reduced (2.5f);
        const bool on = i == index;
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (col::emberHot, seg.getX(), seg.getY(), col::flame, seg.getX(), seg.getBottom(), false));
            g.fillRoundedRectangle (seg, 6.0f);
            g.setColour (col::ember.withAlpha (0.25f));
            g.drawRoundedRectangle (seg.expanded (1.5f), 7.0f, 2.0f);
        }
        else if (i == hover)
        {
            g.setColour (col::panelEdge.withAlpha (0.6f));
            g.fillRoundedRectangle (seg, 6.0f);
        }
        juce::ignoreUnused (letters);
        g.setColour (on ? col::coal : col::ash.withAlpha (i == hover ? 1.0f : 0.8f));
        g.setFont (font (10.0f, true).withExtraKerningFactor (0.04f));
        g.drawFittedText (juce::String (styleName (i)).toUpperCase(), seg.toNearestInt().reduced (2, 0), juce::Justification::centred, 1, 0.75f);
    }
}

// =================================================================== HeatMeter
void HeatMeter::setLevel (float db, float dt)
{
    db = juce::jlimit (-40.0f, 14.0f, db);
    const float k = db > shown ? std::min (1.0f, dt / 0.04f) : std::min (1.0f, dt / 0.35f);
    shown += (db - shown) * k;
    if (db >= peak) { peak = db; peakHold = 1.2f; }
    else if ((peakHold -= dt) <= 0) peak = std::max (shown, peak - dt * 12.0f);
    repaint();
}

void HeatMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    // glass
    g.setColour (col::coal);
    g.fillRoundedRectangle (b, 10.0f);
    const float cx = b.getCentreX(), cy = b.getBottom() - 22.0f;
    const float R = std::min (b.getWidth() * 0.44f, b.getHeight() - 36.0f);

    // embers glowing behind the scale: brighter the harder it is driven
    const float heat01 = juce::jlimit (0.0f, 1.0f, (shown + 24.0f) / 36.0f);
    juce::ColourGradient glow (col::ember.withAlpha (0.08f + 0.42f * heat01 * heat01), cx, cy, col::ember.withAlpha (0.0f), cx, cy - R * 1.25f, true);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (b.reduced (2.0f), 9.0f);

    const float a0 = -1.15f, a1 = 1.15f;
    auto angleFor = [&] (float db) { return a0 + (juce::jlimit (-24.0f, 12.0f, db) + 24.0f) / 36.0f * (a1 - a0); };

    // scale band
    struct Zone { float from, to; juce::Colour c; };
    const Zone zones[] = { { -24, -6, col::ashDim.withAlpha (0.55f) }, { -6, 3, col::ember }, { 3, 12, col::flame } };
    for (auto& z : zones)
    {
        juce::Path arc;
        arc.addCentredArc (cx, cy, R, R, 0.0f, angleFor (z.from), angleFor (z.to), true);
        g.setColour (z.c);
        g.strokePath (arc, juce::PathStrokeType (3.0f));
    }
    // ticks + labels
    g.setFont (font (10.0f, true));
    for (int db = -24; db <= 12; db += 3)
    {
        const float a = angleFor ((float) db);
        const bool major = db % 6 == 0;
        const float r1 = R + 2.0f, r2 = R + (major ? 9.0f : 5.0f);
        g.setColour (db >= 3 ? col::flame : (db >= -6 ? col::ember : col::ashDim));
        g.drawLine (cx + std::sin (a) * r1, cy - std::cos (a) * r1, cx + std::sin (a) * r2, cy - std::cos (a) * r2, major ? 1.6f : 1.0f);
        if (major)
        {
            const float rt = R + 19.0f;
            g.setColour (col::ashDim);
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (cx + std::sin (a) * rt - 14, cy - std::cos (a) * rt - 7, 28, 14),
                        juce::Justification::centred);
        }
    }

    // peak marker
    {
        const float a = angleFor (peak);
        g.setColour (col::emberHot.withAlpha (0.8f));
        g.fillEllipse (cx + std::sin (a) * (R - 8) - 2.5f, cy - std::cos (a) * (R - 8) - 2.5f, 5, 5);
    }
    // needle
    {
        const float a = angleFor (shown);
        const juce::Point<float> tip (cx + std::sin (a) * (R - 4), cy - std::cos (a) * (R - 4));
        g.setColour (col::ember.withAlpha (0.3f));
        g.drawLine (cx, cy, tip.x, tip.y, 5.0f);
        g.setColour (col::emberHot);
        g.drawLine (cx, cy, tip.x, tip.y, 1.8f);
    }
    // pivot
    g.setColour (col::metal);
    g.fillEllipse (cx - 9, cy - 9, 18, 18);
    g.setColour (col::ember.withAlpha (0.4f + 0.6f * heat01));
    g.fillEllipse (cx - 4, cy - 4, 8, 8);

    g.setColour (col::ash);
    g.setFont (font (12.0f, true).withExtraKerningFactor (0.3f));
    g.drawText ("HEAT", juce::Rectangle<float> (b.getX() + 14, b.getBottom() - 22, 80, 14), juce::Justification::centredLeft);
}

// =================================================================== Editor
BrasaEditor::BrasaEditor (BrasaProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    fillPresetMenu();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0) { proc.setCurrentProgram (id - 1); shownProgram = id - 1; }
    };
    prevBtn.onClick = [this] { step (-1); };
    nextBtn.onClick = [this] { step (1); };
    addAndMakeVisible (presetBox);
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    hq = std::make_unique<Toggle> (s, pid::hq, "HQ");
    hq->setTooltip ("8x oversampling: cleaner top end, a bit more CPU");
    punish = std::make_unique<Toggle> (s, pid::punish, "PUNISH", true);
    thump = std::make_unique<Toggle> (s, pid::thump, "THUMP");
    steep = std::make_unique<Toggle> (s, pid::steep, "STEEP");
    autoGain = std::make_unique<Toggle> (s, pid::autoGain, "AUTO");

    drive = std::make_unique<Knob> (s, pid::drive, "Drive", true);
    output = std::make_unique<Knob> (s, pid::output, "Output");
    mix = std::make_unique<Knob> (s, pid::mix, "Mix");
    lowCut = std::make_unique<Knob> (s, pid::lowCut, "Low Cut");
    tone = std::make_unique<Knob> (s, pid::tone, "Tone");
    highCut = std::make_unique<Knob> (s, pid::highCut, "High Cut");
    tone->valueText = [] (double v)
    {
        if (std::abs (v) < 0.05) return juce::String ("Flat");
        return (v < 0 ? "Dark " : "Bright ") + juce::String (std::abs (v), 1);
    };
    style = std::make_unique<StyleSelector> (s);

    for (juce::Component* c : std::initializer_list<juce::Component*> { hq.get(), punish.get(), thump.get(), steep.get(), autoGain.get(),
                                                                       drive.get(), output.get(), mix.get(), lowCut.get(), tone.get(),
                                                                       highCut.get(), style.get(), &heat })
        addAndMakeVisible (c);

    setSize (kW, kH);
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

BrasaEditor::~BrasaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BrasaEditor::fillPresetMenu()
{
    presetBox.clear (juce::dontSendNotification);
    std::string lastCat;
    const auto& list = factoryPresets();
    for (size_t i = 0; i < list.size(); ++i)
    {
        if (list[i].category != lastCat)
        {
            presetBox.addSectionHeading (list[i].category);
            lastCat = list[i].category;
        }
        presetBox.addItem (list[i].name, (int) i + 1);
    }
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    shownProgram = proc.getCurrentProgram();
}

void BrasaEditor::step (int delta)
{
    const int n = proc.getNumPrograms();
    const int i = ((proc.getCurrentProgram() + delta) % n + n) % n;
    proc.setCurrentProgram (i);
    presetBox.setSelectedId (i + 1, juce::dontSendNotification);
    shownProgram = i;
}

void BrasaEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) std::min (0.1, (now - lastTick) * 0.001);
    lastTick = now;
    heat.setLevel (proc.getHeatDb(), dt);
    if (proc.getCurrentProgram() != shownProgram)
    {
        shownProgram = proc.getCurrentProgram();
        presetBox.setSelectedId (shownProgram + 1, juce::dontSendNotification);
    }
    const float a = proc.getAutoGainDb();
    if (std::abs (a - shownAutoDb) > 0.05f)
    {
        shownAutoDb = a;
        repaint (outPanel);
    }
    static int lastStyle = -1;
    const int st = (int) proc.apvts.getRawParameterValue (pid::style)->load();
    if (st != lastStyle) { lastStyle = st; repaint (outPanel); }
}

void BrasaEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (col::bg1, 0, 0, col::bg2, 0, (float) kH, false));
    g.fillAll();
    // faint ember glow from the bottom
    g.setGradientFill (juce::ColourGradient (col::ember.withAlpha (0.07f), kW * 0.5f, (float) kH + 40, col::ember.withAlpha (0.0f),
                                             kW * 0.5f, (float) kH - 220, true));
    g.fillRect (getLocalBounds());

    // logo
    {
        auto logo = juce::Rectangle<float> (24, 10, 260, 40);
        juce::ColourGradient lg (col::emberHot, logo.getX(), 0, col::flame, logo.getX() + 150, 0, false);
        g.setGradientFill (lg);
        g.setFont (font (34.0f, true).withExtraKerningFactor (0.22f));
        g.drawText ("BRASA", logo, juce::Justification::centredLeft);
        g.setColour (col::ashDim);
        g.setFont (font (10.5f, true).withExtraKerningFactor (0.3f));
        g.drawText ("ANALOG SATURATION", juce::Rectangle<float> (26, 46, 260, 12), juce::Justification::centredLeft);
    }

    panelBg (g, drivePanel, "");
    panelBg (g, centrePanel, "");
    panelBg (g, outPanel, "OUTPUT");
    panelBg (g, filterPanel, "FILTERS");

    g.setColour (col::ashDim);
    g.setFont (font (10.0f, true).withExtraKerningFactor (0.25f));
    g.drawText ("CIRCUIT", juce::Rectangle<int> (style->getX(), style->getY() - 17, style->getWidth(), 14), juce::Justification::centred);

    // auto readout + circuit description
    {
        g.setFont (font (11.0f, true));
        const bool on = autoGain->getToggleState();
        g.setColour (on ? col::ember : col::ashDim.withAlpha (0.5f));
        g.drawText (on ? ("auto " + juce::String (shownAutoDb >= 0 ? "+" : "") + juce::String (shownAutoDb, 1) + " dB") : juce::String ("auto off"),
                    juce::Rectangle<int> (autoGain->getX() - 10, autoGain->getBottom() + 4, autoGain->getWidth() + 20, 16),
                    juce::Justification::centred);

        const int st = (int) proc.apvts.getRawParameterValue (pid::style)->load();
        auto txt = juce::Rectangle<int> (outPanel.getX() + 14, outPanel.getBottom() - 52, outPanel.getWidth() - 28, 40);
        g.setColour (col::ember);
        g.setFont (font (12.5f, true).withExtraKerningFactor (0.1f));
        g.drawText (juce::String (styleName (st)).toUpperCase(), txt.removeFromTop (16), juce::Justification::centredLeft);
        g.setColour (col::ashDim);
        g.setFont (font (11.0f));
        g.drawFittedText (styleBlurb (st), txt, juce::Justification::topLeft, 2, 0.9f);
    }

    // dark / bright marks around Tone
    {
        g.setColour (col::ashDim);
        g.setFont (font (9.5f, true));
        auto t = tone->getBounds();
        g.drawText ("DARK", juce::Rectangle<int> (t.getX() - 34, t.getY() + 62, 40, 12), juce::Justification::centredRight);
        g.drawText ("BRIGHT", juce::Rectangle<int> (t.getRight() - 6, t.getY() + 62, 48, 12), juce::Justification::centredLeft);
    }
}

void BrasaEditor::resized()
{
    header = { 0, 0, kW, 64 };
    presetBox.setBounds (330, 16, 260, 32);
    prevBtn.setBounds (294, 17, 30, 30);
    nextBtn.setBounds (596, 17, 30, 30);
    hq->setBounds (kW - 104, 19, 80, 26);

    drivePanel = { 16, 74, 222, 272 };
    centrePanel = { 248, 74, 354, 272 };
    outPanel = { 612, 74, 212, 272 };
    filterPanel = { 16, 356, 808, 130 };

    drive->setBounds (drivePanel.getX() + 6, drivePanel.getY() + 6, 210, 222);
    punish->setBounds (drivePanel.getCentreX() - 55, drivePanel.getBottom() - 42, 110, 28);

    heat.setBounds (centrePanel.getX() + 12, centrePanel.getY() + 12, centrePanel.getWidth() - 24, 170);
    style->setBounds (centrePanel.getX() + 12, centrePanel.getBottom() - 58, centrePanel.getWidth() - 24, 44);

    output->setBounds (outPanel.getX() + 8, outPanel.getY() + 30, 96, 112);
    mix->setBounds (outPanel.getRight() - 104, outPanel.getY() + 30, 96, 112);
    autoGain->setBounds (outPanel.getX() + 18, outPanel.getY() + 150, 76, 24);

    const int fy = filterPanel.getY() + 22;
    lowCut->setBounds (filterPanel.getX() + 40, fy, 96, 104);
    thump->setBounds (filterPanel.getX() + 148, fy + 34, 92, 26);
    tone->setBounds (filterPanel.getCentreX() - 48, fy, 96, 104);
    highCut->setBounds (filterPanel.getRight() - 248, fy, 96, 104);
    steep->setBounds (filterPanel.getRight() - 140, fy + 34, 92, 26);
}
