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
        tree.setProperty ("startBeats", startBeats, nullptr);
        tree.setProperty ("lengthBeats", lengthBeats, nullptr);
        tree.setProperty ("velocity", velocity, nullptr);
        tree.setProperty ("octaveOffset", octaveOffset, nullptr);
        tree.setProperty ("enabled", enabled, nullptr);

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
        event.startBeats     = static_cast<double> (tree.getProperty ("startBeats", 0.0));
        event.lengthBeats    = static_cast<double> (tree.getProperty ("lengthBeats", 4.0));
        event.velocity       = static_cast<int> (tree.getProperty ("velocity", 100));
        event.octaveOffset   = static_cast<int> (tree.getProperty ("octaveOffset", 0));
        event.enabled        = static_cast<bool> (tree.getProperty ("enabled", true));

        return event;
    }
}
