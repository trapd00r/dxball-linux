# Reverse engineering notes (DX-Ball 1.07)

Target: `DXBALL.EXE` PE32 i386, image base `0x00400000`, entry `0x004187A0`.
Ghidra 12.1.4 decompiled 528 functions into `notes/decomp.c`.

## Modes (`DAT_00421074`)

`FUN_00403730` dispatches:

| mode | function     | role |
|------|--------------|------|
| 0    | `FUN_0040e330` | intro / attract; click goes to mode 1 |
| 1    | `FUN_0040f8b0` | gameplay (balls, bricks, bonuses) |
| 2    | `FUN_0040c3b0` | board editor |
| 3    | `FUN_00406730` | high score (`highscor.pcx`) |
| 4    | `FUN_00407990` | title / menu after DirectDraw init |

## Playfield (from the editor)

```
col = (x - 0x14) / 0x1e    // (x - 20) / 30
row = (y - 0x32) / 0x0f    // (y - 50) / 15
```

20x20 tiles, 30x15 pixels, origin (20, 50). Mouse x is clamped 8..599.

Paddle: `FUN_00412df0` sets `y = 0x1c2` (450) and clamps x using paddle
width `DAT_0043fa94`. Default paddle sprite index 68 is 73x14 in `MBALL2.SBK`.

Ball: linked list at `DAT_0043a8b8`. `FUN_00410770` integrates integer `vx,vy`,
bounces on x=20 / x=0x26b-width / y=0, loses the ball below the screen, and
tests the paddle box `(paddle_x +/- width/2, paddle_y .. paddle_y+7)`.

## Assets

`DEFAULT.BDS` is 50 raw 20x20 boards (20000 bytes), loaded by `FUN_0040cc30`.

SBK (`FUN_00404610`):

```
uint32 count
repeat count:
  uint32 width
  uint32 height
  uint8  char_id
  uint32 extra
  uint8  pixels[width * height]   // first row is the bottom of the surface
```

PCX files are standard 8-bit. `INTRO.PCX`, `MAINMENU.PCX`, `MBBKGRND.PCX`,
`HIGHSCOR.PCX` are 640x480. `BIGBOLT.PCX` is a 160x480 side strip.

WAV files are PCM. MDS files are RIFF MIDI streams (`midiStreamOpen`).

Tile-to-sprite map: `FUN_0040ccf0`. Tile 8 is used as steel framing on the
stock boards.

## Imports

DirectDraw (`DirectDrawCreate`) at 640x480x8, DirectSound, WinMM MIDI,
USER32 window/input. Window style `0x80000000` (popup) size 640x480.
