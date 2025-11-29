#ifndef PS2KBD_WRAPPER_H
#define PS2KBD_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

void ps2kbd_init(void);
void ps2kbd_tick(void);
int ps2kbd_get_key(int* pressed, unsigned char* key);

#ifdef __cplusplus
}
#endif

#endif
