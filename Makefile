# Build: make            -> build/undercroft.nes
# Needs cc65 (ca65, cc65, ld65) and python3.
NAME    := undercroft
BUILD   := build
CFLAGS  := -t nes -O -Oirs -Cl --cpu 6502 -I $(BUILD) -I src
AFLAGS  := -t nes --bin-include-dir $(BUILD)

ROM     := $(BUILD)/$(NAME).nes

all: $(ROM) $(BUILD)/$(NAME)-web.html

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/chr.bin $(BUILD)/tiles.h: gfx/tiles.txt tools/mkchr.py | $(BUILD)
	python3 tools/mkchr.py gfx/tiles.txt $(BUILD)/chr.bin $(BUILD)/tiles.h

$(BUILD)/rooms.h: gfx/rooms.txt tools/mkrooms.py | $(BUILD)
	python3 tools/mkrooms.py gfx/rooms.txt $(BUILD)/rooms.h

$(BUILD)/main.s: src/main.c src/nes.h $(BUILD)/tiles.h $(BUILD)/rooms.h
	cc65 $(CFLAGS) -o $@ $<

$(BUILD)/main.o: $(BUILD)/main.s
	ca65 $(AFLAGS) -o $@ $<

$(BUILD)/crt0.o: src/crt0.s $(BUILD)/chr.bin
	ca65 $(AFLAGS) -o $@ $<

$(BUILD)/fast.o: src/fast.s
	ca65 $(AFLAGS) -o $@ $<

$(ROM): $(BUILD)/crt0.o $(BUILD)/main.o $(BUILD)/fast.o src/nrom.cfg
	ld65 -C src/nrom.cfg -o $@ -m $(BUILD)/$(NAME).map -Ln $(BUILD)/$(NAME).labels \
		$(BUILD)/crt0.o $(BUILD)/main.o $(BUILD)/fast.o nes.lib

$(BUILD)/$(NAME)-web.html: web/player.html $(ROM) tools/mkweb.py
	python3 tools/mkweb.py web/player.html $(ROM) $@

dist: all
	mkdir -p dist
	cp $(ROM) $(BUILD)/$(NAME)-web.html dist/

clean:
	rm -rf $(BUILD)

.PHONY: all clean dist
