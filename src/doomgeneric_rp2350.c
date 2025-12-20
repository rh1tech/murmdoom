#include "doomgeneric.h"
#include "doomtype.h"
#include "doomstat.h"
#include "board_config.h"
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "HDMI.h"
#include "psram_init.h"
#include "psram_allocator.h"
#include "sdcard.h"
#include "ff.h"
#include "ps2kbd_wrapper.h"
#include "ps2mouse_wrapper.h"
#include "usbhid_wrapper.h"
#include "murmdoom_log.h"
#include "doomkeys.h"
#include "m_argv.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static void draw_text_5x7(int x, int y, const char *text, pixel_t color);
static void fill_rect(int x, int y, int w, int h, pixel_t color);

static void fill_rect(int x, int y, int w, int h, pixel_t color) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > DOOMGENERIC_RESX) w = DOOMGENERIC_RESX - x;
    if (y + h > DOOMGENERIC_RESY) h = DOOMGENERIC_RESY - y;
    if (w <= 0 || h <= 0) return;

    for (int yy = y; yy < y + h; ++yy) {
        memset(&DG_ScreenBuffer[yy * DOOMGENERIC_RESX + x], color, (size_t)w);
    }
}

static void ascii_to_upper(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) return;
    size_t i = 0;
    for (; src[i] && (i + 1) < dst_size; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');
        dst[i] = (char)c;
    }
    dst[i] = '\0';
}

static void render_iwad_menu(const char *const *available,
                             int available_count,
                             int selected,
                             int menu_x,
                             int menu_y,
                             int line_h,
                             int menu_w,
                             int menu_h,
                             int highlight_h) {
    // Clear menu area.
    fill_rect(menu_x - 2, menu_y - 2, menu_w + 4, menu_h + 4, 0);

    if (available_count <= 0) {
        draw_text_5x7(menu_x, menu_y, "NONE", 1);
        return;
    }

    for (int i = 0; i < available_count; ++i) {
        const int y = menu_y + i * line_h;
        char disp[32];
        ascii_to_upper(disp, sizeof(disp), available[i]);

        if (i == selected) {
            fill_rect(menu_x - 2, y - 1, menu_w + 4, highlight_h, 1);
            draw_text_5x7(menu_x, y, disp, 0);
        } else {
            draw_text_5x7(menu_x, y, disp, 1);
        }
    }
}

static void draw_char_5x7(int x, int y, char ch, pixel_t color) {
    // 5x7 glyphs, bits are MSB->LSB across 5 columns.
    // Only implements characters needed for the start screen text.
    const uint8_t *rows = NULL;
    static const uint8_t glyph_space[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t glyph_dot[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    static const uint8_t glyph_colon[7] = {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
    static const uint8_t glyph_hyphen[7] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};

    static const uint8_t glyph_0[7] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    static const uint8_t glyph_1[7] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
    static const uint8_t glyph_2[7] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    static const uint8_t glyph_3[7] = {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
    static const uint8_t glyph_4[7] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    static const uint8_t glyph_5[7] = {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
    static const uint8_t glyph_6[7] = {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    static const uint8_t glyph_7[7] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    static const uint8_t glyph_8[7] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    static const uint8_t glyph_9[7] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};

    static const uint8_t glyph_a[7] = {0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F};
    static const uint8_t glyph_b[7] = {0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x1E};
    static const uint8_t glyph_c[7] = {0x00, 0x00, 0x0E, 0x11, 0x10, 0x11, 0x0E};
    static const uint8_t glyph_d[7] = {0x01, 0x01, 0x0D, 0x13, 0x11, 0x13, 0x0D};
    static const uint8_t glyph_e[7] = {0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0F};
    static const uint8_t glyph_f[7] = {0x06, 0x08, 0x1E, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t glyph_g[7] = {0x00, 0x00, 0x0F, 0x11, 0x0F, 0x01, 0x0E};
    static const uint8_t glyph_h[7] = {0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x11};
    static const uint8_t glyph_i[7] = {0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E};
    static const uint8_t glyph_j[7] = {0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0C};
    static const uint8_t glyph_k[7] = {0x10, 0x10, 0x11, 0x12, 0x1C, 0x12, 0x11};
    static const uint8_t glyph_l[7] = {0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x06};
    static const uint8_t glyph_m[7] = {0x00, 0x00, 0x1A, 0x15, 0x15, 0x15, 0x15};
    static const uint8_t glyph_n[7] = {0x00, 0x00, 0x1E, 0x11, 0x11, 0x11, 0x11};
    static const uint8_t glyph_o[7] = {0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t glyph_p[7] = {0x00, 0x00, 0x1E, 0x11, 0x1E, 0x10, 0x10};
    static const uint8_t glyph_q[7] = {0x00, 0x00, 0x0D, 0x13, 0x13, 0x0D, 0x01};
    static const uint8_t glyph_r[7] = {0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10};
    static const uint8_t glyph_s[7] = {0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E};
    static const uint8_t glyph_t[7] = {0x04, 0x04, 0x1F, 0x04, 0x04, 0x04, 0x03};
    static const uint8_t glyph_u[7] = {0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0D};
    static const uint8_t glyph_v[7] = {0x00, 0x00, 0x11, 0x11, 0x11, 0x0A, 0x04};
    static const uint8_t glyph_w[7] = {0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0A};
    static const uint8_t glyph_x[7] = {0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11};
    static const uint8_t glyph_y[7] = {0x00, 0x00, 0x11, 0x11, 0x0F, 0x01, 0x0E};
    static const uint8_t glyph_z[7] = {0x00, 0x00, 0x1F, 0x02, 0x04, 0x08, 0x1F};

    static const uint8_t glyph_A[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const uint8_t glyph_B[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    static const uint8_t glyph_C[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    static const uint8_t glyph_D[7] = {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    static const uint8_t glyph_E[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    static const uint8_t glyph_F[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    static const uint8_t glyph_G[7] = {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
    static const uint8_t glyph_H[7] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    static const uint8_t glyph_I[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
    static const uint8_t glyph_J[7] = {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C};
    static const uint8_t glyph_K[7] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    static const uint8_t glyph_L[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    static const uint8_t glyph_M[7] = {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    static const uint8_t glyph_N[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    static const uint8_t glyph_O[7] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t glyph_P[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    static const uint8_t glyph_Q[7] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
    static const uint8_t glyph_R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    static const uint8_t glyph_S[7] = {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    static const uint8_t glyph_T[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    static const uint8_t glyph_U[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    static const uint8_t glyph_V[7] = {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04};
    static const uint8_t glyph_W[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
    static const uint8_t glyph_X[7] = {0x11, 0x0A, 0x04, 0x04, 0x04, 0x0A, 0x11};
    static const uint8_t glyph_Y[7] = {0x11, 0x0A, 0x04, 0x04, 0x04, 0x04, 0x04};
    static const uint8_t glyph_Z[7] = {0x1F, 0x02, 0x04, 0x08, 0x10, 0x10, 0x1F};

    int c = (unsigned char)ch;

    switch (c) {
        case ' ': rows = glyph_space; break;
        case '.': rows = glyph_dot; break;
        case ':': rows = glyph_colon; break;
        case '-': rows = glyph_hyphen; break;

        case '0': rows = glyph_0; break;
        case '1': rows = glyph_1; break;
        case '2': rows = glyph_2; break;
        case '3': rows = glyph_3; break;
        case '4': rows = glyph_4; break;
        case '5': rows = glyph_5; break;
        case '6': rows = glyph_6; break;
        case '7': rows = glyph_7; break;
        case '8': rows = glyph_8; break;
        case '9': rows = glyph_9; break;

        case 'a': rows = glyph_a; break;
        case 'b': rows = glyph_b; break;
        case 'c': rows = glyph_c; break;
        case 'd': rows = glyph_d; break;
        case 'e': rows = glyph_e; break;
        case 'f': rows = glyph_f; break;
        case 'g': rows = glyph_g; break;
        case 'h': rows = glyph_h; break;
        case 'i': rows = glyph_i; break;
        case 'j': rows = glyph_j; break;
        case 'k': rows = glyph_k; break;
        case 'l': rows = glyph_l; break;
        case 'm': rows = glyph_m; break;
        case 'n': rows = glyph_n; break;
        case 'o': rows = glyph_o; break;
        case 'p': rows = glyph_p; break;
        case 'q': rows = glyph_q; break;
        case 'r': rows = glyph_r; break;
        case 's': rows = glyph_s; break;
        case 't': rows = glyph_t; break;
        case 'u': rows = glyph_u; break;
        case 'v': rows = glyph_v; break;
        case 'w': rows = glyph_w; break;
        case 'x': rows = glyph_x; break;
        case 'y': rows = glyph_y; break;
        case 'z': rows = glyph_z; break;

        case 'A': rows = glyph_A; break;
        case 'B': rows = glyph_B; break;
        case 'C': rows = glyph_C; break;
        case 'D': rows = glyph_D; break;
        case 'E': rows = glyph_E; break;
        case 'F': rows = glyph_F; break;
        case 'G': rows = glyph_G; break;
        case 'H': rows = glyph_H; break;
        case 'I': rows = glyph_I; break;
        case 'J': rows = glyph_J; break;
        case 'K': rows = glyph_K; break;
        case 'L': rows = glyph_L; break;
        case 'M': rows = glyph_M; break;
        case 'N': rows = glyph_N; break;
        case 'O': rows = glyph_O; break;
        case 'P': rows = glyph_P; break;
        case 'Q': rows = glyph_Q; break;
        case 'R': rows = glyph_R; break;
        case 'S': rows = glyph_S; break;
        case 'T': rows = glyph_T; break;
        case 'U': rows = glyph_U; break;
        case 'V': rows = glyph_V; break;
        case 'W': rows = glyph_W; break;
        case 'X': rows = glyph_X; break;
        case 'Y': rows = glyph_Y; break;
        case 'Z': rows = glyph_Z; break;

        default: rows = glyph_space; break;
    }

    for (int row = 0; row < 7; ++row) {
        int yy = y + row;
        if (yy < 0 || yy >= DOOMGENERIC_RESY) continue;
        uint8_t bits = rows[row];
        for (int col = 0; col < 5; ++col) {
            int xx = x + col;
            if (xx < 0 || xx >= DOOMGENERIC_RESX) continue;
            if (bits & (1u << (4 - col))) {
                DG_ScreenBuffer[yy * DOOMGENERIC_RESX + xx] = color;
            }
        }
    }
}

static void draw_text_5x7(int x, int y, const char *text, pixel_t color) {
    // 5px glyph + 1px spacing
    const int advance = 6;
    for (const char *p = text; *p; ++p) {
        draw_char_5x7(x, y, *p, color);
        x += advance;
    }
}

static int ascii_tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
    return c;
}

static int ascii_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = ascii_tolower((unsigned char)*a);
        int cb = ascii_tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        ++a;
        ++b;
    }
    return ascii_tolower((unsigned char)*a) - ascii_tolower((unsigned char)*b);
}

static void console_vprintf(const char *fmt, va_list ap) {
    vprintf(fmt, ap);
}

static void console_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    console_vprintf(fmt, ap);
    va_end(ap);
}

static bool has_wad_extension(const char *name) {
    size_t n = strlen(name);
    if (n < 4) return false;
    return (ascii_tolower((unsigned char)name[n - 4]) == '.'
         && ascii_tolower((unsigned char)name[n - 3]) == 'w'
         && ascii_tolower((unsigned char)name[n - 2]) == 'a'
         && ascii_tolower((unsigned char)name[n - 1]) == 'd');
}

static bool is_known_compatible_iwad_name(const char *name) {
    // Keep this aligned with the engine's IWAD table in d_iwad.c.
    static const char *const known[] = {
        "doom2.wad",
        "plutonia.wad",
        "tnt.wad",
        "doom.wad",
        "doom1.wad",
        "chex.wad",
        "hacx.wad",
        "freedm.wad",
        "freedoom2.wad",
        "freedoom1.wad",
        "heretic.wad",
        "heretic1.wad",
        "hexen.wad",
        "strife1.wad",
    };

    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); ++i) {
        if (ascii_strcasecmp(name, known[i]) == 0) return true;
    }
    return false;
}

static void print_available_wads_to_console(void) {
    console_printf("\n=== WAD scan (/) ===\n");
    fflush(stdout);

    // 1) Print known-compatible IWADs found by exact name.
    static const char *const iwads[] = {
        "doom2.wad",
        "plutonia.wad",
        "tnt.wad",
        "doom.wad",
        "doom1.wad",
        "chex.wad",
        "hacx.wad",
        "freedm.wad",
        "freedoom2.wad",
        "freedoom1.wad",
        "heretic.wad",
        "heretic1.wad",
        "hexen.wad",
        "strife1.wad",
    };

    console_printf("Compatible IWAD filenames:\n");
    int compatible_found = 0;
    for (size_t i = 0; i < sizeof(iwads) / sizeof(iwads[0]); ++i) {
        FILINFO info;
        FRESULT fr = f_stat(iwads[i], &info);
        if (fr == FR_OK) {
            console_printf("  [FOUND] %s\n", iwads[i]);
            compatible_found++;
        }
    }
    if (!compatible_found) {
        console_printf("  (none found by standard IWAD names)\n");
    }
    fflush(stdout);

    // 2) Print any other .wad files in the directory (likely PWADs or nonstandard names).
    console_printf("Other .wad files:\n");
    DIR dir;
    FILINFO fno;
    FRESULT fr = f_opendir(&dir, "/");
    if (fr != FR_OK) {
        console_printf("  (failed to open root dir: %d)\n", (int)fr);
        fflush(stdout);
        return;
    }

    int other_count = 0;
    for (;;) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;

        // Skip directories.
        if (fno.fattrib & AM_DIR) continue;
        if (!has_wad_extension(fno.fname)) continue;
        if (is_known_compatible_iwad_name(fno.fname)) continue;

        console_printf("  %s\n", fno.fname);
        other_count++;
    }
    f_closedir(&dir);

    if (!other_count) {
        console_printf("  (none)\n");
    }

    console_printf("=== Press Enter to start ===\n\n");
    fflush(stdout);
}

// External variables from i_video.c (when CMAP256 is defined)
extern boolean palette_changed;

#include "murmdoom_log.h"
// Match struct color from i_video.h (Little Endian: b, g, r, a)
extern struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
} colors[256];

#include "pico/stdio_usb.h"

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

    // Initialize PS/2 Mouse
    ps2mouse_wrapper_init();

    // Initialize USB HID (keyboard/mouse) if enabled
    usbhid_wrapper_init();
}

void DG_StartScreen(void) {
    // Solid black background using palette index 0.
    graphics_set_palette(0, 0x000000);
    graphics_set_palette(1, 0xFFFFFF);
    memset(DG_ScreenBuffer, 0, DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(pixel_t));

    // Draw available Doom IWADs (only Doom, not Heretic/Hexen/etc).
    static const char *const doom_iwads[] = {
        "doom2.wad",
        "plutonia.wad",
        "tnt.wad",
        "doom.wad",
        "doom1.wad",
    };

    const int menu_x = 8;
    const int menu_y = 32;
    const int line_h = 10;
    const int menu_w = 6 * 14; // enough for "PLUTONIA.WAD" + padding
    const int menu_h = (int)(sizeof(doom_iwads) / sizeof(doom_iwads[0])) * line_h;
    const int highlight_h = 9;

    draw_text_5x7(menu_x, 16, "DOOM IWADS:", 1);
    draw_text_5x7(menu_x, DOOMGENERIC_RESY - 16, "PRESS ENTER", 1);

    // Build list of available IWADs.
    const char *available[sizeof(doom_iwads) / sizeof(doom_iwads[0])];
    int available_count = 0;
    for (size_t i = 0; i < sizeof(doom_iwads) / sizeof(doom_iwads[0]); ++i) {
        FILINFO info;
        if (f_stat(doom_iwads[i], &info) == FR_OK) {
            available[available_count++] = doom_iwads[i];
        }
    }

    int selected = 0;
    render_iwad_menu(available, available_count, selected, menu_x, menu_y, line_h, menu_w, menu_h, highlight_h);

    // Print WAD scan after ~3s or as soon as USB CDC is connected.
    // If the first print happens while disconnected (common if the host opens the monitor late),
    // print again once after it connects.
    const uint32_t start_ms = to_ms_since_boot(get_absolute_time());
    bool printed = false;
    bool printed_while_disconnected = false;

    // Wait for Enter.
    while (true) {
        bool usb_connected = true;
#if PICO_STDIO_USB
        usb_connected = stdio_usb_connected();
#endif

        if (!printed) {
            const uint32_t elapsed_ms = to_ms_since_boot(get_absolute_time()) - start_ms;
            if (usb_connected || elapsed_ms >= 3000) {
                print_available_wads_to_console();
                printed = true;
                printed_while_disconnected = !usb_connected;
            }
        } else if (printed_while_disconnected && usb_connected) {
            print_available_wads_to_console();
            printed_while_disconnected = false;
        }

        int pressed = 0;
        unsigned char key = 0;
        while (DG_GetKey(&pressed, &key)) {
            if (pressed && key == KEY_ENTER) {
                if (available_count > 0) {
                    static char *new_argv[4];
                    new_argv[0] = (char *)"doom";
                    new_argv[1] = (char *)"-iwad";
                    new_argv[2] = (char *)available[selected];
                    new_argv[3] = NULL;
                    myargc = 3;
                    myargv = new_argv;
                }
                return;
            }

            if (!pressed || available_count <= 0) continue;

            if (key == KEY_UPARROW || key == 'w' || key == 'W') {
                selected = (selected - 1 + available_count) % available_count;
                render_iwad_menu(available, available_count, selected, menu_x, menu_y, line_h, menu_w, menu_h, highlight_h);
            } else if (key == KEY_DOWNARROW || key == 's' || key == 'S') {
                selected = (selected + 1) % available_count;
                render_iwad_menu(available, available_count, selected, menu_x, menu_y, line_h, menu_w, menu_h, highlight_h);
            }
        }
        sleep_ms(10);
    }
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
    ps2mouse_wrapper_tick();  // Process PS/2 mouse events
    usbhid_wrapper_tick();    // Process USB HID events
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
    putchar_raw('\n');
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
    // There is no OS to return to on bare metal. Reboot to restart the game.
    watchdog_reboot(0, 0, 0);
    while (1) {
        tight_loop_contents();
    }
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
    MURMDOOM_LOG("%s\n", msg);
}

void I_PrintDivider(void) {
    MURMDOOM_LOG("------------------------------------------------\n");
}

void I_PrintStartupBanner(char *gamedescription) {
    I_PrintDivider();
    MURMDOOM_LOG("%s\n", gamedescription);
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
