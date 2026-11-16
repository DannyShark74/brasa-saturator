#include "Styles.h"

namespace brasa
{
const char* styleName (int s)
{
    static const char* names[kNumStyles] = { "Cinta", "Consola", "Germanio", "Triodo", "Pentodo" };
    return (s >= 0 && s < kNumStyles) ? names[s] : "";
}

// ------------------------------------------------------------------ tube stage
// Plate current follows the 3/2 power law of the grid drive. The positive swing is limited by
// grid conduction (fairly hard), the negative swing by cut-off (soft). A pentode also hits a
// plate-current ceiling (the knee of its flat plate curves), which clips the top hard.
float StyleCore::Tube::eval (float x) const
{
    const float xg = gridClamp - smoothRelu (gridClamp - x, gridKnee);
    const float e = smoothRelu (xg + cutoff, cutoffKnee * cutoff) / cutoff;
    float i = e * std::sqrt (e);
    if (plateMax > 0.0f)
        i = plateMax - smoothRelu (plateMax - i, plateKnee);
    return (i - i0) / slope;
}

void StyleCore::Tube::calibrate()
{
    i0 = 0.0f;
    slope = 1.0f;
    const float at0 = eval (0.0f);
    const float d = (eval (1.0e-3f) - eval (-1.0e-3f)) / 2.0e-3f;
    i0 = at0;
    slope = d;
}

// ------------------------------------------------------------------ setup
StyleCore::StyleCore (int s) : style (s)
{
    switch (style)
    {
        case kCinta:
            base = dbToGain (-10.0f);
            fluxHz = 45; fluxSat = 0.7f; fluxAmt = 0.8f;
            useTube = true;
            tube.gridClamp = 2.0f; tube.gridKnee = 0.6f; tube.cutoff = 1.2f; tube.cutoffKnee = 0.6f;
            biasAmt = 0.2f;
            outCeil = 2.0f; outOrder = 2;
            break;
        case kConsola:
            base = dbToGain (-1.5f);
            fluxHz = 40; fluxSat = 0.9f; fluxAmt = 0.6f;
            posLimit = 1.0f; negLimit = 2.5f;
            outCeil = 1.6f; outOrder = 4;
            break;
        case kGermanio:
            base = dbToGain (5.0f);
            fluxHz = 40; fluxSat = 0.12f; fluxAmt = 1.0f;
            posLimit = 2.5f; negLimit = 10.0f;
            outCeil = 3.0f; outOrder = 4;
            break;
        case kTriodo:
            base = dbToGain (-4.0f);
            fluxHz = 35; fluxSat = 1.0f; fluxAmt = 0.3f;
            useTube = true;
            tube.gridClamp = 2.5f; tube.gridKnee = 0.5f; tube.cutoff = 0.7f; tube.cutoffKnee = 0.35f;
            biasAmt = 0.15f;
            outCeil = 3.0f; outOrder = 2;
            break;
        case kPentodo:
        default:
            base = dbToGain (5.0f);
            fluxHz = 35; fluxSat = 1.0f; fluxAmt = 0.3f;
            useTube = true;
            tube.gridClamp = 0.9f; tube.gridKnee = 0.06f; tube.cutoff = 1.0f; tube.cutoffKnee = 0.12f;
            tube.plateMax = 2.83f; tube.plateKnee = 0.12f;
            biasAmt = 0.7f;
            outCeil = 1.8f; outOrder = 2;
            break;
    }
    tube.calibrate();
}

void StyleCore::prepare (double baseRate, double osRate)
{
    osSr = osRate;
    for (auto& c : chan)
    {
        c.preA.setIdentity(); c.preB.setIdentity();
        c.postHp.setIdentity(); c.postLp.setIdentity(); c.postLp2.setIdentity();
        for (auto& b : c.postBq) b.setIdentity();
        c.dc.highpass (baseRate, 7.0);
        c.flux.setup (osRate, fluxHz);
        c.gridEnv.setup (osRate, 2.0, 120.0);

        switch (style)
        {
            case kCinta:
                c.postHp.highpass (baseRate, 22.0);
                c.postBq[0].peak (baseRate, 220.0, 0.8, 1.2);
                c.postLp.lowpass (baseRate, 11500.0);
                break;
            case kConsola:
                c.postHp.highpass (baseRate, 18.0);
                c.postBq[0].lowShelf (baseRate, 75.0, 0.7, 1.0);
                c.postBq[1].highShelf (baseRate, 6500.0, 0.6, 1.2);
                break;
            case kGermanio:
                c.postHp.highpass (baseRate, 20.0);
                c.postBq[0].lowShelf (baseRate, 110.0, 0.7, 0.8);
                c.postBq[1].peak (baseRate, 4500.0, 0.5, 0.8);
                break;
            case kTriodo:
                c.preA.highpass (baseRate, 40.0);
                c.postHp.highpass (baseRate, 40.0);
                c.postBq[0].lowpass (baseRate, 12000.0, 0.6);
                break;
            case kPentodo:
            default:
                c.preA.highpass (baseRate, 45.0);
                c.postHp.highpass (baseRate, 45.0);
                c.postLp.lowpass (baseRate, 7000.0);
                c.postLp2.lowpass (baseRate, 19000.0);
                break;
        }
    }
    reset();
}

void StyleCore::reset()
{
    for (auto& c : chan)
    {
        c.preA.reset(); c.preB.reset(); c.dc.reset(); c.postHp.reset(); c.postLp.reset(); c.postLp2.reset();
        for (auto& b : c.postBq) b.reset();
        c.flux.reset();
        c.gridEnv.reset();
        c.bias = 0;
    }
}

// ------------------------------------------------------------------ the circuit
// Input transformer: the core flux is the integral of the voltage, so low frequencies push it
// harder. When the core saturates the winding stops passing the excess: distortion that grows
// as the frequency goes down.
float StyleCore::transformer (Chan& c, float u) const
{
    if (fluxAmt <= 0.0f) return u;
    const float phi = c.flux.process (u);
    const float excess = phi - fluxSat * clip2 (phi / fluxSat);
    return u - fluxAmt * excess;
}

float StyleCore::shape (int ch, float u)
{
    auto& c = chan[(size_t) ch];
    const float x = transformer (c, u);

    float v;
    if (useTube)
    {
        const float xb = x + c.bias;
        // when the grid swings positive it draws current and charges the coupling capacitor,
        // which pushes the bias towards cut-off; it leaks back over ~0.1 s
        const float env = c.gridEnv.process (std::max (0.0f, xb - tube.gridClamp));
        c.bias = -biasAmt * env;
        v = tube.eval (xb);
    }
    else
    {
        v = x >= 0.0f ? posLimit * clip2 (x / posLimit) : negLimit * clip2 (x / negLimit);
    }

    const float w = v / outCeil;
    switch (outOrder)
    {
        case 8: return outCeil * clip8 (w);
        case 4: return outCeil * clip4 (w);
        default: return outCeil * clip2 (w);
    }
}
}
