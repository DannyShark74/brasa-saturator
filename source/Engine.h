#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "Styles.h"

namespace brasa
{
struct Params
{
    int style = kTriodo;
    float drive = 3.0f;        // 0..10
    bool punish = false;       // +20 dB into the circuit
    float lowCutHz = 20.0f;    // 20..2000, before the circuit
    bool thump = false;        // resonant bump at the low-cut frequency
    float tone = 0.0f;         // -5 (dark) .. +5 (bright), gentle tilt before the circuit
    float highCutHz = 20000.0f;// 1k..20k, after the circuit (20k = off)
    bool steep = false;        // 6 dB/oct -> 30 dB/oct
    float outputDb = 0.0f;     // -24..+12 trim
    bool autoGain = true;      // compensate the level change of Drive / Punish
    float mix = 1.0f;          // 0..1
    bool hq = false;           // 8x instead of 2x oversampling
};

constexpr float kDriveDbPerStep = 3.0f;   // Drive 0..10 = +0..+30 dB
constexpr float kPunishDb = 20.0f;

// Level change of the circuit itself (no filters) for nominal programme material, measured once.
// Auto gain uses the negative of it.
float circuitGainDb (int style, float drive, bool punish);

class Engine
{
public:
    Engine();
    ~Engine();

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const Params& p) { params = p; }
    // channels: 1 or 2 pointers, processed in place
    void process (float* const* ch, int numChannels, int numSamples);

    int latencySamples() const { return latency; }
    float heatDb() const { return heat.load(); }           // level into the circuit, 0 dB = clipping point
    float autoGainDb() const { return autoDb.load(); }     // what Auto is applying right now

private:
    void processChunk (float* const* ch, int numChannels, int n);
    void updateFilters (bool force);

    Params params, applied;
    bool filtersValid = false;
    double sr = 48000;
    static constexpr int kChunk = 512;

    std::unique_ptr<juce::dsp::Oversampling<float>> os2, os8;
    std::array<std::unique_ptr<StyleCore>, kNumStyles> cores2, cores8;   // per oversampling rate
    int latency = 0;

    // brings the wet path of either oversampling mode to the same whole-sample latency
    struct FracDelay
    {
        std::vector<float> line;
        int pos = 0, whole = 0;
        float a = 0, x1 = 0, y1 = 0;
        void setup (double delay);
        void reset() { std::fill (line.begin(), line.end(), 0.0f); pos = 0; x1 = y1 = 0; }
        inline float process (float x)
        {
            const int len = (int) line.size();
            line[(size_t) pos] = x;
            int rd = pos - whole;
            if (rd < 0) rd += len;
            const float v = line[(size_t) rd];
            if (++pos >= len) pos = 0;
            const float y = a * v + x1 - a * y1;   // first-order Thiran all-pass, flat magnitude
            x1 = v; y1 = y;
            return y;
        }
    };
    std::array<FracDelay, 2> align2, align8;

    juce::AudioBuffer<float> wet;
    std::array<std::vector<float>, 2> dryLine;
    int dryPos = 0;

    std::array<Biquad, 2> lowCut;
    std::array<Biquad, 2> tiltLo, tiltHi;
    std::array<OnePole, 2> hcPole;
    std::array<std::array<Biquad, 2>, 2> hcBq;
    bool lowCutOn = false, tiltOn = false, hcOn = false, hcSteep = false;

    Ramp driveRamp, outRamp, mixRamp, fadeRamp;
    int activeStyle = kTriodo;
    bool activeHq = false;
    int pendingStyle = -1, pendingHq = -1;

    std::array<Envelope, 2> heatEnv;
    std::atomic<float> heat { -60.0f }, autoDb { 0.0f };
};
}
