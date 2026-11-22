//---------------------------------------------------------
// file:	wpn_laststand.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	[5] Last Stand: a revolver that gets stronger the LESS HP you have.
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

// ---- [5] Last Stand: a revolver that gets stronger the LESS HP you have ----
// Damage climbs on an exponential curve from the min at full HP to the max near death: weak while your
// health bar is green (less than the pistol), a bit more than the pistol in the yellow, and it explodes
// upward in the red. Bullets turn deeper red AND get bigger (hitbox too) as you get closer to death.
// Once you're down to MIN_HP_AFTER_SHOT (1 HP), shots are free (every level).
// Lv4 DEATH'S DOOR: much higher max damage, and bullets go through zombies (losing half their damage each time).
#define LAST_STAND_COST            3.0f    // HP per shot (flat)
#define LAST_STAND_FIRE_DELAY      0.45f  // about 2.1 shots per second
#define LAST_STAND_MIN_DAMAGE_LV   { 5.0f, 8.0f, 10.0f, 10.0f }      // at full HP
#define LAST_STAND_MAX_DAMAGE_LV   { 30.0f, 52.0f, 90.0f, 200.0f }   // at (almost) 0 HP
#define LAST_STAND_CURVE_LV        { 3.5f, 3.5f, 3.5f, 4.5f }   // how sharply damage bends upward near death (bigger = flatter early, steeper late).
                                                              // Lv1-3 are a bit more linear, so they pay off before you are nearly dead
#define LAST_STAND_BULLET_SPEED    400.0f  // slow, heavy rounds
#define LAST_STAND_BULLET_LIFE     2.4f    // seconds before it disappears (2x the pistol, so the slower bullet keeps the same range)
#define LAST_STAND_MIN_RADIUS      5.0f    // bullet size at full HP
#define LAST_STAND_MAX_RADIUS      12.0f   // bullet size at (almost) 0 HP
#define LAST_STAND_KNOCKBACK       6.0f
#define LAST_STAND_EVO_PIERCE      3       // Lv4: zombies each bullet goes through before stopping
#define LAST_STAND_PIERCE_FALLOFF  0.5f    // Lv4: damage is halved after each zombie it goes through (200 -> 100 -> 50 -> 25)
