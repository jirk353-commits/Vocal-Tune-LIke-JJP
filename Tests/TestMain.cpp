// Offline test runner for Vocal Feel: DSP checks + UI screenshot.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

using namespace juce;

static int failures = 0;
static void check (bool ok, const String& what) { std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what.toRawUTF8()); if (! ok) ++failures; }
static void setP (VocalFeelProcessor& p, const String& id, float v) { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }
static void neutral (VocalFeelProcessor& p) { setP (p, "comp", 0); setP (p, "deess", 0); }
static std::vector<float> run (VocalFeelProcessor& p, std::function<float (int)> in, int total, int block = 512, std::vector<float>* right = nullptr)
{
    std::vector<float> out; out.reserve ((size_t) total);
    AudioBuffer<float> buf (2, block); MidiBuffer m;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = jmin (block, total - pos);
        AudioBuffer<float> b (buf.getArrayOfWritePointers(), 2, 0, n);
        for (int i = 0; i < n; ++i) { const float x = in (pos + i); b.setSample (0, i, x); b.setSample (1, i, x); }
        p.processBlock (b, m);
        for (int i = 0; i < n; ++i) { out.push_back (b.getSample (0, i)); if (right) right->push_back (b.getSample (1, i)); }
    }
    return out;
}
static float rms (const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i]; return (float) std::sqrt (s / (double) (b - a)); }
static float sineDb (VocalFeelProcessor& p, float f, float amp)
{
    auto y = run (p, [&] (int i) { return amp * (float) std::sin (MathConstants<double>::twoPi * f * i / 48000.0); }, 48000);
    return Decibels::gainToDecibels (rms (y, 24000, y.size()) / (amp / std::sqrt (2.0f)));
}
static float harmonicDb (const std::vector<float>& x, double f)
{
    double re = 0, im = 0; const size_t n = x.size();
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (MathConstants<double>::twoPi * (double) i / (double) n);
        re += x[i] * w * std::cos (MathConstants<double>::twoPi * f * (double) i / 48000.0);
        im += x[i] * w * std::sin (MathConstants<double>::twoPi * f * (double) i / 48000.0);
    }
    return (float) Decibels::gainToDecibels (std::sqrt (re * re + im * im) / (double) n * 4.0, -200.0);
}
/** crude singing voice: 180 Hz harmonic stack with formants, 4 Hz syllables and noisy "s" bursts */
static float voiceSample (int i)
{
    static Random rnd (5);
    const double t = i / 48000.0;
    double v = 0;
    for (int h = 1; h <= 30; ++h)
    {
        const double f = 180.0 * h;
        const double formant = std::exp (-std::pow ((f - 700) / 400, 2)) + 0.6 * std::exp (-std::pow ((f - 2600) / 600, 2)) + 0.15;
        v += formant / h * std::sin (MathConstants<double>::twoPi * f * t + h);
    }
    const double syl = std::pow (std::max (0.0, std::sin (MathConstants<double>::pi * 4.0 * t)), 0.5);
    const bool sss = std::fmod (t, 1.0) > 0.85;
    const float noise = rnd.nextFloat() * 2 - 1;
    return (float) (0.12 * v * syl) + (sss ? 0.08f * noise : 0.0f);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const double fs = 48000;

    // ---- neutral path is clean ------------------------------------------
    {
        VocalFeelProcessor p; p.prepareToPlay (fs, 512); neutral (p);
        bool flat = true;
        for (float f : { 60.f, 1000.f, 12000.f }) { const float g = sineDb (p, f, 0.3f); std::printf ("  neutral @%5.0f Hz: %+.3f dB\n", f, g); flat &= std::abs (g) < 0.1f; }
        check (flat, "comp/de-ess/chains off = flat, unity at MAIN 7 / MASTER 7");
        setP (p, "main", 0);
        const float m = sineDb (p, 1000, 0.3f);
        check (m < -90.0f, "MAIN at 0 mutes the dry vocal");
    }

    // ---- tone EQ: LOW / MID / HIGH, centres follow the voice ---------------
    {
        VocalFeelProcessor p; p.prepareToPlay (fs, 512); neutral (p);
        setP (p, "low", 6); setP (p, "mid", 6); setP (p, "high", 6);
        const float l = sineDb (p, 60, 0.1f), m = sineDb (p, 1000, 0.1f), h = sineDb (p, 15000, 0.1f);
        std::printf ("  MALE +6/+6/+6: 60 Hz %+.1f  1 kHz %+.1f  15 kHz %+.1f dB\n", l, m, h);
        check (l > 4.5f && m > 5.0f && h > 4.5f, "LOW / MID / HIGH boost");
        setP (p, "low", 0); setP (p, "high", 0);
        float mids[3];
        for (int v = 0; v < 3; ++v) { setP (p, "voice", (float) v); mids[v] = sineDb (p, 1400, 0.1f); }
        std::printf ("  MID +6 measured at 1.4 kHz: MALE %+.1f  MALE2 %+.1f  FEMALE %+.1f dB\n", mids[0], mids[1], mids[2]);
        check (mids[2] > mids[0] && mids[0] > mids[1], "voice mode moves the EQ centres");
    }

    // ---- dynamic de-ess band ------------------------------------------------
    {
        VocalFeelProcessor p; p.prepareToPlay (fs, 512); neutral (p);
        setP (p, "deess", 80); setP (p, "deess_freq", 6500);
        const float s = sineDb (p, 8000, 0.25f), lowLoud = sineDb (p, 1000, 0.25f), sQuiet = sineDb (p, 8000, 0.002f);
        std::printf ("  de-ess 80%%: 8 kHz loud %+.1f dB, 8 kHz very quiet %+.1f dB, 1 kHz loud %+.1f dB (GR %.1f)\n", s, sQuiet, lowLoud, p.deessGr.load());
        check (s < -3.0f && std::abs (lowLoud) < 0.6f && sQuiet > -1.0f, "de-esser only reduces loud sibilance above FREQ");
        setP (p, "deess_freq", 11000);
        const float s2 = sineDb (p, 8000, 0.25f);
        check (s2 > s + 2.0f, "FREQ moves the band");
    }

    // ---- opto compressor + colours -------------------------------------------
    for (int c = 0; c < 3; ++c)
    {
        VocalFeelProcessor p; p.prepareToPlay (fs, 512); neutral (p);
        setP (p, "comp", 70); setP (p, "comp_colour", (float) c);
        auto y = run (p, [] (int i) { return 0.4f * (float) std::sin (MathConstants<double>::twoPi * 500.0 * i / 48000.0); }, 96000);
        std::vector<float> tail (y.end() - 16384, y.end());
        const float h1 = harmonicDb (tail, 500), h2 = harmonicDb (tail, 1000), h3 = harmonicDb (tail, 1500);
        static const char* names[] = { "FLAT", "WARM", "TUBE" };
        std::printf ("  opto %-4s GR %4.1f dB  level %+5.1f dB  H2 %6.1f dBc  H3 %6.1f dBc\n", names[c], p.compGr.load(), h1 - Decibels::gainToDecibels (0.4f), h2 - h1, h3 - h1);
        const bool colourOk = c == 0 ? jmax (h2, h3) - h1 < -60.0f : jmax (h2, h3) - h1 > -50.0f;
        check (p.compGr.load() > 3.0f && colourOk, String ("opto comp ") + names[c] + (c == 0 ? " is clean" : " adds harmonics"));
    }

    // ---- parallel chains -------------------------------------------------------
    {
        const char* ids[] = { "magic", "space", "attack", "attitude", "presence" };
        float base;
        {
            VocalFeelProcessor p; p.prepareToPlay (fs, 512); neutral (p);
            auto y = run (p, voiceSample, 96000);
            base = rms (y, 24000, y.size());
        }
        for (int k = 0; k < 5; ++k)
        {
            VocalFeelProcessor p; p.prepareToPlay (fs, 512); neutral (p);
            setP (p, ids[k], 10);
            std::vector<float> rr;
            auto y = run (p, voiceSample, 96000, 512, &rr);
            float pk = 0; bool fin = true; double diff = 0;
            for (size_t i = 0; i < y.size(); ++i) { fin &= std::isfinite (y[i]) && std::isfinite (rr[i]); pk = jmax (pk, std::abs (y[i])); diff += std::abs (y[i] - rr[i]); }
            const float gain = Decibels::gainToDecibels (rms (y, 24000, y.size()) / base);
            setP (p, ids[k], 5);
            auto y5 = run (p, voiceSample, 96000);
            const float gain5 = Decibels::gainToDecibels (rms (y5, 24000, y5.size()) / base);
            std::printf ("  %-9s at 10: %+5.1f dB  at 5: %+5.1f dB  peak %.2f  stereo diff %.3f\n", ids[k], gain, gain5, pk, diff / (double) y.size());
            check (fin && gain > 0.8f && gain < 9.0f && gain5 < gain && pk < 2.0f, String (ids[k]).toUpperCase() + " chain adds level, stays stable");
        }
        // space has a tail
        VocalFeelProcessor p; p.prepareToPlay (fs, 512); neutral (p);
        setP (p, "space", 8);
        auto y = run (p, [] (int i) { return i < 24000 ? voiceSample (i) : 0.0f; }, 48000);
        const float tail = rms (y, 24000 + 2400, 24000 + 9600);
        std::printf ("  space tail after the vocal stops: %.1f dBFS\n", Decibels::gainToDecibels (tail));
        check (tail > 0.002f, "SPACE gives slap + plate tail");
    }

    // ---- presets + state ---------------------------------------------------------
    {
        VocalFeelProcessor p; p.prepareToPlay (fs, 333);
        bool ok = true;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            auto y = run (p, voiceSample, 48000, 333);
            float pk = 0; bool fin = true; for (auto v : y) { fin &= std::isfinite (v); pk = jmax (pk, std::abs (v)); }
            std::printf ("  preset %-22s comp GR %4.1f  de-ess GR %4.1f  peak %.2f\n", p.getProgramName (i).toRawUTF8(), p.compGr.load(), p.deessGr.load(), pk);
            ok &= fin && pk < 2.0f;
        }
        check (ok, "all presets stable");
        VocalFeelProcessor a, b; a.setCurrentProgram (3);
        MemoryBlock mb; a.getStateInformation (mb); b.setStateInformation (mb.getData(), (int) mb.getSize());
        check (b.getCurrentProgram() == 3 && std::abs (b.value ("presence") - 6.0f) < 0.01f, "state save/restore");
        VocalFeelProcessor m; m.prepareToPlay (44100, 64); m.setCurrentProgram (9);
        auto y = run (m, voiceSample, 44100, 64);
        bool fin = true; for (auto v : y) fin &= std::isfinite (v);
        check (fin, "44.1 kHz / 64-sample blocks");
    }

    if (argc > 1)
    {
        VocalFeelProcessor p; p.prepareToPlay (fs, 512);
        p.setCurrentProgram (1);
        setP (p, "low", -2); setP (p, "mid", 2);
        int pos = 0;
        run (p, [&] (int) { return voiceSample (pos++); }, 48000);
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (vgui::MainPanel::W, vgui::MainPanel::H);
        for (int k = 0; k < 20; ++k) { run (p, [&] (int) { return voiceSample (pos++); }, 1600); MessageManager::getInstance()->runDispatchLoopUntil (35); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, argc > 2 ? (float) std::atof (argv[2]) : 1.0f);
        File out (argv[1]); out.deleteFile(); FileOutputStream os (out);
        check (PNGImageFormat().writeImageToStream (img, os), "wrote screenshot");
    }
    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
