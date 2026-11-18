#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Curves.h"

class HolterdipolterAudioProcessor : public juce::AudioProcessor
{
public:
    enum Mode { modeSync = 0, modeMidi, modeAudio };

    HolterdipolterAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    const juce::String getName() const override            { return "Holterdipolter Comp"; }
    bool acceptsMidi() const override                      { return true; }
    bool producesMidi() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 0.0; }

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    CurveBank curves;            // die 16 editierbaren Kurven

    // Fuer die Anzeige im Editor
    std::atomic<float> uiPhase { 1.0f };
    std::atomic<bool>  uiTriggered { false };

    // Wellenform ueber einen Kurvendurchlauf (Spitzenwert je Abschnitt der Phase):
    // scopeIn = Original vor dem Ducking, scopeOut = Ergebnis danach
    static constexpr int scopeSize = 512;
    std::array<std::atomic<float>, scopeSize> scopeIn, scopeOut;

private:
    std::atomic<float>* pCurve  = nullptr;
    std::atomic<float>* pMix    = nullptr;
    std::atomic<float>* pMode   = nullptr;
    std::atomic<float>* pRate   = nullptr;
    std::atomic<float>* pBand   = nullptr;
    std::atomic<float>* pXover  = nullptr;
    std::atomic<float>* pThresh = nullptr;

    juce::dsp::LinkwitzRileyFilter<float> crossover;

    double sr = 44100.0;
    double phase = 1.0;          // 0..1, 1 = Kurve durchgelaufen
    float  smoothedGain = 1.0f;
    float  gainCoeff = 0.0f;

    // Wellenform-Aufzeichnung
    int   scopeBin = -1;
    float scopeAccIn = 0.0f, scopeAccOut = 0.0f;
    void  flushScopeBin() noexcept;

    // Audio-Trigger
    float env = 0.0f, envRelease = 0.0f;
    bool  armed = true;
    int   holdoff = 0, holdoffSamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HolterdipolterAudioProcessor)
};
