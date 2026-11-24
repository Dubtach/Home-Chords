#include <catch2/catch_test_macros.hpp>
#include "Preview/PreviewSynth.h"
#include <cmath>

namespace
{
    // Largest sample-to-sample step in the left channel: a click shows up as a
    // jump far bigger than any steady tone can produce.
    struct Rendered
    {
        float maxStep = 0.0f;
        float peak = 0.0f;
    };

    template <typename Events>
    Rendered render (int totalSamples, Events&& events)
    {
        preview::PreviewSynth synth;
        synth.prepare (48000.0, 256);
        synth.setGain (0.7f);

        juce::AudioBuffer<float> buffer (2, totalSamples);
        buffer.clear();

        int pos = 0;
        while (pos < totalSamples)
        {
            const int n = std::min (64, totalSamples - pos);
            events (synth, pos);
            synth.renderBlock (buffer, pos, n);
            pos += n;
        }

        Rendered r;
        const auto* d = buffer.getReadPointer (0);
        for (int i = 1; i < totalSamples; ++i)
        {
            r.maxStep = std::max (r.maxStep, std::abs (d[i] - d[i - 1]));
            r.peak = std::max (r.peak, std::abs (d[i]));
        }
        return r;
    }

    // A steady tone this loud steps by well under this per sample (about 0.01);
    // a phase jump or a cut-off is several times larger.
    constexpr float clickThreshold = 0.025f;
}

TEST_CASE ("retriggering a sounding note does not click", "[synth][clicks]")
{
    // Retrigger every 704 samples (not a whole number of cycles), so a restart
    // from phase zero would land mid-wave and step by a fifth of full scale.
    const auto r = render (48000, [] (preview::PreviewSynth& s, int pos)
    {
        if (pos == 0) s.noteOn (72, 1.0f);
        if (pos > 0 && pos % 704 == 0)
        {
            if ((pos / 704) % 2 == 0) s.noteOff (72);   // sometimes while it is already releasing
            s.noteOn (72, (pos / 704) % 3 == 0 ? 0.4f : 1.0f);
        }
    });

    CHECK (r.maxStep < clickThreshold);
    CHECK (r.peak < 1.0f);
}

TEST_CASE ("hammering one note every block does not click", "[synth][clicks]")
{
    const auto r = render (48000, [] (preview::PreviewSynth& s, int pos)
    {
        if (pos == 0) s.noteOn (60, 0.9f);
        if (pos > 0 && pos % 64 == 0)
        {
            s.noteOff (60);
            s.noteOn (60, 0.9f);
        }
    });

    CHECK (r.maxStep < clickThreshold);
}

TEST_CASE ("stealing voices (more notes than voices) does not click", "[synth][clicks]")
{
    const auto r = render (96000, [] (preview::PreviewSynth& s, int pos)
    {
        // A new 4-note chord every 64 samples, never released: far more than 32 voices.
        if (pos % 64 == 0)
        {
            const int root = 48 + (pos / 64) % 24;
            s.noteOn (root, 0.8f);
            s.noteOn (root + 4, 0.8f);
            s.noteOn (root + 7, 0.8f);
            s.noteOn (root + 11, 0.8f);
        }
    });

    CHECK (r.maxStep < 0.35f);   // dense chords are louder, but there must be no cliff-edge jump
    CHECK (r.peak <= 1.0f);      // the soft clipper keeps the mix in range
}

TEST_CASE ("a fast arpeggio's short notes do not click", "[synth][clicks]")
{
    const auto r = render (48000, [] (preview::PreviewSynth& s, int pos)
    {
        // 16th-note arp at 200 bpm: a note on/off every 900 samples, 70% gate.
        const int step = pos / 64;
        static constexpr int notes[] = { 48, 52, 55, 60, 64, 67 };
        const int period = 14;   // blocks per arp step (~900 samples)
        if (pos % 64 == 0 && step % period == 0)
            s.noteOn (notes[(step / period) % 6], 0.8f);
        if (pos % 64 == 0 && step % period == 10)
            s.noteOff (notes[(step / period) % 6]);
    });

    INFO ("maxStep " << r.maxStep);
    CHECK (r.maxStep < 0.045f);   // several overlapping releasing notes: louder than a single tone
}

TEST_CASE ("all notes off releases smoothly", "[synth][clicks]")
{
    const auto r = render (48000, [] (preview::PreviewSynth& s, int pos)
    {
        if (pos == 0) { s.noteOn (60, 1.0f); s.noteOn (64, 1.0f); s.noteOn (67, 1.0f); }
        if (pos == 9600) s.allNotesOff();
    });

    CHECK (r.maxStep < 0.06f);   // a 3-note chord at full velocity: louder, so a higher steady-state step
}
