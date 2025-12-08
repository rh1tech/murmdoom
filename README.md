# MurmDoom

DOOM for Raspberry Pi Pico 2 (RP2350) with HDMI output, SD card, PS/2 keyboard, and OPL music.

## Features

- Native 320×240 HDMI video output via PIO
- Full OPL2 music emulation (EMU8950 with ARM assembly optimizations)
- 8MB QSPI PSRAM support for game data
- SD card support for WAD files and savegames
- PS/2 keyboard input
- Sound effects and music at 49716 Hz

## Hardware Requirements

- **Raspberry Pi Pico 2** (RP2350 with 8MB PSRAM) or compatible board
- **HDMI connector** (directly connected via resistors, no HDMI encoder needed)
- **SD card module** (SPI mode)
- **PS/2 keyboard** (directly connected)
- **I2S DAC module** (e.g., PCM5102A) for audio output

## Pinout

### HDMI (directly connected via 270Ω resistors)
| Signal | GPIO |
|--------|------|
| CLK-   | 6    |
| CLK+   | 7    |
| D0-    | 8    |
| D0+    | 9    |
| D1-    | 10   |
| D1+    | 11   |
| D2-    | 12   |
| D2+    | 13   |

### SD Card (SPI mode)
| Signal | GPIO |
|--------|------|
| CLK    | 2    |
| CMD/MOSI | 3  |
| D0/MISO  | 4  |
| CS       | 5  |

### PS/2 Keyboard
| Signal | GPIO |
|--------|------|
| CLK    | 0    |
| DATA   | 1    |

### I2S Audio (optional, but recommended)
| Signal | GPIO |
|--------|------|
| DATA   | 26   |
| BCLK   | 27   |
| LRCLK  | 28   |

### PSRAM (QSPI)
| Signal | GPIO |
|--------|------|
| CS     | 47   |

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

# Create build directory and compile
mkdir build
cd build
cmake ..
make -j$(nproc)
```

Or use the build script:

```bash
./build.sh
```

### Flashing

```bash
# With device in BOOTSEL mode:
picotool load build/murmdoom.uf2

# Or with device running:
picotool load -x build/murmdoom.elf
```

## SD Card Setup

1. Format an SD card as FAT32
2. Copy a DOOM WAD file to the root:
   - `doom1.wad` (Shareware)
   - `doom.wad` (Registered)
   - `doom2.wad` (DOOM II)
3. A `.savegame/` directory will be created automatically for save files

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
