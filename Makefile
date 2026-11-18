# Wrapper around CMake. AAX is opt-in: pass AAX=1 when ready.
.DEFAULT_GOAL := all
CMAKE ?= $(if $(wildcard .deps/venv/bin/cmake),.deps/venv/bin/cmake,cmake)
JUCE_DIR ?= $(if $(wildcard .deps/JUCE/CMakeLists.txt),$(CURDIR)/.deps/JUCE,)
CLAP_JUCE_EXTENSIONS_DIR ?= $(if $(wildcard .deps/clap-juce-extensions/CMakeLists.txt),$(CURDIR)/.deps/clap-juce-extensions,)
BUILD_DIR ?= build-system
CONFIG ?= Release
JOBS ?= 4
LOCAL_AAX_HEADERS := $(wildcard aax-sdk*/Interfaces/AAX.h)
AAX_SDK_PATH ?= $(if $(filter 1,$(words $(LOCAL_AAX_HEADERS))),$(patsubst %/Interfaces/AAX.h,%,$(LOCAL_AAX_HEADERS)),)
AAX ?= 0
CLAP ?= 0
VST3_DIR ?= /Library/Audio/Plug-Ins/VST3
CLAP_DIR ?= /Library/Audio/Plug-Ins/CLAP
AAX_DIR ?= /Library/Application Support/Avid/Audio/Plug-Ins

.PHONY: all configure vst3 clap aax install install-vst3 install-clap install-aax help

all: configure
	"$(CMAKE)" --build "$(BUILD_DIR)" --config "$(CONFIG)" --parallel "$(JOBS)" --target FreeqyDelay_VST3 $(if $(filter 1,$(CLAP)),FreeqyDelay_CLAP_LocalReady,) $(if $(filter 1,$(AAX)),FreeqyDelay_AAX,)

configure:
	"$(CMAKE)" -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(CONFIG)" -DJUCE_DIR="$(JUCE_DIR)" -DSPECTRAL_BUILD_CLAP=$(if $(filter 1,$(CLAP)),ON,OFF) -DCLAP_JUCE_EXTENSIONS_DIR="$(CLAP_JUCE_EXTENSIONS_DIR)" -DSPECTRAL_BUILD_AAX=$(if $(filter 1,$(AAX)),ON,OFF) -DAAX_SDK_PATH="$(AAX_SDK_PATH)" -DSPECTRAL_VST3_INSTALL_DIR="$(VST3_DIR)" -DSPECTRAL_CLAP_INSTALL_DIR="$(CLAP_DIR)" -DSPECTRAL_AAX_INSTALL_DIR="$(AAX_DIR)"

vst3: configure
	"$(CMAKE)" --build "$(BUILD_DIR)" --config "$(CONFIG)" --parallel "$(JOBS)" --target FreeqyDelay_VST3

clap: CLAP=1
clap: configure
	"$(CMAKE)" --build "$(BUILD_DIR)" --config "$(CONFIG)" --parallel "$(JOBS)" --target FreeqyDelay_CLAP_LocalReady

aax: configure
	@test "$(AAX)" = 1 || { echo 'AAX disabled: use make aax AAX=1, optionally AAX_SDK_PATH="/path/to/sdk"'; exit 1; }
	"$(CMAKE)" --build "$(BUILD_DIR)" --config "$(CONFIG)" --parallel "$(JOBS)" --target FreeqyDelay_AAX

install: all
	"$(CMAKE)" --install "$(BUILD_DIR)" --config "$(CONFIG)" --component VST3
ifeq ($(CLAP),1)
	"$(CMAKE)" --install "$(BUILD_DIR)" --config "$(CONFIG)" --component CLAP
endif
ifeq ($(AAX),1)
	"$(CMAKE)" --install "$(BUILD_DIR)" --config "$(CONFIG)" --component AAX
endif

install-vst3: vst3
	"$(CMAKE)" --install "$(BUILD_DIR)" --config "$(CONFIG)" --component VST3

install-clap: CLAP=1
install-clap: clap
	"$(CMAKE)" --install "$(BUILD_DIR)" --config "$(CONFIG)" --component CLAP

install-aax: aax
	"$(CMAKE)" --install "$(BUILD_DIR)" --config "$(CONFIG)" --component AAX

help:
	@echo 'make                         Build Release VST3 (AAX disabled by default)'
	@echo 'make install                 Build/install enabled formats; unquarantine VST3/CLAP copies'
	@echo 'make vst3 / make install-vst3  Build / install and unquarantine VST3 only'
	@echo 'make clap / make install-clap  Build / install and unquarantine native CLAP'
	@echo 'make aax AAX=1               Build AAX (bundled SDK when no external path is selected)'
	@echo 'make install AAX=1 AAX_SDK_PATH="/path/to/sdk"  Build/install using an external AAX SDK'
	@echo 'Override CONFIG, BUILD_DIR, JOBS, VST3_DIR, CLAP_DIR, AAX_DIR as needed.'
	@echo 'If installation is denied, run only the printed cmake --install commands with sudo.'
	@echo 'Unsigned AAX requires Pro Tools Developer; retail Pro Tools requires PACE signing.'
