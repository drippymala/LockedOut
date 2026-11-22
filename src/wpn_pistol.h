//---------------------------------------------------------
// file:	wpn_pistol.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	[1] Pistol: cheap, flat cost, small fast bullets.
//			Stats and data types only (tweak balance here).
//			The code that fires, moves and draws it is in weapon.c.
//			Included by weapon.h - include weapon.h, not this file.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#pragma once
#include <stdbool.h>
#include "cprocessing.h"   // CP_Vector, CP_Color
#include "zombie.h"        // MAX_ZOMBIES (shots remember which zombies they already hit)

// ---- [1] Pistol: cheap, flat cost, small fast bullets ----
// Lv5 TWIN FANG: fires 3 bullets in a spread, each bullet goes through 1 zombie, and hits can steal life.
#define PISTOL_RGB                 255, 230, 120   // pistol colour: use as Col(PISTOL_RGB, alpha)
#define PISTOL_COST_LV             { 1.0f, 1.0f, 1.0f, 1.0f, 3.0f }          // HP per shot (Lv5: one shot = the whole 3-bullet spread)
#define PISTOL_DAMAGE_LV           { 10.0f, 16.0f, 21.0f, 28.0f, 33.0f }
#define PISTOL_FIRE_DELAY_LV       { 0.36f, 0.29f, 0.24f, 0.19f, 0.19f }     // seconds between shots
#define PISTOL_BULLET_SPEED        900.0f  // px per second
#define PISTOL_BULLET_RADIUS       5.0f
#define PISTOL_BULLET_LIFE         1.20f   // seconds before the bullet disappears
#define PISTOL_KNOCKBACK           4.0f    // px a zombie is shoved back when hit
#define PISTOL_EVO_BULLETS         3       // Lv5: bullets per shot
#define PISTOL_EVO_SPREAD          0.42f   // Lv5: radians between bullets in the spread (~24 degrees, so only close targets take all 3)
#define PISTOL_EVO_PIERCE          1       // Lv5: zombies each bullet goes through before stopping
#define PISTOL_EVO_PIERCE_FALLOFF  0.35f   // Lv5: a bullet that goes through a zombie only does 35% damage to the next one
#define PISTOL_EVO_LIFESTEAL_CHANCE 0.05f  // Lv5: chance each bullet hit heals you
#define PISTOL_EVO_LIFESTEAL_HEAL  5.0f    // Lv5: HP healed by a life-steal hit
