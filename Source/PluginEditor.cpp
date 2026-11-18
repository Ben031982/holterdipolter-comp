#include "PluginEditor.h"

namespace
{
    // Farbwelt: fast schwarz, ein einziger Akzent
    const juce::Colour kBg      { 0xff0c0d10 };
    const juce::Colour kPanel   { 0xff15171c };
    const juce::Colour kPanelHi { 0xff1c1f26 };
    const juce::Colour kLine    { 0xff2a2e37 };
    const juce::Colour kText    { 0xffe6e9ef };
    const juce::Colour kDim     { 0xff737a87 };
    const juce::Colour kAccent  { 0xffc8ff2e };   // Acid-Gruen

    juce::String utf8 (const char* text)   { return juce::String (juce::CharPointer_UTF8 (text)); }

    juce::Font capsFont (float height)
    {
        juce::Font f (juce::FontOptions (height, juce::Font::bold));
        f.setExtraKerningFactor (0.14f);
        return f;
    }

    void drawCaps (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                   juce::Justification just = juce::Justification::centredLeft)
    {
        g.setColour (kDim);
        g.setFont (capsFont (10.5f));
        g.drawText (text, area, just);
    }

    // Kurve als Pfad in ein Rechteck zeichnen; amount = Mix (1 = volle Tiefe)
    juce::Path curvePath (const std::vector<CurvePoint>& pts, juce::Rectangle<float> area, float amount)
    {
        juce::Path path;
        const int steps = juce::jmax (2, (int) area.getWidth());

        for (int i = 0; i <= steps; ++i)
        {
            const float p    = (float) i / (float) steps;
            const float gain = 1.0f - amount * (1.0f - evalPoints (pts, p));
            const float x    = area.getX() + p * area.getWidth();
            const float y    = area.getBottom() - gain * area.getHeight();
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }

        return path;
    }

    juce::Path closedBelow (const juce::Path& path, juce::Rectangle<float> area)
    {
        juce::Path fill (path);
        fill.lineTo (area.getRight(), area.getBottom());
        fill.lineTo (area.getX(), area.getBottom());
        fill.closeSubPath();
        return fill;
    }

    void strokeGlow (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float width)
    {
        const auto rounded = [] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
        g.setColour (colour.withAlpha (0.07f));  g.strokePath (path, rounded (width * 5.0f));
        g.setColour (colour.withAlpha (0.16f));  g.strokePath (path, rounded (width * 2.6f));
        g.setColour (colour);                    g.strokePath (path, rounded (width));
    }
}

//==============================================================================
HdpLookAndFeel::HdpLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       kText);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,  kAccent.withAlpha (0.3f));
    setColour (juce::Label::textColourId,               kText);
    setColour (juce::TextEditor::textColourId,          kText);
    setColour (juce::TextEditor::backgroundColourId,    kPanel);
    setColour (juce::TextEditor::outlineColourId,       kLine);
    setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
    setColour (juce::CaretComponent::caretColourId,     kAccent);
}

void HdpLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float pos, float startAngle, float endAngle, juce::Slider& slider)
{
    const bool big    = (bool) slider.getProperties().getWithDefault ("big", false);
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (big ? 10.0f : 6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  centre = bounds.getCentre();
    const float thick  = big ? 7.0f : 4.0f;
    const float arcR   = radius - thick * 0.5f;
    const float angle  = startAngle + pos * (endAngle - startAngle);
    const auto  rounded = juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (kLine);
    g.strokePath (track, rounded);

    if (pos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (kAccent.withAlpha (0.10f));
        g.strokePath (value, juce::PathStrokeType (thick * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (kAccent);
        g.strokePath (value, rounded);
    }

    // Knopf-Koerper
    const float bodyR = arcR - thick * 1.6f;
    g.setGradientFill (juce::ColourGradient (kPanelHi.brighter (0.08f), centre.x, centre.y - bodyR,
                                             kPanel.darker (0.25f),     centre.x, centre.y + bodyR, false));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (kLine);
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // Zeiger
    const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
    const float inner = big ? bodyR * 0.74f : bodyR * 0.35f;
    g.setColour (kText);
    g.drawLine ({ centre + dir * inner, centre + dir * (bodyR - 3.0f) }, big ? 2.5f : 2.0f);

    if (big)
    {
        g.setColour (kText);
        g.setFont (juce::FontOptions (bodyR * 0.50f, juce::Font::bold));
        g.drawText (juce::String (juce::roundToInt (slider.getValue())),
                    bounds.withSizeKeepingCentre (bodyR * 1.4f, bodyR * 0.6f).translated (0.0f, -bodyR * 0.04f),
                    juce::Justification::centred);
        g.setColour (kDim);
        g.setFont (capsFont (bodyR * 0.17f));
        g.drawText ("%", bounds.withSizeKeepingCentre (bodyR, bodyR * 0.3f).translated (0.0f, bodyR * 0.36f),
                    juce::Justification::centred);
    }
}

void HdpLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                           bool over, bool down)
{
    const auto r  = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();

    g.setColour (on ? kAccent : (down ? kLine : (over ? kPanelHi : kPanel)));
    g.fillRoundedRectangle (r, 4.0f);

    if (! on)
    {
        g.setColour (over ? kDim : kLine);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
}

void HdpLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool)
{
    g.setFont (capsFont (10.5f));
    g.setColour (b.getToggleState() ? kBg : (over ? kText : kDim));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

//==============================================================================
juce::Rectangle<float> CurveDisplay::plotArea() const
{
    return getLocalBounds().toFloat().reduced (14.0f, 14.0f);
}

juce::Point<float> CurveDisplay::toScreen (float x, float y) const
{
    const auto a = plotArea();
    return { a.getX() + x * a.getWidth(), a.getBottom() - y * a.getHeight() };
}

int CurveDisplay::currentSlot() const
{
    return juce::jlimit (0, CurveBank::numSlots - 1,
                         (int) proc.apvts.getRawParameterValue ("curve")->load());
}

int CurveDisplay::nodeAt (const std::vector<CurvePoint>& pts, juce::Point<float> pos) const
{
    int best = -1;
    float bestDist = 10.0f;   // Fangradius in Pixeln

    for (int i = 0; i < (int) pts.size(); ++i)
    {
        const float d = toScreen (pts[(size_t) i].x, pts[(size_t) i].y).getDistanceFrom (pos);
        if (d < bestDist) { bestDist = d; best = i; }
    }

    return best;
}

void CurveDisplay::paint (juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (kPanelHi, 0.0f, 0.0f, kPanel, 0.0f, full.getHeight(), false));
    g.fillRoundedRectangle (full, 8.0f);
    g.setColour (kLine);
    g.drawRoundedRectangle (full.reduced (0.5f), 8.0f, 1.0f);

    const auto area = plotArea();

    // Raster: Viertel kraeftiger, Sechzehntel nur angedeutet
    for (int i = 0; i <= 16; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / 16.0f;
        g.setColour (kLine.withAlpha (i % 4 == 0 ? 0.9f : 0.3f));
        g.drawLine (x, area.getY(), x, area.getBottom(), 1.0f);
    }
    for (int i = 0; i <= 4; ++i)
    {
        const float y = area.getY() + area.getHeight() * (float) i / 4.0f;
        g.setColour (kLine.withAlpha (i % 2 == 0 ? 0.9f : 0.3f));
        g.drawLine (area.getX(), y, area.getRight(), y, 1.0f);
    }

    const bool  dragging = dragNode >= 0 || dragSegment >= 0;
    const auto  pts = dragging ? editPts : proc.curves.getPoints (currentSlot());
    const float mix = proc.apvts.getRawParameterValue ("mix")->load() * 0.01f;

    // Wellenform ueber einen Durchlauf: Original (blass) und Ergebnis nach dem Ducking (hell)
    {
        const int n = HolterdipolterAudioProcessor::scopeSize;
        std::array<float, (size_t) HolterdipolterAudioProcessor::scopeSize> in, out;
        float maxIn = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            in [(size_t) i] = proc.scopeIn [(size_t) i].load (std::memory_order_relaxed);
            out[(size_t) i] = proc.scopeOut[(size_t) i].load (std::memory_order_relaxed);
            maxIn = juce::jmax (maxIn, in[(size_t) i]);
        }

        if (maxIn > 1.0e-4f)
        {
            // Leise Signale werden hochskaliert, damit man die Form erkennt
            const float target = 0.92f / juce::jmax (maxIn, 0.02f);
            scopeScale += 0.15f * (target - scopeScale);

            auto wavePath = [&] (const auto& data)
            {
                const float mid  = area.getCentreY();
                const float half = area.getHeight() * 0.5f;
                juce::Path p;

                for (int i = 0; i < n; ++i)
                {
                    const float x = area.getX() + area.getWidth() * ((float) i + 0.5f) / (float) n;
                    const float h = juce::jmin (1.0f, data[(size_t) i] * scopeScale) * half;
                    if (i == 0) p.startNewSubPath (area.getX(), mid - h);
                    p.lineTo (x, mid - h);
                }
                p.lineTo (area.getRight(), mid);
                for (int i = n - 1; i >= 0; --i)
                {
                    const float x = area.getX() + area.getWidth() * ((float) i + 0.5f) / (float) n;
                    const float h = juce::jmin (1.0f, data[(size_t) i] * scopeScale) * half;
                    p.lineTo (x, mid + h);
                }
                p.lineTo (area.getX(), mid);
                p.closeSubPath();
                return p;
            };

            g.setColour (kText.withAlpha (0.13f));
            g.fillPath (wavePath (in));
            g.setColour (kText.withAlpha (0.42f));
            g.fillPath (wavePath (out));
        }
    }

    const auto path = curvePath (pts, area, 1.0f);

    g.setGradientFill (juce::ColourGradient (kAccent.withAlpha (0.16f), 0.0f, area.getY(),
                                             kAccent.withAlpha (0.01f), 0.0f, area.getBottom(), false));
    g.fillPath (closedBelow (path, area));

    if (mix < 0.995f)   // tatsaechlich wirksame Kurve bei reduziertem Mix
    {
        g.setColour (kText.withAlpha (0.35f));
        const float dashes[] = { 4.0f, 4.0f };
        juce::Path dashed;
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, curvePath (pts, area, mix), dashes, 2);
        g.fillPath (dashed);
    }

    strokeGlow (g, path, kAccent, 2.2f);

    // Stuetzpunkte
    for (int i = 0; i < (int) pts.size(); ++i)
    {
        const auto  c = toScreen (pts[(size_t) i].x, pts[(size_t) i].y);
        const float r = i == dragNode ? 6.5f : 5.0f;
        g.setColour (kBg);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (i == dragNode ? kText : kAccent);
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 2.0f);
    }

    // Abspielposition mit Punkt auf dem aktuell wirksamen Gain
    const float phase = juce::jlimit (0.0f, 1.0f, proc.uiPhase.load (std::memory_order_relaxed));
    if (phase < 1.0f)
    {
        const float x = area.getX() + phase * area.getWidth();
        g.setGradientFill (juce::ColourGradient (kText.withAlpha (0.0f), x, area.getY(),
                                                 kText.withAlpha (0.55f), x, area.getBottom(), false));
        g.fillRect (x - 0.75f, area.getY(), 1.5f, area.getHeight());

        const auto dot = toScreen (phase, 1.0f - mix * (1.0f - evalPoints (pts, phase)));
        g.setColour (kText.withAlpha (0.18f));
        g.fillEllipse (dot.x - 9.0f, dot.y - 9.0f, 18.0f, 18.0f);
        g.setColour (kText);
        g.fillEllipse (dot.x - 4.0f, dot.y - 4.0f, 8.0f, 8.0f);
    }

    if (isMouseOver (true) && ! dragging)
    {
        g.setColour (kDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (utf8 ("Punkt ziehen  \xc2\xb7  Doppelklick: Punkt +/\xe2\x80\x93  \xc2\xb7  Linie ziehen: Biegung"),
                    getLocalBounds().reduced (18, 6).removeFromBottom (16), juce::Justification::centredLeft);
    }
}

void CurveDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragSlot    = currentSlot();
    editPts     = proc.curves.getPoints (dragSlot);
    dragNode    = nodeAt (editPts, e.position);
    dragSegment = -1;

    if (dragNode >= 0)
        return;

    // Nah an der Linie? Dann die Biegung dieses Segments ziehen.
    const auto  area  = plotArea();
    const float p     = juce::jlimit (0.0f, 1.0f, (e.position.x - area.getX()) / area.getWidth());
    const float lineY = toScreen (p, evalPoints (editPts, p)).y;

    if (std::abs (lineY - e.position.y) < 14.0f)
    {
        for (int i = 0; i + 1 < (int) editPts.size(); ++i)
            if (p >= editPts[(size_t) i].x && p < editPts[(size_t) i + 1].x)
            {
                dragSegment      = i;
                dragStartTension = editPts[(size_t) i].tension;
                break;
            }
    }
}

void CurveDisplay::mouseDrag (const juce::MouseEvent& e)
{
    const auto area = plotArea();

    if (dragNode >= 0)
    {
        auto& pt = editPts[(size_t) dragNode];
        const int last = (int) editPts.size() - 1;

        pt.y = juce::jlimit (0.0f, 1.0f, (area.getBottom() - e.position.y) / area.getHeight());

        if (dragNode == 0)          pt.x = 0.0f;
        else if (dragNode == last)  pt.x = 1.0f;
        else
            pt.x = juce::jlimit (editPts[(size_t) dragNode - 1].x, editPts[(size_t) dragNode + 1].x,
                                 (e.position.x - area.getX()) / area.getWidth());
    }
    else if (dragSegment >= 0)
    {
        const auto& a = editPts[(size_t) dragSegment];
        const auto& b = editPts[(size_t) dragSegment + 1];

        // Nach oben ziehen soll die Linie anheben - je nach Richtung des Segments
        const float up        = (float) -e.getDistanceFromDragStartY() / area.getHeight();
        const float direction = b.y >= a.y ? -1.0f : 1.0f;

        editPts[(size_t) dragSegment].tension = juce::jlimit (-1.0f, 1.0f, dragStartTension + direction * up * 3.0f);
    }
    else
    {
        return;
    }

    proc.curves.setPoints (dragSlot, editPts);
    repaint();
}

void CurveDisplay::mouseUp (const juce::MouseEvent&)
{
    dragNode = dragSegment = -1;
    repaint();
}

void CurveDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int slot = currentSlot();
    auto pts = proc.curves.getPoints (slot);
    const int hit = nodeAt (pts, e.position);

    if (hit >= 0)
    {
        if (hit == 0 || hit == (int) pts.size() - 1)
            return;                                   // Anfangs- und Endpunkt bleiben

        pts.erase (pts.begin() + hit);
    }
    else
    {
        const auto area = plotArea();
        CurvePoint np;
        np.x = juce::jlimit (0.0f, 1.0f, (e.position.x - area.getX()) / area.getWidth());
        np.y = juce::jlimit (0.0f, 1.0f, (area.getBottom() - e.position.y) / area.getHeight());

        size_t insertAt = 1;
        while (insertAt < pts.size() - 1 && pts[insertAt].x <= np.x)
            ++insertAt;

        pts.insert (pts.begin() + (std::ptrdiff_t) insertAt, np);
    }

    dragNode = dragSegment = -1;
    proc.curves.setPoints (slot, std::move (pts));
    repaint();
}

//==============================================================================
CurveThumbs::CurveThumbs (HolterdipolterAudioProcessor& p)
    : proc (p),
      attachment (*p.apvts.getParameter ("curve"),
                  [this] (float v) { selected = juce::roundToInt (v); repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> CurveThumbs::cellBounds (int index) const
{
    const int   cols = CurveBank::numSlots / 2;
    const float gap  = 6.0f;
    const float w    = ((float) getWidth()  - gap * (float) (cols - 1)) / (float) cols;
    const float h    = ((float) getHeight() - gap) / 2.0f;
    return { (float) (index % cols) * (w + gap), (float) (index / cols) * (h + gap), w, h };
}

int CurveThumbs::cellAt (juce::Point<float> pos) const
{
    for (int i = 0; i < CurveBank::numSlots; ++i)
        if (cellBounds (i).expanded (3.0f).contains (pos))
            return i;

    return -1;
}

void CurveThumbs::refreshIfNeeded()
{
    const int v = proc.curves.getVersion();
    if (v != seenVersion)
    {
        seenVersion = v;
        repaint();
    }
}

void CurveThumbs::paint (juce::Graphics& g)
{
    for (int i = 0; i < CurveBank::numSlots; ++i)
    {
        const auto cell  = cellBounds (i);
        const bool isSel = i == selected;
        const bool isHov = i == hovered;

        g.setColour (isSel ? kAccent.withAlpha (0.10f) : (isHov ? kPanelHi : kPanel));
        g.fillRoundedRectangle (cell, 5.0f);
        g.setColour (isSel ? kAccent : (isHov ? kDim : kLine));
        g.drawRoundedRectangle (cell.reduced (0.5f), 5.0f, isSel ? 1.5f : 1.0f);

        const auto inner = cell.reduced (7.0f, 6.0f);
        const auto path  = curvePath (proc.curves.getPoints (i), inner, 1.0f);

        g.setColour ((isSel ? kAccent : kDim).withAlpha (isSel ? 0.22f : 0.12f));
        g.fillPath (closedBelow (path, inner));
        g.setColour (isSel ? kAccent : (isHov ? kText : kDim.brighter (0.3f)));
        g.strokePath (path, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void CurveThumbs::mouseDown (const juce::MouseEvent& e)
{
    const int i = cellAt (e.position);
    if (i >= 0)
        attachment.setValueAsCompleteGesture ((float) i);
}

void CurveThumbs::mouseMove (const juce::MouseEvent& e)
{
    const int i = cellAt (e.position);
    if (i != hovered) { hovered = i; repaint(); }
}

void CurveThumbs::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

//==============================================================================
SegmentedControl::SegmentedControl (juce::RangedAudioParameter& param)
    : attachment (param, [this] (float v) { selected = juce::roundToInt (v); repaint(); })
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&param))
        labels = choice->choices;

    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SegmentedControl::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (kPanel);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (kLine);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    const int   n = juce::jmax (1, labels.size());
    const float w = r.getWidth() / (float) n;

    for (int i = 0; i < n; ++i)
    {
        const auto seg = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());

        if (i == selected)
        {
            g.setColour (kAccent);
            g.fillRoundedRectangle (seg.reduced (3.0f), 3.0f);
        }

        g.setColour (i == selected ? kBg : kDim);
        g.setFont (capsFont (11.0f));
        g.drawText (labels[i].toUpperCase(), seg.toNearestInt(), juce::Justification::centred);
    }
}

void SegmentedControl::mouseDown (const juce::MouseEvent& e)
{
    const int n = juce::jmax (1, labels.size());
    attachment.setValueAsCompleteGesture ((float) juce::jlimit (0, n - 1, e.x * n / juce::jmax (1, getWidth())));
}

//==============================================================================
HolterdipolterAudioProcessorEditor::HolterdipolterAudioProcessorEditor (HolterdipolterAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p), thumbs (p),
      modeControl (*p.apvts.getParameter ("mode")),
      rateControl (*p.apvts.getParameter ("rate"))
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (display);
    addAndMakeVisible (thumbs);
    addAndMakeVisible (modeControl);
    addAndMakeVisible (rateControl);

    addAndMakeVisible (resetButton);
    resetButton.onClick = [this]
    {
        proc.curves.resetSlot ((int) proc.apvts.getRawParameterValue ("curve")->load());
        display.repaint();
    };

    bandButton.setClickingTogglesState (true);
    addAndMakeVisible (bandButton);
    bandAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "band", bandButton);

    setupKnob (mixKnob,    true);
    setupKnob (xoverKnob,  false);
    setupKnob (threshKnob, false);
    xoverKnob.setTextValueSuffix (" Hz");
    threshKnob.setTextValueSuffix (" dB");

    mixAtt    = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "mix",    mixKnob);
    xoverAtt  = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "xover",  xoverKnob);
    threshAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "thresh", threshKnob);

    setSize (780, 500);
    startTimerHz (30);
}

HolterdipolterAudioProcessorEditor::~HolterdipolterAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void HolterdipolterAudioProcessorEditor::setupKnob (juce::Slider& s, bool big)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.getProperties().set ("big", big);
    s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxTextColourId,       kText);

    if (big)  s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    else      s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);

    addAndMakeVisible (s);
}

void HolterdipolterAudioProcessorEditor::timerCallback()
{
    const int  mode  = (int) proc.apvts.getRawParameterValue ("mode")->load();
    const bool split = proc.apvts.getRawParameterValue ("band")->load() > 0.5f;

    threshKnob.setEnabled (mode == HolterdipolterAudioProcessor::modeAudio);
    threshKnob.setAlpha (threshKnob.isEnabled() ? 1.0f : 0.3f);
    xoverKnob.setEnabled (split);
    xoverKnob.setAlpha (split ? 1.0f : 0.3f);

    // Trigger-LED: blitzt bei jedem Kurvenstart
    const float phase = proc.uiPhase.load (std::memory_order_relaxed);
    const bool  fired = proc.uiTriggered.exchange (false) || (mode == HolterdipolterAudioProcessor::modeSync && phase < lastPhase);
    lastPhase = phase;
    ledLevel  = fired ? 1.0f : ledLevel * 0.78f;

    thumbs.refreshIfNeeded();
    display.repaint();
    repaint (ledArea.expanded (8));
}

void HolterdipolterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // Kopfzeile
    auto header = getLocalBounds().removeFromTop (54).reduced (20, 0);

    juce::Font title (juce::FontOptions (19.0f, juce::Font::bold));
    title.setExtraKerningFactor (0.22f);
    g.setFont (title);

    juce::GlyphArrangement ga;
    ga.addLineOfText (title, "HOLTERDIPOLTER", 0.0f, 0.0f);
    const int titleWidth = juce::roundToInt (ga.getBoundingBox (0, -1, true).getWidth());

    g.setColour (kText);
    g.drawText ("HOLTERDIPOLTER", header, juce::Justification::centredLeft);
    g.setColour (kAccent);
    g.drawText ("COMP", header.withTrimmedLeft (titleWidth + 14), juce::Justification::centredLeft);

    // Trigger-LED
    const auto led = ledArea.toFloat();
    g.setColour (kAccent.withAlpha (0.25f * ledLevel));
    g.fillEllipse (led.expanded (6.0f));
    g.setColour (kLine.interpolatedWith (kAccent, ledLevel));
    g.fillEllipse (led);
    drawCaps (g, "TRIG", ledArea.translated (-48, 0).withWidth (40).withHeight (ledArea.getHeight()), juce::Justification::centredRight);

    g.setColour (kLine);
    g.fillRect (0, 54, getWidth(), 1);

    drawCaps (g, "MIX",       mixLabelArea,    juce::Justification::centred);
    drawCaps (g, "SPLIT",     xoverLabelArea,  juce::Justification::centred);
    drawCaps (g, "THRESHOLD", threshLabelArea, juce::Justification::centred);
    drawCaps (g, "TRIGGER",   modeLabelArea);
    drawCaps (g, utf8 ("L\xc3\x84NGE"), rateLabelArea);
}

void HolterdipolterAudioProcessorEditor::resized()
{
    ledArea = { getWidth() - 20 - 10, 22, 10, 10 };

    auto area = getLocalBounds().withTrimmedTop (55).reduced (20, 18);

    // Rechte Spalte
    auto right = area.removeFromRight (184);
    area.removeFromRight (20);

    mixLabelArea = right.removeFromTop (14);
    mixKnob.setBounds (right.removeFromTop (184));
    right.removeFromTop (8);
    bandButton.setBounds (right.removeFromTop (30).reduced (22, 0));
    right.removeFromTop (16);

    auto small = right.removeFromTop (112);
    auto xo = small.removeFromLeft (small.getWidth() / 2);
    xoverLabelArea = xo.removeFromTop (14);
    xoverKnob.setBounds (xo);
    threshLabelArea = small.removeFromTop (14);
    threshKnob.setBounds (small);

    // Linke Spalte: unten Schalter, darueber Miniaturen, Rest fuer die grosse Kurve
    auto controls = area.removeFromBottom (48);
    area.removeFromBottom (14);
    thumbs.setBounds (area.removeFromBottom (82));
    area.removeFromBottom (12);
    display.setBounds (area);

    resetButton.setBounds (display.getRight() - 62, display.getY() + 8, 54, 20);

    auto modeArea = controls.removeFromLeft (juce::roundToInt ((float) controls.getWidth() * 0.42f));
    controls.removeFromLeft (16);
    modeLabelArea = modeArea.removeFromTop (16);
    modeControl.setBounds (modeArea);
    rateLabelArea = controls.removeFromTop (16);
    rateControl.setBounds (controls);
}
