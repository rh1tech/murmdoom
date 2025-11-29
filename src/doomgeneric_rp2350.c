#include "doomgeneric.h"
#include "doomtype.h"
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

// Global FatFs object
FATFS fs;

void DG_Init() {
    printf("DG_Init: Initializing PSRAM...\n");
    // Initialize PSRAM
    psram_init(PSRAM_CS_PIN);
    psram_set_sram_mode(0); // Use PSRAM
    printf("DG_Init: PSRAM Initialized.\n");

    printf("DG_Init: Allocating Screen Buffer in PSRAM...\n");
    DG_ScreenBuffer = (pixel_t*)psram_malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(pixel_t));
    if (!DG_ScreenBuffer) {
        printf("DG_Init: Failed to allocate screen buffer in PSRAM!\n");
        // Try malloc as fallback? No, we are OOM.
        panic("DG_Init: OOM for Screen Buffer");
    }
    printf("DG_Init: Screen Buffer allocated at %p\n", DG_ScreenBuffer);

    printf("DG_Init: Initializing HDMI...\n");
    // Initialize HDMI
    graphics_init(g_out_HDMI);
    graphics_set_res(320, 200);
    
    // Set buffer to DG_ScreenBuffer
    graphics_set_buffer((uint8_t*)DG_ScreenBuffer);
    printf("DG_Init: HDMI Initialized.\n");

    printf("DG_Init: Mounting SD Card...\n");
    // Mount SD Card
    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        printf("Failed to mount SD card: %d\n", fr);
    } else {
        printf("DG_Init: SD Card Mounted.\n");
    }

    printf("DG_Init: Initializing Keyboard...\n");
    // Initialize PS/2 Keyboard
    ps2kbd_init();
    printf("DG_Init: Keyboard Initialized.\n");
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

void I_Quit(void) {
    printf("I_Quit\n");
    while(1) tight_loop_contents();
}

byte *I_ZoneBase(int *size) {
    *size = 8 * 1024 * 1024; // 8MB PSRAM
    void *ptr = psram_malloc(*size);
    if (!ptr) {
        printf("Failed to allocate PSRAM for Zone\n");
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
