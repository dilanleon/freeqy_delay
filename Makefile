# Wrapper around CMake. AAX is opt-in: pass AAX=1 when ready.
.DEFAULT_GOAL := all
CMAKE ?= $(if $(wildcard .deps/venv/bin/cmake),.deps/venv/bin/cmake,cmake)
JUCE_DIR ?= $(if $(wildcard .deps/JUCE/CMakeLists.txt),$(CURDIR)/.deps/JUCE,)
BUILD_DIR ?= build-system
CONFIG ?= Release
JOBS ?= 4
LOCAL_AAX_HEADERS := $(wildcard aax-sdk*/Interfaces/AAX.h)
AAX_SDK_PATH ?= $(if $(filter 1,$(words $(LOCAL_AAX_HEADERS))),$(patsubst %/Interfaces/AAX.h,%,$(LOCAL_AAX_HEADERS)),)
AAX ?= 0
VST3_DIR ?= /Library/Audio/Plug-Ins/VST3
AAX_DIR ?= /Library/Application Support/Avid/Audio/Plug-Ins

.PHONY: all configure vst3 aax install install-vst3 install-aax help

all: configure
	"$(CMAKE)" --build "$(BUILD_DIR)" --config "$(CONFIG)" --parallel "$(JOBS)" --target FreeqyDelay_VST3 $(if $(filter 1,$(AAX)),FreeqyDelay_AAX,)

configure:
	"$(CMAKE)" -S . -B "$(BUILD_DIR)" -DCMAKE_BUILD_TYPE="$(CONFIG)" -DJUCE_DIR="$(JUCE_DIR)" -DSPECTRAL_DIAGNOSTIC=OFF -DSPECTRAL_BUILD_AAX=$(if $(filter 1,$(AAX)),ON,OFF) -DAAX_SDK_PATH="$(AAX_SDK_PATH)" -DSPECTRAL_VST3_INSTALL_DIR="$(VST3_DIR)" -DSPECTRAL_AAX_INSTALL_DIR="$(AAX_DIR)"

vst3: configure
	"$(CMAKE)" --build "$(BUILD_DIR)" --config "$(CONFIG)" --parallel "$(JOBS)" --target FreeqyDelay_VST3

aax: configure
	@test "$(AAX)" = 1 || { echo 'AAX disabled: use make aax AAX=1, optionally AAX_SDK_PATH="/path/to/sdk"'; exit 1; }
	"$(CMAKE)" --build "$(BUILD_DIR)" --config "$(CONFIG)" --parallel "$(JOBS)" --target FreeqyDelay_AAX

install: all
	"$(CMAKE)" --install "$(BUILD_DIR)" --config "$(CONFIG)" --component VST3
ifeq ($(AAX),1)
	"$(CMAKE)" --install "$(BUILD_DIR)" --config "$(CONFIG)" --component AAX
endif

install-vst3: vst3
	"$(CMAKE)" --install "$(BUILD_DIR)" --config "$(CONFIG)" --component VST3

install-aax: aax
	"$(CMAKE)" --install "$(BUILD_DIR)" --config "$(CONFIG)" --component AAX

help:
	@echo 'make                         Build Release VST3 (AAX disabled by default)'
	@echo 'make install                 Build and copy enabled formats to system plugin folders'
	@echo 'make vst3 / make install-vst3  Build / install VST3 only'
	@echo 'make aax AAX=1               Build AAX (bundled SDK when no external path is selected)'
	@echo 'make install AAX=1 AAX_SDK_PATH="/path/to/sdk"  Build/install using an external AAX SDK'
	@echo 'Override CONFIG, BUILD_DIR, JOBS, VST3_DIR, AAX_DIR as needed.'
	@echo 'If installation is denied, run only the printed cmake --install commands with sudo.'
	@echo 'Unsigned AAX requires Pro Tools Developer; retail Pro Tools requires PACE signing.'
