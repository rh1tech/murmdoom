#include "doomgeneric.h"
#include "doomtype.h"
#include "doomstat.h"
#include "board_config.h"
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
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
static int text_width_5x7(const char *text, int scale);

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

typedef struct {
    const char *filename;
    const char *label;
} iwad_entry_t;

static int max_label_width_5x7(const iwad_entry_t *const *entries, int entry_count, int scale) {
    int max_w = 0;
    for (int i = 0; i < entry_count; ++i) {
        const char *label = entries[i]->label;
        int w = text_width_5x7(label, scale);
        if (w > max_w) max_w = w;
    }
    return max_w;
}

static void render_iwad_menu_entries(const iwad_entry_t *const *available,
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
        const char *disp = available[i]->label;

        if (i == selected) {
            fill_rect(menu_x - 2, y - 1, menu_w + 4, highlight_h, 1);
            draw_text_5x7(menu_x, y, disp, 0);
        } else {
            draw_text_5x7(menu_x, y, disp, 1);
        }
    }
}

static const uint8_t *glyph_5x7(char ch) {
    // 5x7 glyphs, bits are MSB->LSB across 5 columns.
    // Only implements ASCII needed for the start screen UI.
    static const uint8_t glyph_space[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t glyph_dot[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    static const uint8_t glyph_comma[7] = {0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x08};
    static const uint8_t glyph_colon[7] = {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00};
    static const uint8_t glyph_hyphen[7] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    static const uint8_t glyph_lparen[7] = {0x04, 0x08, 0x08, 0x08, 0x08, 0x08, 0x04};
    static const uint8_t glyph_rparen[7] = {0x04, 0x02, 0x02, 0x02, 0x02, 0x02, 0x04};
    static const uint8_t glyph_slash[7] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00};

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
        case ' ': return glyph_space;
        case '.': return glyph_dot;
        case ',': return glyph_comma;
        case ':': return glyph_colon;
        case '-': return glyph_hyphen;
        case '(': return glyph_lparen;
        case ')': return glyph_rparen;
        case '/': return glyph_slash;

        case '0': return glyph_0;
        case '1': return glyph_1;
        case '2': return glyph_2;
        case '3': return glyph_3;
        case '4': return glyph_4;
        case '5': return glyph_5;
        case '6': return glyph_6;
        case '7': return glyph_7;
        case '8': return glyph_8;
        case '9': return glyph_9;

        case 'a': return glyph_a;
        case 'b': return glyph_b;
        case 'c': return glyph_c;
        case 'd': return glyph_d;
        case 'e': return glyph_e;
        case 'f': return glyph_f;
        case 'g': return glyph_g;
        case 'h': return glyph_h;
        case 'i': return glyph_i;
        case 'j': return glyph_j;
        case 'k': return glyph_k;
        case 'l': return glyph_l;
        case 'm': return glyph_m;
        case 'n': return glyph_n;
        case 'o': return glyph_o;
        case 'p': return glyph_p;
        case 'q': return glyph_q;
        case 'r': return glyph_r;
        case 's': return glyph_s;
        case 't': return glyph_t;
        case 'u': return glyph_u;
        case 'v': return glyph_v;
        case 'w': return glyph_w;
        case 'x': return glyph_x;
        case 'y': return glyph_y;
        case 'z': return glyph_z;

        case 'A': return glyph_A;
        case 'B': return glyph_B;
        case 'C': return glyph_C;
        case 'D': return glyph_D;
        case 'E': return glyph_E;
        case 'F': return glyph_F;
        case 'G': return glyph_G;
        case 'H': return glyph_H;
        case 'I': return glyph_I;
        case 'J': return glyph_J;
        case 'K': return glyph_K;
        case 'L': return glyph_L;
        case 'M': return glyph_M;
        case 'N': return glyph_N;
        case 'O': return glyph_O;
        case 'P': return glyph_P;
        case 'Q': return glyph_Q;
        case 'R': return glyph_R;
        case 'S': return glyph_S;
        case 'T': return glyph_T;
        case 'U': return glyph_U;
        case 'V': return glyph_V;
        case 'W': return glyph_W;
        case 'X': return glyph_X;
        case 'Y': return glyph_Y;
        case 'Z': return glyph_Z;

        default: return glyph_space;
    }
}

static void draw_char_5x7(int x, int y, char ch, pixel_t color) {
    const uint8_t *rows = glyph_5x7(ch);
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

static void draw_char_5x7_scaled(int x, int y, char ch, pixel_t color, int scale, bool bold) {
    if (scale <= 1) {
        draw_char_5x7(x, y, ch, color);
        if (bold) draw_char_5x7(x + 1, y, ch, color);
        return;
    }

    const uint8_t *rows = glyph_5x7(ch);
    for (int row = 0; row < 7; ++row) {
        uint8_t bits = rows[row];
        for (int col = 0; col < 5; ++col) {
            if (!(bits & (1u << (4 - col)))) continue;
            const int px = x + col * scale;
            const int py = y + row * scale;
            fill_rect(px, py, scale, scale, color);
            if (bold) fill_rect(px + 1, py, scale, scale, color);
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

static void draw_text_5x7_scaled(int x, int y, const char *text, pixel_t color, int scale, bool bold) {
    const int advance = 6 * (scale <= 0 ? 1 : scale);
    if (scale <= 0) scale = 1;
    for (const char *p = text; *p; ++p) {
        draw_char_5x7_scaled(x, y, *p, color, scale, bold);
        x += advance;
    }
}

static int text_width_5x7(const char *text, int scale) {
    if (scale <= 0) scale = 1;
    int n = 0;
    for (const char *p = text; *p; ++p) n++;
    return n * 6 * scale;
}

static void draw_animated_background(uint32_t t_ms) {
    const int t = (int)(t_ms / 80);
    for (int y = 0; y < DOOMGENERIC_RESY; ++y) {
        for (int x = 0; x < DOOMGENERIC_RESX; ++x) {
            // Blocky, clearly animated pattern (still dark Doom-ish palette).
            const int bx = (x >> 3);
            const int by = (y >> 3);
            uint8_t v = (uint8_t)((bx + by + t) & 0x0F);
            v ^= (uint8_t)(((bx << 1) ^ (by + (t >> 1))) & 0x07);
            DG_ScreenBuffer[y * DOOMGENERIC_RESX + x] = (pixel_t)(2 + (v & 0x0F));
        }
    }
}

static void draw_animated_background_border(uint32_t t_ms,
                                           int panel_x,
                                           int panel_y,
                                           int panel_w,
                                           int panel_h) {
    const int t = (int)(t_ms / 80);

    if (panel_x < 0) panel_x = 0;
    if (panel_y < 0) panel_y = 0;
    if (panel_w < 0) panel_w = 0;
    if (panel_h < 0) panel_h = 0;
    if (panel_x + panel_w > DOOMGENERIC_RESX) panel_w = DOOMGENERIC_RESX - panel_x;
    if (panel_y + panel_h > DOOMGENERIC_RESY) panel_h = DOOMGENERIC_RESY - panel_y;

    const int panel_x2 = panel_x + panel_w;
    const int panel_y2 = panel_y + panel_h;

    // Top strip.
    for (int y = 0; y < panel_y; ++y) {
        for (int x = 0; x < DOOMGENERIC_RESX; ++x) {
            const int bx = (x >> 3);
            const int by = (y >> 3);
            uint8_t v = (uint8_t)((bx + by + t) & 0x0F);
            v ^= (uint8_t)(((bx << 1) ^ (by + (t >> 1))) & 0x07);
            DG_ScreenBuffer[y * DOOMGENERIC_RESX + x] = (pixel_t)(2 + (v & 0x0F));
        }
    }

    // Bottom strip.
    for (int y = panel_y2; y < DOOMGENERIC_RESY; ++y) {
        for (int x = 0; x < DOOMGENERIC_RESX; ++x) {
            const int bx = (x >> 3);
            const int by = (y >> 3);
            uint8_t v = (uint8_t)((bx + by + t) & 0x0F);
            v ^= (uint8_t)(((bx << 1) ^ (by + (t >> 1))) & 0x07);
            DG_ScreenBuffer[y * DOOMGENERIC_RESX + x] = (pixel_t)(2 + (v & 0x0F));
        }
    }

    // Left/right strips.
    for (int y = panel_y; y < panel_y2; ++y) {
        for (int x = 0; x < panel_x; ++x) {
            const int bx = (x >> 3);
            const int by = (y >> 3);
            uint8_t v = (uint8_t)((bx + by + t) & 0x0F);
            v ^= (uint8_t)(((bx << 1) ^ (by + (t >> 1))) & 0x07);
            DG_ScreenBuffer[y * DOOMGENERIC_RESX + x] = (pixel_t)(2 + (v & 0x0F));
        }
        for (int x = panel_x2; x < DOOMGENERIC_RESX; ++x) {
            const int bx = (x >> 3);
            const int by = (y >> 3);
            uint8_t v = (uint8_t)((bx + by + t) & 0x0F);
            v ^= (uint8_t)(((bx << 1) ^ (by + (t >> 1))) & 0x07);
            DG_ScreenBuffer[y * DOOMGENERIC_RESX + x] = (pixel_t)(2 + (v & 0x0F));
        }
    }
}

static void fade_start_screen_bg_to_black(const uint32_t bg_pal[16], uint32_t title_hl_rgb, int steps, int step_delay_ms) {
    if (steps <= 0) return;
    if (step_delay_ms < 0) step_delay_ms = 0;

    for (int s = 0; s <= steps; ++s) {
        const int num = steps - s;
        const int den = steps;

        for (int i = 0; i < 16; ++i) {
            const uint32_t base = bg_pal[i];
            uint32_t r = (base >> 16) & 0xFF;
            uint32_t g = (base >> 8) & 0xFF;
            uint32_t b = base & 0xFF;
            r = (r * (uint32_t)num) / (uint32_t)den;
            g = (g * (uint32_t)num) / (uint32_t)den;
            b = (b * (uint32_t)num) / (uint32_t)den;
            graphics_set_palette(2 + i, (r << 16) | (g << 8) | b);
        }

        {
            const uint32_t base = title_hl_rgb;
            uint32_t r = (base >> 16) & 0xFF;
            uint32_t g = (base >> 8) & 0xFF;
            uint32_t b = base & 0xFF;
            r = (r * (uint32_t)num) / (uint32_t)den;
            g = (g * (uint32_t)num) / (uint32_t)den;
            b = (b * (uint32_t)num) / (uint32_t)den;
            graphics_set_palette(18, (r << 16) | (g << 8) | b);
        }

        if (step_delay_ms) sleep_ms((uint32_t)step_delay_ms);
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

// Only available when USB CDC stdio is enabled (not in USB HID host builds).
#if PICO_STDIO_USB
#include "pico/stdio_usb.h"
#endif

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
    // Reserved for the loading window text/border (keep bright even after we dim UI text).
    graphics_set_palette(20, 0xFFFFFF);

    // Background palette (2..17): dark Doom-ish reds/browns.
    static const uint32_t doom_bg_pal[16] = {
        0x050100, 0x080200, 0x0B0301, 0x0E0401,
        0x120501, 0x160602, 0x1A0702, 0x1E0803,
        0x220903, 0x260A04, 0x2A0B04, 0x2E0C05,
        0x330D05, 0x380E06, 0x3D0F06, 0x421007,
    };
    for (int i = 0; i < 16; ++i) {
        graphics_set_palette(2 + i, doom_bg_pal[i]);
    }

    // Title highlight: 20% brighter than the brightest demo red.
    uint32_t title_hl_rgb;
    {
        const uint32_t base = doom_bg_pal[15];
        uint32_t r = (base >> 16) & 0xFF;
        uint32_t g = (base >> 8) & 0xFF;
        uint32_t b = base & 0xFF;
        r = (r * 6) / 5;
        g = (g * 6) / 5;
        b = (b * 6) / 5;
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        title_hl_rgb = (r << 16) | (g << 8) | b;
        graphics_set_palette(18, title_hl_rgb);
    }
    memset(DG_ScreenBuffer, 0, DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(pixel_t));

    // Draw available Doom IWADs (only Doom, not Heretic/Hexen/etc).
    static const iwad_entry_t doom_iwads[] = {
        {"doom2.wad", "DOOM II (doom2.wad)"},
        {"plutonia.wad", "PLUTONIA (plutonia.wad)"},
        {"tnt.wad", "TNT (tnt.wad)"},
        {"doom.wad", "DOOM (doom.wad)"},
        {"doom1.wad", "DOOM SHAREWARE (doom1.wad)"},
    };

    const int panel_x = 24;
    const int panel_y = 24;
    const int panel_w = DOOMGENERIC_RESX - 48;
    const int panel_h = DOOMGENERIC_RESY - 48;

    // Keep left margin small so long status text fits.
    const int menu_x = panel_x + 4;
    const int menu_y = panel_y + 56;
    const int line_h = 10;
    const int menu_h = (int)(sizeof(doom_iwads) / sizeof(doom_iwads[0])) * line_h;
    const int highlight_h = 9;

    // Build list of available IWADs.
    const iwad_entry_t *available[sizeof(doom_iwads) / sizeof(doom_iwads[0])];
    int available_count = 0;
    for (size_t i = 0; i < sizeof(doom_iwads) / sizeof(doom_iwads[0]); ++i) {
        FILINFO info;
        if (f_stat(doom_iwads[i].filename, &info) == FR_OK) {
            available[available_count++] = &doom_iwads[i];
        }
    }

    int selected = 0;

#ifndef MURMDOOM_VERSION
#define MURMDOOM_VERSION "?"
#endif

    const char *title_left = "MurmDOOM";
    char title_right[96];
    snprintf(title_right, sizeof(title_right), " by Mikhail Matveev v%s", MURMDOOM_VERSION);

    // Print WAD scan after ~3s or as soon as USB CDC is connected.
    // If the first print happens while disconnected (common if the host opens the monitor late),
    // print again once after it connects.
    const uint32_t start_ms = to_ms_since_boot(get_absolute_time());
    bool printed = false;
    bool printed_while_disconnected = false;

    // Draw static UI once; animate only outside the panel to avoid overwriting text.
    const int title_scale = 1;
    const int title_left_w = text_width_5x7(title_left, title_scale);
    const int title_right_w = text_width_5x7(title_right, title_scale);
    const int title_w = title_left_w + title_right_w;
    const int title_x = (DOOMGENERIC_RESX - title_w) / 2;

    // Status lines
#ifndef DBOARD_VARIANT
#if defined(BOARD_M2)
#define DBOARD_VARIANT "M2"
#else
#define DBOARD_VARIANT "M1"
#endif
#endif
#ifndef DPSRAM_SPEED
#define DPSRAM_SPEED PSRAM_MAX_FREQ_MHZ
#endif

#ifndef DCPU_SPEED
#define DCPU_SPEED CPU_CLOCK_MHZ
#endif

    const char *cfg = DBOARD_VARIANT;
    // Prefer configured build values for display.
    const uint32_t cpu_mhz = (uint32_t)DCPU_SPEED;
    const uint32_t psram_cs = get_psram_pin();
    char status1[96];
    char status2[96];
    snprintf(status1, sizeof(status1), "Up/Down: select, Enter: confirm");
    snprintf(status2, sizeof(status2), "%s, FREQ: %lu MHz, PSRAM: %d MHz, CS: %lu",
             cfg,
             (unsigned long)cpu_mhz,
             (int)DPSRAM_SPEED,
             (unsigned long)psram_cs);
    const char *status3 = "https://github.com/rh1tech/murmdoom";

    // Compute menu width from available labels.
    const int menu_w = (available_count > 0 ? max_label_width_5x7(available, available_count, 1) : (6 * 4));

    draw_animated_background_border(to_ms_since_boot(get_absolute_time()), panel_x, panel_y, panel_w, panel_h);
    fill_rect(panel_x, panel_y, panel_w, panel_h, 0);
    // Highlight "MurmDOOM" with red background and black text.
    const int title_y = panel_y + 10;
    fill_rect(title_x - 2, title_y - 2, title_left_w + 4, 7 * title_scale + 4, 18);
    draw_text_5x7_scaled(title_x, title_y, title_left, 0, title_scale, false);
    draw_text_5x7_scaled(title_x + title_left_w, title_y, title_right, 1, title_scale, false);

    draw_text_5x7(menu_x, menu_y - 14, "Select WAD:", 1);
    const int bottom_y0 = panel_y + panel_h - 32;
    draw_text_5x7(menu_x, bottom_y0 + 0, status1, 1);
    draw_text_5x7(menu_x, bottom_y0 + 10, status2, 1);
    draw_text_5x7(menu_x, bottom_y0 + 20, status3, 1);
    render_iwad_menu_entries(available, available_count, selected, menu_x, menu_y, line_h, menu_w, menu_h, highlight_h);

    int prev_selected = selected;

    // Wait for Enter.
    while (true) {
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        draw_animated_background_border(now_ms, panel_x, panel_y, panel_w, panel_h);

        if (prev_selected != selected) {
            render_iwad_menu_entries(available, available_count, selected, menu_x, menu_y, line_h, menu_w, menu_h, highlight_h);
            prev_selected = selected;
        }

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
                    // Make all existing UI text dark grey (palette index 1) so it's readable
                    // on the fading background, but keep the loading window text bright.
                    graphics_set_palette(1, 0x404040);

                    // Title left was previously black-on-red; redraw it using the (now grey) text color.
                    draw_text_5x7_scaled(title_x, title_y, title_left, 1, title_scale, false);

                    // Remove selection inversion (black text on grey highlight) by redrawing the menu
                    // without a selected item.
                    render_iwad_menu_entries(available, available_count, -1, menu_x, menu_y, line_h, menu_w, menu_h, highlight_h);

                    // Stop animation, fade out the start-screen background, and show a centered loading window.
                    fade_start_screen_bg_to_black(doom_bg_pal, title_hl_rgb, 10, 20);

                    char msg[96];
                    snprintf(msg, sizeof(msg), "Loading WAD: %s...", available[selected]->filename);

                    const int pad_x = 10;
                    const int pad_y = 8;
                    const int msg_w = text_width_5x7(msg, 1);
                    int win_w = msg_w + pad_x * 2;
                    if (win_w > DOOMGENERIC_RESX - 40) win_w = DOOMGENERIC_RESX - 40;
                    const int win_h = 7 + pad_y * 2;
                    const int win_x = (DOOMGENERIC_RESX - win_w) / 2;
                    const int win_y = (DOOMGENERIC_RESY - win_h) / 2;

                    fill_rect(win_x, win_y, win_w, win_h, 0);
                    // Border
                    fill_rect(win_x, win_y, win_w, 1, 20);
                    fill_rect(win_x, win_y + win_h - 1, win_w, 1, 20);
                    fill_rect(win_x, win_y, 1, win_h, 20);
                    fill_rect(win_x + win_w - 1, win_y, 1, win_h, 20);

                    draw_text_5x7(win_x + pad_x, win_y + pad_y, msg, 20);

                    static char *new_argv[4];
                    new_argv[0] = (char *)"doom";
                    new_argv[1] = (char *)"-iwad";
                    new_argv[2] = (char *)available[selected]->filename;
                    new_argv[3] = NULL;
                    myargc = 3;
                    myargv = new_argv;
                }
                return;
            }

            if (!pressed || available_count <= 0) continue;

            if (key == KEY_UPARROW || key == 'w' || key == 'W') {
                selected = (selected - 1 + available_count) % available_count;
            } else if (key == KEY_DOWNARROW || key == 's' || key == 'S') {
                selected = (selected + 1) % available_count;
            }
        }
        sleep_ms(33);
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
