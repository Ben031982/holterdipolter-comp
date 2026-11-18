#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Laenge eines Kurvendurchlaufs in Vierteln
    constexpr double kBeatsPerCycle[] = { 0.25, 0.5, 1.0, 2.0, 4.0 };
}

juce::AudioProcessorValueTreeState::ParameterLayout HolterdipolterAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    StringArray curveNames;
    for (int i = 1; i <= CurveBank::numSlots; ++i)
        curveNames.add (String (i));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "curve", 1 }, "Kurve", curveNames, 0));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "mix", 1 }, "Mix",
                                                        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                        AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Trigger",
                                                        StringArray { "Sync", "MIDI", "Audio" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rate", 1 }, "Laenge",
                                                        StringArray { "1/16", "1/8", "1/4", "1/2", "1 Takt" }, 2));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "band", 1 }, "Band Split", false));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "xover", 1 }, "Split Frequenz",
                                                        NormalisableRange<float> (40.0f, 1000.0f, 1.0f, 0.4f), 150.0f,
                                                        AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "thresh", 1 }, "Threshold",
                                                        NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -24.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

HolterdipolterAudioProcessor::HolterdipolterAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pCurve  = apvts.getRawParameterValue ("curve");
    pMix    = apvts.getRawParameterValue ("mix");
    pMode   = apvts.getRawParameterValue ("mode");
    pRate   = apvts.getRawParameterValue ("rate");
    pBand   = apvts.getRawParameterValue ("band");
    pXover  = apvts.getRawParameterValue ("xover");
    pThresh = apvts.getRawParameterValue ("thresh");

    for (auto& v : scopeIn)   v.store (0.0f);
    for (auto& v : scopeOut)  v.store (0.0f);
}

bool HolterdipolterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto main = layouts.getMainInputChannelSet();

    if (main != layouts.getMainOutputChannelSet())
        return false;

    if (main != juce::AudioChannelSet::mono() && main != juce::AudioChannelSet::stereo())
        return false;

    const auto sc = layouts.getChannelSet (true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
}

void HolterdipolterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    crossover.prepare (spec);
    crossover.reset();

    gainCoeff      = 1.0f - std::exp (-1.0f / (0.0015f * (float) sampleRate));   // ~1.5 ms gegen Klicks
    envRelease     = std::exp (-1.0f / (0.020f * (float) sampleRate));
    holdoffSamples = (int) (0.040 * sampleRate);

    phase = 1.0;
    smoothedGain = 1.0f;
    env = 0.0f;
    armed = true;
    holdoff = 0;
    scopeBin = -1;
    scopeAccIn = scopeAccOut = 0.0f;
}

void HolterdipolterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBuf = getBusBuffer (buffer, true, 0);
    const int numCh      = juce::jmin (2, mainBuf.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    if (numCh == 0 || numSamples == 0)
        return;

    // Sidechain (falls vorhanden), sonst triggert das eigene Eingangssignal
    juce::AudioBuffer<float> scBuf;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
        scBuf = getBusBuffer (buffer, true, 1);
    const bool hasSidechain = scBuf.getNumChannels() > 0;
    const int  numDetCh     = hasSidechain ? juce::jmin (2, scBuf.getNumChannels()) : numCh;

    // Parameter
    const int   slot    = juce::jlimit (0, CurveBank::numSlots - 1, (int) pCurve->load());
    const float mix     = pMix->load() * 0.01f;
    const int   mode    = (int) pMode->load();
    const int   rateIdx = juce::jlimit (0, 4, (int) pRate->load());
    const bool  split   = pBand->load() > 0.5f;
    const float thresh  = juce::Decibels::decibelsToGain (pThresh->load());

    crossover.setCutoffFrequency (pXover->load());

    // Tempo / Position vom Host
    double bpm = 120.0, ppq = 0.0;
    bool   playing = false, havePpq = false;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())         bpm = *b;
            if (auto p = pos->getPpqPosition()) { ppq = *p; havePpq = true; }
            playing = pos->getIsPlaying();
        }

    const double beats    = kBeatsPerCycle[rateIdx];
    const double phaseInc = (bpm / 60.0) / beats / sr;

    if (mode == modeSync && playing && havePpq)
    {
        const double cycles = ppq / beats;
        phase = cycles - std::floor (cycles);
    }

    auto midiIt = midi.begin();
    bool triggeredThisBlock = false;

    for (int i = 0; i < numSamples; ++i)
    {
        // ---- Trigger ermitteln -------------------------------------------
        bool trig = false;

        while (midiIt != midi.end() && (*midiIt).samplePosition <= i)
        {
            if (mode == modeMidi && (*midiIt).getMessage().isNoteOn())
                trig = true;
            ++midiIt;
        }

        if (mode == modeAudio)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numDetCh; ++ch)
                peak = juce::jmax (peak, std::abs (hasSidechain ? scBuf.getSample (ch, i)
                                                                : mainBuf.getSample (ch, i)));

            env = juce::jmax (peak, env * envRelease);

            if (armed)
            {
                if (peak >= thresh)
                {
                    trig = true;
                    armed = false;
                    holdoff = holdoffSamples;
                }
            }
            else
            {
                if (holdoff > 0)
                    --holdoff;
                else if (env < thresh * 0.5f)
                    armed = true;
            }
        }

        // ---- Phase weiterschalten ----------------------------------------
        if (mode == modeSync)
        {
            phase += phaseInc;
            if (phase >= 1.0)
                phase -= std::floor (phase);
        }
        else
        {
            if (trig)
            {
                phase = 0.0;
                triggeredThisBlock = true;
            }
            else if (phase < 1.0)
            {
                phase = juce::jmin (1.0, phase + phaseInc);
            }
        }

        // ---- Gain ---------------------------------------------------------
        const float target = 1.0f - mix * (1.0f - curves.eval (slot, (float) phase));
        smoothedGain += gainCoeff * (target - smoothedGain);

        float inPeak = 0.0f, outPeak = 0.0f;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float in = mainBuf.getSample (ch, i);
            float out;

            if (split)
            {
                float low = 0.0f, high = 0.0f;
                crossover.processSample (ch, in, low, high);
                out = low * smoothedGain + high;
            }
            else
            {
                out = in * smoothedGain;
            }

            mainBuf.setSample (ch, i, out);
            inPeak  = juce::jmax (inPeak,  std::abs (in));
            outPeak = juce::jmax (outPeak, std::abs (out));
        }

        // ---- Wellenform fuer die Anzeige mitschreiben -----------------------
        if (mode == modeSync || phase < 1.0)
        {
            const int bin = juce::jmin (scopeSize - 1, (int) (phase * scopeSize));

            if (bin != scopeBin)
            {
                flushScopeBin();
                scopeBin = bin;
            }

            scopeAccIn  = juce::jmax (scopeAccIn,  inPeak);
            scopeAccOut = juce::jmax (scopeAccOut, outPeak);
        }
        else if (scopeBin >= 0)
        {
            flushScopeBin();      // Durchlauf zu Ende: letztes Stueck sichern, Bild bleibt stehen
            scopeBin = -1;
        }
    }

    crossover.snapToZero();

    uiPhase.store ((float) phase, std::memory_order_relaxed);
    if (triggeredThisBlock)
        uiTriggered.store (true, std::memory_order_relaxed);
}

void HolterdipolterAudioProcessor::flushScopeBin() noexcept
{
    if (scopeBin >= 0)
    {
        scopeIn [(size_t) scopeBin].store (scopeAccIn,  std::memory_order_relaxed);
        scopeOut[(size_t) scopeBin].store (scopeAccOut, std::memory_order_relaxed);
    }

    scopeAccIn = scopeAccOut = 0.0f;
}

void HolterdipolterAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("curveData", curves.toString(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void HolterdipolterAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);

            if (state.hasProperty ("curveData"))
                curves.fromString (state["curveData"].toString());

            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* HolterdipolterAudioProcessor::createEditor()
{
    return new HolterdipolterAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HolterdipolterAudioProcessor();
}
