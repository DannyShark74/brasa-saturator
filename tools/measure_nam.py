"""Measure NAM captures: harmonic profile vs level and small-signal frequency response.
usage: PYTHONPATH=stubs python3 -I? measure_nam.py <out.json> <file.nam>...
"""
import json, sys, math
import numpy as np
import torch
from nam.models import init_from_nam

SR = 48000
torch.set_num_threads(2)


def load(path):
    j = json.load(open(path))
    if j.get("architecture") == "SlimmableContainer":
        sm = j["config"]["submodels"][-1]["model"]
        sm.setdefault("sample_rate", j.get("sample_rate"))
        meta = j.get("metadata", {})
    else:
        sm, meta = j, j.get("metadata", {})
    m = init_from_nam(sm)
    m.eval()
    return m, meta


def run(m, x):
    with torch.no_grad():
        return m(torch.from_numpy(x.astype(np.float32))[None])[0].numpy().astype(np.float64)


def harmonics(m, f0, level_db, n_h=9):
    dur = 1.0
    t = np.arange(int(SR * dur)) / SR
    amp = 10 ** (level_db / 20)
    fade = np.minimum(1, t / 0.05)
    x = amp * fade * np.sin(2 * np.pi * f0 * t)
    y = run(m, x)
    seg = y[int(0.4 * SR):int(0.4 * SR) + 24000]   # 0.5 s, integer cycles for 100/1000 Hz
    seg = seg - seg.mean()
    w = np.hanning(len(seg))
    S = np.abs(np.fft.rfft(seg * w)) * 2 / w.sum()
    fr = np.fft.rfftfreq(len(seg), 1 / SR)
    def peak(f):
        k = int(round(f / (fr[1] - fr[0])))
        return S[max(0, k - 2):k + 3].max()
    h = [peak(f0 * i) for i in range(1, n_h + 1) if f0 * i < SR / 2 - 500]
    h1 = h[0]
    rel = [20 * math.log10(max(v, 1e-12) / h1) for v in h[1:]]
    gain = 20 * math.log10(h1 / amp)
    thd = math.sqrt(sum(v * v for v in h[1:])) / h1
    even = math.sqrt(sum(v * v for v in h[1::2]))
    odd = math.sqrt(sum(v * v for v in h[2::2]))
    return {"gain_db": round(gain, 2), "thd_pct": round(100 * thd, 3),
            "h_rel_db": [round(r, 1) for r in rel],
            "even_over_odd_db": round(20 * math.log10(max(even, 1e-12) / max(odd, 1e-12)), 1)}


def freq_response(m, level_db=-40):
    rng = np.random.default_rng(1)
    n = SR * 4
    x = rng.standard_normal(n) * 10 ** (level_db / 20)
    y = run(m, x)
    x, y = x[SR // 2:], y[SR // 2:]
    nfft = 8192
    hop = nfft // 2
    w = np.hanning(nfft)
    Pxx = np.zeros(nfft // 2 + 1); Pxy = np.zeros(nfft // 2 + 1, complex)
    for s in range(0, len(x) - nfft, hop):
        X = np.fft.rfft(x[s:s + nfft] * w); Y = np.fft.rfft(y[s:s + nfft] * w)
        Pxx += np.abs(X) ** 2; Pxy += np.conj(X) * Y
    H = np.abs(Pxy / Pxx)
    fr = np.fft.rfftfreq(nfft, 1 / SR)
    pts = [20, 30, 50, 80, 100, 150, 200, 300, 500, 800, 1000, 2000, 3000, 5000, 8000, 10000, 12000, 15000, 18000, 20000]
    ref = H[np.argmin(abs(fr - 1000))]
    out = {}
    for p in pts:
        band = (fr > p / 1.12) & (fr < p * 1.12)
        out[p] = round(20 * math.log10(H[band].mean() / ref), 2)
    return out, round(20 * math.log10(ref), 2)


def main():
    out_path, files = sys.argv[1], sys.argv[2:]
    res = {}
    for f in files:
        m, meta = load(f)
        name = f.split("/")[-1][:-4]
        r = {"meta": {k: meta.get(k) for k in ("input_level_dbu", "output_level_dbu", "loudness", "gain")}}
        fresp, g1k = freq_response(m)
        r["fr_db_rel_1k"] = fresp
        r["small_signal_gain_1k_db"] = g1k
        r["h1k"] = {lv: harmonics(m, 1000, lv) for lv in (-36, -24, -18, -12, -6, 0)}
        r["h100"] = {lv: harmonics(m, 100, lv) for lv in (-24, -12, -6, 0)}
        res[name] = r
        print(name, "gain", g1k, "thd1k", [r["h1k"][lv]["thd_pct"] for lv in r["h1k"]], flush=True)
    json.dump(res, open(out_path, "w"), indent=1)


if __name__ == "__main__":
    main()
