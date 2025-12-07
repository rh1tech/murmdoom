cmake_minimum_required(VERSION 3.13)

# Set board to Pico 2 (RP2350)
set(PICO_BOARD pico2 CACHE STRING "Pico board type")

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
)

# Remove duplicates
list(REMOVE_DUPLICATES DOOMGENERIC_SOURCES)

add_executable(murmdoom
    src/main.c
    src/doomgeneric_rp2350.c
    src/doomgeneric_fatfs/w_file_fatfs.c
    src/doomgeneric_fatfs/m_misc_fatfs.c
    src/doomgeneric_fatfs/stdio_fatfs.c
    ${DOOMGENERIC_SOURCES}
)

target_include_directories(murmdoom PRIVATE
    src
    src/doomgeneric/doomgeneric
    src/pico
    src/fatfs
    drivers
    drivers/sdcard
)

target_compile_definitions(murmdoom PRIVATE
    CMAP256
    DOOMGENERIC_RESX=320
    DOOMGENERIC_RESY=240
    FEATURE_SOUND  # Enable sound support
    USE_PICO_SOUND  # Use Pico audio i2s backend
    PICO_AUDIO_I2S_DATA_PIN=26
    PICO_AUDIO_I2S_CLOCK_PIN_BASE=27
    PICO_AUDIO_I2S_PIO=1
    PICO_AUDIO_I2S_STATE_MACHINE=2
    PICO_AUDIO_I2S_DMA_IRQ=1
    NUM_SOUND_CHANNELS=16
    NO_USE_LIBSAMPLERATE
    NO_USE_TIMIDITY
    NO_USE_GUS
    NO_USE_MUSIC_PACKS
)

target_link_libraries(murmdoom
    pico_stdlib
    pico_multicore
    pico_audio_i2s
    hardware_spi
    hardware_dma
    hardware_pio
    hardware_pwm
    hardware_irq
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
