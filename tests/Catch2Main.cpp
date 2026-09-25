// All test files are included in the executable via target_sources() in
// tests/CMakeLists.txt.

#include "juce_gui_basics/juce_gui_basics.h"
#include <catch2/catch_session.hpp>

int main (int argc, char* argv[])
{
    // Lets tests safely touch anything that expects a MessageManager
    // (ValueTree change broadcasting, juce::Uuid, etc.) without leaking.
    juce::ScopedJuceInitialiser_GUI gui;

    const int result = Catch::Session().run (argc, argv);

    return result;
}
