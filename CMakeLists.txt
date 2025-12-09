cmake_minimum_required(VERSION 3.13)

# Set board to Pico 2 (RP2350)
set(PICO_BOARD pico2 CACHE STRING "Pico board type")

# Board configuration variant (M1, M2)
set(BOARD_VARIANT "M1" CACHE STRING "Board variant: M1 or M2")

include(pico_sdk_import.cmake)

# Import pico-extras for audio_i2s library
set(PICO_EXTRAS_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-extras CACHE PATH "Path to pico-extras")
if(EXISTS ${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
    include(${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
endif()

project(murmdoom C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Initialize pico-extras if available
if(COMMAND pico_extras_init)
    pico_extras_init()
endif()

# Add fatfs
add_subdirectory(src/fatfs)
# Add drivers
add_subdirectory(drivers/sdcard)
add_subdirectory(drivers/ps2kbd)

# Create a library for other drivers
add_library(drivers
    drivers/HDMI.c
    drivers/psram_init.c
    drivers/psram_allocator.c
)
target_include_directories(drivers PUBLIC drivers)
target_link_libraries(drivers pico_stdlib hardware_dma hardware_pio hardware_spi)

# Doomgeneric sources
file(GLOB DOOMGENERIC_SOURCES src/doomgeneric/doomgeneric/*.c)

# Exclude platform specific and replaced files
list(FILTER DOOMGENERIC_SOURCES EXCLUDE REGEX ".*doomgeneric_.*\\.c$") # Exclude all doomgeneric_*.c
list(FILTER DOOMGENERIC_SOURCES EXCLUDE REGEX ".*i_.*\\.c$") # Exclude all i_*.c
list(FILTER DOOMGENERIC_SOURCES EXCLUDE REGEX ".*w_file_stdc\\.c$")
list(FILTER DOOMGENERIC_SOURCES EXCLUDE REGEX ".*m_misc\\.c$")

# Add back necessary i_*.c files
# We need i_video.c, i_timer.c, i_input.c, i_sound.c, i_endoom.c
list(APPEND DOOMGENERIC_SOURCES
    src/doomgeneric/doomgeneric/i_video.c
    src/doomgeneric/doomgeneric/i_timer.c
    src/doomgeneric/doomgeneric/i_input.c
    src/doomgeneric/doomgeneric/i_sound.c
    src/doomgeneric/doomgeneric/i_endoom.c
    src/doomgeneric/doomgeneric/doomgeneric.c # Re-add doomgeneric.c if it was excluded
    src/doomgeneric/doomgeneric/wi_stuff.c
    src/pico/i_picosound.c
    src/pico/i_oplmusic.c
    src/midifile.c
    src/opl/emu8950.c
    src/opl/emuadpcm.c
    src/opl/opl_api.c
    src/opl/opl_pico.c
    src/opl/slot_render.cpp
)

# Remove duplicates
list(REMOVE_DUPLICATES DOOMGENERIC_SOURCES)

add_executable(murmdoom
    src/main.c
    src/doomgeneric_rp2350.c
    src/doomgeneric_fatfs/w_file_fatfs.c
    src/doomgeneric_fatfs/m_misc_fatfs.c
    src/doomgeneric_fatfs/stdio_fatfs.c
    src/opl/slot_render_pico.S
    ${DOOMGENERIC_SOURCES}
)

target_include_directories(murmdoom PRIVATE
    src
    src/doomgeneric/doomgeneric
    src/pico
    src/fatfs
    src/opl
    drivers
    drivers/sdcard
)

target_compile_definitions(murmdoom PRIVATE
    BOARD_${BOARD_VARIANT}
    CMAP256
    DOOMGENERIC_RESX=320
    DOOMGENERIC_RESY=240
    FEATURE_SOUND  # Enable sound support
    USE_PICO_SOUND  # Use Pico audio i2s backend
    USE_OPL_MUSIC=1  # Enable OPL music
    USE_EMU8950_OPL=1  # Use emu8950 OPL emulator
    PICO_AUDIO_I2S_PIO=1
    PICO_AUDIO_I2S_STATE_MACHINE=2
    PICO_AUDIO_I2S_DMA_IRQ=1
    NUM_SOUND_CHANNELS=16
    NO_USE_LIBSAMPLERATE
    NO_USE_TIMIDITY
    NO_USE_GUS
    NO_USE_MUSIC_PACKS
    USE_DIRECT_MIDI_LUMP=0  # Use file-based MIDI (works, uses PSRAM allocation)
    # EMU8950 optimizations from rp2040-doom
    EMU8950_NO_WAVE_TABLE_MAP=1
    EMU8950_NO_TLL=1
    EMU8950_NO_FLOAT=1
    EMU8950_NO_TIMER=1
    EMU8950_NO_TEST_FLAG=1
    EMU8950_SIMPLER_NOISE=1
    EMU8950_SHORT_NOISE_UPDATE_CHECK=1
    EMU8950_LINEAR_SKIP=1
    EMU8950_LINEAR_END_OF_NOTE_OPTIMIZATION=1
    EMU8950_NO_PERCUSSION_MODE=1
    EMU8950_LINEAR=1
    EMU8950_ASM=1
    EMU8950_SLOT_RENDER=1
    EMU8950_NO_RATECONV=1
    PICO_ON_DEVICE=1
)

# Set I2S pins based on board variant
if(BOARD_VARIANT STREQUAL "M1")
    target_compile_definitions(murmdoom PRIVATE
        PICO_AUDIO_I2S_DATA_PIN=26
        PICO_AUDIO_I2S_CLOCK_PIN_BASE=27
    )
elseif(BOARD_VARIANT STREQUAL "M2")
    target_compile_definitions(murmdoom PRIVATE
        PICO_AUDIO_I2S_DATA_PIN=9
        PICO_AUDIO_I2S_CLOCK_PIN_BASE=10
    )
endif()

target_link_libraries(murmdoom
    pico_stdlib
    pico_multicore
    pico_audio_i2s
    hardware_spi
    hardware_dma
    hardware_pio
    hardware_pwm
    hardware_irq
    hardware_interp
    sdcard
    ps2kbd
    fatfs
    drivers
)

# Wrap stdio functions to use FatFS
target_link_options(murmdoom PRIVATE
    -Wl,--wrap=fopen
    -Wl,--wrap=fclose
    -Wl,--wrap=fread
    -Wl,--wrap=fgetc
    -Wl,--wrap=fwrite
    -Wl,--wrap=fseek
    -Wl,--wrap=ftell
    -Wl,--wrap=remove
    -Wl,--wrap=rename
)

# Enable USB stdio
pico_enable_stdio_usb(murmdoom 1)
pico_enable_stdio_uart(murmdoom 0)

pico_add_extra_outputs(murmdoom)
