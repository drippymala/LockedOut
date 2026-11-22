//---------------------------------------------------------
// file:	wpn_bloodbomb.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	[4] Blood Bomb: hold to pour HP into an orb, release to throw.
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

// ---- [4] Blood Bomb: hold to pour your HP into an orb, release to throw ----
// The more HP inside, the bigger the blast and the damage. It explodes on the first zombie it touches,
// or at the spot you aimed at. Zombies the blast kills can drop a blood heal (blood rain):
// 80% of them at Lv1-3, every one at Lv4.
// Lv4 BLOOD CLUSTER: the blast throws out 4 mini bombs that explode again inside it, and leaves a blood pool that hurts zombies standing in it.
#define BLOOD_BOMB_RGB             230, 40, 110
#define BOMB_CHARGE_RATE           30.0f   // HP per second poured in while holding the mouse
#define BOMB_MAX_CHARGE_PERCENT    0.40f   // the orb holds at most 40% of the HP you had when you started charging...
#define BOMB_EVO_MAX_CHARGE_PERCENT 0.60f  // ...or 60% at Lv4
#define BOMB_HP_COST_SHARE         0.75f   // you pay 75% of the HP that goes into the orb (it still counts the full amount for damage)
#define BOMB_DAMAGE_PER_HP_LV      { 1.2f, 1.9f, 2.8f, 3.3f }   // blast damage for each HP inside
#define BOMB_BLOOD_RAIN_LV         { 0.80f, 0.80f, 0.80f, 1.00f }   // chance each zombie the blast kills drops a blood heal
#define BOMB_BASE_RADIUS           55.0f   // blast radius with an almost empty orb
#define BOMB_RADIUS_PER_HP         3.6f    // ...plus this many px for each HP inside (full 40 HP orb: about 200 px)
#define BOMB_THROW_SPEED           700.0f  // px per second
#define BOMB_ORB_BASE_RADIUS       6.0f    // orb size (and hitbox) with nothing inside
#define BOMB_ORB_RADIUS_PER_HP     0.35f   // ...plus this many px for each HP inside
#define BOMB_KNOCKBACK             22.0f   // px zombies are thrown away from the blast
#define BOMB_FIRE_DELAY            0.2f    // seconds after a throw before you can start charging again
#define BOMB_DIRECT_HIT_BONUS      1.5f    // the zombie the orb explodes ON takes 50% more blast damage (rewards aiming)
#define BOMB_CHARGE_MOVE_SPEED     0.6f    // you move at 60% speed while charging
#define BOMB_BLAST_SHOW_TIME       0.35f   // how long the explosion stays on screen
#define BOMB_EVO_MINI_COUNT        4       // Lv4: mini bombs thrown out by each blast
#define BOMB_EVO_MINI_SHARE        0.40f   // Lv4: each mini bomb counts as holding this share of the big bomb's HP
#define BOMB_EVO_MINI_DISTANCE     0.6f    // Lv4: mini bombs land this many blast-radii away from the centre (inside the blast, so they hit the pack)
#define BOMB_EVO_MINI_SPEED        500.0f  // Lv4: px per second the mini bombs fly
#define BOMB_EVO_MINI_DAMAGE_SCALE 0.35f   // Lv4: mini bombs deal only 35% of their normal damage (their blast size is unchanged)
#define BOMB_EVO_POOL_SIZE         0.6f    // Lv4: the big blast leaves a blood pool this share of its radius
#define BOMB_EVO_POOL_TIME         2.0f    // Lv4: seconds the pool lasts
#define BOMB_EVO_POOL_DPS_SHARE    0.15f   // Lv4: every second, zombies in the pool take this share of the blast's damage
#define BOMB_EVO_POOL_TICK         0.25f   // Lv4: the pool deals its damage in pulses this many seconds apart
#define MAX_POOLS                  8
#define MAX_BOMBS                  24      // thrown blood bombs (and Lv4 mini bombs) in the air at once
#define MAX_BLASTS                 24      // explosion effects on screen at once

// Blood bomb orb in the air.
typedef struct
{
	bool active;
	bool mini;                   // Lv4 mini bomb (doesn't split again)
	CP_Vector pos, vel, target;  // target = where you aimed; it explodes there if it hits nothing first
	float stored;                // HP inside it
	float damagePerHp;           // set when thrown, from the gun's level
} BloodBomb;

// Explosion effect (drawing only).
typedef struct
{
	bool active;
	CP_Vector pos;
	float radius, life;
} Blast;

// Lv4 blood pool left by a big blast: hurts every zombie standing in it, in pulses.
typedef struct
{
	bool active;
	CP_Vector pos;
	float radius;
	float life;                  // seconds left
	float tickTimer;             // seconds until the next pulse
	float damagePerTick;         // damage each zombie inside takes per pulse
} BloodPool;
