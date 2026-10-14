#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include "ReverbDSP.h"

namespace vf
{
using AC = juce::dsp::IIR::ArrayCoefficients<float>;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-7f)); }
inline float timeCoef (float ms, double fs) noexcept { return (float) std::exp (-1.0 / (std::max (0.01, (double) ms) * 0.001 * fs)); }

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] {}, z2[2] {};
    void set (const std::array<float, 6>& c) noexcept
    {
        const float inv = 1.0f / c[3];
        b0 = c[0] * inv; b1 = c[1] * inv; b2 = c[2] * inv; a1 = c[4] * inv; a2 = c[5] * inv;
    }
    void reset() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    float process (int ch, float x) noexcept
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
};

/** Voice character presets that retune every chain. */
struct Voice
{
    const char* name;
    float lowF, midF, highF;        // tone EQ centres
    float deessF;                   // default sibilance centre
    float magicF, presenceF;        // parallel chain centres
    float attitudeLo, attitudeHi;   // attitude band
    float slapMs;                   // space slap-back time
};
inline const std::array<Voice, 3>& voices()
{
    static const std::array<Voice, 3> v {{
        { "MALE",    150.0f, 1000.0f,  9000.0f, 6000.0f, 2600.0f, 3200.0f, 180.0f, 6500.0f, 110.0f },
        { "MALE 2",  120.0f,  800.0f,  8000.0f, 5500.0f, 2200.0f, 2700.0f, 150.0f, 5500.0f, 125.0f },
        { "FEMALE",  220.0f, 1400.0f, 11000.0f, 7500.0f, 3600.0f, 4500.0f, 250.0f, 8000.0f,  95.0f },
    }};
    return v;
}

//==============================================================================
/** Simple stereo-linked compressor used inside the chains (fast, fixed character). */
struct ChainComp
{
    float env = 0, gr = 0, aAtt = 0, aRel = 0, thr = -30, ratio = 6;
    void prepare (double fs, float attMs, float relMs, float threshold, float r)
    {
        aAtt = timeCoef (attMs, fs); aRel = timeCoef (relMs, fs); thr = threshold; ratio = r; env = gr = 0;
    }
    /** returns the linear gain to apply */
    float gain (float detector) noexcept
    {
        const float a = std::abs (detector);
        env = a > env ? aAtt * env + (1 - aAtt) * a : aRel * env + (1 - aRel) * a;
        const float over = gainToDb (env) - thr;
        const float target = over > 0 ? over * (1.0f - 1.0f / ratio) : 0.0f;
        gr += 0.2f * (target - gr);
        return dbToGain (-gr);
    }
};

//==============================================================================
/** Opto compressor with three colours: FLAT, WARM, TUBE. Amount 0..100. */
class OptoComp
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate; env = s1 = s2 = memory = gr = 0;
        aAtt = timeCoef (10.0f, fs); aRel1 = timeCoef (60.0f, fs); aRms = 1.0f - timeCoef (10.0f, fs);
        aMemUp = 1.0f - timeCoef (2500.0f, fs); aMemDn = 1.0f - timeCoef (6000.0f, fs);
        warmShelf.reset(); warmShelf.set (AC::makeLowShelf (fs, 180.0f, 0.7f, dbToGain (1.2f)));
    }
    void setParams (float amount, int colour) { amt = amount; col = colour; }
    void process (float& l, float& r) noexcept
    {
        if (amt < 0.5f) { gr = 0; return; }
        const float thr = -6.0f - amt * 0.36f;            // 0..100 -> -6..-42 dB
        const float pk = std::max (l * l, r * r);
        env += aRms * (pk - env);
        const float lvl = 10.0f * std::log10 (env + 1.0e-12f) + 3.0f;
        const float over = lvl - thr;
        const float knee = 10.0f, slope = 1.0f - 1.0f / 3.5f;
        float target = 0;
        if (2 * over > knee) target = slope * over;
        else if (2 * over > -knee) target = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2 * knee);
        memory += target > 1.5f ? aMemUp * (1 - memory) : aMemDn * (0 - memory);
        const float aRel2 = (float) (1.0 / ((0.5 + 3.0 * memory) * fs));
        auto stage = [&] (float& s, float aRel) { s = target > s ? aAtt * s + (1 - aAtt) * target : s + aRel * (target - s) * (aRel < 0.5f ? 1.0f : 1.0f); };
        s1 = target > s1 ? aAtt * s1 + (1 - aAtt) * target : aRel1 * s1 + (1 - aRel1) * target;
        stage (s2, aRel2);
        gr = 0.5f * (s1 + s2);
        const float g = dbToGain (-gr) * dbToGain (gr * 0.55f);   // automatic make-up (partial)
        l *= g; r *= g;
        if (col == 1)       // WARM: thick low end + soft round saturation
        {
            l = warmShelf.process (0, l); r = warmShelf.process (1, r);
            l += 0.35f * (warm (l) - l); r += 0.35f * (warm (r) - r);
        }
        else if (col == 2)  // TUBE: asymmetric triode glow (2nd harmonic)
        {
            l += 0.45f * (tube (l) - l); r += 0.45f * (tube (r) - r);
        }
    }
    float getGr() const noexcept { return gr; }
private:
    static float warm (float x) noexcept { constexpr float b = 0.2f; static const float tb = std::tanh (b), n = 1.0f / (1.0f - tb * tb); return (std::tanh (x + b) - tb) * n; }
    static float tube (float x) noexcept
    {
        const float h = x * 0.8f;
        const float y = h >= 0 ? 1.0f - std::exp (-h) : -(1.0f - std::exp (0.55f * h)) / 0.55f;
        return y / 0.8f;
    }
    double fs = 48000;
    float amt = 0; int col = 0;
    float env = 0, s1 = 0, s2 = 0, memory = 0, gr = 0, aAtt = 0, aRel1 = 0, aRms = 1, aMemUp = 0, aMemDn = 0;
    Biquad warmShelf;
};

//==============================================================================
/** Dynamic de-esser band: Linkwitz-Riley split at the chosen frequency, only the upper band is reduced. */
class DeEssBand
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        split.prepare ({ fs, 512, 2 }); split.setType (juce::dsp::LinkwitzRileyFilterType::lowpass); split.reset();
        det.reset(); env = gr = 0; lastF = -1;
        aAtt = timeCoef (0.5f, fs); aRel = timeCoef (60.0f, fs);
    }
    void setParams (float amount, float freq)
    {
        amt = amount;
        if (freq != lastF)
        {
            lastF = freq;
            split.setCutoffFrequency (std::min (freq * 0.8f, (float) fs * 0.45f));
            det.set (AC::makeBandPass (fs, std::min (freq * 1.15f, (float) fs * 0.45f), 1.4f));
        }
    }
    void process (float& l, float& r) noexcept
    {
        float ll, hl, lr, hr;
        split.processSample (0, l, ll, hl);
        split.processSample (1, r, lr, hr);
        if (amt < 0.5f) { l = ll + hl; r = lr + hr; gr = 0; return; }
        const float d = std::max (std::abs (det.process (0, l)), std::abs (det.process (1, r)));
        env = d > env ? aAtt * env + (1 - aAtt) * d : aRel * env + (1 - aRel) * d;
        const float thr = -10.0f - amt * 0.4f;                 // amount 0..100 -> -10..-50 dB
        const float target = juce::jlimit (0.0f, 6.0f + amt * 0.12f, (gainToDb (env) - thr) * 0.7f);
        gr += 0.05f * (target - gr);
        const float g = dbToGain (-gr);
        l = ll + hl * g; r = lr + hr * g;
    }
    float getGr() const noexcept { return gr; }
private:
    double fs = 48000;
    juce::dsp::LinkwitzRileyFilter<float> split;
    Biquad det;
    float amt = 0, lastF = -1, env = 0, gr = 0, aAtt = 0, aRel = 0;
};

//==============================================================================
/** Small delay line with fractional read for the doubler / slap-back. */
struct Line
{
    std::vector<float> b; int mask = 0, pos = 0;
    void prepare (int n) { int s = 1; while (s < n + 4) s <<= 1; b.assign ((size_t) s, 0.0f); mask = s - 1; pos = 0; }
    void push (float x) noexcept { b[(size_t) pos] = x; pos = (pos + 1) & mask; }
    float read (float d) const noexcept
    {
        const int i = (int) d; const float f = d - (float) i;
        const float a = b[(size_t) ((pos - i) & mask)], c = b[(size_t) ((pos - i - 1) & mask)];
        return a + f * (c - a);
    }
};

} // namespace vf
