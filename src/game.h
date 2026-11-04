//---------------------------------------------------------
// file:	game.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	This header file contains the function 
//			declarations for the game state, including initialization, 
//			updating, and exiting the game.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#pragma once

void Game_Init(void);

void Game_Update(void);

void Game_Exit(void);

int Game_GetBestScore(void);
int Game_GetBestWave(void);