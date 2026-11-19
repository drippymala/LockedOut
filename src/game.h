//---------------------------------------------------------
// file:	game.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	This header file contains the function
//			declarations for the game state, including initialization,
//			updating, and exiting the game, plus the types and
//			globals that game.c, player.c and zombie.c use.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#pragma once
#include <stdbool.h>
#include "cprocessing.h"   // needed for CP_Vector

/* ---------- Shared sizes and values (used by game.c and player.c) ---------- */
// (zombie types, MAX_ZOMBIES and s_zombies live in zombie.h)
#define MAX_BULLETS          512             // max amt of bullets
#define PLAYER_RADIUS        18.0f
#define GUN_BARREL_LENGTH    14.0f
#define PISTOL_RGB           255, 230, 120   // pistol colour (barrel, crosshair, bullets): use as Col(PISTOL_RGB, alpha)

/* ---------- Shared types ---------- */
typedef enum { W_PISTOL, W_COUNT } WeaponType;

typedef struct
{
	bool active;
	WeaponType type;
	CP_Vector pos, vel; // velocity of the bullet
	float life, damage, radius;
} Bullet;

typedef struct
{
	CP_Vector position;
	CP_Vector aimDirection;   // unit vector from the player toward the mouse

	float hp;       // Player's current health
	float maxHp;    // Player's maximum health
	float ghostHp;  // Health value used to display the trailing ghost bar

	WeaponType weapon;//let player be able to use different weapons.
	bool owned[W_COUNT];

	float noFireFlash;  // red flash on the health bar when a shot is refused

	float regenPool;    // bandage: HP still waiting to be healed over time
	float bloodlust;    // bloodlust: seconds left (kills heal you while this is above 0)

	float combatTimer;  // tracks time since last shot or damage taken
} Player;

/* ---------- Shared globals (defined in game.c) ---------- */
// "extern" = this variable exists, but its memory is created in game.c.
// Any file that includes game.h can then read and change the same variable.
extern Player player;
extern Bullet M_bullets[MAX_BULLETS];
extern float  windowWidth;
extern float  windowHeight;

/* ---------- Game state callbacks ---------- */
void Game_Init(void);

void Game_Update(void);

void Game_Exit(void);

int Game_GetBestScore(void);
int Game_GetBestWave(void);
