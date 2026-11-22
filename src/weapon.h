//---------------------------------------------------------
// file:	weapon.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	Shared gun settings, the gun list, and the functions
//			that fire, move, hit-test and draw shots (weapon.c).
//			Every gun runs on the player's own HP.
//			Each gun's own stats live in its own header:
//			  [1] Pistol        wpn_pistol.h
//			  [2] Health Laser  wpn_healthlaser.h
//			  [3] Bloodrang     wpn_bloodrang.h
//			  [4] Blood Bomb    wpn_bloodbomb.h
//			  [5] Last Stand    wpn_laststand.h
//			The pistol has 5 levels, every other gun 4 (player.gunLevel).
//			The last level is the "evolved" form with an extra ability.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#pragma once
#include <stdbool.h>
#include <stddef.h>        // size_t
#include "cprocessing.h"   // CP_Vector, CP_Color
#include "zombie.h"        // MAX_ZOMBIES (piercing shots remember which zombies they already hit)

/* =========================================================
   Shared gun settings  (each gun's own stats are in its wpn_*.h header)
   Anything ending in _LV is a list of numbers, one per level: { Lv1, Lv2, ... }.
   The pistol has 5 levels (Lv5 = evolved); every other gun has 4 (Lv4 = evolved).
   Lv3 is how strong every gun was before upgrades existed (Lv2 for the pistol).
   ========================================================= */

// ---- shared ----
#define MAX_WEAPON_LEVEL           5       // the most levels any gun has (size of the stat tables). Use GetMaxWeaponLevel(w) for a specific gun
#define GUN_MAX_LEVEL              4       // Lv4 = evolved, for every gun except the pistol
#define PISTOL_MAX_LEVEL           5       // the pistol gets an extra level: Lv5 = evolved (Twin Fang)
#define ZOMBIE_HIT_FLASH_TIME      0.08f   // white flash when a shot hits a zombie
#define MIN_HP_AFTER_SHOT          1.0f    // you can't shoot yourself below this much HP
#define LAST_SHOT_MIN_SPARE        0.05f   // a shot you can't fully pay for still fires (one last time) and leaves you at
                                           // MIN_HP_AFTER_SHOT, as long as you have more than this much HP above it
#define START_FIRE_DELAY           0.3f    // short wait before the first shot of a new game
#define MAX_BULLET_HITS            4       // a piercing bullet remembers up to this many zombies it went through

// ---- each gun's stats and data types ----
#include "wpn_pistol.h"        // [1] Pistol
#include "wpn_healthlaser.h"   // [2] Health Laser
#include "wpn_bloodrang.h"     // [3] Bloodrang
#include "wpn_bloodbomb.h"     // [4] Blood Bomb
#include "wpn_laststand.h"     // [5] Last Stand

// ---- pools ----
#define MAX_BULLETS                512     // pistol / last stand bullets on screen at once

/* =========================================================
   Types
   ========================================================= */
typedef enum { W_PISTOL, W_HEALTH_LASER, W_BLOODRANG, W_BLOOD_BOMB, W_LAST_STAND, W_COUNT } WeaponType;

// Bullet (pistol and last stand): a small circle that stops at the first zombie it hits
// (or goes through a few, when piercing).
typedef struct
{
	bool active;
	WeaponType type;
	CP_Vector pos, vel; // velocity of the bullet
	float life, damage, radius, knockback;
	CP_Color color;
	int pierceLeft;                  // zombies it can still go through (0 = stops at the next one)
	int hitCount;                    // how many zombies it already hit
	int hitId[MAX_BULLET_HITS];      // their ids, so it never hits the same zombie twice
} Bullet;

// Live numbers for the stats panel next to the health bar (drawn by game.c).
// They are rebuilt every frame, so they follow your HP, gun level, boost stacks and charge.

typedef struct
{
	char  title[40];         // "LAST STAND  Lv2"  (Lv4 adds " MAX")
	char  cost[48];          // what firing costs right now
	char  damage[48];        // what a hit deals right now
	char  extraLabel[12];    // label of the 4th row ("RATE", "BOOST", "RADIUS"...)
	char  extraValue[48];    // value of the 4th row
	bool  cantAfford;        // true when you don't have the HP to fire right now
} WeaponStats;

// Called by UpdateShots when a shot kills a zombie (index = its slot in s_zombies, killedBy = the gun that did it).
typedef void (*ZombieKilledFn)(int index, WeaponType killedBy);

/* =========================================================
   Gun info (used by the shooting code and the HUD)
   ========================================================= */
int         GetWeaponLevel(WeaponType w);           // 1..GetMaxWeaponLevel(w) (from player.gunLevel)
int         GetMaxWeaponLevel(WeaponType w);        // the evolved level of gun w (5 for the pistol, 4 for the rest)
float       GetShotCost(WeaponType w);              // HP the gun would cost to fire right now
float       GetFireDelay(WeaponType w);             // seconds between shots
const char* GetWeaponName(WeaponType w);            // the Lv4 guns get their evolved name
CP_Color    GetWeaponColor(WeaponType w, int alpha); // barrel / crosshair / shot / pickup colour
void        GetWeaponStats(WeaponType w, WeaponStats* out);

#define UPGRADE_TEXT_LINES   3
#define UPGRADE_TEXT_LEN     48
void        GetUpgradeText(WeaponType w, int toLevel, char lines[UPGRADE_TEXT_LINES][UPGRADE_TEXT_LEN]);   // what upgrading to toLevel changes (upgrade cards)
void        GetNewWeaponText(WeaponType w, char lines[UPGRADE_TEXT_LINES][UPGRADE_TEXT_LEN]);              // what the gun does at Lv1 (new weapon cards)
float       GetMoveSpeedMultiplier(void);           // 1.0 normally, slower while charging a blood bomb
float       GetBombBloodRainChance(void);           // chance a blood bomb kill drops a blood heal (by level)

/* =========================================================
   Shots
   ========================================================= */
void ResetWeapons(void);                                   // clear every shot and charge (new game)
void CancelCharging(void);                                 // stop charging a blood bomb and give the HP back
void HandleShooting(float deltaTime);                      // left mouse: fire / charge / throw the current gun, paying HP
void BlockFireUntilRelease(void);                          // ignore the mouse until it's let go (a UI click shouldn't also shoot)
void UpdateShots(float deltaTime, ZombieKilledFn onZombieKilled);   // move shots, hit zombies, report kills
void DrawShots(void);
bool IsLaserActive(void);                                  // true while a health laser beam is still flying
