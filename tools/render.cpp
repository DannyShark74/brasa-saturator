// Offline checks, measurements and file rendering for the Brasa engine.
//   BrasaRender --test                          engine checks (levels, NaN, latency, aliasing, filters, CPU)
//   BrasaRender --measure <out.json>            harmonic profile / frequency response per style and drive
//   BrasaRender --file <in.wav> <out.wav> [preset name] [id=value ...]
#include <juce_audio_formats/juce_audio_formats.h>
#include "Engine.h"
#include "ParamTable.h"
#include <chrono>
#include <cstdio>
#include <random>

using namespace brasa;

static Params paramsWith (std::map<std::string, float> overrides, const Preset* preset = nullptr)
{
    std::map<std::string, float> v;
    for (auto& d : paramTable()) v[d.id] = d.def;
    if (preset) for (auto& kv : preset->values) v[kv.first] = kv.second;
    for (auto& kv : overrides) v[kv.first] = kv.second;
    return engineParamsFrom ([&v] (const std::string& id)
    {
        auto it = v.find (id);
        if (it == v.end()) { std::fprintf (stderr, "unknown param %s\n", id.c_str()); std::exit (2); }
        return it->second;
    });
}

using Buf = std::vector<std::vector<float>>;   // [channel][sample]

static Buf run (const Params& p, const Buf& in, double sr, int block = 256, int* latencyOut = nullptr,
                std::function<void (Engine&, int)> perBlock = {})
{
    Engine e;
    e.setParams (p);
    e.prepare (sr, block);
    if (latencyOut) *latencyOut = e.latencySamples();
    Buf out = in;
    const int n = (int) in[0].size();
    const int nch = (int) in.size();
    for (int pos = 0; pos < n; pos += block)
    {
        const int k = std::min (block, n - pos);
        if (perBlock) perBlock (e, pos);
        float* ptr[2] = { out[0].data() + pos, nch > 1 ? out[1].data() + pos : nullptr };
        e.process (ptr, nch, k);
    }
    return out;
}

static Buf sine (double sr, double f, float amp, double seconds, int nch = 2)
{
    const int n = (int) (sr * seconds);
    Buf b ((size_t) nch, std::vector<float> ((size_t) n));
    for (int i = 0; i < n; ++i)
    {
        const float fade = std::min (1.0f, (float) i / (float) (sr * 0.02));
        const float v = amp * fade * (float) std::sin (2 * kPi * f * i / sr);
        for (auto& c : b) c[(size_t) i] = v;
    }
    return b;
}

static Buf noise (double sr, float rmsDb, double seconds, bool pink, unsigned seed = 1)
{
    const int n = (int) (sr * seconds);
    std::mt19937 rng (seed);
    std::normal_distribution<float> nd (0.0f, 1.0f);
    Buf b (2, std::vector<float> ((size_t) n));
    for (int c = 0; c < 2; ++c)
    {
        float b0 = 0, b1 = 0, b2 = 0;
        double acc = 0;
        for (int i = 0; i < n; ++i)
        {
            float w = nd (rng);
            if (pink)
            {
                b0 = 0.99765f * b0 + w * 0.0990460f; b1 = 0.96300f * b1 + w * 0.2965164f; b2 = 0.57000f * b2 + w * 1.0526913f;
                w = b0 + b1 + b2 + w * 0.1848f;
            }
            b[(size_t) c][(size_t) i] = w;
            acc += (double) w * w;
        }
        const float g = dbToGain (rmsDb) / (float) std::sqrt (acc / n);
        for (auto& v : b[(size_t) c]) v *= g;
    }
    return b;
}

static double rmsDb (const std::vector<float>& x, int from, int to)
{
    double a = 0;
    for (int i = from; i < to; ++i) a += (double) x[(size_t) i] * x[(size_t) i];
    return 10 * std::log10 (std::max (a / std::max (1, to - from), 1e-30));
}

// amplitude of frequency f in x[from..from+len) (Hann window, direct DFT)
static double amplitudeAt (const std::vector<float>& x, int from, int len, double sr, double f)
{
    double re = 0, im = 0, ws = 0;
    for (int i = 0; i < len; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * kPi * i / (len - 1));
        const double ph = 2 * kPi * f * i / sr;
        re += w * x[(size_t) (from + i)] * std::cos (ph);
        im -= w * x[(size_t) (from + i)] * std::sin (ph);
        ws += w;
    }
    return 2 * std::sqrt (re * re + im * im) / ws;
}

struct Harm
{
    double thd = 0, evenOverOddDb = 0, gainDb = 0;
    std::vector<double> relDb;   // H2..H9 relative to H1
};

static Harm harmonics (const Params& p, double sr, double f0, float levelDbRms)
{
    const float amp = dbToGain (levelDbRms) * std::sqrt (2.0f);
    auto in = sine (sr, f0, amp, 0.9, 1);
    auto out = run (p, in, sr);
    const int from = (int) (sr * 0.4), len = (int) (sr * 0.5);
    std::vector<double> h;
    for (int k = 1; k <= 9 && f0 * k < sr * 0.45; ++k) h.push_back (amplitudeAt (out[0], from, len, sr, f0 * k));
    Harm r;
    r.gainDb = 20 * std::log10 (h[0] / amp);
    double s = 0, ev = 0, od = 0;
    for (size_t k = 1; k < h.size(); ++k)
    {
        r.relDb.push_back (20 * std::log10 (std::max (h[k], 1e-12) / h[0]));
        s += h[k] * h[k];
        ((k % 2) ? ev : od) += h[k] * h[k];   // k=1 -> H2 (even)
    }
    r.thd = 100 * std::sqrt (s) / h[0];
    r.evenOverOddDb = 10 * std::log10 (std::max (ev, 1e-24) / std::max (od, 1e-24));
    return r;
}

// small-signal response of the whole engine at f, relative to 1 kHz (dB)
static double responseDb (const Params& p, double sr, double f)
{
    auto at = [&] (double fr)
    {
        auto in = sine (sr, fr, dbToGain (-50.0f), 0.8, 1);
        auto out = run (p, in, sr);
        return amplitudeAt (out[0], (int) (sr * 0.3), (int) (sr * 0.45), sr, fr);
    };
    return 20 * std::log10 (at (f) / at (1000.0));
}

// --------------------------------------------------------------------------------------- tests
static int failures = 0;
static void check (bool ok, const char* what, double value = 0)
{
    std::printf ("  %s %-62s %10.3f\n", ok ? "ok  " : "FAIL", what, value);
    if (! ok) ++failures;
}

static int runTests()
{
    const double sr = 48000;

    std::printf ("finite output, sane level (every style, punish, HQ, extreme settings)\n");
    {
        auto loud = noise (sr, -3, 1.0, false, 7);
        for (int i = 0; i < (int) loud[0].size(); i += 4800)   // DC steps and full-scale clicks
            for (int k = 0; k < 200 && i + k < (int) loud[0].size(); ++k) { loud[0][(size_t) (i + k)] = 1.0f; loud[1][(size_t) (i + k)] = -1.0f; }
        double worst = 0;
        bool finite = true;
        for (int s = 0; s < kNumStyles; ++s)
            for (int pun = 0; pun < 2; ++pun)
                for (int hq = 0; hq < 2; ++hq)
                    for (int set = 0; set < 3; ++set)
                    {
                        std::map<std::string, float> o { { pid::style, (float) s }, { pid::punish, (float) pun }, { pid::hq, (float) hq },
                                                         { pid::drive, 10 }, { pid::autoGain, 0 }, { pid::output, 12 } };
                        if (set == 1) { o[pid::lowCut] = 2000; o[pid::thump] = 1; o[pid::tone] = -5; o[pid::highCut] = 1000; o[pid::steep] = 1; }
                        if (set == 2) { o[pid::drive] = 0; o[pid::tone] = 5; o[pid::mix] = 0.5f; o[pid::lowCut] = 20; o[pid::thump] = 1; }
                        auto out = run (paramsWith (o), loud, sr, 333);
                        for (auto& c : out)
                            for (float v : c)
                            {
                                if (! std::isfinite (v)) finite = false;
                                worst = std::max (worst, (double) std::abs (v));
                            }
                    }
        check (finite, "no NaN / Inf");
        check (worst < 40.0, "peak below +32 dBFS even at Drive 10 + Punish + Output +12", worst);
    }

    std::printf ("silence\n");
    {
        Buf z (2, std::vector<float> ((size_t) (sr * 1.0), 0.0f));
        double worst = 0;
        for (int s = 0; s < kNumStyles; ++s)
            for (int hq = 0; hq < 2; ++hq)
            {
                auto out = run (paramsWith ({ { pid::style, (float) s }, { pid::drive, 10 }, { pid::punish, 1 }, { pid::hq, (float) hq } }), z, sr);
                for (auto& c : out) for (float v : c) worst = std::max (worst, (double) std::abs (v));
            }
        check (worst < 1.0e-6, "silence in -> silence out", worst);
    }

    std::printf ("latency and the dry path\n");
    {
        auto in = noise (sr, -12, 0.5, false, 3);
        for (int hq = 0; hq < 2; ++hq)
        {
            int lat = 0;
            auto out = run (paramsWith ({ { pid::mix, 0 }, { pid::hq, (float) hq } }), in, sr, 256, &lat);
            double err = 0;
            for (int c = 0; c < 2; ++c)
                for (int i = lat; i < (int) in[0].size(); ++i)
                    err = std::max (err, (double) std::abs (out[(size_t) c][(size_t) i] - in[(size_t) c][(size_t) (i - lat)]));
            check (err < 1.0e-6, hq ? "Mix 0 = input delayed by the latency (HQ)" : "Mix 0 = input delayed by the latency (2x)", err);
            check (lat > 0 && lat < 200, "reported latency (samples)", lat);
        }
        // the wet signal of 2x and HQ lines up (same latency, no comb filter when switching)
        auto quiet = noise (sr, -50, 0.6, true, 4);
        auto a = run (paramsWith ({ { pid::style, kConsola }, { pid::drive, 0 }, { pid::hq, 0 } }), quiet, sr);
        auto b = run (paramsWith ({ { pid::style, kConsola }, { pid::drive, 0 }, { pid::hq, 1 } }), quiet, sr);
        double ab = 0, aa = 0, bb = 0;
        for (int i = (int) (sr * 0.1); i < (int) a[0].size(); ++i)
        { ab += (double) a[0][(size_t) i] * b[0][(size_t) i]; aa += (double) a[0][(size_t) i] * a[0][(size_t) i]; bb += (double) b[0][(size_t) i] * b[0][(size_t) i]; }
        // (the 8x filters turn the phase a little above ~12 kHz, so not exactly 1)
        check (ab / std::sqrt (aa * bb) > 0.98, "2x and HQ outputs aligned (correlation, pink noise)", ab / std::sqrt (aa * bb));
        // dry / wet line up: Mix 50% of a near-linear setting must not comb-filter
        for (int hq = 0; hq < 2; ++hq)
            for (double f : { 3000.0, 12000.0, 18000.0 })
            {
                const double r = responseDb (paramsWith ({ { pid::style, kConsola }, { pid::drive, 0 }, { pid::mix, 0.5f }, { pid::hq, (float) hq } }), sr, f)
                                 - responseDb (paramsWith ({ { pid::style, kConsola }, { pid::drive, 0 }, { pid::mix, 1.0f }, { pid::hq, (float) hq } }), sr, f);
                char buf[96];
                std::snprintf (buf, sizeof buf, "Mix 50%% vs 100%%, %s, %.0f kHz: no comb filtering (dB)", hq ? "HQ" : "2x", f / 1000);
                check (std::abs (r) < 1.5, buf, r);
            }
    }

    std::printf ("aliasing (6.5 kHz sine at -6 dBFS, Pentodo, Drive 8)\n");
    {
        double alias[2] = { 0, 0 };
        for (int hq = 0; hq < 2; ++hq)
        {
            auto in = sine (sr, 6500, 0.5f, 0.8, 1);
            auto out = run (paramsWith ({ { pid::style, kPentodo }, { pid::drive, 8 }, { pid::hq, (float) hq } }), in, sr);
            const int from = (int) (sr * 0.3), len = 16384;
            // energy away from the harmonics, via a coarse scan
            const double fund = amplitudeAt (out[0], from, len, sr, 6500);
            double worst = 0;
            for (double f = 200; f < 20000; f += 37)
            {
                const double nearest = std::round (f / 6500) * 6500;
                if (std::abs (f - nearest) < 150) continue;
                worst = std::max (worst, amplitudeAt (out[0], from, len, sr, f));
            }
            alias[hq] = 20 * std::log10 (worst / fund);
            check (true, hq ? "strongest alias, HQ (dB rel. fundamental)" : "strongest alias, 2x (dB rel. fundamental)", alias[hq]);
        }
        check (alias[1] < alias[0] - 10, "HQ removes at least 10 dB more aliasing", alias[0] - alias[1]);
        check (alias[1] < -50, "HQ aliases below -50 dB", alias[1]);
    }

    std::printf ("auto gain (pink noise at -18 dBFS RMS)\n");
    {
        auto in = noise (sr, -18, 1.2, true, 9);
        double worst = 0;
        for (int s = 0; s < kNumStyles; ++s)
            for (int pun = 0; pun < 2; ++pun)
                for (float d : { 0.0f, 3.0f, 5.0f, 8.0f, 10.0f })
                {
                    auto out = run (paramsWith ({ { pid::style, (float) s }, { pid::drive, d }, { pid::punish, (float) pun } }), in, sr);
                    const int from = (int) (sr * 0.3), to = (int) in[0].size();
                    const double diff = rmsDb (out[0], from, to) - rmsDb (in[0], from - 0, to);
                    worst = std::max (worst, std::abs (diff));
                }
        check (worst < 2.0, "Auto keeps the level within 2 dB (worst case, dB)", worst);
    }

    std::printf ("character at Drive 5, 1 kHz at -18 dBFS RMS (THD %%)\n");
    {
        double thd[kNumStyles];
        for (int s = 0; s < kNumStyles; ++s)
        {
            auto h = harmonics (paramsWith ({ { pid::style, (float) s }, { pid::drive, 5 } }), sr, 1000, -18);
            thd[s] = h.thd;
            std::printf ("      %-9s THD %6.2f%%  even/odd %+6.1f dB   H2 %6.1f  H3 %6.1f  H5 %6.1f  H9 %6.1f\n", styleName (s), h.thd,
                         h.evenOverOddDb, h.relDb[0], h.relDb[1], h.relDb[3], h.relDb.size() > 7 ? h.relDb[7] : -200.0);
        }
        check (thd[kCinta] < thd[kConsola] && thd[kCinta] < thd[kGermanio], "Cinta is the smoothest", thd[kCinta]);
        check (thd[kTriodo] > thd[kConsola] && thd[kTriodo] > thd[kGermanio], "Triodo drives harder than the transistor styles", thd[kTriodo]);
        check (thd[kPentodo] > thd[kTriodo], "Pentodo has the most distortion", thd[kPentodo]);
        auto t = harmonics (paramsWith ({ { pid::style, kTriodo }, { pid::drive, 5 } }), sr, 1000, -18);
        check (t.evenOverOddDb > 3, "Triodo: even harmonics dominate (dB)", t.evenOverOddDb);
        auto lo = harmonics (paramsWith ({ { pid::style, kGermanio }, { pid::drive, 2 } }), sr, 100, -18);
        auto hi = harmonics (paramsWith ({ { pid::style, kGermanio }, { pid::drive, 2 } }), sr, 1000, -18);
        check (lo.thd > 1.5 * hi.thd, "Germanio: the iron distorts lows more than mids (ratio)", lo.thd / hi.thd);
        for (int s = 0; s < kNumStyles; ++s)
        {
            auto z = harmonics (paramsWith ({ { pid::style, (float) s }, { pid::drive, 0 } }), sr, 1000, -18);
            if (z.thd < 0.02 || z.thd > 5.0) check (false, (std::string ("Drive 0 still colours a little: ") + styleName (s)).c_str(), z.thd);
        }
        check (true, "Drive 0 colours every style a little (0.02..5 % THD)");
    }

    std::printf ("filters (-50 dBFS, Consola at Drive 0)\n");
    {
        auto P = [] (std::map<std::string, float> o) { o[pid::style] = kConsola; o[pid::drive] = 0; return paramsWith (o); };
        auto lc = P ({ { pid::lowCut, 200 } });
        check (responseDb (lc, sr, 50) < -20, "Low Cut 200 Hz: 50 Hz down > 20 dB", responseDb (lc, sr, 50));
        const double bump = responseDb (P ({ { pid::lowCut, 200 }, { pid::thump, 1 } }), sr, 200) - responseDb (lc, sr, 200);
        check (bump > 2.5 && bump < 7.0, "Thump: bump at the cut-off frequency (dB)", bump);
        const double g = responseDb (P ({ { pid::highCut, 4000 } }), sr, 8000);
        check (g < -4 && g > -9, "High Cut 4 kHz gentle: 8 kHz (dB)", g);
        const double st = responseDb (P ({ { pid::highCut, 4000 }, { pid::steep, 1 } }), sr, 8000);
        check (st < -25, "High Cut 4 kHz steep: 8 kHz (dB)", st);
        const double tilt = responseDb (P ({ { pid::tone, 5 } }), sr, 10000) - responseDb (P ({ { pid::tone, 5 } }), sr, 100);
        check (tilt > 4 && tilt < 10, "Tone +5: 10 kHz vs 100 Hz (dB)", tilt);
    }

    std::printf ("style / HQ switching while playing\n");
    {
        auto in = sine (sr, 220, 0.3f, 2.0);
        double jump = 0;
        bool finite = true;
        auto out = run (paramsWith ({ { pid::drive, 6 } }), in, sr, 256, nullptr, [] (Engine& e, int pos)
        {
            auto p = paramsWith ({ { pid::drive, 6 } });
            p.style = (pos / 9600) % kNumStyles;
            p.hq = ((pos / 14400) % 2) == 1;
            e.setParams (p);
        });
        for (size_t i = 1; i < out[0].size(); ++i)
        {
            if (! std::isfinite (out[0][i])) finite = false;
            jump = std::max (jump, (double) std::abs (out[0][i] - out[0][i - 1]));
        }
        check (finite, "finite while switching");
        check (jump < 0.25, "no clicks (largest sample-to-sample step)", jump);
    }

    std::printf ("sample rates\n");
    for (double rate : { 44100.0, 88200.0, 96000.0, 192000.0 })
    {
        auto h = harmonics (paramsWith ({ { pid::drive, 5 } }), rate, 1000, -18);
        auto ref = harmonics (paramsWith ({ { pid::drive, 5 } }), 48000, 1000, -18);
        char buf[96];
        std::snprintf (buf, sizeof buf, "%.1f kHz: same character as 48 kHz (THD diff, %%)", rate / 1000);
        check (std::isfinite (h.thd) && std::abs (h.thd - ref.thd) < 0.25 * ref.thd + 0.5, buf, h.thd - ref.thd);
    }

    std::printf ("CPU (stereo, 10 s at 48 kHz, block 256)\n");
    for (int hq = 0; hq < 2; ++hq)
    {
        auto in = noise (sr, -12, 10.0, true, 11);
        const auto t0 = std::chrono::steady_clock::now();
        run (paramsWith ({ { pid::style, kPentodo }, { pid::drive, 7 }, { pid::hq, (float) hq }, { pid::lowCut, 80 }, { pid::thump, 1 },
                           { pid::tone, 2 }, { pid::highCut, 9000 }, { pid::steep, 1 }, { pid::mix, 0.7f } }), in, sr);
        const double cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 10.0 * 100.0;
        check (cpu < (hq ? 25.0 : 8.0), hq ? "HQ: % of one core" : "2x: % of one core", cpu);
    }

    std::printf (failures ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", failures);
    return failures ? 1 : 0;
}

// --------------------------------------------------------------------------------------- measure
static int runMeasure (const char* path)
{
    const double sr = 48000;
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    for (int s = 0; s < kNumStyles; ++s)
        for (float d : { 0.0f, 3.0f, 5.0f, 7.0f, 10.0f })
            for (int pun = 0; pun < 2; ++pun)
            {
                if (pun && d < 10) continue;
                auto p = paramsWith ({ { pid::style, (float) s }, { pid::drive, d }, { pid::punish, (float) pun }, { pid::autoGain, 0 } });
                juce::DynamicObject::Ptr o = new juce::DynamicObject();
                auto harmObj = [&] (double f0, std::initializer_list<int> levels)
                {
                    juce::DynamicObject::Ptr h = new juce::DynamicObject();
                    for (int lv : levels)
                    {
                        auto r = harmonics (p, sr, f0, (float) lv - 3.0f);   // sine peak at lv dBFS
                        juce::DynamicObject::Ptr e = new juce::DynamicObject();
                        e->setProperty ("thd_pct", r.thd);
                        e->setProperty ("even_over_odd_db", r.evenOverOddDb);
                        e->setProperty ("gain_db", r.gainDb);
                        juce::Array<juce::var> rel;
                        for (double v : r.relDb) rel.add (std::round (v * 10) / 10);
                        e->setProperty ("h_rel_db", rel);
                        h->setProperty (juce::String (lv), juce::var (e.get()));
                    }
                    return juce::var (h.get());
                };
                o->setProperty ("h1k", harmObj (1000, { -36, -24, -18, -12, -6, 0 }));
                o->setProperty ("h100", harmObj (100, { -24, -12, -6, 0 }));
                juce::DynamicObject::Ptr fr = new juce::DynamicObject();
                for (double f : { 30.0, 50.0, 100.0, 200.0, 500.0, 3000.0, 5000.0, 10000.0, 15000.0, 20000.0 })
                    fr->setProperty (juce::String ((int) f), std::round (responseDb (p, sr, f) * 100) / 100);
                o->setProperty ("fr_db_rel_1k", juce::var (fr.get()));
                const auto key = juce::String (styleName (s)) + " d" + juce::String ((int) d) + (pun ? " punish" : "");
                root->setProperty (key, juce::var (o.get()));
                std::printf ("%s\n", key.toRawUTF8());
            }
    juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (path)).replaceWithText (juce::JSON::toString (juce::var (root.get())));
    return 0;
}

// --------------------------------------------------------------------------------------- files
static int runFile (int argc, char** argv)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    const juce::File inFile (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (inFile));
    if (! rd) { std::fprintf (stderr, "cannot read %s\n", argv[2]); return 1; }
    juce::AudioBuffer<float> b ((int) rd->numChannels, (int) rd->lengthInSamples);
    rd->read (&b, 0, b.getNumSamples(), 0, true, true);

    const Preset* preset = nullptr;
    std::map<std::string, float> o;
    for (int i = 4; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a.contains ("=")) o[a.upToFirstOccurrenceOf ("=", false, false).toStdString()] = a.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
        else for (auto& pr : factoryPresets()) if (a == juce::String (pr.name)) preset = &pr;
    }
    Buf in (2, std::vector<float> ((size_t) b.getNumSamples()));
    for (int c = 0; c < 2; ++c)
        std::copy (b.getReadPointer (std::min (c, b.getNumChannels() - 1)), b.getReadPointer (std::min (c, b.getNumChannels() - 1)) + b.getNumSamples(), in[(size_t) c].begin());
    int lat = 0;
    auto out = run (paramsWith (o, preset), in, rd->sampleRate, 512, &lat);

    juce::AudioBuffer<float> ob (2, b.getNumSamples() - lat);
    for (int c = 0; c < 2; ++c) std::copy (out[(size_t) c].begin() + lat, out[(size_t) c].end(), ob.getWritePointer (c));
    const juce::File outFile (juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]));
    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (outFile), rd->sampleRate, 2, 24, {}, 0));
    if (! w) return 1;
    w->writeFromAudioSampleBuffer (ob, 0, ob.getNumSamples());
    return 0;
}

// quick look: BrasaRender --harm <style> <drive> <freq> <level dBFS RMS> [id=value ...]
static int runHarm (int argc, char** argv)
{
    std::map<std::string, float> o { { pid::style, (float) atof (argv[2]) }, { pid::drive, (float) atof (argv[3]) } };
    for (int i = 6; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        o[a.upToFirstOccurrenceOf ("=", false, false).toStdString()] = a.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
    }
    auto h = harmonics (paramsWith (o), 48000, atof (argv[4]), (float) atof (argv[5]));
    std::printf ("THD %6.2f%%  e/o %+5.1f  gain %+5.1f dB  H2..:", h.thd, h.evenOverOddDb, h.gainDb);
    for (double v : h.relDb) std::printf (" %6.1f", v);
    std::printf ("\n");
    return 0;
}

int main (int argc, char** argv)
{
    if (argc >= 6 && juce::String (argv[1]) == "--harm") return runHarm (argc, argv);
    if (argc >= 2 && juce::String (argv[1]) == "--test") return runTests();
    if (argc >= 3 && juce::String (argv[1]) == "--measure") return runMeasure (argv[2]);
    if (argc >= 4 && juce::String (argv[1]) == "--file") return runFile (argc, argv);
    std::fprintf (stderr, "usage: BrasaRender --test | --measure out.json | --file in.wav out.wav [preset] [id=value ...]\n");
    return 2;
}
