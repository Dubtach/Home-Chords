#pragma once

#include <JuceHeader.h>
#include <functional>
#include <cmath>

// =============================================================================
// Home series look, carried over from Home-Disto / Home-Sidechain: near-
// black chassis, saturated gradient cards, dark embossed text on top of
// them. Colours, font(), and the card/well/brand painters below are the
// same tokens those two plugins use, so Home-Chords reads as the same
// product family.
//
// This file additionally defines ChordCard, the one genuinely new
// component this plugin needs -- everything else here is the existing kit.
//
// Header-only and defined inline, matching the existing convention for
// this file (each "Home" plugin currently carries its own copy rather
// than a shared submodule).
// =============================================================================

namespace homeUI
{
    inline const juce::Colour chassis  (0xff09090b);
    inline const juce::Colour face     (0xff111114);
    inline const juce::Colour faceEdge (0xff222228);
    inline const juce::Colour rule     (0xff1e1e24);
    inline const juce::Colour slot     (0xff161618);
    inline const juce::Colour slotAlt  (0xff1a1a1e);
    inline const juce::Colour slotEdge (0xff2a2a30);
    inline const juce::Colour ink      (0xff09090b);   // text sitting on a card
    inline const juce::Colour cyan     (0xff00e5ff);
    inline const juce::Colour green    (0xff00ff87);
    inline const juce::Colour purple   (0xffb900ff);
    inline const juce::Colour pink     (0xffff007f);
    inline const juce::Colour warn     (0xffff6b7a);

    inline juce::Font font (float size, bool bold = true)
    {
        return juce::Font (juce::FontOptions (size).withName ("Helvetica")
                                                   .withStyle (bold ? "Bold" : "Plain"));
    }

    // A saturated card: drop shadow, vertical gradient, glass sheen along the
    // top, a fine cross-hatch, and a hard dark edge.
    inline void drawCard (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour base)
    {
        for (int i = 1; i <= 6; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.11f * static_cast<float> (7 - i)));
            g.fillRoundedRectangle (bounds.expanded (static_cast<float> (i) * 0.4f)
                                          .translated (0.0f, 1.6f + static_cast<float> (i) * 0.45f), 8.0f);
        }

        juce::ColourGradient body (base, bounds.getX(), bounds.getY(),
                                   base.darker (0.20f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (bounds, 6.0f);

        {
            const auto sheenArea = bounds.withHeight (bounds.getHeight() * 0.42f);
            juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.10f), sheenArea.getX(), sheenArea.getY(),
                                        juce::Colours::white.withAlpha (0.0f), sheenArea.getX(), sheenArea.getBottom(), false);
            juce::Path clip;
            clip.addRoundedRectangle (bounds, 6.0f);

            g.saveState();
            g.reduceClipRegion (clip);
            g.setGradientFill (sheen);
            g.fillRect (sheenArea);
            g.restoreState();
        }

        g.setColour (juce::Colours::black.withAlpha (0.12f));

        for (float y = bounds.getY() + 4.0f; y < bounds.getBottom() - 2.0f; y += 4.0f)
            g.drawLine (bounds.getX() + 2.0f, y, bounds.getRight() - 2.0f, y, 1.2f);

        for (float x = bounds.getX() + 4.0f; x < bounds.getRight() - 2.0f; x += 4.0f)
            g.drawLine (x, bounds.getY() + 2.0f, x, bounds.getBottom() - 2.0f, 1.2f);

        g.setColour (juce::Colours::black.withAlpha (0.50f));
        g.drawRoundedRectangle (bounds, 6.0f, 2.0f);
    }

    // Dark text on a card, with the same faint emboss Home-Disto uses.
    inline void drawCardText (juce::Graphics& g, const juce::String& text,
                              juce::Rectangle<float> area, float size,
                              juce::Justification justification = juce::Justification::centred,
                              float alpha = 1.0f)
    {
        g.setFont (font (size));
        g.setColour (juce::Colours::black.withAlpha (0.30f * alpha));
        g.drawText (text, area.translated (0.0f, 1.0f), justification, false);
        g.setColour (ink.withAlpha (alpha));
        g.drawText (text, area, justification, false);
    }

    inline void drawCardTitle (juce::Graphics& g, const juce::String& title, juce::Rectangle<float> card)
    {
        const auto row = juce::Rectangle<float> (card.getX(), card.getY() + 7.0f, card.getWidth(), 15.0f);
        drawCardText (g, title, row, 12.0f);

        const float width = 20.0f;
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawLine (card.getCentreX() - width, row.getBottom() + 1.0f,
                    card.getCentreX() + width, row.getBottom() + 1.0f, 1.2f);
    }

    // A recessed dark well, for anything that needs to read as a screen.
    inline void drawWell (juce::Graphics& g, juce::Rectangle<float> bounds, float radius = 5.0f)
    {
        g.setColour (chassis.withAlpha (0.92f));
        g.fillRoundedRectangle (bounds, radius);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (bounds, radius, 1.4f);
    }

    inline void drawTrackedText (juce::Graphics& g, const juce::String& text,
                                 float x, float y, float height, float totalWidth, float size)
    {
        if (text.isEmpty())
            return;

        const auto f = font (size);
        g.setFont (f);

        float natural = 0.0f;

        for (int i = 0; i < text.length(); ++i)
            natural += juce::TextLayout::getStringWidth (f, text.substring (i, i + 1));

        const float extra = text.length() > 1
            ? (totalWidth - natural) / static_cast<float> (text.length() - 1)
            : 0.0f;

        float cursor = x;

        for (int i = 0; i < text.length(); ++i)
        {
            const auto glyph = text.substring (i, i + 1);
            const auto width = juce::TextLayout::getStringWidth (f, glyph);
            g.drawText (glyph, juce::Rectangle<float> (cursor, y, width + 2.0f, height),
                        juce::Justification::centredLeft, false);
            cursor += width + extra;
        }
    }

    // The Home-series header: name, accent half, company line, hairline rule.
    inline void drawBrand (juce::Graphics& g, const juce::String& tail, juce::Colour accent,
                           float x, float y, float ruleRight)
    {
        const auto titleFont = font (22.0f);
        g.setFont (titleFont);

        const juce::String head ("Home-");
        const auto headWidth = juce::TextLayout::getStringWidth (titleFont, head);
        const auto tailWidth = juce::TextLayout::getStringWidth (titleFont, tail);

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawText (head, juce::Rectangle<float> (x + 1.0f, y + 1.0f, headWidth, 28.0f),
                    juce::Justification::centredLeft, false);
        g.drawText (tail, juce::Rectangle<float> (x + headWidth + 1.0f, y + 1.0f, tailWidth + 8.0f, 28.0f),
                    juce::Justification::centredLeft, false);

        g.setColour (juce::Colours::white);
        g.drawText (head, juce::Rectangle<float> (x, y, headWidth, 28.0f),
                    juce::Justification::centredLeft, false);
        g.setColour (accent);
        g.drawText (tail, juce::Rectangle<float> (x + headWidth, y, tailWidth + 8.0f, 28.0f),
                    juce::Justification::centredLeft, false);

        g.setColour (juce::Colours::white.withAlpha (0.4f));
        drawTrackedText (g, "DUBTACH DSP", x + 1.0f, y + 28.0f, 11.0f, headWidth + tailWidth, 8.0f);

        g.setColour (rule);
        g.drawLine (x, y + 46.0f, ruleRight, y + 46.0f, 2.0f);
    }

    // A small square lamp with a caption, used for MIDI/link activity.
    inline void drawLamp (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& label,
                          juce::Colour colour, float activity, bool steady)
    {
        const float level = juce::jlimit (0.0f, 1.0f, (steady ? 0.55f : 0.0f) + activity * 0.75f);
        const auto dot = juce::Rectangle<float> (area.getX(), area.getCentreY() - 3.0f, 6.0f, 6.0f);

        if (level > 0.45f)
        {
            g.setColour (colour.withAlpha ((level - 0.45f) * 0.55f));
            g.fillEllipse (dot.expanded (4.0f));
        }

        g.setColour (colour.withAlpha (juce::jmax (0.16f, level)));
        g.fillEllipse (dot);

        g.setFont (font (8.5f));
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawText (label, area.withTrimmedLeft (11.0f), juce::Justification::centredLeft, false);
    }

    // =========================================================================
    // Controls
    // =========================================================================

    // A stepper arrow -- a triangle on a small dark tile, for "previous /
    // next" through a list or a value (Octave -/+ here).
    class ChevronButton : public juce::Button
    {
    public:
        enum Direction { left, right };

        ChevronButton (Direction directionIn, juce::Colour accentColour = cyan)
            : juce::Button ("Chevron"), direction (directionIn), accent (accentColour)
        {
        }

        void setAccent (juce::Colour c) { accent = c; repaint(); }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            const auto r = getLocalBounds().toFloat().reduced (1.0f);
            const bool enabled = isEnabled();

            g.setColour (juce::Colours::black.withAlpha (! enabled ? 0.20f : (down ? 0.55f : (over ? 0.42f : 0.30f))));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (accent.withAlpha (! enabled ? 0.15f : (over ? 0.75f : 0.45f)));
            g.drawRoundedRectangle (r, 6.0f, 1.1f);

            const float cx = r.getCentreX();
            const float cy = r.getCentreY();
            const float size = juce::jmin (r.getWidth(), r.getHeight()) * 0.26f;

            juce::Path tri;

            if (direction == left)
                tri.addTriangle (cx + size * 0.55f, cy - size, cx + size * 0.55f, cy + size, cx - size * 0.65f, cy);
            else
                tri.addTriangle (cx - size * 0.55f, cy - size, cx - size * 0.55f, cy + size, cx + size * 0.65f, cy);

            g.setColour (enabled ? juce::Colours::white : juce::Colours::white.withAlpha (0.3f));
            g.fillPath (tri);
        }

    private:
        Direction direction;
        juce::Colour accent;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChevronButton)
    };

    // Matches Home-Disto's rotary slider exactly: dark body, tick ring, a
    // neon glow arc under a white arc, and a glowing pointer needle.
    class Knob : public juce::Slider
    {
    public:
        Knob (const juce::String& captionText, juce::Colour accentColour = cyan)
            : caption (captionText), accent (accentColour)
        {
            setSliderStyle (juce::Slider::RotaryVerticalDrag);
            setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            setRotaryParameters (startAngle, endAngle, true);
            setName (captionText);
        }

        std::function<juce::String (double)> valueText;

        void setCaption (const juce::String& c) { caption = c; repaint(); }
        void setAccent (juce::Colour c) { accent = c; repaint(); }

        // A compact dial with its caption and value stacked beside it:
        //
        //    (dial)  VELOCITY
        //            100
        //
        // Thin arcs and a single pointer line, so it stays crisp at this size.
        void paint (juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat().reduced (2.0f, 0.0f);

            const float dialSize = juce::jmin (area.getHeight(), 42.0f);
            auto dialArea = area.removeFromLeft (dialSize).withSizeKeepingCentre (dialSize, dialSize);
            area.removeFromLeft (6.0f);

            const float cx = dialArea.getCentreX();
            const float cy = dialArea.getCentreY();
            const float radius = dialSize * 0.5f - 2.0f;
            const float trackRadius = radius - 2.0f;
            const auto proportion = juce::jlimit (0.0f, 1.0f,
                                                  static_cast<float> (valueToProportionOfLength (getValue())));
            const float angle = startAngle + proportion * (endAngle - startAngle);
            const bool on = isEnabled();
            const auto colour = on ? accent : juce::Colours::grey;

            // Body.
            g.setColour (juce::Colour (0xff101014));
            g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);

            // Track and value arcs.
            const float arcRadius = trackRadius - 1.0f;
            juce::Path track;
            track.addCentredArc (cx, cy, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            if (angle > startAngle + 0.001f)
            {
                juce::Path value;
                value.addCentredArc (cx, cy, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
                g.setColour (colour);
                g.strokePath (value, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            // Pointer.
            const float inner = radius * 0.18f;
            const float outer = arcRadius - 4.0f;
            g.setColour (juce::Colours::white.withAlpha (on ? 1.0f : 0.5f));
            g.drawLine (cx + std::sin (angle) * inner, cy - std::cos (angle) * inner,
                        cx + std::sin (angle) * outer, cy - std::cos (angle) * outer, 2.0f);

            // Caption over value, to the right of the dial.
            const auto captionRow = area.removeFromTop (area.getHeight() * 0.5f);
            g.setFont (font (captionSize - 1.0f));
            g.setColour (juce::Colours::black.withAlpha (on ? 0.72f : 0.4f));
            g.drawText (caption, captionRow.withTrimmedTop (4.0f), juce::Justification::bottomLeft, false);

            g.setFont (font (12.5f));
            g.setColour (juce::Colours::white.withAlpha (on ? 1.0f : 0.5f));
            g.drawText (valueText != nullptr ? valueText (getValue()) : juce::String (getValue(), 2),
                        area.withTrimmedBottom (4.0f), juce::Justification::topLeft, false);
        }

    private:
        static constexpr float startAngle = juce::MathConstants<float>::pi * 1.22f;
        static constexpr float endAngle = juce::MathConstants<float>::pi * 2.78f;

        juce::String caption;
        juce::Colour accent;
        float captionSize = 10.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
    };

    // The little "metronome + 4/4" pill that sits in the timeline's corner.
    // Click it (the editor shows the menu) to change the time signature.
    class TimeSigChip : public juce::Button
    {
    public:
        TimeSigChip() : juce::Button ("TimeSig") {}

        void setLabel (const juce::String& newLabel)
        {
            if (label != newLabel)
            {
                label = newLabel;
                repaint();
            }
        }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (0.5f);

            g.setColour (juce::Colour (0xff101014).withAlpha (0.95f));
            g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
            g.setColour (cyan.withAlpha (down ? 1.0f : (over ? 0.9f : 0.55f)));
            g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, 1.0f);

            // Metronome: a trapezoid body, a swinging arm and a weight.
            auto icon = bounds.removeFromLeft (bounds.getHeight()).reduced (4.5f, 3.0f);
            juce::Path body;
            body.startNewSubPath (icon.getCentreX() - icon.getWidth() * 0.22f, icon.getY());
            body.lineTo (icon.getCentreX() + icon.getWidth() * 0.22f, icon.getY());
            body.lineTo (icon.getRight(), icon.getBottom());
            body.lineTo (icon.getX(), icon.getBottom());
            body.closeSubPath();
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.strokePath (body, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved));
            g.drawLine (icon.getCentreX(), icon.getBottom() - 1.5f, icon.getCentreX() + icon.getWidth() * 0.22f, icon.getY() + 1.5f, 1.2f);

            g.setFont (font (10.5f, true));
            g.setColour (juce::Colours::white);
            g.drawText (label, bounds.withTrimmedRight (3.0f), juce::Justification::centred, false);
        }

    private:
        juce::String label { "4/4" };
    };

    // A small labelled on/off tile -- a tick on a dark square when on,
    // empty when off, with a text label (set via setButtonText()) to its
    // right. Used for Auto Inversion; general-purpose beyond that.
    class Checkbox : public juce::Button
    {
    public:
        explicit Checkbox (juce::Colour accentColour = cyan)
            : juce::Button ("Checkbox"), accent (accentColour)
        {
            setClickingTogglesState (true);
        }

        void setAccent (juce::Colour c) { accent = c; repaint(); }

        void paintButton (juce::Graphics& g, bool over, bool /*down*/) override
        {
            auto bounds = getLocalBounds().toFloat();
            const auto box = bounds.removeFromLeft (bounds.getHeight()).reduced (2.0f);
            const bool on = getToggleState();

            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour ((on ? accent : juce::Colours::white).withAlpha (on ? 0.9f : (over ? 0.5f : 0.3f)));
            g.drawRoundedRectangle (box, 3.0f, 1.2f);

            if (on)
            {
                juce::Path tick;
                tick.startNewSubPath (box.getX() + box.getWidth() * 0.22f, box.getCentreY());
                tick.lineTo (box.getX() + box.getWidth() * 0.42f, box.getBottom() - box.getHeight() * 0.22f);
                tick.lineTo (box.getRight() - box.getWidth() * 0.18f, box.getY() + box.getHeight() * 0.22f);

                g.setColour (accent);
                g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            auto textArea = bounds.withTrimmedLeft (6.0f);
            g.setFont (font (10.5f, false));
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.drawText (getButtonText(), textArea, juce::Justification::centredLeft, false);
        }

    private:
        juce::Colour accent;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Checkbox)
    };

    // =========================================================================
    // ChordCard -- new for Home-Chords.
    //
    // One diatonic-chord pad: QWERTY key cap, roman numeral, chord name.
    // Colour follows chord quality (set by the caller) rather than an
    // arbitrary per-slot rainbow, since quality is genuine information.
    // Pressed state is driven every frame from a 0-1 "lit" amount so a key
    // release fades rather than snapping off; mouse clicks audition the
    // chord through the same onPressStart/onPressEnd path a keyboard key
    // uses, via the processor's real-time-safe slot-hold API.
    // =========================================================================
    class ChordCard : public juce::Component,
                      public juce::SettableTooltipClient
    {
    public:
        ChordCard() = default;

        void setContent (const juce::String& keyLabel, const juce::String& roman,
                         const juce::String& name, juce::Colour accentColour)
        {
            keyCap = keyLabel;
            romanText = roman;
            nameText = name;
            accent = accentColour;
            repaint();
        }

        void setEmpty()
        {
            keyCap.clear();
            romanText.clear();
            nameText.clear();
            repaint();
        }

        bool hasChord() const noexcept { return nameText.isNotEmpty(); }

        // Shows a small dot in the corner when this slot has a quality/
        // extension override active, so an edited slot reads differently
        // from a plain diatonic one at a glance.
        void setHasOverride (bool overridden) noexcept
        {
            if (hasOverrideFlag != overridden)
            {
                hasOverrideFlag = overridden;
                repaint();
            }
        }

        // 0 = at rest, 1 = fully lit. The editor's Timer drives this from a
        // value that jumps to 1 on press and decays on release, so the
        // card fades out instead of switching off instantly.
        void setLitAmount (float amount01) noexcept
        {
            const auto clamped = juce::jlimit (0.0f, 1.0f, amount01);
            if (std::abs (clamped - lit) > 0.001f)
            {
                lit = clamped;
                repaint();
            }
        }

        // "What could come next" hint: rank 1 (strongest) to 3, or 0 for none.
        // Drawn as a dashed outline (plus a NEXT tag on the best one) and
        // explained in the card's tooltip.
        void setSuggestion (int rank, const juce::String& reason)
        {
            rank = juce::jlimit (0, 3, rank);

            if (suggestionRank != rank)
            {
                suggestionRank = rank;
                repaint();
            }

            setTooltip (rank > 0 ? "Next? " + reason : juce::String());
        }

        // onPressStart/onPressEnd: left click/release, auditions the chord.
        // onEditRequested: right click (or ctrl-click on macOS), opens the
        // chord's context menu -- doesn't also fire onPressStart, so a right
        // click never sounds the chord.
        // onDragRequested: the mouse moved a few pixels with the button down;
        // the audition has already been released, and the owner should start
        // a drag-and-drop (e.g. onto the timeline).
        // onDoubleClicked: double-click (e.g. append the chord to the timeline).
        std::function<void()> onPressStart, onPressEnd, onEditRequested, onDragRequested, onDoubleClicked;

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (3.0f);

            if (! hasChord())
            {
                drawWell (g, bounds, 6.0f);
                return;
            }

            const auto lifted = bounds.translated (0.0f, -lit * 2.0f);
            drawCard (g, lifted, accent.brighter (lit * 0.30f));

            if (lit > 0.02f)
            {
                g.setColour (juce::Colours::white.withAlpha (lit * 0.55f));
                g.drawRoundedRectangle (lifted, 6.0f, 1.6f + lit * 1.2f);
            }

            if (suggestionRank > 0)
            {
                const float strength = suggestionRank == 1 ? 0.95f : (suggestionRank == 2 ? 0.60f : 0.38f);

                juce::Path outline;
                outline.addRoundedRectangle (lifted.expanded (1.0f), 7.0f);

                juce::Path dashed;
                const float dashes[] = { 5.0f, 4.0f };
                juce::PathStrokeType (1.6f).createDashedStroke (dashed, outline, dashes, 2);

                g.setColour (juce::Colours::white.withAlpha (strength));
                g.fillPath (dashed);

                if (suggestionRank == 1)
                {
                    const auto tag = juce::Rectangle<float> (lifted.getRight() - 40.0f, lifted.getBottom() - 15.0f, 34.0f, 11.0f);
                    g.setColour (juce::Colours::black.withAlpha (0.38f));
                    g.fillRoundedRectangle (tag, 3.0f);
                    drawCardText (g, "NEXT", tag, 8.0f);
                }
            }

            auto content = lifted.reduced (6.0f);
            auto keyRow = content.removeFromTop (14.0f);
            auto keyChip = keyRow.withWidth (16.0f);

            g.setColour (juce::Colours::black.withAlpha (0.32f));
            g.fillRoundedRectangle (keyChip, 3.0f);
            drawCardText (g, keyCap, keyChip, 10.0f);

            if (hasOverrideFlag)
            {
                const auto dot = juce::Rectangle<float> (lifted.getRight() - 12.0f, lifted.getY() + 5.0f, 5.0f, 5.0f);
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.fillEllipse (dot);
            }

            content.removeFromTop (2.0f);
            auto romanRow = content.removeFromTop (content.getHeight() * 0.58f);
            drawCardText (g, romanText, romanRow, juce::jmin (25.0f, romanRow.getHeight() * 0.75f));
            drawCardText (g, nameText, content, 12.5f);
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            if (! hasChord())
                return;

            dragStarted = false;

            if (event.mods.isPopupMenu())
            {
                if (onEditRequested != nullptr)
                    onEditRequested();
            }
            else if (onPressStart != nullptr)
            {
                onPressStart();
            }
        }

        void mouseDrag (const juce::MouseEvent& event) override
        {
            if (! hasChord() || dragStarted || event.mods.isPopupMenu())
                return;

            if (event.getDistanceFromDragStart() < 6)
                return;

            dragStarted = true;

            // Stop auditioning -- from here on it's a drag, not a key press.
            if (onPressEnd != nullptr)
                onPressEnd();

            if (onDragRequested != nullptr)
                onDragRequested();
        }

        void mouseDoubleClick (const juce::MouseEvent& event) override
        {
            if (hasChord() && ! event.mods.isPopupMenu() && onDoubleClicked != nullptr)
                onDoubleClicked();
        }

        void mouseUp (const juce::MouseEvent& event) override
        {
            // A right-click's mouseUp shouldn't stop a chord it never
            // started, and a drag has already released its audition.
            if (hasChord() && ! event.mods.isPopupMenu() && ! dragStarted && onPressEnd != nullptr)
                onPressEnd();

            dragStarted = false;
        }

    private:
        juce::String keyCap, romanText, nameText;
        juce::Colour accent = cyan;
        float lit = 0.0f;
        bool hasOverrideFlag = false;
        bool dragStarted = false;
        int suggestionRank = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordCard)
    };

    // Smaller, lighter text for combo boxes, text buttons and popup menus so
    // the compact layout doesn't look cramped.
    class CompactLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        juce::Font getComboBoxFont (juce::ComboBox& box) override
        {
            return homeUI::font (juce::jmin (12.5f, static_cast<float> (box.getHeight()) * 0.55f), false);
        }

        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        {
            return homeUI::font (juce::jmin (12.0f, static_cast<float> (buttonHeight) * 0.5f), false);
        }

        juce::Font getPopupMenuFont() override { return homeUI::font (13.0f, false); }
    };
}
