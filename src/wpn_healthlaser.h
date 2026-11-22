//---------------------------------------------------------
// file:	wpn_healthlaser.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	[2] Health Laser: fires your health bar as a piercing beam.
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

// ---- [2] Health Laser: damage = your HP x multiplier, length = the filled part of your health bar, thickness = the bar's height ----
// (HEALTH_BAR_WIDTH / HEALTH_BAR_HEIGHT are in game.h, which includes this file before using these macros)
// The beam is the health bar's colour (green / yellow / red) when you fire, and the bar is hidden while it flies.
// You can't fire anything else until the beam has left the screen.
// Lv4 LIFELINE LANCE: much stronger, and every zombie the beam passes through gives you HP back.
#define LASER_RGB                  255, 50, 50     // barrel / crosshair colour: use as Col(LASER_RGB, alpha)
#define LASER_COST_FLAT            8.0f    // each shot costs this much HP...
#define LASER_COST_PERCENT         0.10f   // ...plus 10% of your current HP (more max HP = a bit more per shot, but far more damage)
#define LASER_FIRE_DELAY           1.0f    // minimum seconds between shots (the beam usually takes longer to cross the screen)
#define LASER_SPEED                1100.0f // px per second the beam travels (it must leave the screen before the next shot)
#define LASER_DAMAGE_MULTIPLIER_LV { 0.6f, 1.0f, 1.5f, 2.5f }   // damage = your HP x this
#define LASER_EVO_HEAL_PER_HIT     4.0f    // Lv4: HP you get back for each zombie the beam hits
#define LASER_SIZE_SCALE           0.9f    // beam is 90% of the health bar's size (length and thickness)
#define LASER_WIDTH                (HEALTH_BAR_HEIGHT * LASER_SIZE_SCALE)   // beam thickness (the hitbox is this wide too)
#define LASER_BORDER               3.0f    // black outline around the beam in px (drawing only, not part of the hitbox)
#define LASER_KNOCKBACK            6.0f    // px a zombie is shoved back when the beam passes through it
#define LASER_PIERCE_FALLOFF       0.90f   // each zombie after the first takes 90% of what the one before took (1st 100%, 2nd 90%, 3rd 81%...)
#define MAX_LASERS                 8       // laser beams on screen at once

// Health laser beam: a line that flies forward and pierces every zombie until it leaves the screen.
// The beam runs from its tail back to its head:   tail ======> head
typedef struct
{
	bool active;
	CP_Vector head;              // front tip of the beam
	CP_Vector dir;               // unit direction it flies in
	float length;                // full length (px) = filled health bar width when fired
	CP_Color color;              // health bar colour when fired
	float traveled;              // px the head has moved since firing (the beam "grows" out of the gun until it reaches full length)
	float damage;                // damage to each zombie it passes through, set from HP when fired
	float healPerHit;            // Lv4: HP given back for each zombie it hits (0 below Lv4)
	int   hitId[MAX_ZOMBIES];    // id of the zombie in each slot this beam already hurt (0 = none), so each zombie is hit once per shot
} Laser;
