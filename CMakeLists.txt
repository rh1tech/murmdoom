cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(murmdoom C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

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
)

# Remove duplicates
list(REMOVE_DUPLICATES DOOMGENERIC_SOURCES)

add_executable(murmdoom
    src/main.c
    src/doomgeneric_rp2350.c
    src/doomgeneric_fatfs/w_file_fatfs.c
    src/doomgeneric_fatfs/m_misc_fatfs.c
    ${DOOMGENERIC_SOURCES}
)

target_include_directories(murmdoom PRIVATE
    src
    src/doomgeneric/doomgeneric
    src/fatfs
    drivers
    drivers/sdcard
)

target_compile_definitions(murmdoom PRIVATE
    CMAP256
    DOOMGENERIC_RESX=320
    DOOMGENERIC_RESY=200
    NO_SOUND # For now
)

target_link_libraries(murmdoom
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_dma
    hardware_pio
    sdcard
    ps2kbd
    fatfs
    drivers
)

# Enable USB stdio
pico_enable_stdio_usb(murmdoom 1)
pico_enable_stdio_uart(murmdoom 0)

pico_add_extra_outputs(murmdoom)
