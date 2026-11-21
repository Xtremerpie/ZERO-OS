# ZERO OS 0.0.0 build. Targets: all, image, run, test, clean, info.
# Requires: clang, ld.lld, nasm, make (build); qemu-system-x86_64, OVMF, mtools,
# sgdisk, python3 (image + test). Run `scripts/check-env.sh` to see what is missing.

BUILD    ?= build
KERNEL   := $(BUILD)/zero-kernel.elf
IMAGE    := $(BUILD)/zero-os.img

CC       := clang
LD       := ld.lld
NASM     := nasm

BUILD_ID := $(shell git describe --always --dirty 2>/dev/null || echo nogit)
GEN_DIR  := $(BUILD)/generated
BUILDID_H := $(GEN_DIR)/zero/buildid.h
EXTRA_CFLAGS ?=

CFLAGS := --target=x86_64-unknown-none-elf -std=c11 -O2 -g \
          -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
          -mcmodel=kernel -mno-red-zone -mgeneral-regs-only \
          -fno-builtin -fno-omit-frame-pointer \
          -Wall -Wextra -Werror \
          -Ikernel -Ikernel/include -Ithird_party -Ithird_party/limine -I$(GEN_DIR) \
          $(EXTRA_CFLAGS)

LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -T kernel/linker.ld

C_SRCS   := $(shell find kernel -name '*.c' | sort)
ASM_SRCS := $(shell find kernel -name '*.asm' | sort)
OBJS     := $(patsubst %.c,$(BUILD)/obj/%.o,$(C_SRCS)) \
            $(patsubst %.asm,$(BUILD)/obj/%.o,$(ASM_SRCS))
DEPS     := $(OBJS:.o=.d)

.PHONY: all image run test clean info analyze ubsan FORCE
all: $(KERNEL)

# The build id lives in a generated header that is rewritten ONLY when its value changes,
# so objects that include it (via -MMD dependency tracking) rebuild exactly when needed.
FORCE:
$(BUILDID_H): FORCE
	@mkdir -p $(dir $@)
	@printf '#define ZERO_BUILD_ID "%s"\n' '$(BUILD_ID)' > $@.tmp
	@if cmp -s $@.tmp $@; then rm -f $@.tmp; else mv $@.tmp $@; fi

# First build: the generated header must exist before any object is compiled.
$(OBJS): | $(BUILDID_H)

$(BUILD)/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/obj/%.o: %.asm
	@mkdir -p $(dir $@)
	$(NASM) -f elf64 -g -F dwarf $< -o $@

$(KERNEL): $(OBJS) kernel/linker.ld
	$(LD) $(LDFLAGS) $(OBJS) -o $@
	@echo "built $@ ($$(stat -c %s $@) bytes)"

image: $(KERNEL)
	scripts/mkimage.sh $(IMAGE) ""

run: image
	scripts/run-qemu.sh $(IMAGE)

test: $(KERNEL)
	python3 tests/run_tests.py

info: $(KERNEL)
	readelf -lW $(KERNEL) | head -30
	size $(KERNEL)

# Clang static analyzer over every kernel C file (reports defects; non-zero exit on any warning).
analyze: | $(BUILDID_H)
	@set -e; for f in $(C_SRCS); do \
	    $(CC) $(CFLAGS) --analyze -Xclang -analyzer-werror $$f -o /dev/null; \
	done; echo "static analysis: $(words $(C_SRCS)) files clean"

# UBSan build (trap mode, no runtime needed): any undefined behaviour executes ud2 and the
# boot tests fail with #UD. Output goes to build-ubsan/ and is tested with ZERO_BUILD_DIR.
ubsan:
	$(MAKE) BUILD=build-ubsan EXTRA_CFLAGS='-fsanitize=undefined -fsanitize-trap=undefined' all

clean:
	rm -rf $(BUILD) build-ubsan

-include $(DEPS)
