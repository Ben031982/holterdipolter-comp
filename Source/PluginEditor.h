#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

//==============================================================================
class HdpLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HdpLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

//==============================================================================
// Grosse, editierbare Kurvenanzeige
class CurveDisplay : public juce::Component
{
public:
    explicit CurveDisplay (HolterdipolterAudioProcessor& p) : proc (p) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> toScreen (float x, float y) const;
    int currentSlot() const;
    int nodeAt (const std::vector<CurvePoint>&, juce::Point<float>) const;

    HolterdipolterAudioProcessor& proc;

    float scopeScale = 1.0f;     // automatische Hoehenanpassung der Wellenform
    std::vector<CurvePoint> editPts;
    int   dragNode = -1, dragSegment = -1, dragSlot = 0;
    float dragStartTension = 0.0f;
};

//==============================================================================
// Die 16 Kurven als anklickbare Miniaturen
class CurveThumbs : public juce::Component
{
public:
    explicit CurveThumbs (HolterdipolterAudioProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void refreshIfNeeded();

private:
    juce::Rectangle<float> cellBounds (int index) const;
    int cellAt (juce::Point<float>) const;

    HolterdipolterAudioProcessor& proc;
    juce::ParameterAttachment attachment;
    int selected = 0, hovered = -1, seenVersion = -1;
};

//==============================================================================
// Umschalter fuer Auswahl-Parameter (Trigger, Laenge)
class SegmentedControl : public juce::Component
{
public:
    explicit SegmentedControl (juce::RangedAudioParameter&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::StringArray labels;
    juce::ParameterAttachment attachment;
    int selected = 0;
};

//==============================================================================
class HolterdipolterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit HolterdipolterAudioProcessorEditor (HolterdipolterAudioProcessor&);
    ~HolterdipolterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob (juce::Slider&, bool big);

    using APVTS = juce::AudioProcessorValueTreeState;

    HolterdipolterAudioProcessor& proc;
    HdpLookAndFeel lnf;

    CurveDisplay     display;
    CurveThumbs      thumbs;
    SegmentedControl modeControl, rateControl;

    juce::TextButton resetButton { "RESET" }, bandButton { "BAND SPLIT" };
    juce::Slider     mixKnob, xoverKnob, threshKnob;

    std::unique_ptr<APVTS::ButtonAttachment> bandAtt;
    std::unique_ptr<APVTS::SliderAttachment> mixAtt, xoverAtt, threshAtt;

    juce::Rectangle<int> mixLabelArea, xoverLabelArea, threshLabelArea, modeLabelArea, rateLabelArea, ledArea;
    float ledLevel = 0.0f, lastPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HolterdipolterAudioProcessorEditor)
};
