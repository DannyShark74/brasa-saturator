#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "Engine.h"

namespace brasa
{
namespace pid
{
    inline constexpr const char* style = "style";
    inline constexpr const char* drive = "drive";
    inline constexpr const char* punish = "punish";
    inline constexpr const char* lowCut = "low_cut";
    inline constexpr const char* thump = "thump";
    inline constexpr const char* tone = "tone";
    inline constexpr const char* highCut = "high_cut";
    inline constexpr const char* steep = "steep";
    inline constexpr const char* output = "output";
    inline constexpr const char* autoGain = "auto";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* hq = "hq";
}

// One table describes every parameter; the plugin builds its layout from it and the offline
// tool uses the same defaults and mapping.
struct ParamDef
{
    enum Kind { Float, Bool, Choice };
    std::string id, name;
    Kind kind;
    float min, max, def;
    std::vector<std::string> choices {};
    float skewCentre = 0.0f;
    std::string unit {};
    float step = 0.0f;
};

const std::vector<ParamDef>& paramTable();

using ParamGetter = std::function<float (const std::string&)>;
Params engineParamsFrom (const ParamGetter& get);

struct Preset
{
    std::string name, category;
    std::map<std::string, float> values;   // everything else keeps its default
};
const std::vector<Preset>& factoryPresets();
}
