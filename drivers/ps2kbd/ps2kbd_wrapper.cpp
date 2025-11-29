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
    if (code == 0x2C) return KEY_USE;  // Space key mapped to USE action
    if (code == 0x4F) return KEY_RIGHTARROW;
    if (code == 0x50) return KEY_LEFTARROW;
    if (code == 0x51) return KEY_DOWNARROW;
    if (code == 0x52) return KEY_UPARROW;
    
    // Function keys
    if (code >= 0x3A && code <= 0x45) {
        return KEY_F1 + (code - 0x3A);
    }
    
    // Note: Modifiers (E0-E7) are handled separately in modifier byte,
    // not as keycodes, so we don't map them here
    
    return 0;
}

static void key_handler(hid_keyboard_report_t *curr, hid_keyboard_report_t *prev) {
    // Check modifiers - Map Ctrl to FIRE, Alt to ALT (for strafe)
    uint8_t changed_mods = curr->modifier ^ prev->modifier;
    if (changed_mods) {
        // Map Ctrl (left or right) to KEY_FIRE for shooting
        if (changed_mods & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL)) {
            int ctrl_pressed = (curr->modifier & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL)) != 0;
            event_queue.push({ctrl_pressed, KEY_FIRE});  // Changed from KEY_RCTRL to KEY_FIRE
        }
        // Map Shift to Shift (for running)
        if (changed_mods & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) {
            int shift_pressed = (curr->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT)) != 0;
            event_queue.push({shift_pressed, KEY_RSHIFT});
        }
        // Map Alt to Alt (for strafing)
        if (changed_mods & (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT)) {
            int alt_pressed = (curr->modifier & (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT)) != 0;
            event_queue.push({alt_pressed, KEY_RALT});
        }
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
