//---------------------------------------------------------
// file:    game.c
// author:  LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:   Source file containing the implementation of the 
//          game state, including initialization, game updates, and 
//          exit functions.
//
// Copyright © 2026 DigiPen, All rights reserved
//---------------------------------------------------------

#include "cprocessing.h"
#include "utils.h"
#include "game.h"
#include "mainmenu.h"
#include <stdbool.h>

#define MAX_ZOMBIES 128

// Enums for different types of zombies
typedef enum { Z_WALKER, Z_RUNNER, Z_BRUTE } ZombieType;

// Zombie structure
typedef struct
{
	bool active;
	int id;
	ZombieType type;
	CP_Vector pos;
	float hp, maxHp, speed, radius, damage;
	float attackTimer, swayPhase, swayAmp, hitFlash;
} Zombie;

// State variables
static CP_Color s_zombieColor;
static Zombie   s_zombies[MAX_ZOMBIES];

/* ---------- Helper Drawing Functions ---------- */
static CP_Color Col(int r, int g, int b, int a)
{
	return CP_Color_Create(r, g, b, a);
}

static void G_Circle(float x, float y, float radius, CP_Color c)
{
	CP_Settings_EllipseMode(CP_POSITION_CENTER);
	CP_Settings_NoStroke();
	CP_Settings_Fill(c);
	CP_Graphics_DrawCircle(x, y, radius * 2.0f); // CProcessing takes diameter
}

/* ---------- Zombie Rendering ---------- */
static void DrawZombies(void)
{
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		const Zombie* z = &s_zombies[i];
		if (!z->active) continue;

		// Hit flash overrides the base color to white
		CP_Color c = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : s_zombieColor;
		G_Circle(z->pos.x, z->pos.y, z->radius, c);
	}
}

/* ---------- State Functions ---------- */
void Game_Init(void)
{
	// Set the common base color for zombies (green)
	s_zombieColor = Col(95, 155, 85, 255);

	// Clear zombie array
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		s_zombies[i].active = false;
	}
}

void Game_Update(void)
{
	// Press Q to return to Main Menu
	if (CP_Input_KeyTriggered(KEY_Q))
	{
		CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
		return;
	}

	// Clear background
	CP_Graphics_ClearBackground(Col(28, 34, 26, 255));

	// Render zombies
	DrawZombies();
}

void Game_Exit(void)
{
}