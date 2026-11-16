#include "ParamTable.h"
#include <cmath>

namespace brasa
{
const std::vector<ParamDef>& paramTable()
{
    static const std::vector<ParamDef> table = []
    {
        using K = ParamDef;
        std::vector<ParamDef> t;
        t.push_back ({ pid::style, "Style", K::Choice, 0, kNumStyles - 1, (float) kTriodo,
                       { "Cinta", "Consola", "Germanio", "Triodo", "Pentodo" } });
        t.push_back ({ pid::drive, "Drive", K::Float, 0, 10, 3, {}, 0, "", 0.01f });
        t.push_back ({ pid::punish, "Punish", K::Bool, 0, 1, 0 });
        t.push_back ({ pid::lowCut, "Low Cut", K::Float, 20, 2000, 20, {}, 150, "Hz" });
        t.push_back ({ pid::thump, "Thump", K::Bool, 0, 1, 0 });
        t.push_back ({ pid::tone, "Tone", K::Float, -5, 5, 0, {}, 0, "", 0.01f });
        t.push_back ({ pid::highCut, "High Cut", K::Float, 1000, 20000, 20000, {}, 5000, "Hz" });
        t.push_back ({ pid::steep, "Steep", K::Bool, 0, 1, 0 });
        t.push_back ({ pid::output, "Output", K::Float, -24, 12, 0, {}, 0, "dB", 0.01f });
        t.push_back ({ pid::autoGain, "Auto", K::Bool, 0, 1, 1 });
        t.push_back ({ pid::mix, "Mix", K::Float, 0, 1, 1 });
        t.push_back ({ pid::hq, "HQ", K::Bool, 0, 1, 0 });
        return t;
    }();
    return table;
}

Params engineParamsFrom (const ParamGetter& get)
{
    auto on = [&get] (const char* id) { return get (id) >= 0.5f; };
    Params p;
    p.style = (int) std::lround (get (pid::style));
    p.drive = get (pid::drive);
    p.punish = on (pid::punish);
    p.lowCutHz = get (pid::lowCut);
    p.thump = on (pid::thump);
    p.tone = get (pid::tone);
    p.highCutHz = get (pid::highCut);
    p.steep = on (pid::steep);
    p.outputDb = get (pid::output);
    p.autoGain = on (pid::autoGain);
    p.mix = get (pid::mix);
    p.hq = on (pid::hq);
    return p;
}

const std::vector<Preset>& factoryPresets()
{
    using namespace pid;
    static const std::vector<Preset> list = {
        { "Default", "Start", {} },

        { "Vocal Warmth", "Vocals", { { style, kCinta }, { drive, 3.5f }, { lowCut, 90 }, { tone, -0.5f }, { mix, 0.7f } } },
        { "Vocal Edge", "Vocals", { { style, kConsola }, { drive, 5 }, { lowCut, 120 }, { tone, 1 }, { highCut, 14000 }, { mix, 0.55f } } },
        { "Vocal Grit", "Vocals", { { style, kPentodo }, { drive, 6 }, { lowCut, 180 }, { highCut, 9000 }, { mix, 0.35f } } },
        { "Telephone", "Vocals", { { style, kPentodo }, { drive, 7 }, { lowCut, 700 }, { highCut, 3200 }, { steep, 1 }, { mix, 1 } } },

        { "Drum Bus Glue", "Drums", { { style, kGermanio }, { drive, 4 }, { lowCut, 30 }, { mix, 0.6f } } },
        { "Drum Smash", "Drums", { { style, kTriodo }, { drive, 8 }, { punish, 1 }, { thump, 1 }, { lowCut, 60 }, { highCut, 12000 }, { mix, 0.3f } } },
        { "Snare Crack", "Drums", { { style, kPentodo }, { drive, 5 }, { lowCut, 150 }, { tone, 1.5f }, { mix, 0.5f } } },
        { "Kick Thump", "Drums", { { style, kTriodo }, { drive, 4.5f }, { lowCut, 55 }, { thump, 1 }, { highCut, 9000 } } },

        { "808 Weight", "Bass", { { style, kGermanio }, { drive, 5.5f }, { tone, -1 }, { highCut, 8000 }, { mix, 0.8f } } },
        { "Bass Fatten", "Bass", { { style, kConsola }, { drive, 5 }, { lowCut, 35 }, { thump, 1 }, { mix, 0.75f } } },
        { "Bass Growl", "Bass", { { style, kPentodo }, { drive, 7 }, { lowCut, 100 }, { highCut, 5000 }, { steep, 1 }, { mix, 0.4f } } },

        { "Keys Tape", "Instruments", { { style, kCinta }, { drive, 4 }, { tone, -1 }, { highCut, 12000 } } },
        { "Synth Lead Bite", "Instruments", { { style, kPentodo }, { drive, 5.5f }, { lowCut, 120 }, { mix, 0.7f } } },
        { "Guitar Cab", "Instruments", { { style, kTriodo }, { drive, 7 }, { lowCut, 90 }, { highCut, 4800 }, { steep, 1 } } },
        { "Dark Pad", "Instruments", { { style, kCinta }, { drive, 3 }, { tone, -2.5f }, { highCut, 7000 } } },

        { "Mix Bus Color", "Bus", { { style, kConsola }, { drive, 2 }, { mix, 0.5f } } },
        { "Mix Bus Iron", "Bus", { { style, kGermanio }, { drive, 2.5f }, { lowCut, 25 }, { mix, 0.6f } } },

        { "Punished", "Extreme", { { style, kPentodo }, { drive, 10 }, { punish, 1 }, { highCut, 9000 } } },
        { "Lo-Fi Radio", "Extreme", { { style, kCinta }, { drive, 9 }, { punish, 1 }, { lowCut, 400 }, { highCut, 4000 }, { steep, 1 } } },
    };
    return list;
}
}
