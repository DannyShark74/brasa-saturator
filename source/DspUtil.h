#pragma once
#include <algorithm>
#include <cmath>

namespace brasa
{
constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

// Smooth max(0, x): the kink is rounded over a width of about k.
inline float smoothRelu (float x, float k) { return 0.5f * (x + std::sqrt (x * x + k * k)); }

// Soft clippers with unit slope at 0 and a ceiling of 1. The higher the order, the harder the knee.
inline float clip2 (float x) { return x / std::sqrt (1.0f + x * x); }
inline float clip4 (float x)
{
    const float x2 = x * x;
    return x / std::sqrt (std::sqrt (1.0f + x2 * x2));
}
inline float clip8 (float x)
{
    const float x2 = x * x, x4 = x2 * x2;
    return x / std::sqrt (std::sqrt (std::sqrt (1.0f + x4 * x4)));
}

// Transposed direct form II biquad (RBJ cookbook designs).
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }
    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }
    void lowpass (double sr, double f, double q)
    {
        const double w = 2 * kPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), a = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a);
    }
    void highpass (double sr, double f, double q)
    {
        const double w = 2 * kPi * std::min (f, sr * 0.49) / sr, c = std::cos (w), a = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a);
    }
    void peak (double sr, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * std::min (f, sr * 0.49) / sr;
        const double c = std::cos (w), a = std::sin (w) / (2 * q);
        set (1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A);
    }
    void lowShelf (double sr, double f, double slope, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * std::min (f, sr * 0.49) / sr;
        const double c = std::cos (w), s = std::sin (w);
        const double a = s / 2 * std::sqrt ((A + 1 / A) * (1 / slope - 1) + 2), sa = 2 * std::sqrt (A) * a;
        set (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
             (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
    }
    void highShelf (double sr, double f, double slope, double db)
    {
        const double A = std::pow (10.0, db / 40), w = 2 * kPi * std::min (f, sr * 0.49) / sr;
        const double c = std::cos (w), s = std::sin (w);
        const double a = s / 2 * std::sqrt ((A + 1 / A) * (1 / slope - 1) + 2), sa = 2 * std::sqrt (A) * a;
        set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
             (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
};

// One-pole filters (bilinear, exact at DC / Nyquist).
struct OnePole
{
    float b0 = 1, b1 = 0, a1 = 0, z = 0;
    void reset() { z = 0; }
    inline float process (float x)
    {
        const float y = b0 * x + z;
        z = b1 * x - a1 * y;
        return y;
    }
    void setIdentity() { b0 = 1; b1 = a1 = 0; }
    void lowpass (double sr, double f)
    {
        const double k = std::tan (kPi * std::min (f, sr * 0.49) / sr);
        b0 = b1 = (float) (k / (1 + k));
        a1 = (float) ((k - 1) / (k + 1));
    }
    void highpass (double sr, double f)
    {
        const double k = std::tan (kPi * std::min (f, sr * 0.49) / sr);
        b0 = (float) (1 / (1 + k));
        b1 = -b0;
        a1 = (float) ((k - 1) / (k + 1));
    }
};

// Plain leaky integrator used as a "flux" low-pass (no bilinear warping needed at these rates).
struct Leaky
{
    float a = 0, s = 0;
    void setup (double sr, double f) { a = (float) (1.0 - std::exp (-2 * kPi * f / sr)); }
    inline float process (float x) { s += a * (x - s); return s; }
    void reset() { s = 0; }
};

// Attack / release envelope on a positive input.
struct Envelope
{
    float att = 0, rel = 0, v = 0;
    void setup (double sr, double attMs, double relMs)
    {
        att = (float) (1.0 - std::exp (-1.0 / (sr * attMs * 0.001)));
        rel = (float) (1.0 - std::exp (-1.0 / (sr * relMs * 0.001)));
    }
    inline float process (float x)
    {
        v += (x > v ? att : rel) * (x - v);
        return v;
    }
    void reset() { v = 0; }
};

// Linear ramp for gains, reaches the target in a fixed number of samples.
struct Ramp
{
    float cur = 0, target = 0, step = 0;
    int left = 0, len = 64;
    void setLength (int n) { len = std::max (1, n); }
    void snap (float v) { cur = target = v; left = 0; }
    void set (float v)
    {
        if (v == target) return;
        target = v;
        left = len;
        step = (target - cur) / (float) len;
    }
    inline float next()
    {
        if (left > 0) { cur += step; if (--left == 0) cur = target; }
        return cur;
    }
};
}
