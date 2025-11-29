#include "ps2kbd_wrapper.h"
#include "ps2kbd_mrmltr.h"
#include "doomkeys.h"
#include <queue>

struct KeyEvent {
    int pressed;
    unsigned char key;
};

static std::queue<KeyEvent> event_queue;

// HID to Doom mapping (partial)
static unsigned char hid_to_doom(uint8_t code) {
    if (code >= 0x04 && code <= 0x1D) return 'a' + (code - 0x04);
    if (code >= 0x1E && code <= 0x27) {
        if (code == 0x27) return '0';
        return '1' + (code - 0x1E);
    }
    if (code == 0x28) return KEY_ENTER;
    if (code == 0x29) return KEY_ESCAPE;
    if (code == 0x2A) return KEY_BACKSPACE;
    if (code == 0x2B) return KEY_TAB;
    if (code == 0x2C) return ' ';
    if (code == 0x4F) return KEY_RIGHTARROW;
    if (code == 0x50) return KEY_LEFTARROW;
    if (code == 0x51) return KEY_DOWNARROW;
    if (code == 0x52) return KEY_UPARROW;
    
    // Modifiers (mapped to Doom keys if possible)
    // Doom uses KEY_RCTRL etc?
    // doomkeys.h defines:
    // KEY_RIGHTARROW	0xae
    // KEY_LEFTARROW	0xac
    // KEY_UPARROW	0xad
    // KEY_DOWNARROW	0xaf
    // KEY_ESCAPE	27
    // KEY_ENTER	13
    // KEY_TAB		9
    // KEY_F1		(0x80+0x3b)
    // KEY_BACKSPACE	127
    // KEY_PAUSE	0xff
    // KEY_EQUALS	0x3d
    // KEY_MINUS	0x2d
    // KEY_RSHIFT	(0x80+0x36)
    // KEY_RCTRL	(0x80+0x1d)
    // KEY_RALT		(0x80+0x38)
    // KEY_LALT		KEY_RALT
    
    if (code == 0xE0) return KEY_RCTRL; // Left Control
    if (code == 0xE1) return KEY_RSHIFT; // Left Shift
    if (code == 0xE2) return KEY_RALT; // Left Alt
    if (code == 0xE4) return KEY_RCTRL; // Right Control
    if (code == 0xE5) return KEY_RSHIFT; // Right Shift
    if (code == 0xE6) return KEY_RALT; // Right Alt

    return 0;
}

static void key_handler(hid_keyboard_report_t *curr, hid_keyboard_report_t *prev) {
    // Check modifiers
    // This is tricky because modifier is a bitmask.
    // We should check bits.
    // But for now, let's just rely on keycodes if they are reported there?
    // HID report usually puts modifiers in 'modifier' byte, not keycode array.
    // So we need to check 'modifier' byte.
    
    uint8_t changed_mods = curr->modifier ^ prev->modifier;
    if (changed_mods) {
        if (changed_mods & KEYBOARD_MODIFIER_LEFTCTRL) event_queue.push({(curr->modifier & KEYBOARD_MODIFIER_LEFTCTRL) ? 1 : 0, KEY_RCTRL});
        if (changed_mods & KEYBOARD_MODIFIER_LEFTSHIFT) event_queue.push({(curr->modifier & KEYBOARD_MODIFIER_LEFTSHIFT) ? 1 : 0, KEY_RSHIFT});
        if (changed_mods & KEYBOARD_MODIFIER_LEFTALT) event_queue.push({(curr->modifier & KEYBOARD_MODIFIER_LEFTALT) ? 1 : 0, KEY_RALT});
        if (changed_mods & KEYBOARD_MODIFIER_RIGHTCTRL) event_queue.push({(curr->modifier & KEYBOARD_MODIFIER_RIGHTCTRL) ? 1 : 0, KEY_RCTRL});
        if (changed_mods & KEYBOARD_MODIFIER_RIGHTSHIFT) event_queue.push({(curr->modifier & KEYBOARD_MODIFIER_RIGHTSHIFT) ? 1 : 0, KEY_RSHIFT});
        if (changed_mods & KEYBOARD_MODIFIER_RIGHTALT) event_queue.push({(curr->modifier & KEYBOARD_MODIFIER_RIGHTALT) ? 1 : 0, KEY_RALT});
    }

    // Check keys
    for (int i = 0; i < 6; i++) {
        if (curr->keycode[i] != 0) {
            bool found = false;
            for (int j = 0; j < 6; j++) {
                if (prev->keycode[j] == curr->keycode[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                unsigned char k = hid_to_doom(curr->keycode[i]);
                if (k) event_queue.push({1, k});
            }
        }
    }

    for (int i = 0; i < 6; i++) {
        if (prev->keycode[i] != 0) {
            bool found = false;
            for (int j = 0; j < 6; j++) {
                if (curr->keycode[j] == prev->keycode[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                unsigned char k = hid_to_doom(prev->keycode[i]);
                if (k) event_queue.push({0, k});
            }
        }
    }
}

static Ps2Kbd_Mrmltr* kbd = nullptr;

extern "C" void ps2kbd_init(void) {
    // GPIO 0 (Data), GPIO 1 (Clk)
    kbd = new Ps2Kbd_Mrmltr(pio0, 0, key_handler);
    kbd->init_gpio();
}

extern "C" void ps2kbd_tick(void) {
    if (kbd) kbd->tick();
}

extern "C" int ps2kbd_get_key(int* pressed, unsigned char* key) {
    if (event_queue.empty()) return 0;
    KeyEvent e = event_queue.front();
    event_queue.pop();
    *pressed = e.pressed;
    *key = e.key;
    return 1;
}
