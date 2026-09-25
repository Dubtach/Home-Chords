cmake_minimum_required(VERSION 3.25)

set(PROJECT_NAME "HomeChords")

project(${PROJECT_NAME} VERSION 0.1.0 LANGUAGES C CXX)

set(PRODUCT_NAME "Home-Chords")
set(COMPANY_NAME "Dubtach")
set(BUNDLE_ID "com.dubtach.homechords")
set(FORMATS Standalone VST3)

# Keep JUCE as the normal Pamplejuce git submodule in the repository root,
# same as Home-Disto / Home-Sidechain. If you're starting this checkout
# fresh:
#   git submodule add https://github.com/juce-framework/JUCE.git JUCE
#   git -C JUCE checkout develop
add_subdirectory(JUCE)

juce_add_plugin("${PROJECT_NAME}"
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"
    COPY_PLUGIN_AFTER_BUILD TRUE

    # A four-character plugin id (first character must be uppercase for AU)
    PLUGIN_MANUFACTURER_CODE Dubt

    # A unique four-character plugin id
    PLUGIN_CODE Hchd

    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Instrument Synth

    FORMATS ${FORMATS}
    PRODUCT_NAME "${PRODUCT_NAME}"
)

juce_generate_juce_header("${PROJECT_NAME}")

target_sources("${PROJECT_NAME}" PRIVATE
    source/PluginProcessor.h
    source/PluginProcessor.cpp
    source/PluginEditor.h
    source/PluginEditor.cpp

    source/MusicTheory/MusicTheoryTypes.h
    source/MusicTheory/Scale.h
    source/MusicTheory/Scale.cpp
    source/MusicTheory/Chord.h
    source/MusicTheory/Chord.cpp
    source/MusicTheory/MusicTheoryEngine.h
    source/MusicTheory/MusicTheoryEngine.cpp

    source/Midi/ChordKeyboardEngine.h
    source/Midi/ChordKeyboardEngine.cpp

    source/Preview/PreviewSynth.h
    source/Preview/PreviewSynth.cpp

    source/Progression/ChordEvent.h
    source/Progression/ChordEvent.cpp
    source/Progression/ProgressionModel.h
    source/Progression/ProgressionModel.cpp

    source/Shared/HomeSeriesUI.h
)

target_include_directories("${PROJECT_NAME}" PRIVATE source)

target_link_libraries("${PROJECT_NAME}" PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
)

target_compile_definitions("${PROJECT_NAME}" PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# ---------------------------------------------------------------------------
# Tests (Catch2, fetched via CPM at configure time -- needs network access).
# Real coverage of the music-theory layer and the progression data model;
# see tests/README in spirit via the test file comments themselves.
# ---------------------------------------------------------------------------
option(HOMECHORDS_BUILD_TESTS "Build the Home-Chords Tests target" ON)

if(HOMECHORDS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
