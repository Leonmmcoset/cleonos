.RECIPEPREFIX := >

NO_COLOR ?= 0

BUILD_ROOT := build/x86_64
OBJ_ROOT := $(BUILD_ROOT)/obj
ISO_ROOT := $(BUILD_ROOT)/iso_root
KERNEL_ELF := $(BUILD_ROOT)/clks_kernel.elf
RAMDISK_IMAGE := $(BUILD_ROOT)/cleonos_ramdisk.tar
ISO_IMAGE := build/CLeonOS-x86_64.iso

LIMINE_DIR ?= limine
LIMINE_REPO ?= https://gh-proxy.com/https://github.com/limine-bootloader/limine.git
LIMINE_REF ?=
LIMINE_BIN_DIR ?= $(LIMINE_DIR)/bin
LIMINE_SETUP_STAMP := $(LIMINE_DIR)/.cleonos-limine-setup.stamp
LIMINE_BUILD_STAMP := $(LIMINE_DIR)/.cleonos-limine-build.stamp
LIMINE_CONFIGURE_FLAGS ?= --enable-bios-cd --enable-uefi-cd --enable-uefi-x86-64
LIMINE_SKIP_CONFIGURE ?= 0
OBJCOPY_FOR_TARGET ?= llvm-objcopy
OBJDUMP_FOR_TARGET ?= llvm-objdump
READELF_FOR_TARGET ?= llvm-readelf

XORRISO ?= xorriso
TAR ?= tar

QEMU_X86_64 ?= qemu-system-x86_64

CC ?= x86_64-elf-gcc
LD ?= x86_64-elf-ld
ARCH_CFLAGS := -DCLKS_ARCH_X86_64=1 -m64 -mno-red-zone -mcmodel=kernel -fno-pic -fno-pie
LINKER_SCRIPT := clks/arch/x86_64/linker.ld
RUN_COMMAND := $(QEMU_X86_64) -M q35 -m 1024M -cdrom $(ISO_IMAGE) -serial stdio
DEBUG_COMMAND := $(QEMU_X86_64) -M q35 -m 1024M -cdrom $(ISO_IMAGE) -serial stdio -s -S

ifeq ($(NO_COLOR),1)
COLOR_RESET :=
COLOR_INFO :=
COLOR_WARN :=
COLOR_ERROR :=
COLOR_STEP :=
else
COLOR_RESET := \033[0m
COLOR_INFO := \033[1;36m
COLOR_WARN := \033[1;33m
COLOR_ERROR := \033[1;31m
COLOR_STEP := \033[1;35m
endif

define log_info
@printf '%b\n' "$(COLOR_INFO)[INFO]$(COLOR_RESET) $(1)"
endef

define log_warn
@printf '%b\n' "$(COLOR_WARN)[WARN]$(COLOR_RESET) $(1)"
endef

define log_error
@printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) $(1)"
endef

define log_step
@printf '%b\n' "$(COLOR_STEP)[STEP]$(COLOR_RESET) $(1)"
endef

C_SOURCES := \
    clks/kernel/kmain.c \
    clks/kernel/log.c \
    clks/kernel/limine_requests.c \
    clks/kernel/tty.c \
    clks/kernel/pmm.c \
    clks/kernel/heap.c \
    clks/kernel/interrupts.c \
    clks/kernel/scheduler.c \
    clks/lib/string.c \
    clks/drivers/serial/serial.c \
    clks/drivers/video/framebuffer.c \
    clks/drivers/video/font8x8.c \
    clks/arch/x86_64/boot.c

ASM_SOURCES := \
    clks/arch/x86_64/interrupt_stubs.S

C_OBJECTS := $(patsubst %.c,$(OBJ_ROOT)/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst %.S,$(OBJ_ROOT)/%.o,$(ASM_SOURCES))
OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)

CFLAGS_COMMON := -std=c11 -ffreestanding -fno-stack-protector -fno-builtin -Wall -Wextra -Werror -Iclks/include
ASFLAGS_COMMON := -ffreestanding -Iclks/include
LDFLAGS_COMMON := -nostdlib -z max-page-size=0x1000

.PHONY: all setup setup-tools setup-limine kernel ramdisk iso run debug clean clean-all help

all: iso

setup: setup-tools setup-limine
> $(call log_info,environment ready)

setup-tools:
> $(call log_step,checking host tools)
> @command -v git >/dev/null 2>&1 || (printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) missing tool: git" && exit 1)
> @command -v $(TAR) >/dev/null 2>&1 || (printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) missing tool: $(TAR)" && exit 1)
> @command -v $(XORRISO) >/dev/null 2>&1 || (printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) missing tool: $(XORRISO)" && exit 1)
> @command -v clang >/dev/null 2>&1 || (printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) missing tool: clang" && exit 1)
> @command -v ld.lld >/dev/null 2>&1 || (printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) missing tool: ld.lld" && exit 1)
> @command -v $(OBJCOPY_FOR_TARGET) >/dev/null 2>&1 || (printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) missing tool: $(OBJCOPY_FOR_TARGET)" && exit 1)
> @command -v $(OBJDUMP_FOR_TARGET) >/dev/null 2>&1 || (printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) missing tool: $(OBJDUMP_FOR_TARGET)" && exit 1)
> @command -v $(READELF_FOR_TARGET) >/dev/null 2>&1 || (printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) missing tool: $(READELF_FOR_TARGET)" && exit 1)
> $(call log_info,required tools are available)

setup-limine:
> $(call log_step,preparing limine)
> @if [ ! -d "$(LIMINE_DIR)" ]; then \
>     if [ -n "$(LIMINE_REF)" ]; then \
>         printf '%b\n' "$(COLOR_INFO)[INFO]$(COLOR_RESET) cloning limine ($(LIMINE_REF)) into $(LIMINE_DIR)"; \
>         git clone --branch "$(LIMINE_REF)" --depth 1 "$(LIMINE_REPO)" "$(LIMINE_DIR)"; \
>     else \
>         printf '%b\n' "$(COLOR_INFO)[INFO]$(COLOR_RESET) cloning limine (default branch) into $(LIMINE_DIR)"; \
>         git clone --depth 1 "$(LIMINE_REPO)" "$(LIMINE_DIR)"; \
>     fi; \
> fi
> @if [ "$(LIMINE_SKIP_CONFIGURE)" = "1" ]; then \
>     test -f "$(LIMINE_DIR)/Makefile" || (printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) LIMINE_SKIP_CONFIGURE=1 but $(LIMINE_DIR)/Makefile is missing" && exit 1); \
>     printf '%b\n' "$(COLOR_WARN)[WARN]$(COLOR_RESET) skipping limine Makefile generation (LIMINE_SKIP_CONFIGURE=1)"; \
> else \
>     cfg_fingerprint="FLAGS=$(LIMINE_CONFIGURE_FLAGS);OBJCOPY=$(OBJCOPY_FOR_TARGET);OBJDUMP=$(OBJDUMP_FOR_TARGET);READELF=$(READELF_FOR_TARGET)"; \
>     need_configure=0; \
>     if [ ! -f "$(LIMINE_DIR)/Makefile" ]; then need_configure=1; fi; \
>     if [ ! -f "$(LIMINE_SETUP_STAMP)" ]; then need_configure=1; fi; \
>     if [ -f "$(LIMINE_SETUP_STAMP)" ] && ! grep -qx "$$cfg_fingerprint" "$(LIMINE_SETUP_STAMP)"; then need_configure=1; fi; \
>     if [ "$$need_configure" -eq 1 ]; then \
>         printf '%b\n' "$(COLOR_STEP)[STEP]$(COLOR_RESET) generating/reconfiguring limine Makefile"; \
>         if [ -x "$(LIMINE_DIR)/bootstrap" ]; then \
>             (cd "$(LIMINE_DIR)" && ./bootstrap); \
>         fi; \
>         test -x "$(LIMINE_DIR)/configure" || (printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) limine configure script missing" && exit 1); \
>         (cd "$(LIMINE_DIR)" && OBJCOPY_FOR_TARGET="$(OBJCOPY_FOR_TARGET)" OBJDUMP_FOR_TARGET="$(OBJDUMP_FOR_TARGET)" READELF_FOR_TARGET="$(READELF_FOR_TARGET)" ./configure $(LIMINE_CONFIGURE_FLAGS)); \
>         printf '%s\n' "$$cfg_fingerprint" > "$(LIMINE_SETUP_STAMP)"; \
>         rm -f "$(LIMINE_BUILD_STAMP)"; \
>     else \
>         printf '%b\n' "$(COLOR_INFO)[INFO]$(COLOR_RESET) limine configure state unchanged"; \
>     fi; \
> fi
> @need_build=0; \
> if [ ! -f "$(LIMINE_BUILD_STAMP)" ]; then need_build=1; fi; \
> for f in limine limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin; do \
>     if [ ! -f "$(LIMINE_BIN_DIR)/$$f" ]; then need_build=1; fi; \
> done; \
> if [ "$$need_build" -eq 1 ]; then \
>     printf '%b\n' "$(COLOR_INFO)[INFO]$(COLOR_RESET) building limine"; \
>     $(MAKE) -C "$(LIMINE_DIR)"; \
>     touch "$(LIMINE_BUILD_STAMP)"; \
> else \
>     printf '%b\n' "$(COLOR_INFO)[INFO]$(COLOR_RESET) limine already built, skipping compile"; \
> fi
> @test -f "$(LIMINE_BIN_DIR)/limine" || (printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) limine build failed" && exit 1)
> @test -f "$(LIMINE_BIN_DIR)/limine-bios.sys" || (printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) limine-bios.sys missing" && exit 1)
> @test -f "$(LIMINE_BIN_DIR)/limine-bios-cd.bin" || (printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) limine-bios-cd.bin missing" && exit 1)
> @test -f "$(LIMINE_BIN_DIR)/limine-uefi-cd.bin" || (printf '%b\n' "$(COLOR_ERROR)[ERROR]$(COLOR_RESET) limine-uefi-cd.bin missing" && exit 1)
> $(call log_info,limine artifacts ready)

kernel: $(KERNEL_ELF)

ramdisk: $(RAMDISK_IMAGE)

$(KERNEL_ELF): $(OBJECTS) $(LINKER_SCRIPT) Makefile
> $(call log_step,linking kernel -> $(KERNEL_ELF))
> @mkdir -p $(dir $@)
> @$(LD) $(LDFLAGS_COMMON) -T $(LINKER_SCRIPT) -o $@ $(OBJECTS)

$(OBJ_ROOT)/%.o: %.c Makefile
> $(call log_step,compiling $<)
> @mkdir -p $(dir $@)
> @$(CC) $(CFLAGS_COMMON) $(ARCH_CFLAGS) -c $< -o $@

$(OBJ_ROOT)/%.o: %.S Makefile
> $(call log_step,assembling $<)
> @mkdir -p $(dir $@)
> @$(CC) $(ASFLAGS_COMMON) $(ARCH_CFLAGS) -c $< -o $@


$(RAMDISK_IMAGE):
> $(call log_step,packing ramdisk -> $(RAMDISK_IMAGE))
> @mkdir -p $(dir $@)
> @$(TAR) -C ramdisk -cf $@ .

iso: setup-tools setup-limine $(KERNEL_ELF) $(RAMDISK_IMAGE) configs/limine.conf
> $(call log_step,assembling iso root)
> @rm -rf $(ISO_ROOT)
> @mkdir -p $(ISO_ROOT)/boot/limine
> @cp $(KERNEL_ELF) $(ISO_ROOT)/boot/clks_kernel.elf
> @cp $(RAMDISK_IMAGE) $(ISO_ROOT)/boot/cleonos_ramdisk.tar
> @cp configs/limine.conf $(ISO_ROOT)/boot/limine/limine.conf
> @cp $(LIMINE_BIN_DIR)/limine-bios.sys $(ISO_ROOT)/boot/limine/
> @cp $(LIMINE_BIN_DIR)/limine-bios-cd.bin $(ISO_ROOT)/boot/limine/
> @cp $(LIMINE_BIN_DIR)/limine-uefi-cd.bin $(ISO_ROOT)/boot/limine/
> @mkdir -p $(dir $(ISO_IMAGE))
> $(call log_step,building iso -> $(ISO_IMAGE))
> @$(XORRISO) -as mkisofs \
>     -b boot/limine/limine-bios-cd.bin \
>     -no-emul-boot \
>     -boot-load-size 4 \
>     -boot-info-table \
>     --efi-boot boot/limine/limine-uefi-cd.bin \
>     -efi-boot-part \
>     --efi-boot-image \
>     --protective-msdos-label \
>     $(ISO_ROOT) \
>     -o $(ISO_IMAGE)
> $(call log_step,installing limine boot sectors)
> @$(LIMINE_BIN_DIR)/limine bios-install $(ISO_IMAGE)
> $(call log_info,iso ready: $(ISO_IMAGE))

run: iso
> $(call log_step,launching qemu run)
> @$(RUN_COMMAND)

debug: iso
> $(call log_step,launching qemu debug (-s -S))
> @$(DEBUG_COMMAND)

clean:
> $(call log_step,cleaning $(BUILD_ROOT))
> @rm -rf $(BUILD_ROOT)
> $(call log_info,clean done)

clean-all:
> $(call log_step,cleaning build)
> @rm -rf build
> $(call log_info,clean-all done)

help:
> @echo "CLeonOS build system (x86_64 only)"
> @echo "  make setup"
> @echo "  make setup LIMINE_REF=<branch-or-tag>"
> @echo "  make setup LIMINE_SKIP_CONFIGURE=1"
> @echo "  make setup LIMINE_CONFIGURE_FLAGS='--enable-bios-cd --enable-uefi-cd --enable-uefi-x86-64'"
> @echo "  make setup OBJCOPY_FOR_TARGET=llvm-objcopy OBJDUMP_FOR_TARGET=llvm-objdump READELF_FOR_TARGET=llvm-readelf"
> @echo "  make iso"
> @echo "  make run"
> @echo "  make debug"
> @echo "  make NO_COLOR=1 <target>"