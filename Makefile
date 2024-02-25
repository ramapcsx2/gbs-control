ARDUINO_CLI_VERSION := 0.35.3
ARDUINO_CLI_URL := https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh
ARDUINO_LINT_VERSION := 1.2.1
ARDUINO_LINT_URL := https://raw.githubusercontent.com/arduino/arduino-lint/main/etc/install.sh

SKETCH := gbs-control
BOARD := nodemcuv2
PORT := /dev/ttyACM0

ADDITIONAL_URLS := \
	https://arduino.esp8266.com/stable/package_esp8266com_index.json
CORE := esp8266:esp8266@2.7.3
LIBRARIES := \
	"ESP8266 and ESP32 OLED driver for SSD1306 displays"@4.4.1 \
	"ESP8266-ping"@2.0.1
LIBRARIES_URLS := \
	https://github.com/me-no-dev/ESPAsyncTCP \
	https://github.com/me-no-dev/ESPAsyncWebServer

# The FQBN is the core, followed by the board and other build options.
CORE_NAME := $(shell echo $(CORE) | cut -f1 -d@)
FQBN := $(CORE_NAME):$(BOARD):xtal=160,eesz=4M1M,ip=lm2f

# Treat all warnings as errors.
BUILDPROP := compiler.warning_flags.all='-Wall -Wextra'

################################################################################

ROOT := $(PWD)
BINDIR := $(ROOT)/bin
ETCDIR := $(ROOT)/etc
SRCDIR := $(ROOT)
VARDIR := $(ROOT)/var
LOGDIR := $(VARDIR)/log
BUILDDIR := $(VARDIR)/build
CACHEDIR := $(BUILDDIR)/cache

# Build a list of source files for dependency management.
SRCS := $(shell find $(SRCDIR) -name "*.ino" -or -name "*.cpp" -or -name "*.c" -or -name "*.h")

# Compute version string from git sandbox status
VERSION_STRING := $(shell git describe --always --tags --dirty)

# Set the location of the Arduino environment.
export ARDUINO_DATA_DIR = $(VARDIR)

################################################################################

.PHONY: all help env print-core-version properties lint build upload clean clean-all

all: build

################################################################################

##
# Tell the user what the targets are.
##

help:
	@echo
	@echo "Targets:"
	@echo "   env        Install the Arduino CLI environment."
	@echo "   properties Show all build properties used instead of compiling."
	@echo "   lint       Validate the sketch with arduino-lint."
	@echo "   build      Compile the sketch."
	@echo "   upload     Upload to the board."
	@echo "   clean      Remove only files ignored by Git."
	@echo "   clean-all  Remove all untracked files."
	@echo

################################################################################

# Run in --silent mode unless the user sets VERBOSE=1 on the
# command-line.

ifndef VERBOSE
.SILENT:
else
ARGS_VERBOSE := --verbose
endif

# curl|sh really is the documented install method.
# https://arduino.github.io/arduino-cli/latest/installation/
# https://arduino.github.io/arduino-lint/latest/installation/

$(BINDIR)/arduino-cli:
	mkdir -p $(BINDIR) $(ETCDIR)
	curl -fsSL $(ARDUINO_CLI_URL) | BINDIR=$(BINDIR) sh -s $(ARDUINO_CLI_VERSION)

$(BINDIR)/arduino-lint:
	mkdir -p $(BINDIR) $(ETCDIR)
	curl -fsSL $(ARDUINO_LINT_URL) | BINDIR=$(BINDIR) sh -s $(ARDUINO_LINT_VERSION)

$(ETCDIR)/arduino-cli.yaml: $(BINDIR)/arduino-cli
	mkdir -p $(ETCDIR) $(VARDIR) $(LOGDIR)
	$(BINDIR)/arduino-cli config init --log-file $(LOGDIR)/arduino.log $(ARGS_VERBOSE)
	mv $(VARDIR)/arduino-cli.yaml $(ETCDIR)
	sed -i -e 's%\(  user:\)\(.*\)%\1 $(VARDIR)%' $(ETCDIR)/arduino-cli.yaml
ifdef ADDITIONAL_URLS
	$(BINDIR)/arduino-cli --config-file $(ETCDIR)/arduino-cli.yaml config add board_manager.additional_urls $(ADDITIONAL_URLS)
endif
# Installing libraries from repositories is considered unsafe
# https://arduino.github.io/arduino-cli/0.16/configuration/#configuration-keys
ifdef LIBRARIES_URLS
	$(BINDIR)/arduino-cli --config-file $(ETCDIR)/arduino-cli.yaml config set library.enable_unsafe_install true
endif

print-core-version:
	@echo $$($(BINDIR)/arduino-cli --config-file $(ETCDIR)/arduino-cli.yaml core list | grep "$(CORE_NAME)" | cut -f2 -d' ') ; \

env: $(BINDIR)/arduino-cli $(BINDIR)/arduino-lint $(ETCDIR)/arduino-cli.yaml
	mkdir -p $(BUILDDIR)
	$(BINDIR)/arduino-cli --config-file $(ETCDIR)/arduino-cli.yaml core update-index
	$(BINDIR)/arduino-cli --config-file $(ETCDIR)/arduino-cli.yaml core install $(CORE)
ifdef LIBRARIES
	$(BINDIR)/arduino-cli --config-file $(ETCDIR)/arduino-cli.yaml lib update-index
	$(BINDIR)/arduino-cli --config-file $(ETCDIR)/arduino-cli.yaml lib install $(LIBRARIES)
endif
ifdef LIBRARIES_URLS
	$(BINDIR)/arduino-cli --config-file $(ETCDIR)/arduino-cli.yaml lib install --git-url $(LIBRARIES_URLS)
endif

sketch:
	$(BINDIR)/arduino-cli --config-file $(ETCDIR)/arduino-cli.yaml sketch new $(SRCDIR)

properties:
	$(BINDIR)/arduino-cli --config-file $(ETCDIR)/arduino-cli.yaml compile \
	--build-path $(BUILDDIR) --build-cache-path $(CACHEDIR) \
	--build-property $(BUILDPROP) \
	--build-property 'compiler.cpp.extra_flags=-DVERSION_STRING="$(VERSION_STRING)" $(CPP_EXTRA_FLAGS)' \
	--warnings all --log-file $(LOGDIR)/build.log --log-level debug $(ARGS_VERBOSE) \
	--fqbn $(FQBN) $(SRCDIR) --show-properties

lint:
	$(BINDIR)/arduino-lint $(SRCDIR)

$(BUILDDIR)/$(SKETCH).ino.elf: $(SRCS)
	$(BINDIR)/arduino-cli --config-file $(ETCDIR)/arduino-cli.yaml compile \
	--build-path $(BUILDDIR) --build-cache-path $(CACHEDIR) \
	--build-property $(BUILDPROP) \
	--build-property 'compiler.cpp.extra_flags=-DVERSION_STRING="$(VERSION_STRING)" $(CPP_EXTRA_FLAGS)' \
	--warnings all --log-file $(LOGDIR)/build.log --log-level debug $(ARGS_VERBOSE) \
	--fqbn $(FQBN) $(SRCDIR)

build: $(BUILDDIR)/$(SKETCH).ino.elf
	rm -rf $(SRCDIR)/build

upload: build
	$(BINDIR)/arduino-cli --config-file $(ETCDIR)/arduino-cli.yaml upload \
	--log-file $(LOGDIR)/upload.log --log-level debug $(ARGS_VERBOSE) \
	--port $(PORT) --fqbn $(FQBN) --input-file $(BUILDDIR)/$(SKETCH).ino.bin

clean:
	rm -rf $(BUILDDIR)

clean-all:
	git clean -dxf

################################################################################
