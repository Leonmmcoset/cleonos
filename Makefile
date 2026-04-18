.RECIPEPREFIX := >
MAKEFLAGS += --no-print-directory

CMAKE ?= cmake
CMAKE_BUILD_DIR ?= build-cmake
CMAKE_BUILD_TYPE ?= Release
CMAKE_GENERATOR ?=
CMAKE_EXTRA_ARGS ?=
NO_COLOR ?= 0
LIMINE_SKIP_CONFIGURE ?=
LIMINE_REF ?=
LIMINE_REPO ?=
LIMINE_DIR ?=
LIMINE_BIN_DIR ?=
OBJCOPY_FOR_TARGET ?=
OBJDUMP_FOR_TARGET ?=
READELF_FOR_TARGET ?=
PYTHON ?= python3
MENUCONFIG_ARGS ?=

ifeq ($(strip $(CMAKE_GENERATOR)),)
GEN_ARG :=
else
GEN_ARG := -G "$(CMAKE_GENERATOR)"
endif

CMAKE_PASSTHROUGH_ARGS :=

ifneq ($(strip $(LIMINE_SKIP_CONFIGURE)),)
CMAKE_PASSTHROUGH_ARGS += -DLIMINE_SKIP_CONFIGURE=$(LIMINE_SKIP_CONFIGURE)
endif
ifneq ($(strip $(LIMINE_REF)),)
CMAKE_PASSTHROUGH_ARGS += -DLIMINE_REF=$(LIMINE_REF)
endif
ifneq ($(strip $(LIMINE_REPO)),)
CMAKE_PASSTHROUGH_ARGS += -DLIMINE_REPO=$(LIMINE_REPO)
endif
ifneq ($(strip $(LIMINE_DIR)),)
CMAKE_PASSTHROUGH_ARGS += -DLIMINE_DIR=$(LIMINE_DIR)
endif
ifneq ($(strip $(LIMINE_BIN_DIR)),)
CMAKE_PASSTHROUGH_ARGS += -DLIMINE_BIN_DIR=$(LIMINE_BIN_DIR)
endif
ifneq ($(strip $(OBJCOPY_FOR_TARGET)),)
CMAKE_PASSTHROUGH_ARGS += -DOBJCOPY_FOR_TARGET=$(OBJCOPY_FOR_TARGET)
endif
ifneq ($(strip $(OBJDUMP_FOR_TARGET)),)
CMAKE_PASSTHROUGH_ARGS += -DOBJDUMP_FOR_TARGET=$(OBJDUMP_FOR_TARGET)
endif
ifneq ($(strip $(READELF_FOR_TARGET)),)
CMAKE_PASSTHROUGH_ARGS += -DREADELF_FOR_TARGET=$(READELF_FOR_TARGET)
endif

.PHONY: all configure reconfigure menuconfig setup setup-tools setup-limine kernel userapps ramdisk-root ramdisk iso run debug clean clean-all help

all: iso

configure:
> @$(CMAKE) -S . -B $(CMAKE_BUILD_DIR) $(GEN_ARG) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) -DNO_COLOR=$(NO_COLOR) $(CMAKE_EXTRA_ARGS) $(CMAKE_PASSTHROUGH_ARGS)

reconfigure:
> @rm -rf $(CMAKE_BUILD_DIR)
> @$(MAKE) configure CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) CMAKE_GENERATOR="$(CMAKE_GENERATOR)" CMAKE_EXTRA_ARGS="$(CMAKE_EXTRA_ARGS)" NO_COLOR="$(NO_COLOR)" LIMINE_SKIP_CONFIGURE="$(LIMINE_SKIP_CONFIGURE)" LIMINE_REF="$(LIMINE_REF)" LIMINE_REPO="$(LIMINE_REPO)" LIMINE_DIR="$(LIMINE_DIR)" LIMINE_BIN_DIR="$(LIMINE_BIN_DIR)" OBJCOPY_FOR_TARGET="$(OBJCOPY_FOR_TARGET)" OBJDUMP_FOR_TARGET="$(OBJDUMP_FOR_TARGET)" READELF_FOR_TARGET="$(READELF_FOR_TARGET)"

menuconfig:
> @if command -v $(PYTHON) >/dev/null 2>&1; then \
>     $(PYTHON) scripts/menuconfig.py $(MENUCONFIG_ARGS); \
> elif command -v python >/dev/null 2>&1; then \
>     python scripts/menuconfig.py $(MENUCONFIG_ARGS); \
> else \
>     echo "python3/python not found"; \
>     exit 1; \
> fi
> @$(MAKE) configure CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) CMAKE_GENERATOR="$(CMAKE_GENERATOR)" CMAKE_EXTRA_ARGS="$(CMAKE_EXTRA_ARGS)" NO_COLOR="$(NO_COLOR)" LIMINE_SKIP_CONFIGURE="$(LIMINE_SKIP_CONFIGURE)" LIMINE_REF="$(LIMINE_REF)" LIMINE_REPO="$(LIMINE_REPO)" LIMINE_DIR="$(LIMINE_DIR)" LIMINE_BIN_DIR="$(LIMINE_BIN_DIR)" OBJCOPY_FOR_TARGET="$(OBJCOPY_FOR_TARGET)" OBJDUMP_FOR_TARGET="$(OBJDUMP_FOR_TARGET)" READELF_FOR_TARGET="$(READELF_FOR_TARGET)"

setup: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target setup

setup-tools: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target setup-tools

setup-limine: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target setup-limine

kernel: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target kernel

userapps: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target userapps

ramdisk-root: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target ramdisk-root

ramdisk: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target ramdisk

iso: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target iso

run: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target run

debug: configure
> @$(CMAKE) --build $(CMAKE_BUILD_DIR) --target debug

clean:
> @if [ -d "$(CMAKE_BUILD_DIR)" ]; then \
>     $(CMAKE) --build $(CMAKE_BUILD_DIR) --target clean-x86; \
> else \
>     rm -rf build/x86_64; \
> fi

clean-all:
> @if [ -d "$(CMAKE_BUILD_DIR)" ]; then \
>     $(CMAKE) --build $(CMAKE_BUILD_DIR) --target clean-all; \
> else \
>     rm -rf build build-cmake; \
> fi

help:
> @echo "CLeonOS (CMake-backed wrapper)"
> @echo "  make configure"
> @echo "  make menuconfig"
> @echo "  make setup"
> @echo "  make userapps"
> @echo "  make iso"
> @echo "  make run"
> @echo "  make debug"
> @echo "  make clean"
> @echo "  make clean-all"
> @echo ""
> @echo "Pass custom CMake cache args via:"
> @echo "  make configure CMAKE_EXTRA_ARGS='-DLIMINE_SKIP_CONFIGURE=1 -DOBJCOPY_FOR_TARGET=objcopy'"
> @echo "Direct passthrough is also supported:"
> @echo "  make run LIMINE_SKIP_CONFIGURE=1"
