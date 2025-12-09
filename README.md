# MurmDoom

DOOM for Raspberry Pi Pico 2 (RP2350) with HDMI output, SD card, PS/2 keyboard, and OPL music.

## Supported Boards

This firmware is designed for the following RP2350-based boards with integrated HDMI, SD card, PS/2, and PSRAM:

- **[Murmulator](https://murmulator.ru)** — A compact retro-computing platform based on RP Pico 2, designed for emulators and classic games.
- **[FRANK](https://rh1.tech/projects/frank?area=about)** — A versatile development board based on RP Pico 2, HDMI output, and extensive I/O options.

Both boards provide all necessary peripherals out of the box—no additional wiring required.

## Features

- Native 320×240 HDMI video output via PIO
- Full OPL2 music emulation (EMU8950 with ARM assembly optimizations)
- 8MB QSPI PSRAM support for game data
- SD card support for WAD files and savegames
- PS/2 keyboard input
- Sound effects and music at 49716 Hz

## Hardware Requirements

- **Raspberry Pi Pico 2** (RP2350) or compatible board
- **8MB QSPI PSRAM** (mandatory!)
- **HDMI connector** (directly connected via resistors, no HDMI encoder needed)
- **SD card module** (SPI mode)
- **PS/2 keyboard** (directly connected)
- **I2S DAC module** (e.g., TDA1387) for audio output

### PSRAM Options

MurmDoom requires 8MB PSRAM to run. You can obtain PSRAM-equipped hardware in several ways:

1. **Solder a PSRAM chip** on top of the Flash chip on a Pico 2 clone (SOP-8 flash chips are only available on clones, not the original Pico 2)
2. **Build a [Nyx 2](https://rh1.tech/projects/nyx?area=nyx2)** — a DIY RP2350 board with integrated PSRAM
3. **Purchase a [Pimoroni Pico Plus 2](https://shop.pimoroni.com/products/pimoroni-pico-plus-2?variant=42092668289107)** — a ready-made Pico 2 with 8MB PSRAM

## Board Configurations

Two GPIO layouts are supported: **M1** and **M2**. The PSRAM pin is auto-detected based on chip package:
- **RP2350B**: GPIO47 (both M1 and M2)
- **RP2350A**: GPIO19 (M1) or GPIO8 (M2)

### HDMI (via 270Ω resistors)
| Signal | M1 GPIO | M2 GPIO |
|--------|---------|---------|
| CLK-   | 6       | 12      |
| CLK+   | 7       | 13      |
| D0-    | 8       | 14      |
| D0+    | 9       | 15      |
| D1-    | 10      | 16      |
| D1+    | 11      | 17      |
| D2-    | 12      | 18      |
| D2+    | 13      | 19      |

### SD Card (SPI mode)
| Signal  | M1 GPIO | M2 GPIO |
|---------|---------|---------|
| CLK     | 2       | 6       |
| CMD     | 3       | 7       |
| DAT0    | 4       | 4       |
| DAT3/CS | 5       | 5       |

### PS/2 Keyboard
| Signal | M1 GPIO | M2 GPIO |
|--------|---------|---------|
| CLK    | 0       | 2       |
| DATA   | 1       | 3       |

### I2S Audio
| Signal | M1 GPIO | M2 GPIO |
|--------|---------|---------|
| DATA   | 26      | 9       |
| BCLK   | 27      | 10      |
| LRCLK  | 28      | 11      |

## Building

### Prerequisites

1. Install the [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) (version 2.0+)
2. Set environment variable: `export PICO_SDK_PATH=/path/to/pico-sdk`
3. Install ARM GCC toolchain

### Build Steps

```bash
# Clone the repository with submodules
git clone --recursive https://github.com/rh1tech/murmdoom.git
cd murmdoom

# Or if already cloned, initialize submodules
git submodule update --init --recursive

# Build for M1 layout (default)
mkdir build && cd build
cmake -DBOARD_VARIANT=M1 ..
make -j$(nproc)

# Or build for M2 layout
cmake -DBOARD_VARIANT=M2 ..
make -j$(nproc)
```

Or use the build script (builds M1 by default):

```bash
./build.sh
```

### Release Builds

To build both M1 and M2 variants with version numbering:

```bash
./release.sh
```

This creates versioned UF2 files in the `release/` directory:
- `murmdoom_m1_X_XX.uf2`
- `murmdoom_m2_X_XX.uf2`

### Flashing

```bash
# With device in BOOTSEL mode:
picotool load build/murmdoom.uf2

# Or with device running:
picotool load -f build/murmdoom.uf2
```

## SD Card Setup

1. Format an SD card as FAT32
2. Copy any DOOM-engine WAD file to the root (e.g., `doom1.wad`, `doom.wad`, `doom2.wad`, `plutonia.wad`, `tnt.wad`, or custom WADs)
3. A `.savegame/` directory will be created automatically for save files

### Shareware WAD Downloads

If you don't have a DOOM WAD file, you can download the shareware versions:
- **DOOM Shareware** (`doom1.wad`): [Download from Doomworld](https://www.doomworld.com/classicdoom/info/shareware.php) or [id Software FTP](https://www.idsoftware.com/games/doom/doom-shareware)
- **Freedoom** (free open-source alternative): [freedoom.github.io](https://freedoom.github.io/)

For the full game, purchase DOOM or DOOM II from [Steam](https://store.steampowered.com/app/2280/DOOM_1993/) or [GOG](https://www.gog.com/game/doom_doom_ii).

## Controls

Standard DOOM keyboard controls:
- Arrow keys: Move/Turn
- Ctrl: Fire
- Space: Open doors/Use
- Shift: Run
- 1-7: Select weapon
- Escape: Menu

## License

GNU General Public License v2. See [LICENSE](LICENSE) for details.

This project is based on:
- [Chocolate Doom](https://github.com/chocolate-doom/chocolate-doom) by Simon Howard
- [doomgeneric](https://github.com/ozkl/doomgeneric) by ozkl
- [rp2040-doom](https://github.com/kilograham/rp2040-doom) by Graham Sanderson (OPL and Pico optimizations)
- [EMU8950](https://github.com/digital-sound-antiques/emu8950) by Mitsutaka Okazaki (OPL2 emulator)

## Author

Mikhail Matveev <xtreme@outlook.com>

## Acknowledgments

- id Software for the original DOOM
- The Chocolate Doom team for the clean portable source port
- Graham Sanderson for the incredible RP2040 optimizations and EMU8950 ARM assembly
- The Raspberry Pi foundation for the RP2350 and Pico SDK
