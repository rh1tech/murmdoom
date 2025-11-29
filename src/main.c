#include "doomgeneric.h"
#include "pico/stdlib.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include <stdio.h>

int main() {
    // Set system clock to 252 MHz for HDMI
    // This is required because the PIO DVI implementation needs a high clock rate (10x pixel clock)
    // 640x480@60Hz pixel clock is ~25.2MHz, so we need ~252MHz.
    set_sys_clock_khz(252000, true);

    stdio_init_all();
    
    // Wait for USB connection
    for (int i = 0; i < 5; i++) {
        printf("Waiting for USB connection... %d\n", 5 - i);
        sleep_ms(1000);
    }
    printf("System Clock: %lu Hz\n", clock_get_hz(clk_sys));
    printf("Starting Doom...\n");

    char *argv[] = {"doom", NULL};
    doomgeneric_Create(1, argv);

    while (1) {
        doomgeneric_Tick();
    }

    return 0;
}
