# VACANT - An Endless City After Dark. by Synth Centric.
# `make` builds release/VACANT_by_Synth_Centric.gbc
GBDK_HOME ?= .toolchain/gbdk
LCC       := $(GBDK_HOME)/bin/lcc
PYTHON    ?= python3
HOSTCC    ?= cc
ROM       := release/VACANT_by_Synth_Centric.gbc
GEN       := build/gen
OBJ       := build/obj
TOOLS     := tools/asset_generation

# MBC5 + RAM + battery (0x1B), one 8 KiB SRAM bank, Game Boy Color only,
# ROM size chosen automatically by the linker.
CARTFLAGS := -Wm-yt0x1B -Wm-ya1 -Wm-yC -Wm-yn"VACANT" -Wm-yk"SC" -Wm-yl0x33 -Wm-yoA
CFLAGS    := -Isrc -I$(GEN) -Wf--max-allocs-per-node30000 -Wf--opt-code-speed

SRC := $(shell find src -name '*.c')
GENSRC := $(GEN)/home_data.c $(GEN)/world_data.c $(GEN)/render_data.c $(GEN)/gfx_data.c $(GEN)/ui_data.c
OBJS := $(patsubst src/%.c,$(OBJ)/%.o,$(SRC)) $(patsubst $(GEN)/%.c,$(OBJ)/gen/%.o,$(GENSRC))
HEADERS := $(shell find src -name '*.h')
ART := assets/tiles/metatiles.png assets/tiles/metatiles.json assets/tiles/animations.png \
       assets/sprites/player.png assets/sprites/rain.png assets/sprites/train.png \
       assets/interface/font.png assets/interface/title.png assets/palettes/palettes.json

all: $(ROM)

# Re-paint the source art from the painters (not part of a normal build)
paint:
	$(PYTHON) $(TOOLS)/paint_world.py
	$(PYTHON) $(TOOLS)/paint_sprites.py
	$(PYTHON) $(TOOLS)/paint_interface.py

$(GEN)/assets.h $(GENSRC) &: $(ART) $(TOOLS)/build_assets.py
	@mkdir -p $(GEN)
	$(PYTHON) $(TOOLS)/build_assets.py $(GEN)

$(GEN)/audio_tables.inc: $(TOOLS)/gen_audio_tables.py
	@mkdir -p $(GEN)
	$(PYTHON) $< $@

$(OBJ)/%.o: src/%.c $(HEADERS) $(GEN)/assets.h $(GEN)/audio_tables.inc
	@mkdir -p $(dir $@)
	$(LCC) $(CFLAGS) -c -o $@ $<

$(OBJ)/gen/%.o: $(GEN)/%.c $(GEN)/assets.h
	@mkdir -p $(dir $@)
	$(LCC) $(CFLAGS) -c -o $@ $<

$(ROM): $(OBJS)
	@mkdir -p release
	$(LCC) $(CARTFLAGS) -Wl-m -Wl-j -o build/vacant.gbc $(OBJS)
	cp build/vacant.gbc $@
	$(GBDK_HOME)/bin/romusage build/vacant.map -g > build/romusage.txt || true
	$(PYTHON) tools/validation/verify_rom.py $@

# ---------------------------------------------------------------- tests
HOSTFLAGS := -std=c99 -O2 -DHOST -Wall -Wno-unknown-pragmas -Isrc -I$(GEN)

test: host-test $(ROM)
	$(PYTHON) tools/validation/verify_rom.py $(ROM)

host-test: $(GEN)/assets.h $(GEN)/audio_tables.inc
	@mkdir -p build
	$(HOSTCC) $(HOSTFLAGS) tests/host_world.c src/world/worldgen.c $(GEN)/world_data.c -o build/host_world
	./build/host_world
	$(HOSTCC) $(HOSTFLAGS) tests/host_music.c tests/host_sound_stub.c src/music/music.c -lm -o build/host_music
	./build/host_music
	$(HOSTCC) $(HOSTFLAGS) tests/host_save.c src/save/save.c -o build/host_save
	./build/host_save

test-emulator: $(ROM)
	$(PYTHON) tests/emu_tests.py $(ROM)

clean:
	rm -rf build

.PHONY: all paint test host-test test-emulator clean
