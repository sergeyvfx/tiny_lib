# Copyright (c) 2022 tiny lib authors
#
# SPDX-License-Identifier: MIT

# A shortcut for building/testing/installing pipeline.
#
# Allows to type `make` with the same arguments on both Windows and Unix-like
# systems.
# This script takes care of Unix-like systems.
#
# Targets:
#
#   release, test, install - Build, test, and install the project in the relase
#                            build configuration.
#
#   debug, debug-test, debug-install - Build, test, and install the project in
#                                      the relase build configuration.
#
# Configuration variables:
#
#   DEVELOPER=1 - Enable all configurations used during development.
#                 This includes address sanitizer, extra strict compiler flags.
#
#                 NOTE: Only value of 1 is recognized as the flag enabled.
#
#   INSTALL_PREFIX=<prefix> - Use given path as a prefix for installation.
#
#   BUILD_DIR=<build directory> - Override automatically chosen build directory.

# ==============================================================================
# Initial variables and configuration setup.
# ==============================================================================

ifndef CMAKE_ARGS
	CMAKE_ARGS :=
endif

ifndef BUILD_DIR_RELEASE
	BUILD_DIR_RELEASE := $(CURDIR)/build-release
endif

ifndef BUILD_DIR_DEBUG
	BUILD_DIR_DEBUG := $(CURDIR)/build-debug
endif

CMAKE_GENERATOR_ARGS :=
CMAKE_DEVELOPER_ARGS :=
CMAKE_INSTALL_ARGS :=

# ==============================================================================
# Parse arguments.
# ==============================================================================

# Default to ninja build system if it is available.
ifneq (, $(shell which ninja))
  CMAKE_GENERATOR_ARGS += -G Ninja
endif

# Enable extra flags for development.
ifdef DEVELOPER
  ifeq ($(DEVELOPER),1)
    CMAKE_DEVELOPER_ARGS += -D WITH_DEVELOPER_SANITIZER=ON
    CMAKE_DEVELOPER_ARGS += -D WITH_DEVELOPER_STRICT=ON
  endif
endif

ifdef INSTALL_PREFIX
  CMAKE_INSTALL_ARGS += -D CMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX)
endif

# ==============================================================================
# Common utilities.
# ==============================================================================

# Compile target using configured build system.
# Will check whether it is make or ninja which is to used by the project and
# invoke corresponding command with the first macro argument passes as a name
# of the target.
define compile-target
	@# Check whether the project is to be compiled with Ninja or make.
	@if [ -f $($@_BUILD_DIR)/build.ninja ]; then \
		ninja -C $($@_BUILD_DIR) $(2); \
	elif [ -f $($@_BUILD_DIR)/Makefile ]; then \
		$(MAKE) -C $($@_BUILD_DIR) --no-print-directory $(2); \
	fi
endef

# ==============================================================================
# Targets initialization.
# ==============================================================================

.PHONY: all
all: release

# ==============================================================================
# Configure the project.
# ==============================================================================
#
# The configure happens on every run of `make`. While this seems to be an
# unnecessary step which only slows overall process down there are some good
# reasons for it:
#
#   - This allows to tweak CMake options even after the project was generated.
#
#   - Behavior is consistent with the make.bat, where the re-configuration is
#     needed due to even more reasons.
#
# If faster iteration is needed it is always possible to cd into the build
# directory and type commands in there.

define configure-project
	$(eval $@_BUILD_DIR := $1)
	$(eval $@_BUILD_TYPE := $2)

 	@echo
	@echo "**********************************************************************"
	@echo "** Configuring the project for $($@_BUILD_TYPE) ..."
	@echo "**********************************************************************"
	@echo

	@mkdir -p $($@_BUILD_DIR)

	@cmake -B $($@_BUILD_DIR) \
	       -D CMAKE_BUILD_TYPE=$($@_BUILD_TYPE) \
	          $(CMAKE_ARGS) \
	          $(CMAKE_GENERATOR_ARGS) \
	          $(CMAKE_DEVELOPER_ARGS) \
	          $(CMAKE_INSTALL_ARGS) \
	          $(CURDIR)

 	@echo
 	@echo Configuration finished.
endef

# ==============================================================================
# Perform build of the current configuration.
# ==============================================================================

define build-project
	$(eval $@_BUILD_DIR := $1)
	$(eval $@_BUILD_TYPE := $2)

 	@echo
 	@echo "**********************************************************************"
 	@echo "** Building the project for $($@_BUILD_TYPE) ..."
 	@echo "**********************************************************************"
 	@echo

 	+$(call compile-target,$($@_BUILD_DIR))

 	@echo
 	@echo Build succeeded.
endef

# ==============================================================================
# Regression tests.
# ==============================================================================

# TODO(sergey): With CMake 3.20 simplify to `@ctest --test-dir $(BUILD_DIR)`

define test-project
	$(eval $@_BUILD_DIR := $1)
	$(eval $@_BUILD_TYPE := $2)

	@echo
	@echo "**********************************************************************"
	@echo "** Running regression tests for $($@_BUILD_TYPE) ..."
	@echo "**********************************************************************"
	@echo

	@cd $($@_BUILD_DIR) && ctest

	@echo
	@echo Tests passed.
endef

# ==============================================================================
# Installation.
# ==============================================================================

define install-project
	$(eval $@_BUILD_DIR := $1)
	$(eval $@_BUILD_TYPE := $2)

	@echo
	@echo "**********************************************************************"
	@echo "** Installing the project for $($@_BUILD_TYPE) ..."
	@echo "**********************************************************************"
	@echo

	+$(call compile-target,install)

	@echo
	@echo Installation completed.
endef

# ==============================================================================
# Targets for RELEASE build type.
# ==============================================================================

.PHONY: configure
configure:
	$(call configure-project,$(BUILD_DIR_RELEASE),Release)

.PHONY: build
build: | configure
	$(call build-project,$(BUILD_DIR_RELEASE),Release)

.PHONY: release
release: configure build

.PHONY: test
test: release
	$(call test-project,$(BUILD_DIR_RELEASE),Release)

.PHONY: install
install: release
	$(call install-project,$(BUILD_DIR_RELEASE),Release)

# ==============================================================================
# Targets for DEBUG build type.
# ==============================================================================

.PHONY: debug-configure
debug-configure:
	$(call configure-project,$(BUILD_DIR_DEBUG),Debug)

.PHONY: build-debug
debug-build: | debug-configure
	$(call build-project,$(BUILD_DIR_DEBUG),Debug)

.PHONY: debug
debug: debug-configure debug-build

.PHONY: debug-test
debug-test: debug
	$(call test-project,$(BUILD_DIR_DEBUG),Debug)

.PHONY: debug-install
debug-install: debug
	$(call install-project,$(BUILD_DIR_DEBUG),Debug)


# ==============================================================================
# Targets for backup.
# ==============================================================================

backup:
	@7z a tiny_lib-`date +"%Y-%m-%dT%H-%M-%S"`.7z $(CURDIR)/. \
	  -xr!build-* \
	  -x!test/data/aprs/tnc_test_cd/*.wav
