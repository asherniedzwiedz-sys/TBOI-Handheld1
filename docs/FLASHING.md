# Putting Undercroft on the handheld

**Short version:** it's possible on this kind of handheld, but it's a soldering
project, not a software install. Budget for a chip programmer, expect to need
fine-pitch rework skills, and accept that you might break the unit. If that
sounds like too much, play `dist/undercroft.nes` in an emulator instead.

## What is inside (and what is not known yet)

Your unit looks like a "Sup Game Box 400-in-1" clone with university branding.
From people who have taken these apart (sources below):

- **CPU/graphics:** an NES-on-a-chip under a black epoxy blob. It is believed
  to be a VTxx "OneBus" Famiclone chip. The die has never been identified, so
  that is not confirmed.
- **Games:** all stored in one parallel NOR flash chip. Two variants have been
  seen:
  - Older units: a **BGA** flash on a small breakout board soldered onto the
    main board. It's painful to remove; one researcher broke a unit doing it.
  - Newer units (around 2024): a **TSOP56** chip, usually Spansion/Cypress
    S29GL128 or S29GL256. It can be removed with hot air, but the pads lift
    easily.
- **Screen:** the first 512 KB of the flash holds code that sets up the LCD
  and runs the menu. **You must keep that code.** Without it the screen stays
  dark. Different units use different LCD controllers, and the code differs too.

**I have not seen inside your unit.** Before buying anything, open it (6
screws on the back; take the battery out first) and photograph the board
close up, especially any chip with writing on it. That tells you which variant
you have.

## What works on these handhelds

Per the research below: mapper 0 (NROM) and mapper 4 (MMC3) games with
CHR-ROM. Mapper 0 games that use CHR-RAM did **not** work. Undercroft is
mapper 0 with CHR-ROM, the supported case. It also avoids the risky features:
it uses one nametable only (so mirroring doesn't matter), no DPCM audio, no
sprite-0 tricks and no mid-frame effects. It has still **not been tested on
real hardware.**

## The process

1. **Open the unit and identify the flash** (see above).
2. **Remove the flash chip.** Use hot air and flux for TSOP56. A BGA breakout
   needs low-melt solder alloy (e.g. ChipQuik) and patience. Don't flex the
   board.
3. **Dump the original flash and keep the backup somewhere safe.** For TSOP56,
   use a programmer that supports TSOP56 NOR flash. The research below used an
   XGecu T56 with a TSOP adapter. For the BGA breakout there is a Raspberry Pi
   Pico based dumper. Dump twice and compare; loose wiring produced corrupted
   dumps.
4. **Build a new flash image** with the custom menu tools below. They keep
   your unit's startup and LCD code and add NES games. Add `undercroft.nes`.
5. **Test the image in the EmuVT emulator** before writing anything.
6. **Program the chip and solder it back.** Wait on the chip's busy signal
   when programming; skipping it corrupted writes in the research.

Alternatively, the research project designed an adapter board so you can use
a socketed TSOP48 flash (S29GL064N) instead of resoldering every time.

## Tools and sources

- Custom ROM guide for the Sup 400-in-1 (teardown, dumping, custom loader):
  https://nyh-workshop.github.io/Custom-ROM-Sup-Game-Box-400in1/
- Project log with the later findings (TSOP56 units, LCD variants, which
  mappers work): https://hackaday.io/project/175322-dissecting-a-hand-held-noac-console-sup-400-in-1
- Menu builder: https://github.com/nyh-workshop/MenuBuilder_SupGameBox
- Custom menu template: https://github.com/nyh-workshop/CustomMenu_Sup400in1
- RP2040 flash dumper: https://github.com/nyh-workshop/arduino-teensy41-noac
- Disassembly of the stock software: https://github.com/Wassermann1/sup400

The author of those projects says they won't share ROM images, so you need
your own dump.

## Honest assessment

- **Difficulty: high.** TSOP56 has a 0.5 mm pin pitch. BGA is harder still.
- **Risk:** lifted pads or a bad write can leave the unit dead. Keep the dump.
- **Unknowns:** which flash and LCD your unit has, and whether its startup
  code matches the units the tools were built for.
- **Cheaper ways to get Undercroft into your hands:** any phone NES emulator,
  or a low-cost Linux handheld that runs NES ROMs from an SD card.
