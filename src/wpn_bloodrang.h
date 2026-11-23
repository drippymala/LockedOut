//---------------------------------------------------------
// file:	wpn_bloodrang.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	[3] Bloodrang: a boomerang made of your blood.
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

// ---- [3] Bloodrang: a boomerang made of your blood ----
// Throw it at the mouse: it holds the HP you paid. It flies out, curves back and homes in on you,
// hitting each zombie once per trip. Catch it to get its HP back (only all of it at Lv4, see RANG_REFUND_LV)
// AND a damage boost for every zombie it hit.
// It can only turn so sharply, so if you keep moving it overshoots and loops round again,
// bleeding HP each missed lap. If it bleeds out you lose the HP and your boost.
// Lv4 TWIN RANG: throws 2 at once on mirrored arcs that cross at the mouse, and the boost can go higher (+80%).
#define BLOODRANG_RGB              200, 20, 50
#define RANG_COST                  15.0f   // HP stored inside each boomerang when you throw it
#define RANG_DAMAGE_LV             { 12.0f, 21.0f, 32.0f, 40.0f }   // damage per hit, before the boost
#define RANG_REFUND_LV             { 0.30f, 0.60f, 0.80f, 1.00f }   // share of the HP inside it you get back when you catch it
#define RANG_BOOST_PER_STACK       0.10f   // each stack = +10% damage on later throws
#define RANG_MAX_STACKS            4       // boost caps at +40% (Lv1-3)
#define RANG_EVO_MAX_STACKS        8       // Lv4: boost caps at +80%
#define RANG_EVO_COUNT             2       // Lv4: boomerangs per throw
#define RANG_EVO_ARC_WIDTH         110.0f  // Lv4: the two fly out on mirrored arcs that meet at the mouse ("scissor"),
                                           //      bowing out at most this far to each side halfway there
#define RANG_EVO_ARC_SHARE         0.35f   // Lv4: ...or this share of the throw distance if that is less (close throws stay tight)
#define MAX_RANGS                  2
#define RANG_OUT_SPEED             1100.0f // px per second flying out
#define RANG_RETURN_SPEED          950.0f  // px per second flying back
#define RANG_TURN_RATE             6.1f    // how sharply it can turn on the way back (radians per second; raised with the return speed so its loops stay the same size)
#define RANG_TURN_GROWTH           1.5f    // each missed lap it turns this much sharper, so it can't orbit you forever
#define RANG_LAP_DRAIN             3.0f    // HP it loses each lap it misses you
#define RANG_RADIUS_LV             { 14.0f, 14.0f, 17.0f, 20.0f }   // hitbox, catch and drawn size (grows a bit at Lv3 and Lv4)
#define RANG_KNOCKBACK             8.0f
#define RANG_FIRE_DELAY            0.3f
#define RANG_SPIN_SPEED            18.0f   // radians per second (just for looks)
#define RANG_MAX_LIFE              12.0f   // safety: it bleeds out after this many seconds no matter what

// Bloodrang: one at a time (two at Lv4).
typedef struct
{
	bool active;
	bool returning;              // false = flying out to the target, true = curving back to the player
	bool approaching;            // was it flying toward the player last frame? (used to spot a missed lap)
	CP_Vector pos, vel, target;
	CP_Vector start, control;    // flying out follows a curve from start to target, bent toward control (straight if control is the midpoint)
	float outT, outLength;       // how far along that curve it is (0..1), and the curve's length in px
	float stored;                // HP inside it; you get this back when you catch it
	float damage;                // damage per hit (boost included), set when thrown
	float turnRate;              // grows every missed lap
	float spin;                  // spin angle, for drawing
	float life;                  // seconds left before it bleeds out (safety limit)
	int   hits;                  // zombies hit this throw (each one becomes a boost stack when you catch it)
	int   hitId[MAX_ZOMBIES];    // zombies already hit this trip (same idea as the laser)
} Bloodrang;
