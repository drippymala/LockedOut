//---------------------------------------------------------
// file:    main.c
// author:  LockedOut (Sherilyn, Joshua, Sean, Syahmi)
// 
// brief:   Main entry point for the sample project
//          of the CProcessing library
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include "cprocessing.h"
#include "mainmenu.h"
#include "game.h"
#include "splash.h"

int main(void)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	CP_Engine_SetNextGameState(Splash_Init, Splash_Update, Splash_Exit);   // the splash screen goes to the main menu by itself
	CP_System_SetWindowSize(1600, 900);
	CP_Engine_Run(0);
	return 0;
}
