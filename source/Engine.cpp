#include "Engine.h"
#include <cstdint>

namespace brasa
{
// ------------------------------------------------------------------ auto gain table
namespace
{
constexpr int kDriveSteps = 11;

struct GainTable
{
    float db[kNumStyles][2][kDriveSteps];
};

const GainTable& gainTable()
{
    static const GainTable table = []
    {
        GainTable t {};
        const double fs = 48000.0;
        const int n = 24000, skip = 8000;

        // pink-ish noise (Kellet's economy filter), normalised to -18 dBFS RMS
        std::vector<float> noise ((size_t) n);
        uint32_t seed = 12345u;
        float b0 = 0, b1 = 0, b2 = 0;
        double acc = 0;
        for (int i = 0; i < n; ++i)
        {
            seed = seed * 1664525u + 1013904223u;
            const float w = (float) ((seed >> 8) & 0xFFFFFF) / 8388608.0f - 1.0f;
            b0 = 0.99765f * b0 + w * 0.0990460f;
            b1 = 0.96300f * b1 + w * 0.2965164f;
            b2 = 0.57000f * b2 + w * 1.0526913f;
            noise[(size_t) i] = b0 + b1 + b2 + w * 0.1848f;
            if (i >= skip) acc += (double) noise[(size_t) i] * noise[(size_t) i];
        }
        const float norm = dbToGain (-18.0f) / (float) std::sqrt (acc / (n - skip));
        for (auto& v : noise) v *= norm;

        for (int s = 0; s < kNumStyles; ++s)
            for (int p = 0; p < 2; ++p)
                for (int d = 0; d < kDriveSteps; ++d)
                {
                    StyleCore core (s);
                    core.prepare (fs, fs);
                    const float g = dbToGain ((float) d * kDriveDbPerStep + (p ? kPunishDb : 0.0f)) * core.baseGain();
                    double in = 0, out = 0;
                    for (int i = 0; i < n; ++i)
                    {
                        const float x = noise[(size_t) i];
                        const float y = core.post (0, core.shape (0, core.pre (0, x) * g)) / core.baseGain();
                        if (i >= skip) { in += (double) x * x; out += (double) y * y; }
                    }
                    t.db[s][p][d] = (float) (10.0 * std::log10 (std::max (out, 1e-20) / std::max (in, 1e-20)));
                }
        return t;
    }();
    return table;
}
}

float circuitGainDb (int style, float drive, bool punish)
{
    const auto& t = gainTable();
    style = juce::jlimit (0, kNumStyles - 1, style);
    const float d = juce::jlimit (0.0f, 10.0f, drive);
    const int i = std::min ((int) d, kDriveSteps - 2);
    const float f = d - (float) i;
    const auto& row = t.db[style][punish ? 1 : 0];
    return row[i] + f * (row[i + 1] - row[i]);
}

// ------------------------------------------------------------------ engine
void Engine::FracDelay::setup (double delay)
{
    // whole part in the line, the remaining 0.5..1.5 samples in the all-pass
    whole = std::max (0, (int) std::floor (delay - 0.5));
    const double d = delay - whole;
    a = (float) ((1.0 - d) / (1.0 + d));
    line.assign ((size_t) (whole + 1), 0.0f);
    reset();
}

Engine::Engine()
{
    for (int s = 0; s < kNumStyles; ++s)
    {
        cores2[(size_t) s] = std::make_unique<StyleCore> (s);
        cores8[(size_t) s] = std::make_unique<StyleCore> (s);
    }
    (void) gainTable();
}

Engine::~Engine() = default;

void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    using OS = juce::dsp::Oversampling<float>;
    os2 = std::make_unique<OS> (2, 1, OS::filterHalfBandFIREquiripple, true, false);
    os8 = std::make_unique<OS> (2, 3, OS::filterHalfBandFIREquiripple, true, false);
    os2->initProcessing ((size_t) kChunk);
    os8->initProcessing ((size_t) kChunk);
    // the filters' latencies are fractional and differ between 2x and 8x: pad both to the same
    // whole number of samples (at least one extra sample so the all-pass stays in its good range)
    const double l2 = os2->getLatencyInSamples(), l8 = os8->getLatencyInSamples();
    latency = (int) std::ceil (std::max (l2, l8)) + 1;
    for (int c = 0; c < 2; ++c)
    {
        align2[(size_t) c].setup (latency - l2);
        align8[(size_t) c].setup (latency - l8);
    }

    for (int s = 0; s < kNumStyles; ++s)
    {
        cores2[(size_t) s]->prepare (sr, sr * 2.0);
        cores8[(size_t) s]->prepare (sr, sr * 8.0);
    }

    wet.setSize (4, kChunk);   // 0,1 wet  2,3 dry
    for (auto& v : dryLine) v.assign ((size_t) (latency + kChunk + 1), 0.0f);

    for (auto& e : heatEnv) e.setup (sr, 1.0, 250.0);
    const int rampLen = (int) (sr * 0.02);
    driveRamp.setLength (rampLen);
    outRamp.setLength (rampLen);
    mixRamp.setLength (rampLen);
    fadeRamp.setLength ((int) (sr * 0.006));
    reset();
}

void Engine::reset()
{
    activeStyle = params.style;
    activeHq = params.hq;
    pendingStyle = pendingHq = -1;
    if (os2) os2->reset();
    if (os8) os8->reset();
    for (auto& c : cores2) c->reset();
    for (auto& c : cores8) c->reset();
    for (auto& v : dryLine) std::fill (v.begin(), v.end(), 0.0f);
    for (auto& d : align2) d.reset();
    for (auto& d : align8) d.reset();
    dryPos = 0;
    for (int c = 0; c < 2; ++c)
    {
        lowCut[(size_t) c].reset(); tiltLo[(size_t) c].reset(); tiltHi[(size_t) c].reset();
        hcPole[(size_t) c].reset(); hcBq[(size_t) c][0].reset(); hcBq[(size_t) c][1].reset();
        heatEnv[(size_t) c].reset();
    }
    filtersValid = false;
    updateFilters (true);

    auto& core = *cores2[(size_t) activeStyle];
    driveRamp.snap (dbToGain (params.drive * kDriveDbPerStep + (params.punish ? kPunishDb : 0.0f)) * core.baseGain());
    const float a = params.autoGain ? -circuitGainDb (activeStyle, params.drive, params.punish) : 0.0f;
    outRamp.snap (dbToGain (params.outputDb + a));
    mixRamp.snap (juce::jlimit (0.0f, 1.0f, params.mix));
    fadeRamp.snap (1.0f);
}

void Engine::updateFilters (bool force)
{
    const auto& p = params;
    if (force || ! filtersValid || p.lowCutHz != applied.lowCutHz || p.thump != applied.thump)
    {
        lowCutOn = p.thump || p.lowCutHz > 20.5f;
        for (auto& f : lowCut) f.highpass (sr, juce::jlimit (10.0f, 2000.0f, p.lowCutHz), p.thump ? 1.5 : 0.7071);
    }
    if (force || ! filtersValid || p.tone != applied.tone)
    {
        const float t = juce::jlimit (-1.0f, 1.0f, p.tone / 5.0f);
        tiltOn = std::abs (t) > 1.0e-4f;
        for (int c = 0; c < 2; ++c)
        {
            tiltLo[(size_t) c].lowShelf (sr, 250.0, 0.5, -4.0 * t);
            tiltHi[(size_t) c].highShelf (sr, 3000.0, 0.5, 4.0 * t);
        }
    }
    if (force || ! filtersValid || p.highCutHz != applied.highCutHz || p.steep != applied.steep)
    {
        const double f = juce::jlimit (1000.0, 20000.0, (double) p.highCutHz);
        hcOn = f < 19900.0 && f < sr * 0.45;
        hcSteep = p.steep;
        for (int c = 0; c < 2; ++c)
        {
            hcPole[(size_t) c].lowpass (sr, f);
            // 5th-order Butterworth = one pole + two biquads
            hcBq[(size_t) c][0].lowpass (sr, f, 0.6180340);
            hcBq[(size_t) c][1].lowpass (sr, f, 1.6180340);
        }
    }
    applied = p;
    filtersValid = true;
}

void Engine::process (float* const* ch, int numChannels, int numSamples)
{
    if (numChannels <= 0 || numSamples <= 0 || os2 == nullptr) return;
    float* ptrs[2] = { ch[0], numChannels > 1 ? ch[1] : nullptr };
    int done = 0;
    while (done < numSamples)
    {
        const bool changing = pendingStyle >= 0 || pendingHq >= 0
                              || params.style != activeStyle || params.hq != activeHq;
        const int n = std::min (changing ? 64 : kChunk, numSamples - done);
        float* sub[2] = { ptrs[0] + done, ptrs[1] ? ptrs[1] + done : nullptr };
        processChunk (sub, numChannels > 1 ? 2 : 1, n);
        done += n;
    }
}

void Engine::processChunk (float* const* ch, int numChannels, int n)
{
    // style / quality changes: fade the wet signal out, switch, fade back in
    if ((params.style != activeStyle || params.hq != activeHq) && pendingStyle < 0 && pendingHq < 0)
    {
        pendingStyle = params.style;
        pendingHq = params.hq ? 1 : 0;
        fadeRamp.set (0.0f);
    }
    if ((pendingStyle >= 0 || pendingHq >= 0) && fadeRamp.left == 0 && fadeRamp.cur == 0.0f)
    {
        const bool hqChanged = (pendingHq == 1) != activeHq;
        activeStyle = juce::jlimit (0, kNumStyles - 1, pendingStyle);
        activeHq = pendingHq == 1;
        pendingStyle = pendingHq = -1;
        (activeHq ? cores8 : cores2)[(size_t) activeStyle]->reset();
        if (hqChanged)
        {
            (activeHq ? os8 : os2)->reset();
            for (auto& d : (activeHq ? align8 : align2)) d.reset();
        }
        fadeRamp.set (1.0f);
    }

    updateFilters (false);
    auto& core = *(activeHq ? cores8 : cores2)[(size_t) activeStyle];
    const float base = core.baseGain();
    driveRamp.set (dbToGain (params.drive * kDriveDbPerStep + (params.punish ? kPunishDb : 0.0f)) * base);
    const float a = params.autoGain ? -circuitGainDb (activeStyle, params.drive, params.punish) : 0.0f;
    autoDb.store (a);
    outRamp.set (dbToGain (params.outputDb + a));
    mixRamp.set (juce::jlimit (0.0f, 1.0f, params.mix));

    float g[kChunk], og[kChunk], mx[kChunk];
    for (int i = 0; i < n; ++i)
    {
        g[i] = driveRamp.next();
        og[i] = outRamp.next() * fadeRamp.next() / base;
        mx[i] = mixRamp.next();
    }

    // ---- input side (host rate)
    const int dlen = (int) dryLine[0].size();
    for (int c = 0; c < 2; ++c)
    {
        const float* src = ch[std::min (c, numChannels - 1)];
        float* w = wet.getWritePointer (c);
        float* dry = wet.getWritePointer (2 + c);
        auto& line = dryLine[(size_t) c];
        int pos = dryPos;
        float peak = 0;
        for (int i = 0; i < n; ++i)
        {
            const float x = src[i];
            line[(size_t) pos] = x;
            int rd = pos - latency;
            if (rd < 0) rd += dlen;
            dry[i] = line[(size_t) rd];
            if (++pos >= dlen) pos = 0;

            float y = x;
            if (lowCutOn) y = lowCut[(size_t) c].process (y);
            if (tiltOn) y = tiltHi[(size_t) c].process (tiltLo[(size_t) c].process (y));
            y = core.pre (c, y) * g[i];
            w[i] = y;
            peak = std::max (peak, heatEnv[(size_t) c].process (std::abs (y)));
        }
        if (c == 1) dryPos = pos;
        if (c == 0) heat.store (gainToDb (peak));
        else heat.store (std::max (heat.load(), gainToDb (peak)));
    }

    // ---- the circuit (oversampled)
    {
        auto& os = activeHq ? *os8 : *os2;
        float* chans[2] = { wet.getWritePointer (0), wet.getWritePointer (1) };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        auto up = os.processSamplesUp (block);
        const int m = (int) up.getNumSamples();
        for (int c = 0; c < 2; ++c)
        {
            float* d = up.getChannelPointer ((size_t) c);
            for (int i = 0; i < m; ++i) d[i] = core.shape (c, d[i]);
        }
        os.processSamplesDown (block);
    }

    // ---- output side (host rate)
    for (int c = 0; c < 2; ++c)
    {
        float* w = wet.getWritePointer (c);
        const float* dry = wet.getReadPointer (2 + c);
        auto& align = (activeHq ? align8 : align2)[(size_t) c];
        for (int i = 0; i < n; ++i)
        {
            float y = align.process (w[i]);
            y = core.post (c, y);
            if (hcOn)
            {
                y = hcPole[(size_t) c].process (y);
                if (hcSteep) y = hcBq[(size_t) c][1].process (hcBq[(size_t) c][0].process (y));
            }
            y *= og[i];
            w[i] = dry[i] + mx[i] * (y - dry[i]);
        }
    }

    for (int c = 0; c < numChannels; ++c)
        std::copy (wet.getReadPointer (c), wet.getReadPointer (c) + n, ch[c]);
    if (numChannels == 1)
        for (int i = 0; i < n; ++i) ch[0][i] = 0.5f * (wet.getSample (0, i) + wet.getSample (1, i));
}
}
