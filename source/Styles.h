#pragma once
#include "DspUtil.h"
#include <array>

namespace brasa
{
// The five circuits. Order = order of the buttons.
enum Style
{
    kCinta = 0,    // tube tape-machine record amp (smooth, dark, thick low-mids)
    kConsola,      // late-60s British transistor console channel (full lows, airy top, creamy even harmonics)
    kGermanio,     // germanium class-A channel with transformers (clean until it isn't, iron on the lows)
    kTriodo,       // overdriven triode (2nd harmonic, punchy, compresses)
    kPentodo,      // overdriven pentode (odd + high harmonics, aggressive, band-limited)
    kNumStyles
};

const char* styleName (int s);

// One circuit model. pre() and post() run at the host rate, shape() at the oversampled rate.
// Everything is per channel (two channels).
class StyleCore
{
public:
    explicit StyleCore (int style);

    void prepare (double baseRate, double osRate);
    void reset();

    float baseGain() const { return base; }   // level into the circuit at Drive 0 (linear)

    inline float pre (int ch, float x) { auto& c = chan[(size_t) ch]; return c.preB.process (c.preA.process (x)); }
    float shape (int ch, float u);
    inline float post (int ch, float x)
    {
        auto& c = chan[(size_t) ch];
        x = c.dc.process (x);
        for (auto& b : c.postBq) x = b.process (x);
        return c.postLp2.process (c.postLp.process (c.postHp.process (x)));
    }

private:
    struct Chan
    {
        OnePole preA, preB, dc, postHp, postLp, postLp2;
        std::array<Biquad, 3> postBq;
        Leaky flux;             // transformer core flux
        Envelope gridEnv;       // grid-current charge of the coupling capacitor (tube styles)
        float bias = 0;
    };

    // tube stage: x -> normalised plate-current swing (unit slope at rest)
    struct Tube
    {
        float gridClamp = 0.6f, gridKnee = 0.1f;   // positive swing: grid conduction
        float cutoff = 1.0f, cutoffKnee = 0.3f;    // negative swing: plate current cut off
        float plateMax = 0.0f, plateKnee = 0.1f;   // > 0: pentode plate saturation ceiling
        float i0 = 1, slope = 1.5f;
        float eval (float x) const;
        void calibrate();
    };

    float transformer (Chan& c, float u) const;

    int style;
    float base = 1.0f;
    double osSr = 96000;
    std::array<Chan, 2> chan;

    // circuit settings (set in the constructor per style)
    float fluxHz = 50, fluxSat = 1, fluxAmt = 0;   // input transformer
    Tube tube;
    bool useTube = false;
    float biasAmt = 0;                              // how far grid current shifts the bias
    float posLimit = 1, negLimit = 1;               // transistor stage asymmetry
    float outCeil = 2;                              // output stage ceiling
    int outOrder = 2;                               // output stage knee (2, 4 or 8)
};
}
