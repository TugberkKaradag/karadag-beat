cmake_minimum_required(VERSION 3.22)

project(KaradagBeat VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 8.0.10 kaynak agaci.  Yerelde indirilmis kopya kullanilir; yoksa (ornegin
# GitHub Actions'ta) ayni surum GitHub'dan cekilir.
set(JUCE_DIR "C:/Users/user/Downloads/juce-8.0.10-windows/JUCE" CACHE PATH "JUCE kaynak klasoru")

if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.10
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(KaradagBeat
    COMPANY_NAME            "Karadag"
    BUNDLE_ID               "com.karadag.karadagbeat"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Krdg
    PLUGIN_CODE             Kbt1
    FORMATS                 VST3 Standalone
    VST3_CATEGORIES         "Fx" "Modulation"
    PRODUCT_NAME            "Karadag Beat")

target_sources(KaradagBeat PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Envelope.cpp
    Source/GrossEngine.cpp
    Source/Presets.cpp
    Source/Swing.cpp
    Source/EnvelopeEditor.cpp
    Source/Theme.cpp)

target_compile_definitions(KaradagBeat PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(KaradagBeat PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

#===============================================================================
# DSP dogrulama testleri (host olmadan calisan konsol uygulamasi)
#===============================================================================
juce_add_console_app(DspTest PRODUCT_NAME "DspTest")

target_sources(DspTest PRIVATE
    Tests/DspTest.cpp
    Source/Envelope.cpp
    Source/GrossEngine.cpp
    Source/Presets.cpp
    Source/Swing.cpp)

target_compile_definitions(DspTest PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(DspTest PRIVATE
    juce::juce_audio_basics
  PUBLIC
    juce::juce_recommended_config_flags)

#===============================================================================
# Pattern gorsellerini PNG olarak ureten yardimci (GUI dogrulamasi)
#===============================================================================
juce_add_console_app(PresetRender PRODUCT_NAME "PresetRender")

target_sources(PresetRender PRIVATE
    Tests/PresetRender.cpp
    Source/Envelope.cpp
    Source/Presets.cpp
    Source/EnvelopeEditor.cpp
    Source/Theme.cpp)

target_compile_definitions(PresetRender PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(PresetRender PRIVATE
    juce::juce_gui_basics
  PUBLIC
    juce::juce_recommended_config_flags)

#===============================================================================
# Proje durumu (kaydet/ac) ve kanal duzeni testleri
# Eklenti hedefine link ederek JucePlugin_* tanimlarini ve paylasilan kodu alir.
#===============================================================================
juce_add_console_app(StateTest PRODUCT_NAME "StateTest")

target_sources(StateTest PRIVATE Tests/StateTest.cpp)

target_compile_definitions(StateTest PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(StateTest PRIVATE
    KaradagBeat
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags)

#===============================================================================
# Eklenti penceresini ekran disinda PNG'ye cizen yardimci (arayuz duzeni kontrolu)
#===============================================================================
juce_add_console_app(EditorRender PRODUCT_NAME "EditorRender")

target_sources(EditorRender PRIVATE Tests/EditorRender.cpp)

target_compile_definitions(EditorRender PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(EditorRender PRIVATE
    KaradagBeat
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags)

#===============================================================================
# Hizli calmada aliasing olcumu (FFT)
#===============================================================================
juce_add_console_app(AliasTest PRODUCT_NAME "AliasTest")

target_sources(AliasTest PRIVATE
    Tests/AliasTest.cpp
    Source/Envelope.cpp
    Source/GrossEngine.cpp)

target_compile_definitions(AliasTest PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(AliasTest PRIVATE
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags)
