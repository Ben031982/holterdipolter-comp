#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

//==============================================================================
// Werks-Kurven (Startzustand der 16 Slots). Jeder Slot ist danach frei editierbar.
// Die Slots haben bewusst keine Namen - ausgewaehlt wird im Editor ueber die Vorschau.
//   attack : Anteil der Phase, in dem von 1 nach unten abgeblendet wird (0 = sofort unten)
//   hold   : Anteil der Phase, in dem der Gain unten bleibt
//   end    : Phase, bei der der Gain wieder voll da ist
//   power  : Form des Anstiegs (<1 = schnell hoch, >1 = spaet hoch, <0 = S-Form)
//   floor  : Rest-Gain am tiefsten Punkt (0 = komplett stumm)
struct CurveShape
{
    float attack, hold, end, power, floor;
};

inline constexpr std::array<CurveShape, 16> kCurves {{
    { 0.00f, 0.02f, 0.75f, 1.00f, 0.00f },
    { 0.00f, 0.02f, 0.40f, 1.00f, 0.00f },
    { 0.00f, 0.03f, 1.00f, 1.00f, 0.00f },
    { 0.00f, 0.00f, 0.80f, 0.55f, 0.00f },
    { 0.00f, 0.05f, 0.85f, 2.20f, 0.00f },
    { 0.00f, 0.25f, 0.90f, 1.60f, 0.00f },
    { 0.00f, 0.40f, 0.70f, 1.00f, 0.00f },
    { 0.00f, 0.48f, 0.52f, 1.00f, 0.00f },
    { 0.00f, 0.01f, 0.18f, 0.80f, 0.00f },
    { 0.00f, 0.00f, 0.70f, 1.00f, 0.35f },
    { 0.00f, 0.03f, 0.60f, 1.30f, 0.50f },
    { 0.04f, 0.03f, 0.75f, 1.00f, 0.00f },
    { 0.12f, 0.08f, 0.85f, 1.20f, 0.00f },
    { 0.00f, 0.05f, 0.90f, -1.0f, 0.00f },
    { 0.00f, 0.10f, 0.55f, 2.80f, 0.00f },
    { 0.00f, 0.18f, 1.00f, 0.70f, 0.10f },
}};

//==============================================================================
// Ein Stuetzpunkt der editierbaren Kurve. x = Phase 0..1, y = Gain 0..1,
// tension (-1..1) biegt das Segment von diesem Punkt zum naechsten.
struct CurvePoint
{
    float x = 0.0f, y = 0.0f, tension = 0.0f;
};

inline float shapeSegment (float t, float tension) noexcept
{
    if (std::abs (tension) < 1.0e-4f)
        return t;

    return std::pow (t, std::pow (8.0f, tension));
}

inline float evalPoints (const std::vector<CurvePoint>& pts, float p) noexcept
{
    if (pts.empty())            return 1.0f;
    if (p <= pts.front().x)     return pts.front().y;

    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        const auto& a = pts[i];
        const auto& b = pts[i + 1];

        if (p < b.x)
        {
            const float w = b.x - a.x;
            if (w <= 1.0e-6f)
                return b.y;

            return a.y + (b.y - a.y) * shapeSegment ((p - a.x) / w, a.tension);
        }
    }

    return pts.back().y;
}

inline std::vector<CurvePoint> presetPoints (const CurveShape& s)
{
    std::vector<CurvePoint> pts;
    const float riseStart = s.attack + s.hold;

    if (s.attack > 0.0f)
    {
        pts.push_back ({ 0.0f, 1.0f, 0.0f });
        pts.push_back ({ s.attack, s.floor, 0.0f });
    }
    else
    {
        pts.push_back ({ 0.0f, s.floor, 0.0f });
    }

    if (riseStart > pts.back().x)
        pts.push_back ({ riseStart, s.floor, 0.0f });

    if (s.power < 0.0f)   // S-Form: zwei gegenlaeufig gebogene Haelften
    {
        pts.back().tension = 1.0f / 3.0f;
        pts.push_back ({ 0.5f * (riseStart + s.end), s.floor + 0.5f * (1.0f - s.floor), -1.0f / 3.0f });
    }
    else
    {
        pts.back().tension = juce::jlimit (-1.0f, 1.0f, std::log (s.power) / std::log (8.0f));
    }

    pts.push_back ({ s.end, 1.0f, 0.0f });

    if (s.end < 1.0f)
        pts.push_back ({ 1.0f, 1.0f, 0.0f });

    return pts;
}

//==============================================================================
// Haelt die 16 editierbaren Kurven. Die Punkte werden nur ausserhalb des
// Audio-Threads angefasst; der Audio-Thread liest eine vorberechnete Tabelle.
class CurveBank
{
public:
    static constexpr int numSlots  = 16;
    static constexpr int tableSize = 2048;

    CurveBank()
    {
        for (int i = 0; i < numSlots; ++i)
            resetSlot (i);
    }

    // Audio-Thread: lock-frei
    float eval (int slot, float phase) const noexcept
    {
        const auto& t   = tables[(size_t) juce::jlimit (0, numSlots - 1, slot)];
        const float pos = juce::jlimit (0.0f, 1.0f, phase) * (float) (tableSize - 1);
        const int   i0  = (int) pos;
        const int   i1  = juce::jmin (tableSize - 1, i0 + 1);
        const float a   = t[(size_t) i0].load (std::memory_order_relaxed);
        const float b   = t[(size_t) i1].load (std::memory_order_relaxed);
        return a + (b - a) * (pos - (float) i0);
    }

    std::vector<CurvePoint> getPoints (int slot) const
    {
        const juce::ScopedLock sl (lock);
        return points[(size_t) juce::jlimit (0, numSlots - 1, slot)];
    }

    // Zaehlt bei jeder Aenderung hoch (fuer die Vorschau im Editor)
    int getVersion() const noexcept   { return version.load (std::memory_order_relaxed); }

    void setPoints (int slot, std::vector<CurvePoint> pts)
    {
        if (slot < 0 || slot >= numSlots)
            return;

        sanitise (pts);

        const juce::ScopedLock sl (lock);
        points[(size_t) slot] = std::move (pts);
        rebuild (slot);
        version.fetch_add (1, std::memory_order_relaxed);
    }

    void resetSlot (int slot)
    {
        if (slot >= 0 && slot < numSlots)
            setPoints (slot, presetPoints (kCurves[(size_t) slot]));
    }

    // Format: Slots mit '|' getrennt, Punkte mit ';', Werte "x,y,tension"
    juce::String toString() const
    {
        const juce::ScopedLock sl (lock);
        juce::StringArray slots;

        for (auto& pts : points)
        {
            juce::StringArray items;
            for (auto& p : pts)
                items.add (juce::String (p.x, 5) + "," + juce::String (p.y, 5) + "," + juce::String (p.tension, 4));
            slots.add (items.joinIntoString (";"));
        }

        return slots.joinIntoString ("|");
    }

    void fromString (const juce::String& text)
    {
        auto slots = juce::StringArray::fromTokens (text, "|", "");

        for (int s = 0; s < juce::jmin (numSlots, slots.size()); ++s)
        {
            std::vector<CurvePoint> pts;

            for (auto& item : juce::StringArray::fromTokens (slots[s], ";", ""))
            {
                auto v = juce::StringArray::fromTokens (item, ",", "");
                if (v.size() >= 2)
                    pts.push_back ({ v[0].getFloatValue(), v[1].getFloatValue(),
                                     v.size() > 2 ? v[2].getFloatValue() : 0.0f });
            }

            if (pts.size() >= 2)
                setPoints (s, std::move (pts));
        }
    }

private:
    static void sanitise (std::vector<CurvePoint>& pts)
    {
        if (pts.size() < 2)
            pts = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };

        float lastX = 0.0f;
        for (auto& p : pts)
        {
            if (! std::isfinite (p.x))       p.x = lastX;
            if (! std::isfinite (p.y))       p.y = 1.0f;
            if (! std::isfinite (p.tension)) p.tension = 0.0f;

            p.x       = juce::jlimit (lastX, 1.0f, p.x);
            p.y       = juce::jlimit (0.0f, 1.0f, p.y);
            p.tension = juce::jlimit (-1.0f, 1.0f, p.tension);
            lastX     = p.x;
        }

        pts.front().x = 0.0f;
        pts.back().x  = 1.0f;
    }

    void rebuild (int slot)
    {
        const auto& pts = points[(size_t) slot];
        auto& t = tables[(size_t) slot];

        for (int i = 0; i < tableSize; ++i)
            t[(size_t) i].store (evalPoints (pts, (float) i / (float) (tableSize - 1)), std::memory_order_relaxed);
    }

    std::atomic<int> version { 0 };
    juce::CriticalSection lock;
    std::array<std::vector<CurvePoint>, numSlots> points;
    std::array<std::array<std::atomic<float>, tableSize>, numSlots> tables;
};
