#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <unordered_map>
#include "DSP.h"

/** Lock-free mono ring used to feed the spectrum display (visual only). */
struct ScopeRing
{
    static constexpr int size = 8192;
    std::array<float, size> data {};
    std::atomic<int> write { 0 };
    void push (float x) noexcept { const int w = write.load (std::memory_order_relaxed); data[(size_t) w] = x; write.store ((w + 1) & (size - 1), std::memory_order_release); }
    void copyLatest (float* dest, int n) const noexcept
    {
        const int w = write.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i) dest[i] = data[(size_t) ((w - n + i) & (size - 1))];
    }
};

class VocalFeelProcessor : public juce::AudioProcessor
{
public:
    VocalFeelProcessor();
    ~VocalFeelProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Vocal Feel"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float value (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    double getFs() const { return fs; }

    /** fader 0..10 -> gain, 7 = unity, 0 = silent */
    static float faderGain (float v) noexcept { return v < 0.05f ? 0.0f : vf::dbToGain ((v - 7.0f) * 3.0f); }
    /** fader 0..10 -> parallel chain amount (smooth taper) */
    static float chainAmount (float v) noexcept { const float t = juce::jlimit (0.0f, 1.0f, v * 0.1f); return t * std::sqrt (t); }

    std::atomic<float> inLevel { -100 }, outLevel { -100 }, compGr { 0 }, deessGr { 0 };
    std::atomic<float> chainLevel[5] {};   // magic, space, attack, attitude, presence (dB, for the display)
    ScopeRing scopeIn, scopeOut;

private:
    void processChunk (juce::AudioBuffer<float>&);
    void updateVoice (int v);
    void updateToneEq();
    float p (const char* id) const { return cache.at (id)->load(); }

    // main path
    vf::Biquad lowEq, midEq, highEq;
    vf::DeEssBand deess;
    vf::OptoComp opto;
    // magic
    vf::Biquad magicHp, magicPk; vf::ChainComp magicComp; vf::Line dblL, dblR; float lfo1 = 0, lfo2 = 0.37f;
    // space
    vf::Biquad slapHp, slapLp; vf::Line slap; float slapFb = 0; gv::Reverb plate;
    // attack
    vf::Biquad attBp; float envFast = 0, envSlow = 0, aFastA = 0, aFastR = 0, aSlowA = 0, aSlowR = 0;
    // attitude
    vf::Biquad attiHp, attiLp; vf::ChainComp attiComp;
    // presence
    vf::Biquad presPk, presAir; vf::ChainComp presComp;

    juce::SmoothedValue<float> sensG, mainG, masterG, chainSm[5];
    float lastTone[3] { 99, 99, 99 };
    double fs = 48000;
    int maxBlock = 512, voice = -1, currentProgram = 0;
    std::unordered_map<std::string, std::atomic<float>*> cache;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalFeelProcessor)
};
