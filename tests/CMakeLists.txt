# -----------------------------------------------------------------------------
# This builds the same way Home-Sidechain's tests/ folder is *meant* to (per
# its CLAUDE.md: a Catch2-based Tests target discovered via ctest), but
# without depending on the private sudara/cmake-includes submodule (its
# pamplejuce_add_tests() helper) since that submodule's contents weren't part
# of what was available when this project was generated. This CPM+Catch2
# setup is a well-established, standalone equivalent -- swap it for
# pamplejuce_add_tests() later if you want to match the other Home-* repos
# exactly.
#
# Requires network access at CMake configure time (to fetch CPM.cmake itself,
# then Catch2 through it).
# -----------------------------------------------------------------------------

set(CPM_DOWNLOAD_VERSION 0.40.2)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD
         https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
         ${CPM_DOWNLOAD_LOCATION}
    )
endif()

include(${CPM_DOWNLOAD_LOCATION})

CPMAddPackage(
    NAME Catch2
    GITHUB_REPOSITORY catchorg/Catch2
    GIT_TAG v3.7.1
)

add_executable(Tests
    Catch2Main.cpp
    MusicTheoryTests.cpp
    ProgressionModelTests.cpp
)

target_include_directories(Tests PRIVATE ${CMAKE_SOURCE_DIR}/source)

# Only the plain-C++ layers (music theory + progression model) are tested
# directly -- PluginProcessor/PluginEditor need a full JUCE plugin host
# context and aren't included here.
target_sources(Tests PRIVATE
    ${CMAKE_SOURCE_DIR}/source/MusicTheory/Scale.cpp
    ${CMAKE_SOURCE_DIR}/source/MusicTheory/Chord.cpp
    ${CMAKE_SOURCE_DIR}/source/MusicTheory/MusicTheoryEngine.cpp
    ${CMAKE_SOURCE_DIR}/source/Progression/ChordEvent.cpp
    ${CMAKE_SOURCE_DIR}/source/Progression/ProgressionModel.cpp
)

target_link_libraries(Tests PRIVATE
    Catch2::Catch2
    juce::juce_core
    juce::juce_data_structures
    juce::juce_gui_basics
)

target_compile_features(Tests PRIVATE cxx_std_20)

include(CTest)

# Tell CMake where to find Catch.cmake inside the downloaded Catch2 package
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)

include(Catch)
catch_discover_tests(Tests DISCOVERY_MODE PRE_TEST)