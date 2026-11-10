# MOSS — A SILT Story. Build: `make` -> release/MOSS_by_Synth_Centric.gb
GBDK_HOME ?= .toolchain/gbdk
LCC       := $(GBDK_HOME)/bin/lcc
PYTHON    ?= python3
HOSTCC    ?= cc
ROM       := release/MOSS_by_Synth_Centric.gb
GEN       := build/gen
OBJ       := build/obj

# Cartridge: MBC5 + RAM + battery (0x1B), one 8 KiB SRAM bank, ROM size auto.
CARTFLAGS := -Wm-yt0x1B -Wm-ya1 -Wm-yoA -Wm-yn"MOSS" -Wm-yk"SC" -Wm-yl0x33
CFLAGS    := -Isource -I$(GEN) -Wf--max-allocs-per-node50000 -Wf--opt-code-size

SRC       := $(wildcard source/*.c)
GENSRC    := $(GEN)/assets.c $(GEN)/maps.c
OBJS      := $(patsubst source/%.c,$(OBJ)/%.o,$(SRC)) $(patsubst $(GEN)/%.c,$(OBJ)/gen_%.o,$(GENSRC))

all: $(ROM)

$(GEN)/audio_tables.inc: tools/gen_audio_tables.py
	$(PYTHON) $< $@

$(GEN)/assets.c $(GEN)/assets.h $(GEN)/maps.c $(GEN)/maps.h: tools/gen_assets.py $(wildcard assets/*/*.txt) $(wildcard tools/art/*.py)
	$(PYTHON) tools/gen_assets.py $(GEN)

$(OBJ)/%.o: source/%.c $(wildcard source/*.h) $(GEN)/audio_tables.inc $(GEN)/assets.h $(GEN)/maps.h
	@mkdir -p $(OBJ)
	$(LCC) $(CFLAGS) -c -o $@ $<

$(OBJ)/gen_%.o: $(GEN)/%.c $(GEN)/assets.h $(GEN)/maps.h
	@mkdir -p $(OBJ)
	$(LCC) $(CFLAGS) -c -o $@ $<

$(ROM): $(OBJS)
	@mkdir -p release
	$(LCC) $(CARTFLAGS) -Wl-m -Wl-j -o build/moss.gb $(OBJS)
	cp build/moss.gb $@
	$(GBDK_HOME)/bin/romusage build/moss.map -g > build/romusage.txt || true

test: host-test
	$(PYTHON) tools/verify_rom.py $(ROM)

host-test:
	@mkdir -p build
	$(HOSTCC) -std=c99 -O1 -Wall -Wno-unused-function -Isource -I$(GEN) tests/host_compose.c \
	    source/compose.c source/garden.c source/memory.c -o build/host_compose
	./build/host_compose
	$(HOSTCC) -std=c99 -O1 -Wall -Isource tests/host_save.c -o build/host_save
	./build/host_save

test-emulator: $(ROM)
	$(PYTHON) tests/emu_playthrough.py $(ROM)

clean:
	rm -rf build

.PHONY: all test host-test test-emulator clean
