#include "Params.h"
#include "ParamTable.h"

namespace brasa
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& d : paramTable())
    {
        const juce::ParameterID id { d.id, 1 };
        switch (d.kind)
        {
            case ParamDef::Float:
            {
                juce::NormalisableRange<float> r (d.min, d.max, d.step);
                if (d.skewCentre > 0.0f) r.setSkewForCentre (d.skewCentre);
                auto attrs = juce::AudioParameterFloatAttributes().withLabel (d.unit);
                const std::string sid = d.id;
                if (sid == pid::mix)
                    attrs = attrs.withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; });
                else if (d.unit == "Hz")
                    attrs = attrs.withStringFromValueFunction ([sid] (float v, int)
                    {
                        if (sid == pid::highCut && v >= 19900.0f) return juce::String ("Off");
                        if (sid == pid::lowCut && v <= 20.5f) return juce::String ("Off");
                        return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
                    });
                else if (d.unit == "dB")
                    attrs = attrs.withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; });
                else
                    attrs = attrs.withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); });
                layout.add (std::make_unique<juce::AudioParameterFloat> (id, d.name, r, d.def, attrs));
                break;
            }
            case ParamDef::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (id, d.name, d.def >= 0.5f));
                break;
            case ParamDef::Choice:
            {
                juce::StringArray ch;
                for (auto& c : d.choices) ch.add (c);
                layout.add (std::make_unique<juce::AudioParameterChoice> (id, d.name, ch, (int) d.def));
                break;
            }
        }
    }
    return layout;
}
}
