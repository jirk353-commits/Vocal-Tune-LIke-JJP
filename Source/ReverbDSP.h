#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>

namespace gv
{
//==============================================================================
/** Character of each environment. User parameters (decay, size, ...) work on top. */
struct EnvType
{
    const char* name;
    float sizeScale;     // scales all FDN delay lengths
    float erSpreadMs;    // time span of the early reflection pattern
    float erLevel;       // early reflection level
    float modDepthMs;    // base chorus depth inside the tank
    float modRate;       // Hz
    bool  lofi;          // vintage band-limit + grit
    // defaults loaded when the environment is picked
    float defDecay, defPredelay, defSize, defDiffusion, defDamping;
};

inline const std::array<EnvType, 9>& envTypes()
{
    static const std::array<EnvType, 9> t {{
        //  name         size  erMs  erLvl  mod   rate  lofi   decay pre  size diff damp
        { "AMBIENCE",   0.35f,  22.f, 1.00f, 0.10f, 0.6f, false, 0.6f,  0.f, 50.f, 60.f, 45.f },
        { "ROOM",       0.50f,  38.f, 0.85f, 0.15f, 0.5f, false, 0.9f,  4.f, 50.f, 65.f, 40.f },
        { "STUDIO",     0.62f,  30.f, 0.65f, 0.12f, 0.4f, false, 1.2f,  8.f, 50.f, 62.f, 35.f },
        { "CHAMBER",    0.85f,  55.f, 0.60f, 0.20f, 0.5f, false, 1.8f, 12.f, 50.f, 72.f, 38.f },
        { "PLATE",      0.90f,   0.f, 0.00f, 0.25f, 0.9f, false, 2.2f, 10.f, 50.f, 85.f, 18.f },
        { "HALL",       1.40f,  80.f, 0.50f, 0.30f, 0.4f, false, 2.8f, 22.f, 50.f, 75.f, 40.f },
        { "CATHEDRAL",  2.40f, 120.f, 0.40f, 0.35f, 0.3f, false, 6.5f, 35.f, 55.f, 80.f, 55.f },
        { "ARENA",      2.00f, 160.f, 0.75f, 0.25f, 0.3f, false, 4.0f, 55.f, 50.f, 60.f, 45.f },
        { "VINTAGE",    1.00f,  50.f, 0.40f, 0.80f, 1.2f, true,  2.5f, 18.f, 50.f, 70.f, 50.f },
    }};
    return t;
}

//==============================================================================
/** Simple circular delay line with fractional (linear) read. */
struct Delay
{
    std::vector<float> buf;
    int mask = 0, pos = 0;

    void prepare (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        pos = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) noexcept { buf[(size_t) pos] = x; pos = (pos + 1) & mask; }
    float read (float d) const noexcept      // d >= 1 samples behind the last pushed sample
    {
        const int di = (int) d;
        const float fr = d - (float) di;
        const float a = buf[(size_t) ((pos - di) & mask)];
        const float b = buf[(size_t) ((pos - di - 1) & mask)];
        return a + fr * (b - a);
    }
    float readInt (int d) const noexcept { return buf[(size_t) ((pos - d) & mask)]; }
};

/** Schroeder allpass used for input diffusion. */
struct Allpass
{
    Delay d; int len = 1;
    void prepare (int maxSamples) { d.prepare (maxSamples); }
    float process (float x, float g) noexcept
    {
        const float delayed = d.readInt (len);
        const float v = x + g * delayed;
        d.push (v);
        return delayed - g * v;
    }
};

//==============================================================================
/** Stereo reverb: pre-delay -> input diffusion -> 8-line modulated FDN, plus early reflections. */
class Reverb
{
public:
    static constexpr int N = 8;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        const int maxLine = (int) (0.40f * fs);       // up to ~400 ms per line
        for (auto& l : lines) l.prepare (maxLine);
        preDelay.prepare ((int) (0.30f * fs));
        erL.prepare ((int) (0.40f * fs));
        erR.prepare ((int) (0.40f * fs));
        for (auto& a : diffL) a.prepare ((int) (0.05f * fs));
        for (auto& a : diffR) a.prepare ((int) (0.05f * fs));
        lastType = -1;
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.clear();
        preDelay.clear(); erL.clear(); erR.clear();
        for (auto& a : diffL) a.d.clear();
        for (auto& a : diffR) a.d.clear();
        std::fill (std::begin (damp), std::end (damp), 0.0f);
        erLpL = erLpR = 0.0f;
        for (int i = 0; i < N; ++i) { lfo[i] = (float) i / (float) N; curLen[i] = targetLen[i]; }
    }

    struct Params
    {
        int type = 5;
        float decay = 2.8f, predelayMs = 20.f, size = 50.f, diffusion = 70.f, damping = 40.f,
              mod = 50.f, early = 50.f;
    };

    void setParams (const Params& p)
    {
        const auto& t = envTypes()[(size_t) juce::jlimit (0, 8, p.type)];
        if (p.type != lastType)
        {
            lastType = p.type;
            reset();
        }
        env = &t;

        const float sizeMul = 0.5f + p.size * 0.01f;           // 0.5 .. 1.5
        const float scale = t.sizeScale * sizeMul;
        static const float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.1f, 59.3f, 67.9f, 73.1f };
        for (int i = 0; i < N; ++i)
        {
            targetLen[i] = juce::jlimit (8.0f, 0.38f * fs, baseMs[i] * scale * 0.001f * fs);
            gain[i] = std::pow (10.0f, -3.0f * (targetLen[i] / fs) / juce::jmax (0.05f, p.decay * 1.22f));
        }
        if (firstParams) { for (int i = 0; i < N; ++i) curLen[i] = targetLen[i]; firstParams = false; }

        // diffusion allpasses (lengths follow size a little)
        static const float apMs[4] = { 4.77f, 3.59f, 12.73f, 9.30f };
        const float apScale = 0.6f + 0.4f * juce::jmin (1.5f, t.sizeScale);
        for (int i = 0; i < 4; ++i)
        {
            diffL[(size_t) i].len = juce::jlimit (1, (int) (0.045f * fs), (int) (apMs[i] * apScale * 0.001f * fs));
            diffR[(size_t) i].len = juce::jlimit (1, (int) (0.045f * fs), (int) (apMs[i] * apScale * 1.13f * 0.001f * fs));
        }
        apGain = 0.30f + 0.48f * juce::jlimit (0.0f, 1.0f, p.diffusion * 0.01f);

        // damping: one-pole low-pass inside the loop, 20 kHz .. 1.5 kHz
        const float fc = 20000.0f * std::pow (1500.0f / 20000.0f, juce::jlimit (0.0f, 1.0f, p.damping * 0.01f));
        dampA = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (fc, 0.45f * fs) / fs);

        preSamples = juce::jlimit (0.0f, 0.29f * fs, p.predelayMs * 0.001f * fs);
        modDepth = t.modDepthMs * 0.001f * fs * (p.mod * 0.02f);
        modInc = t.modRate / fs;
        erGain = t.erLevel * (p.early * 0.02f);
        erScale = t.erSpreadMs * sizeMul * 0.001f * fs;
        lofi = t.lofi;
    }

    /** returns wet stereo (unity-ish level) */
    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        // pre-delay (mono sum feeds ER, stereo into the tank)
        preDelay.push (0.5f * (inL + inR));
        const float pre = preSamples < 1.0f ? 0.5f * (inL + inR) : preDelay.read (preSamples);
        const float side = 0.5f * (inL - inR);

        float xL = pre + side * 0.7f, xR = pre - side * 0.7f;

        // early reflections
        float eL = 0.0f, eR = 0.0f;
        if (erGain > 0.0001f && erScale > 1.0f)
        {
            erL.push (pre); erR.push (pre);
            static const float tL[10] = { 0.043f, 0.097f, 0.151f, 0.229f, 0.307f, 0.398f, 0.519f, 0.633f, 0.781f, 0.947f };
            static const float tR[10] = { 0.061f, 0.113f, 0.187f, 0.241f, 0.353f, 0.442f, 0.571f, 0.689f, 0.823f, 1.000f };
            static const float gE[10] = { 0.84f, -0.71f, 0.63f, -0.55f, 0.47f, -0.41f, 0.34f, -0.28f, 0.22f, -0.17f };
            for (int k = 0; k < 10; ++k)
            {
                eL += gE[k] * erL.read (1.0f + tL[k] * erScale);
                eR += gE[k] * erR.read (1.0f + tR[k] * erScale);
            }
            erLpL += 0.55f * (eL - erLpL);
            erLpR += 0.55f * (eR - erLpR);
            eL = erLpL * erGain * 0.6f;
            eR = erLpR * erGain * 0.6f;
            // feed some ER into the tank for a natural build-up
            xL += eL * 0.5f; xR += eR * 0.5f;
        }

        // input diffusion
        for (int k = 0; k < 4; ++k)
        {
            xL = diffL[(size_t) k].process (xL, apGain);
            xR = diffR[(size_t) k].process (xR, apGain);
        }

        // FDN
        float v[N];
        for (int i = 0; i < N; ++i)
        {
            curLen[i] += 0.0005f * (targetLen[i] - curLen[i]);
            lfo[i] += modInc * (1.0f + 0.13f * (float) i);
            if (lfo[i] >= 1.0f) lfo[i] -= 1.0f;
            const float m = modDepth * (0.5f + 0.5f * fastSin (lfo[i]));
            const float d = lines[i].read (juce::jmax (2.0f, curLen[i] - m));
            damp[i] += dampA * (d - damp[i]);
            v[i] = damp[i] * gain[i];
        }
        // late output taps
        const float lateL = v[0] - v[2] + v[4] - v[6] + 0.5f * (v[1] + v[5]);
        const float lateR = v[1] - v[3] + v[5] - v[7] + 0.5f * (v[2] + v[6]);

        hadamard (v);
        static const float sgn[N] = { 1, -1, 1, -1, -1, 1, -1, 1 };
        for (int i = 0; i < N; ++i)
            lines[i].push (v[i] + sgn[i] * (i < 4 ? xL : xR) * 0.5f);

        outL = lateL * 0.72f + eL * 1.4f;
        outR = lateR * 0.72f + eR * 1.4f;

        if (lofi)
        {
            lofiL += 0.35f * (outL - lofiL);
            lofiR += 0.35f * (outR - lofiR);
            outL = std::tanh (lofiL * 1.4f) / 1.0f;
            outR = std::tanh (lofiR * 1.4f) / 1.0f;
        }
    }

private:
    static inline float fastSin (float phase01) noexcept
    {
        // parabolic sine approximation, phase 0..1
        const float x = phase01 * 2.0f - 1.0f;
        return 4.0f * x * (1.0f - std::abs (x));
    }

    static inline void hadamard (float* v) noexcept
    {
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h * 2)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = v[j], b = v[j + h];
                    v[j] = a + b; v[j + h] = a - b;
                }
        constexpr float norm = 0.35355339f; // 1/sqrt(8)
        for (int i = 0; i < N; ++i) v[i] *= norm;
    }

    float fs = 48000.0f;
    std::array<Delay, N> lines;
    Delay preDelay, erL, erR;
    std::array<Allpass, 4> diffL, diffR;
    const EnvType* env = nullptr;

    float targetLen[N] {}, curLen[N] {}, gain[N] {}, damp[N] {}, lfo[N] {};
    float dampA = 0.5f, apGain = 0.6f, preSamples = 0.0f, modDepth = 0.0f, modInc = 0.0f;
    float erGain = 0.5f, erScale = 0.0f, erLpL = 0.0f, erLpR = 0.0f, lofiL = 0.0f, lofiR = 0.0f;
    bool lofi = false, firstParams = true;
    int lastType = -1;
};

//==============================================================================
namespace shapers
{
    inline float warm (float x) noexcept   { return std::tanh (x); }
    inline float tube (float x) noexcept
    {
        constexpr float b = 0.25f;
        static const float tb = std::tanh (b), norm = 1.0f / (1.0f - tb * tb);
        return (std::tanh (x + b) - tb) * norm;
    }
    inline float tape (float x) noexcept  { const float a = std::abs (x); return x / std::cbrt (1.0f + a * a * a); }
}

} // namespace gv
