#include "doomgeneric.h"
#include "doomtype.h"
#include "doomstat.h"
#include "board_config.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "HDMI.h"
#include "psram_init.h"
#include "psram_allocator.h"
#include "sdcard.h"
#include "ff.h"
#include "ps2kbd_wrapper.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// External variables from i_video.c (when CMAP256 is defined)
extern boolean palette_changed;
// Match struct color from i_video.h (Little Endian: b, g, r, a)
extern struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
} colors[256];

// External stdio init for FatFS
extern void stdio_fatfs_init(void);

// Global FatFs object
FATFS fs;

void DG_Init() {
    // Initialize PSRAM (pin auto-detected based on chip package)
    uint psram_pin = get_psram_pin();
    psram_init(psram_pin);
    psram_set_sram_mode(0); // Use PSRAM

    // Allocate screen buffer in PSRAM
    DG_ScreenBuffer = (pixel_t*)psram_malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(pixel_t));
    if (!DG_ScreenBuffer) {
        panic("DG_Init: OOM for Screen Buffer");
    }
    
    // Clear screen buffer to black
    memset(DG_ScreenBuffer, 0, DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(pixel_t));

    // Initialize HDMI
    graphics_init(g_out_HDMI);
    graphics_set_res(320, 240);
    graphics_set_buffer((uint8_t*)DG_ScreenBuffer);

    // Mount SD Card
    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        panic("Failed to mount SD card");
    }
    
    // Set current directory to root (required for relative paths)
    f_chdir("/");
    
    // Initialize stdio wrapper for FatFS
    stdio_fatfs_init();

    // Initialize PS/2 Keyboard
    ps2kbd_init();
}

void DG_DrawFrame() {
    if (palette_changed) {
        for (int i = 0; i < 256; i++) {
            uint32_t color = (colors[i].r << 16) | (colors[i].g << 8) | colors[i].b;
            graphics_set_palette(i, color);
        }
        palette_changed = false;
    }
}

void DG_SleepMs(uint32_t ms) {
    sleep_ms(ms);
}

uint32_t DG_GetTicksMs() {
    return to_ms_since_boot(get_absolute_time());
}

int DG_GetKey(int* pressed, unsigned char* key) {
    ps2kbd_tick();
    return ps2kbd_get_key(pressed, key);
}

void DG_SetWindowTitle(const char * title) {
}

// I_System implementations

void I_Error(char *error, ...) {
    va_list argptr;
    va_start(argptr, error);
    vprintf(error, argptr);
    va_end(argptr);
    printf("\n");
    while(1) tight_loop_contents();
}

void *I_Realloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (size != 0 && new_ptr == NULL) {
        I_Error("I_Realloc: failed on reallocation of %zu bytes", size);
    }
    return new_ptr;
}

void I_Quit(void) {
    printf("I_Quit\n");
    while(1) tight_loop_contents();
}

byte *I_ZoneBase(int *size) {
    // 4MB PERM minus 512KB scratch = 3.5MB usable for zone + other allocations
    *size = 3 * 1024 * 1024; // 3MB PSRAM for zone
    void *ptr = psram_malloc(*size);
    
    if (!ptr) {
        *size = 2 * 1024 * 1024; // Try 2MB
        ptr = psram_malloc(*size);
    }
    return (byte *)ptr;
}

void I_AtExit(void (*func)(void), boolean run_on_error) {
}

void I_PrintBanner(char *msg) {
    printf("%s\n", msg);
}

void I_PrintDivider(void) {
    printf("------------------------------------------------\n");
}

void I_PrintStartupBanner(char *gamedescription) {
    I_PrintDivider();
    printf("%s\n", gamedescription);
    I_PrintDivider();
}

boolean I_ConsoleStdout(void) {
    return true;
}

// I_Init implementation
void I_InitGraphics(void);
void I_InitTimer(void);

void I_Init(void) {
    I_InitTimer();
    I_InitGraphics();
}

// Missing I_ functions

void I_InitJoystick(void) {}
void I_BindJoystickVariables(void) {}
void I_Tactile(int on, int off, int total) {}

boolean I_GetMemoryValue(unsigned int offset, void *value, int size)
{
    return false;
}
