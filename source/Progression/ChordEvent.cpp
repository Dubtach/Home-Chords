#include "ChordEvent.h"

namespace progression
{
    juce::ValueTree ChordEvent::toValueTree() const
    {
        juce::ValueTree tree ("CHORD_EVENT");

        tree.setProperty ("id", id.toString(), nullptr);
        tree.setProperty ("scaleDegree", scaleDegree, nullptr);
        tree.setProperty ("rootPitchClass", rootPitchClass, nullptr);
        tree.setProperty ("quality", static_cast<int> (quality), nullptr);
        tree.setProperty ("extension", static_cast<int> (extension), nullptr);
        tree.setProperty ("startBeats", startBeats, nullptr);
        tree.setProperty ("lengthBeats", lengthBeats, nullptr);
        tree.setProperty ("velocity", velocity, nullptr);
        tree.setProperty ("octaveOffset", octaveOffset, nullptr);
        tree.setProperty ("enabled", enabled, nullptr);
        tree.setProperty ("rhythm", static_cast<int> (rhythm), nullptr);
        tree.setProperty ("arpMode", static_cast<int> (arpMode), nullptr);
        tree.setProperty ("arpRateBeats", arpRateBeats, nullptr);
        tree.setProperty ("arpGate", arpGate, nullptr);
        tree.setProperty ("bassMode", static_cast<int> (bassMode), nullptr);
        tree.setProperty ("customBassPitchClass", customBassPitchClass, nullptr);

        return tree;
    }

    ChordEvent ChordEvent::fromValueTree (const juce::ValueTree& tree)
    {
        ChordEvent event;

        const auto idString = tree.getProperty ("id").toString();
        event.id = idString.isNotEmpty() ? juce::Uuid (idString) : juce::Uuid();

        event.scaleDegree    = static_cast<int> (tree.getProperty ("scaleDegree", 0));
        event.rootPitchClass = static_cast<int> (tree.getProperty ("rootPitchClass", 0));
        event.quality        = static_cast<musictheory::ChordQuality> (static_cast<int> (tree.getProperty ("quality", 0)));
        event.extension      = static_cast<musictheory::Extension> (static_cast<int> (tree.getProperty ("extension", 0)));
        event.startBeats     = static_cast<double> (tree.getProperty ("startBeats", 0.0));
        event.lengthBeats    = static_cast<double> (tree.getProperty ("lengthBeats", 4.0));
        event.velocity       = static_cast<int> (tree.getProperty ("velocity", 100));
        event.octaveOffset   = static_cast<int> (tree.getProperty ("octaveOffset", 0));
        event.enabled        = static_cast<bool> (tree.getProperty ("enabled", true));
        event.rhythm         = static_cast<RhythmPattern> (static_cast<int> (tree.getProperty ("rhythm", 0)));
        event.arpMode         = static_cast<ArpMode> (static_cast<int> (tree.getProperty ("arpMode", 0)));
        event.arpRateBeats    = static_cast<double> (tree.getProperty ("arpRateBeats", 0.25));
        event.arpGate         = static_cast<double> (tree.getProperty ("arpGate", 0.8));
        event.bassMode        = static_cast<BassMode> (static_cast<int> (tree.getProperty ("bassMode", 3)));
        event.customBassPitchClass = static_cast<int> (tree.getProperty ("customBassPitchClass", 0));

        return event;
    }
}
