#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };
    const std::vector<Preset>& presets()
    {
        // voice: 0 MALE 1 MALE 2 2 FEMALE   comp_colour: 0 FLAT 1 WARM 2 TUBE
        static const std::vector<Preset> list =
        {
            { "Init",               {} },
            { "Pop Lead",           { { "comp", 45 }, { "deess", 40 }, { "high", 2.5f }, { "low", -1.5f }, { "magic", 5 }, { "space", 3.5f }, { "presence", 4.5f }, { "attack", 2 } } },
            { "Rap Power",          { { "comp", 60 }, { "comp_colour", 2 }, { "deess", 35 }, { "mid", 1.5f }, { "attitude", 6 }, { "attack", 4 }, { "presence", 4 }, { "magic", 2 }, { "space", 1.5f } } },
            { "Bhojpuri Bright",    { { "comp", 50 }, { "deess", 50 }, { "high", 4 }, { "mid", 2 }, { "low", -2 }, { "presence", 6 }, { "magic", 4 }, { "space", 4 }, { "attitude", 3 } } },
            { "Devotional Warm",    { { "comp", 35 }, { "comp_colour", 1 }, { "deess", 30 }, { "low", 2 }, { "high", 1 }, { "magic", 4 }, { "space", 6 }, { "presence", 2 } } },
            { "Podcast / Voice-Over",{ { "comp", 55 }, { "comp_colour", 0 }, { "deess", 45 }, { "low", 1 }, { "mid", -1 }, { "presence", 3 }, { "attack", 1.5f } } },
            { "Female Air",         { { "voice", 2 }, { "comp", 40 }, { "deess", 45 }, { "high", 4.5f }, { "low", -2 }, { "magic", 5 }, { "presence", 4 }, { "space", 4 } } },
            { "Slapback Retro",     { { "comp", 50 }, { "comp_colour", 2 }, { "mid", 2 }, { "high", -1 }, { "space", 7 }, { "attitude", 4 }, { "magic", 2 } } },
            { "Intimate Close",     { { "comp", 30 }, { "comp_colour", 1 }, { "deess", 25 }, { "low", 2.5f }, { "high", 1.5f }, { "magic", 3 }, { "space", 1 } } },
            { "Big Chorus Vocal",   { { "comp", 55 }, { "deess", 40 }, { "high", 3 }, { "magic", 7 }, { "space", 6 }, { "presence", 5 }, { "attitude", 3 }, { "attack", 2 } } },
            { "Deep Male",          { { "voice", 1 }, { "comp", 45 }, { "comp_colour", 1 }, { "low", 3 }, { "mid", -1.5f }, { "presence", 3 }, { "magic", 3 }, { "space", 3 } } },
        };
        return list;
    }
}

AudioProcessorValueTreeState::ParameterLayout VocalFeelProcessor::createLayout()
{
    using F = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> v;
    auto db = A().withLabel ("dB").withStringFromValueFunction ([] (float x, int) { return (x > 0.05f ? "+" : "") + String (x, 1) + " dB"; });
    auto pct = A().withLabel ("%").withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x)) + "%"; });
    auto hz = A().withLabel ("Hz").withStringFromValueFunction ([] (float x, int) { return x >= 1000 ? String (x / 1000.0f, 1) + " kHz" : String (roundToInt (x)) + " Hz"; });
    auto fader = A().withStringFromValueFunction ([] (float x, int) { return String (x, 1); });
    auto c = [&] (const char* id, const char* n, const StringArray& s, int d) { v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, n, s, d)); };
    auto f = [&] (const char* id, const char* n, NormalisableRange<float> r, float d, A a) { v.push_back (std::make_unique<F> (ParameterID { id, 1 }, n, r, d, a)); };
    NormalisableRange<float> dsRange (3000, 12000); dsRange.setSkewForCentre (6500);

    c ("voice", "Voice", { "MALE", "MALE 2", "FEMALE" }, 0);
    f ("sens", "Sens", { -12, 12, 0.1f }, 0, db);
    f ("low", "Low", { -12, 12, 0.1f }, 0, db);
    f ("mid", "Mid", { -12, 12, 0.1f }, 0, db);
    f ("high", "High", { -12, 12, 0.1f }, 0, db);
    f ("deess", "De-Esser", { 0, 100 }, 30, pct);
    f ("deess_freq", "De-Ess Freq", dsRange, 6500, hz);
    f ("comp", "Comp", { 0, 100 }, 35, pct);
    c ("comp_colour", "Comp Colour", { "FLAT", "WARM", "TUBE" }, 1);
    f ("main", "Main", { 0, 10, 0.01f }, 7, fader);
    f ("magic", "Magic", { 0, 10, 0.01f }, 0, fader);
    f ("space", "Space", { 0, 10, 0.01f }, 0, fader);
    f ("attack", "Attack", { 0, 10, 0.01f }, 0, fader);
    f ("attitude", "Attitude", { 0, 10, 0.01f }, 0, fader);
    f ("presence", "Presence", { 0, 10, 0.01f }, 0, fader);
    f ("master", "Master", { 0, 10, 0.01f }, 7, fader);
    ignoreUnused (pct);
    return { v.begin(), v.end() };
}

VocalFeelProcessor::VocalFeelProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
            cache[r->getParameterID().toStdString()] = apvts.getRawParameterValue (r->getParameterID());
}

bool VocalFeelProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    const auto i = l.getMainInputChannelSet();
    return (o == AudioChannelSet::mono() || o == AudioChannelSet::stereo()) && (i == o || (i == AudioChannelSet::mono() && o == AudioChannelSet::stereo()));
}

void VocalFeelProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    maxBlock = jmax (1, samplesPerBlock);
    setLatencySamples (0);

    for (auto* b : { &lowEq, &midEq, &highEq, &magicHp, &magicPk, &slapHp, &slapLp, &attBp, &attiHp, &attiLp, &presPk, &presAir }) b->reset();
    deess.prepare (fs);
    opto.prepare (fs);
    magicComp.prepare (fs, 4.0f, 90.0f, -34.0f, 8.0f);
    attiComp.prepare (fs, 1.5f, 60.0f, -30.0f, 12.0f);
    presComp.prepare (fs, 3.0f, 110.0f, -36.0f, 5.0f);
    dblL.prepare ((int) (0.04 * fs)); dblR.prepare ((int) (0.04 * fs));
    slap.prepare ((int) (0.2 * fs)); slapFb = 0;
    plate.prepare (fs);
    gv::Reverb::Params rp; rp.type = 4; rp.decay = 1.3f; rp.predelayMs = 25; rp.size = 45; rp.diffusion = 80; rp.damping = 40; rp.mod = 50; rp.early = 0;
    plate.setParams (rp);
    envFast = envSlow = 0;
    aFastA = vf::timeCoef (0.3f, fs); aFastR = vf::timeCoef (25.0f, fs);
    aSlowA = vf::timeCoef (40.0f, fs); aSlowR = vf::timeCoef (160.0f, fs);
    lfo1 = 0; lfo2 = 0.37f;

    voice = -1; updateVoice ((int) p ("voice"));
    lastTone[0] = lastTone[1] = lastTone[2] = 99; updateToneEq();

    sensG.reset (fs, 0.05); mainG.reset (fs, 0.05); masterG.reset (fs, 0.05);
    sensG.setCurrentAndTargetValue (vf::dbToGain (p ("sens")));
    mainG.setCurrentAndTargetValue (faderGain (p ("main")));
    masterG.setCurrentAndTargetValue (faderGain (p ("master")));
    static const char* ids[5] = { "magic", "space", "attack", "attitude", "presence" };
    for (int k = 0; k < 5; ++k) { chainSm[k].reset (fs, 0.05); chainSm[k].setCurrentAndTargetValue (chainAmount (p (ids[k]))); }
}

void VocalFeelProcessor::updateVoice (int v)
{
    v = jlimit (0, 2, v);
    if (v == voice) return;
    voice = v;
    const auto& vc = vf::voices()[(size_t) v];
    const float ny = (float) fs * 0.45f;
    magicHp.set (vf::AC::makeHighPass (fs, 140.0f, 0.7071f));
    magicPk.set (vf::AC::makePeakFilter (fs, vc.magicF, 0.7f, vf::dbToGain (6.0f)));
    slapHp.set (vf::AC::makeHighPass (fs, 300.0f, 0.7071f));
    slapLp.set (vf::AC::makeLowPass (fs, 4000.0f, 0.7071f));
    attBp.set (vf::AC::makeBandPass (fs, 2500.0f, 0.55f));
    attiHp.set (vf::AC::makeHighPass (fs, vc.attitudeLo, 0.7071f));
    attiLp.set (vf::AC::makeLowPass (fs, jmin (vc.attitudeHi, ny), 0.7071f));
    presPk.set (vf::AC::makeBandPass (fs, vc.presenceF, 0.8f));
    presAir.set (vf::AC::makeHighShelf (fs, jmin (9000.0f, ny), 0.7f, vf::dbToGain (4.0f)));
    lastTone[0] = 99;   // force tone EQ retune to the new centres
}

void VocalFeelProcessor::updateToneEq()
{
    const float lo = p ("low"), mi = p ("mid"), hi = p ("high");
    if (lo == lastTone[0] && mi == lastTone[1] && hi == lastTone[2]) return;
    lastTone[0] = lo; lastTone[1] = mi; lastTone[2] = hi;
    const auto& vc = vf::voices()[(size_t) jlimit (0, 2, voice)];
    lowEq.set (vf::AC::makeLowShelf (fs, vc.lowF, 0.7f, vf::dbToGain (lo)));
    midEq.set (vf::AC::makePeakFilter (fs, vc.midF, 0.9f, vf::dbToGain (mi)));
    highEq.set (vf::AC::makeHighShelf (fs, jmin (vc.highF, (float) fs * 0.45f), 0.7f, vf::dbToGain (hi)));
}

void VocalFeelProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals nd;
    const int total = buffer.getNumSamples();
    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, total);
    for (int start = 0; start < total; start += maxBlock)
    {
        AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, jmin (maxBlock, total - start));
        processChunk (chunk);
    }
}

void VocalFeelProcessor::processChunk (AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int numCh = jmin (2, getTotalNumOutputChannels(), buffer.getNumChannels());
    if (n == 0 || numCh == 0) return;

    updateVoice ((int) p ("voice"));
    updateToneEq();
    const auto& vc = vf::voices()[(size_t) voice];
    deess.setParams (p ("deess"), p ("deess_freq"));
    opto.setParams (p ("comp"), (int) p ("comp_colour"));
    sensG.setTargetValue (vf::dbToGain (p ("sens")));
    mainG.setTargetValue (faderGain (p ("main")));
    masterG.setTargetValue (faderGain (p ("master")));
    static const char* ids[5] = { "magic", "space", "attack", "attitude", "presence" };
    for (int k = 0; k < 5; ++k) chainSm[k].setTargetValue (chainAmount (p (ids[k])));

    // chain make-up gains (calibrated so a full fader is roughly as loud as the dry vocal)
    constexpr float kMagic = 0.55f, kSpace = 1.6f, kAttack = 0.85f, kAtti = 0.40f, kPres = 0.75f;
    const float slapD = vc.slapMs * 0.001f * (float) fs;
    const float d1 = 0.011f * (float) fs, d2 = 0.0165f * (float) fs, depth = 0.0005f * (float) fs;
    const float lfoInc = 0.23f / (float) fs, lfoInc2 = 0.31f / (float) fs;

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : buffer.getWritePointer (0);
    float inPk = 0, outPk = 0, cgr = 0, dgr = 0;
    double chainSq[5] {};

    for (int i = 0; i < n; ++i)
    {
        const float sg = sensG.getNextValue();
        float l = L[i] * sg, r = (numCh > 1 ? R[i] : L[i]) * sg;
        inPk = jmax (inPk, jmax (std::abs (l), std::abs (r)));
        scopeIn.push (0.5f * (l + r));

        // ---- tone EQ + dynamic de-ess band ----------------------------------
        l = highEq.process (0, midEq.process (0, lowEq.process (0, l)));
        r = highEq.process (1, midEq.process (1, lowEq.process (1, r)));
        deess.process (l, r);
        dgr = jmax (dgr, deess.getGr());
        const float sl = l, sr = r, sm = 0.5f * (l + r);   // source for the parallel chains

        // ---- main: opto compressor with colour -----------------------------
        opto.process (l, r);
        cgr = jmax (cgr, opto.getGr());
        const float mg = mainG.getNextValue();
        float outL = l * mg, outR = r * mg;

        float a[5]; for (int k = 0; k < 5; ++k) a[k] = chainSm[k].getNextValue();

        // ---- MAGIC: bright compressed doubler ------------------------------
        {
            float ml = magicPk.process (0, magicHp.process (0, sl));
            float mr = magicPk.process (1, magicHp.process (1, sr));
            const float g = magicComp.gain (0.5f * (ml + mr)) * vf::dbToGain (12.0f);
            ml *= g; mr *= g;
            const float mono = 0.5f * (ml + mr);
            dblL.push (mono); dblR.push (mono);
            lfo1 += lfoInc; if (lfo1 >= 1) lfo1 -= 1;
            lfo2 += lfoInc2; if (lfo2 >= 1) lfo2 -= 1;
            const float wL = dblL.read (d1 + depth * std::sin (MathConstants<float>::twoPi * lfo1));
            const float wR = dblR.read (d2 + depth * std::sin (MathConstants<float>::twoPi * lfo2));
            const float yl = (0.55f * ml + 0.6f * wL) * kMagic * a[0], yr = (0.55f * mr + 0.6f * wR) * kMagic * a[0];
            outL += yl; outR += yr; chainSq[0] += (double) yl * yl;
        }
        // ---- SPACE: slap-back + short plate ---------------------------------
        if (a[1] > 1.0e-6f)
        {
            slap.push (slapLp.process (0, slapHp.process (0, sm + slapFb * 0.22f)));
            const float s1 = slap.read (slapD), s2 = slap.read (slapD * 1.09f);
            slapFb = s1;
            float rl, rr; plate.process (sl, sr, rl, rr);
            const float yl = (0.75f * s1 + 0.55f * rl) * kSpace * a[1], yr = (0.75f * s2 + 0.55f * rr) * kSpace * a[1];
            outL += yl; outR += yr; chainSq[1] += (double) yl * yl;
        }
        else { slap.push (0); float rl, rr; plate.process (0, 0, rl, rr); slapFb = 0; }
        // ---- ATTACK: consonant / transient emphasis -------------------------
        {
            const float bl = attBp.process (0, sl), br = attBp.process (1, sr);
            const float d = std::max (std::abs (bl), std::abs (br));
            envFast = d > envFast ? aFastA * envFast + (1 - aFastA) * d : aFastR * envFast + (1 - aFastR) * d;
            envSlow = d > envSlow ? aSlowA * envSlow + (1 - aSlowA) * d : aSlowR * envSlow + (1 - aSlowR) * d;
            const float t = jlimit (0.0f, 3.0f, envFast / (envSlow + 1.0e-5f) - 1.0f);
            const float yl = bl * t * kAttack * a[2], yr = br * t * kAttack * a[2];
            outL += yl; outR += yr; chainSq[2] += (double) yl * yl;
        }
        // ---- ATTITUDE: driven, squashed mid band -----------------------------
        {
            float tl = attiLp.process (0, attiHp.process (0, sl)), tr = attiLp.process (1, attiHp.process (1, sr));
            tl = std::tanh (tl * 4.0f); tr = std::tanh (tr * 4.0f);
            const float g = attiComp.gain (0.5f * (tl + tr)) * vf::dbToGain (10.0f);
            const float yl = tl * g * kAtti * a[3], yr = tr * g * kAtti * a[3];
            outL += yl; outR += yr; chainSq[3] += (double) yl * yl;
        }
        // ---- PRESENCE: compressed articulation band + air -------------------
        {
            float pl = presPk.process (0, sl), pr = presPk.process (1, sr);
            const float g = presComp.gain (0.5f * (pl + pr)) * vf::dbToGain (14.0f);
            pl = presAir.process (0, std::tanh (pl * g * 1.5f) / 1.5f);
            pr = presAir.process (1, std::tanh (pr * g * 1.5f) / 1.5f);
            const float yl = pl * kPres * a[4], yr = pr * kPres * a[4];
            outL += yl; outR += yr; chainSq[4] += (double) yl * yl;
        }

        // ---- master ----------------------------------------------------------
        const float og = masterG.getNextValue();
        outL *= og; outR *= og;
        outPk = jmax (outPk, jmax (std::abs (outL), std::abs (outR)));
        scopeOut.push (0.5f * (outL + outR));
        L[i] = outL; if (numCh > 1) R[i] = outR;
    }

    inLevel.store (Decibels::gainToDecibels (inPk, -100.0f));
    outLevel.store (Decibels::gainToDecibels (outPk, -100.0f));
    compGr.store (cgr);
    deessGr.store (dgr);
    for (int k = 0; k < 5; ++k) chainLevel[k].store (Decibels::gainToDecibels ((float) std::sqrt (chainSq[k] / n) * 1.414f, -100.0f));
}

//==============================================================================
int VocalFeelProcessor::getNumPrograms() { return (int) presets().size(); }
const String VocalFeelProcessor::getProgramName (int i) { return isPositiveAndBelow (i, getNumPrograms()) ? String (presets()[(size_t) i].name) : String(); }

void VocalFeelProcessor::setCurrentProgram (int index)
{
    if (! isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    auto set = [this] (const String& id, float v, bool normalised)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (normalised ? v : prm->convertTo0to1 (v));
            prm->endChangeGesture();
        }
    };
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<RangedAudioParameter*> (prm))
            if (r->getParameterID() != "sens" && r->getParameterID() != "master")
                set (r->getParameterID(), r->getDefaultValue(), true);
    for (auto& [id, val] : presets()[(size_t) index].values)
        set (id, val, false);
}

void VocalFeelProcessor::getStateInformation (MemoryBlock& dest)
{
    auto s = apvts.copyState();
    s.setProperty ("program", currentProgram, nullptr);
    if (auto xml = s.createXml()) copyXmlToBinary (*xml, dest);
}

void VocalFeelProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto t = ValueTree::fromXml (*xml);
            currentProgram = (int) t.getProperty ("program", 0);
            apvts.replaceState (t);
        }
}

AudioProcessorEditor* VocalFeelProcessor::createEditor() { return new VocalFeelEditor (*this); }

#if ! VF_TEST_BUILD
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalFeelProcessor(); }
#endif
