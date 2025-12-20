#include <stdio.h>

#include "m_argv.h"

#include "doomgeneric.h"

pixel_t* DG_ScreenBuffer = NULL;

// Optional platform hook to show a start screen before D_DoomMain().
// Platforms can override this by providing a strong definition.
void DG_StartScreen(void) __attribute__((weak));
void DG_StartScreen(void) {}

void M_FindResponseFile(void);
void D_DoomMain (void);


void doomgeneric_Create(int argc, char **argv)
{
	// save arguments
    myargc = argc;
    myargv = argv;

	M_FindResponseFile();

	// DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);

	DG_Init();
	DG_StartScreen();

	D_DoomMain ();
}

