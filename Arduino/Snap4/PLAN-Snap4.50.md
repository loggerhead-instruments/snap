# Snap4 4.51: the LS1-LS2 v4.50/4.51 changes brought to Snap4

## Where Snap4 stood

Snap4 was copied from LS1-LS2-v4 and last matched it at LS1-LS2 commit c5552ac
(2024-12-21, "fix display character overlap"). Since then:

- LS1-LS2-v4 went 4.10 → 4.30 → 4.40 → 4.41 → 4.42 → 4.50 → 4.51 (all in 2026): DS3231
  fallback clocks, wake on the internal RTC alarm, hardware watchdog, audio watchdog,
  SD Sleep, card back-off and resume after a card restart, day folders, free-space recheck,
  cluster-size file count, SdFat 2.x write check, setup page over USB, low-voltage cutoff,
  start-up order, bundled SdFat 2.1.2. See LS1-LS2/Arduino/LS1-LS2-v4/PLAN-v4.50.md.
- Snap4 got one change: the LiPo battery variant (mAhPerBat 24000, second hex file).

The PCB is the same as LS1/LS2 v4; only one audio channel is connected. Same pins
(hydrophone power 2, SD power 20, codec enable 6, LED 17, CS 10, buttons 3/4/8, vSense 21,
DS3231 INT 16), same Teensy 3.2 at 96 MHz, same libraries.

## How it was done (2026-11-09)

The LS1-LS2-v4 sources were copied into Arduino/Snap4 (LS1-LS2-v4.ino as Snap4.ino,
display.ino, rtc.ino, sgtl5000_LHI.ino, cmd.ino, setup.txt, the new diag.ino and usb.ino,
and the whole src/SdFat folder) and the Snap differences were put back. Snap4 and LS1-LS2-v4
now share the version number: Snap 4.51 is the same code as LS 4.51.

Acceptance check, to repeat after any later change to either sketch:

    diff -r Arduino/Snap4 ../LS1-LS2/Arduino/LS1-LS2-v4 -x .DS_Store -x hex -x 'PLAN*'

must show only the differences below (plus the main sketch's file name).

## The Snap differences

| File | Difference |
|---|---|
| Snap4.ino | Header comment names Snap4 and points here |
| Snap4.ino, display.ino | `NCHAN` is 1 and is forced to 1 in `readEEPROM()`: one channel is connected. EEPROM byte 16 is still used, as on LS1/LS2 |
| display.ino | Battery: `nBatPacks` default 1, range 1–12; `mAhPerBat` 24000 for the LiPo build, 12000 for the D-cell build (the one-line difference between the two hex files) |
| display.ino | Button menu has 10 items: no Channel, Mode or Diel. Diel mode can still be set from the setup page or setup.txt |
| display.ino | Splash says "Snap 4" |
| display.ino | Continuous mode does not force 600 s files |
| usb.ino | `DEVICE Snap4`; SETTINGS has no `chan=` (so the setup page shows no Channels field) and no `chan` setter; `batpacks` accepts 1–12 |

Dropped from the old Snap4, on purpose: `printDiags = 1` (now 0, as LS); the delay-start
clearing in EEPROM after the first start (LS 4.50 skips the delay on a restart but serves it
again after a power cycle; both products now do the same); the "Clock read fail" print
(rtc.ino was rewritten).

Shared with the LS1-LS2 repo and kept identical there: src/SdFat (SOURCE.txt lists both
sketches) and tools/logger-setup.html (its battery-pack limit follows the DEVICE line:
12 for a Snap, 8 for an LS1/LS2).

## Build

Teensy 3.2, 96 MHz, same libraries as LS1-LS2-v4 (the sketch carries its own SdFat).
Two builds as before, into Arduino/Snap4/hex and hex/:
- Snap4-25AhLiPo.ino.hex: `mAhPerBat = 24000.0` (as committed)
- Snap4-D-cell.ino.hex: `mAhPerBat = 12000.0`

## Bench tests (Snap4 board)

- Power-up: splash, DS3231 check, card free/total, 10-item menu, "Snap 4" wording.
- Setup page: connects, shows Snap4 and 4.51, no Channels field, battery packs up to 12,
  settings read and write, status counts.
- Mono recording at the usual Snap rates; files land in day folders; WAV header says 1 channel.
- Duty cycle with SD Sleep Power off and a sleep of 2 min or more: card restarts each wake.
- Delay start of 1 day; a power cycle during the delay serves the delay again.
- Pull the card while recording: restart, resume, SD restart count on the setup page.
- Battery estimate on the display for both hex variants (1 pack × 24000 / 12000).
- Overnight run on battery.

## Out of scope

- The Titanium Snap4 manual (menu and firmware sections will need updating afterwards).
- Snap3 and the original snap sketch.
