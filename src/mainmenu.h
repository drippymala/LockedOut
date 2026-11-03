//---------------------------------------------------------
// file:	mainmenu.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	This header file contains the function 
//			declarations for the main menu game state, including 
//			initialization, updating, and exiting the main menu.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------
#pragma once
#include "cprocessing.h"

// Kept from the template (declared here, defined once in mainmenu.c)
extern CP_Font myFont;
extern CP_Image myImage;

// Loads a TrueType font once (CProcessing has no built-in default font) and selects it
void Font_EnsureLoaded(void);

void Main_Menu_Init(void);
void Main_Menu_Update(void);
void Main_Menu_Exit(void);
