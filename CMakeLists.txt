cmake_minimum_required(VERSION 3.22)
project(FreeqyDelay VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
include(FetchContent)
option(SPECTRAL_DIAGNOSTIC "Input FFT and diagnostic sine output" OFF)
if(JUCE_DIR)
 add_subdirectory(${JUCE_DIR} JUCE)
else()
 FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG d60b7a78030661574a615b586d9325ba7a7fa7dd GIT_SHALLOW FALSE)
 FetchContent_MakeAvailable(JUCE)
endif()
option(SPECTRAL_BUILD_AAX "Build the AAX Native format" OFF)
option(SPECTRAL_BUILD_CLAP "Build the native CLAP format" OFF)
set(AAX_SDK_PATH "" CACHE PATH "Optional external AAX SDK root (Interfaces and Libs)")
set(CLAP_JUCE_EXTENSIONS_DIR "" CACHE PATH "Optional clap-juce-extensions checkout")
set(SPECTRAL_VST3_INSTALL_DIR "/Library/Audio/Plug-Ins/VST3" CACHE PATH "System VST3 installation directory")
set(SPECTRAL_CLAP_INSTALL_DIR "/Library/Audio/Plug-Ins/CLAP" CACHE PATH "System CLAP installation directory")
set(SPECTRAL_AAX_INSTALL_DIR "/Library/Application Support/Avid/Audio/Plug-Ins" CACHE PATH "System AAX installation directory")
set(spectral_formats VST3 Standalone)
if(SPECTRAL_BUILD_AAX)
 if(AAX_SDK_PATH)
  juce_set_aax_sdk_path("${AAX_SDK_PATH}")
 endif()
 # JUCE 9 also provides a bundled SDK when no external path is supplied.
 list(APPEND spectral_formats AAX)
endif()
if(SPECTRAL_BUILD_CLAP)
 set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
 if(CLAP_JUCE_EXTENSIONS_DIR)
  add_subdirectory("${CLAP_JUCE_EXTENSIONS_DIR}" clap-juce-extensions EXCLUDE_FROM_ALL)
 else()
  FetchContent_Declare(clap_juce_extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG 6bde108329ff9fc24404b79b8b397261a75cf6a8
    GIT_SHALLOW FALSE
    GIT_SUBMODULES_RECURSE TRUE)
  FetchContent_MakeAvailable(clap_juce_extensions)
 endif()
endif()
# Keep the bundle and four-character codes stable: hosts use them to identify saved plug-in instances.
juce_add_plugin(FreeqyDelay COMPANY_NAME "Station Sound" BUNDLE_ID "com.dleon.freeqydelay" PLUGIN_MANUFACTURER_CODE Dleo PLUGIN_CODE Frqy FORMATS ${spectral_formats} AAX_CATEGORY Delay PRODUCT_NAME "FREEQY DELAY" IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE MICROPHONE_PERMISSION_ENABLED TRUE MICROPHONE_PERMISSION_TEXT "Analyze and process audio input")
target_sources(FreeqyDelay PRIVATE Source/Engine.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(FreeqyDelay PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 SPECTRAL_DIAGNOSTIC=$<BOOL:${SPECTRAL_DIAGNOSTIC}>)
target_link_libraries(FreeqyDelay PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
if(SPECTRAL_BUILD_CLAP)
 clap_juce_extensions_plugin(TARGET FreeqyDelay
   CLAP_ID "com.stationsound.freeqydelay"
   CLAP_FEATURES audio-effect delay stereo
   CLAP_USE_JUCE_PARAMETER_RANGES ALL)
 add_executable(ClapCheck Tests/ClapCheck.cpp)
 target_link_libraries(ClapCheck PRIVATE clap-core juce::juce_gui_basics ${CMAKE_DL_LIBS})
 add_dependencies(ClapCheck FreeqyDelay_CLAP)
 add_test(NAME clap_host COMMAND ClapCheck "$<TARGET_BUNDLE_DIR:FreeqyDelay_CLAP>")
endif()
enable_testing()
juce_add_console_app(DspTests PRODUCT_NAME "DSP Tests")
target_sources(DspTests PRIVATE Tests/Main.cpp Source/Engine.cpp)
target_include_directories(DspTests PRIVATE Source)
target_compile_definitions(DspTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(DspTests PRIVATE juce::juce_dsp)
add_test(NAME dsp COMMAND DspTests)
juce_add_console_app(HostCheck PRODUCT_NAME "VST3 Host Check")
target_sources(HostCheck PRIVATE Tests/HostCheck.cpp)
add_dependencies(HostCheck FreeqyDelay_VST3)
target_compile_definitions(HostCheck PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(HostCheck PRIVATE juce::juce_audio_processors)
if(SPECTRAL_DIAGNOSTIC)
 add_test(NAME vst3_host COMMAND HostCheck "$<TARGET_BUNDLE_DIR:FreeqyDelay_VST3>" --diagnostic)
else()
 add_test(NAME vst3_host COMMAND HostCheck "$<TARGET_BUNDLE_DIR:FreeqyDelay_VST3>")
endif()
add_executable(StateTests)
target_sources(StateTests PRIVATE Tests/StateTests.cpp)
target_include_directories(StateTests PRIVATE Source)
target_link_libraries(StateTests PRIVATE FreeqyDelay juce::juce_audio_utils juce::juce_dsp)
add_test(NAME bank_state COMMAND StateTests)
set_tests_properties(vst3_host bank_state PROPERTIES TIMEOUT 30)
if(SPECTRAL_BUILD_CLAP)
 set_tests_properties(clap_host PROPERTIES TIMEOUT 30)
endif()
set_tests_properties(dsp PROPERTIES TIMEOUT 120)

# Explicit installation is separate from compilation; never require root to build.
if(APPLE)
 # JUCE emits moduleinfo.json after its initial signing step. Seal the finished bundle.
 add_custom_command(TARGET FreeqyDelay_VST3 POST_BUILD
   COMMAND /usr/bin/codesign --force --sign - --timestamp=none "$<TARGET_BUNDLE_DIR:FreeqyDelay_VST3>"
   COMMENT "Ad-hoc signing the completed VST3 bundle" VERBATIM)
 install(DIRECTORY "$<TARGET_BUNDLE_DIR:FreeqyDelay_VST3>"
         DESTINATION "${SPECTRAL_VST3_INSTALL_DIR}" USE_SOURCE_PERMISSIONS COMPONENT VST3)
 install(CODE "
   set(_spectral_vst3_dir [==[${SPECTRAL_VST3_INSTALL_DIR}]==])
   if(NOT IS_ABSOLUTE \"\${_spectral_vst3_dir}\")
     set(_spectral_vst3_dir \"\${CMAKE_INSTALL_PREFIX}/\${_spectral_vst3_dir}\")
   endif()
   set(_spectral_installed_vst3 \"\$ENV{DESTDIR}\${_spectral_vst3_dir}/FREEQY DELAY.vst3\")
   execute_process(
     COMMAND /usr/bin/xattr -dr com.apple.quarantine \"\${_spectral_installed_vst3}\"
     RESULT_VARIABLE _spectral_xattr_result
     ERROR_VARIABLE _spectral_xattr_error)
   if(NOT _spectral_xattr_result EQUAL 0)
     message(FATAL_ERROR
       \"Failed to clear quarantine from \${_spectral_installed_vst3}: \${_spectral_xattr_error}\")
   endif()
   message(STATUS \"Cleared quarantine from \${_spectral_installed_vst3}\")
 " COMPONENT VST3)
 if(SPECTRAL_BUILD_CLAP)
  add_custom_command(TARGET FreeqyDelay_CLAP POST_BUILD
    COMMAND /usr/bin/codesign --force --sign - --timestamp=none
            "$<TARGET_BUNDLE_DIR:FreeqyDelay_CLAP>"
    COMMENT "Ad-hoc signing the completed CLAP bundle" VERBATIM)
  add_custom_target(FreeqyDelay_CLAP_LocalReady
    COMMAND /usr/bin/xattr -dr com.apple.quarantine
            "$<TARGET_BUNDLE_DIR:FreeqyDelay_CLAP>"
    DEPENDS FreeqyDelay_CLAP
    COMMENT "Clearing quarantine from the local CLAP bundle" VERBATIM)
  add_dependencies(ClapCheck FreeqyDelay_CLAP_LocalReady)
  install(DIRECTORY "$<TARGET_BUNDLE_DIR:FreeqyDelay_CLAP>"
          DESTINATION "${SPECTRAL_CLAP_INSTALL_DIR}" USE_SOURCE_PERMISSIONS COMPONENT CLAP)
  install(CODE "
    set(_spectral_clap_dir [==[${SPECTRAL_CLAP_INSTALL_DIR}]==])
    if(NOT IS_ABSOLUTE \"\${_spectral_clap_dir}\")
      set(_spectral_clap_dir \"\${CMAKE_INSTALL_PREFIX}/\${_spectral_clap_dir}\")
    endif()
    set(_spectral_installed_clap \"\$ENV{DESTDIR}\${_spectral_clap_dir}/FREEQY DELAY.clap\")
    execute_process(
      COMMAND /usr/bin/xattr -dr com.apple.quarantine \"\${_spectral_installed_clap}\"
      RESULT_VARIABLE _spectral_clap_xattr_result
      ERROR_VARIABLE _spectral_clap_xattr_error)
    if(NOT _spectral_clap_xattr_result EQUAL 0)
      message(FATAL_ERROR
        \"Failed to clear quarantine from \${_spectral_installed_clap}: \${_spectral_clap_xattr_error}\")
    endif()
    message(STATUS \"Cleared quarantine from \${_spectral_installed_clap}\")
  " COMPONENT CLAP)
 endif()
 if(SPECTRAL_BUILD_AAX)
  install(DIRECTORY "$<TARGET_BUNDLE_DIR:FreeqyDelay_AAX>"
          DESTINATION "${SPECTRAL_AAX_INSTALL_DIR}" USE_SOURCE_PERMISSIONS COMPONENT AAX)
 endif()
endif()
