/*
 * USB HID Wrapper for DOOM
 * Maps USB HID keyboard/mouse events to DOOM events
 * 
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Include d_event.h first - it pulls in doomtype.h which defines its own boolean type
#include "d_event.h"
#include "doomkeys.h"
#include "usbhid.h"
#include "usbhid_wrapper.h"
#include <stdint.h>

// External DOOM function to post events
extern void D_PostEvent(event_t *ev);

// Mouse sensitivity multiplier (increase for faster response)
#define MOUSE_SENSITIVITY_MULT 2

// Maximum delta per tick to prevent abrupt jumps
#define MOUSE_MAX_DELTA 40

// Track previous button state to detect changes
static uint8_t prev_usb_buttons = 0;

// Clamp value to range
static inline int16_t clamp_delta(int16_t val, int16_t max_val) {
    if (val > max_val) return max_val;
    if (val < -max_val) return -max_val;
    return val;
}

//--------------------------------------------------------------------
// HID Keycode to DOOM Key Mapping
//--------------------------------------------------------------------

static unsigned char hid_to_doom_key(uint8_t hid_keycode) {
    // Modifier pseudo-keycodes (from hid_app.c)
    if (hid_keycode == 0xE0) return KEY_FIRE;    // Ctrl -> FIRE action (shoot)
    if (hid_keycode == 0xE1) return KEY_RSHIFT;  // Shift -> run
    if (hid_keycode == 0xE2) return KEY_RALT;    // Alt -> strafe
    
    // Letters A-Z (HID 0x04-0x1D -> 'a'-'z')
    if (hid_keycode >= 0x04 && hid_keycode <= 0x1D) {
        return 'a' + (hid_keycode - 0x04);
    }
    
    // Numbers 1-9, 0 (HID 0x1E-0x27)
    if (hid_keycode >= 0x1E && hid_keycode <= 0x26) {
        return '1' + (hid_keycode - 0x1E);
    }
    if (hid_keycode == 0x27) return '0';
    
    // Function keys F1-F12 (HID 0x3A-0x45)
    if (hid_keycode >= 0x3A && hid_keycode <= 0x45) {
        return KEY_F1 + (hid_keycode - 0x3A);
    }
    
    // Special keys
    switch (hid_keycode) {
        case 0x28: return KEY_ENTER;       // Enter
        case 0x29: return KEY_ESCAPE;      // Escape
        case 0x2A: return KEY_BACKSPACE;   // Backspace
        case 0x2B: return KEY_TAB;         // Tab
        case 0x2C: return KEY_USE;         // Space -> USE action (open doors)
        case 0x2D: return KEY_MINUS;       // Minus
        case 0x2E: return KEY_EQUALS;      // Equals
        case 0x2F: return '[';             // Left bracket
        case 0x30: return ']';             // Right bracket
        case 0x31: return '\\';            // Backslash
        case 0x33: return ';';             // Semicolon
        case 0x34: return '\'';            // Quote
        case 0x35: return '`';             // Grave/tilde
        case 0x36: return ',';             // Comma
        case 0x37: return '.';             // Period
        case 0x38: return '/';             // Forward slash
        
        // Arrow keys
        case 0x4F: return KEY_RIGHTARROW;
        case 0x50: return KEY_LEFTARROW;
        case 0x51: return KEY_DOWNARROW;
        case 0x52: return KEY_UPARROW;
        
        // Navigation keys
        case 0x49: return KEY_INS;
        case 0x4A: return KEY_HOME;
        case 0x4B: return KEY_PGUP;
        case 0x4C: return KEY_DEL;
        case 0x4D: return KEY_END;
        case 0x4E: return KEY_PGDN;
        
        // Pause
        case 0x48: return KEY_PAUSE;
        
        default: return 0; // Unknown key
    }
}

//--------------------------------------------------------------------
// Initialization
//--------------------------------------------------------------------

static int usb_hid_initialized = 0;

void usbhid_wrapper_init(void) {
#ifdef USB_HID_ENABLED
    usbhid_init();
    usb_hid_initialized = 1;
    prev_usb_buttons = 0;
#endif
}

//--------------------------------------------------------------------
// Mouse Tick - Process USB HID mouse events only
// Keyboard events flow through usbhid_wrapper_get_key() -> DG_GetKey()
//--------------------------------------------------------------------

void usbhid_wrapper_mouse_tick(void) {
#ifdef USB_HID_ENABLED
    if (!usb_hid_initialized) return;
    
    // Process USB host events (this also queues keyboard events for get_key)
    usbhid_task();
    
    // Process mouse events
    usbhid_mouse_state_t mouse;
    usbhid_get_mouse_state(&mouse);
    
    // Only post event if there's actual motion or button change
    // Check for real motion (non-zero deltas) or actual button state change
    int has_real_motion = (mouse.dx != 0 || mouse.dy != 0);
    int buttons_changed = ((mouse.buttons & 0x07) != prev_usb_buttons);
    
    if (has_real_motion || buttons_changed) {
        event_t ev;
        ev.type = ev_mouse;
        
        // USB mouse buttons: bit 0=left, 1=right, 2=middle (same as DOOM)
        // Only update buttons when they actually change
        ev.data1 = mouse.buttons & 0x07;
        
        // Clamp deltas to prevent abrupt movements
        int16_t dx = clamp_delta(mouse.dx, MOUSE_MAX_DELTA);
        int16_t dy = clamp_delta(mouse.dy, MOUSE_MAX_DELTA);
        
        // USB mouse: dx=turn, dy=forward/back  
        // Y axis already inverted in hid_app.c
        ev.data2 = dx * MOUSE_SENSITIVITY_MULT;   // Turn left/right
        ev.data3 = dy * MOUSE_SENSITIVITY_MULT;   // Forward/back
        ev.data4 = 0;
        
        D_PostEvent(&ev);
        
        prev_usb_buttons = mouse.buttons & 0x07;
    }
#endif // USB_HID_ENABLED
}

//--------------------------------------------------------------------
// Check if USB input is available
//--------------------------------------------------------------------

int usbhid_wrapper_keyboard_connected(void) {
#ifdef USB_HID_ENABLED
    if (!usb_hid_initialized) return 0;
    return usbhid_keyboard_connected();
#else
    return 0;
#endif
}

int usbhid_wrapper_mouse_connected(void) {
#ifdef USB_HID_ENABLED
    if (!usb_hid_initialized) return 0;
    return usbhid_mouse_connected();
#else
    return 0;
#endif
}

int usbhid_wrapper_get_key(int *pressed, unsigned char *key) {
#ifdef USB_HID_ENABLED
    if (!usb_hid_initialized) return 0;
    
    // Process USB events first
    usbhid_task();
    
    // Get next key action from the queue
    uint8_t hid_keycode;
    int down;
    if (usbhid_get_key_action(&hid_keycode, &down)) {
        unsigned char doom_key = hid_to_doom_key(hid_keycode);
        if (doom_key != 0) {
            *pressed = down;
            *key = doom_key;
            return 1;
        }
    }
#else
    (void)pressed;
    (void)key;
#endif
    return 0;
}
