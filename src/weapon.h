//---------------------------------------------------------
// file:	weapon.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	Every gun's stats in one place, plus the functions
//			that fire, move, hit-test and draw shots.
//			Every gun runs on the player's own HP.
//			Guns: [1] Pistol  [2] Health Laser  [3] Bloodrang
//			      [4] Blood Bomb  [5] Last Stand
//			Every gun has 4 levels (player.gunLevel). Lv4 is the
//			"evolved" form with an extra ability.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#pragma once
#include <stdbool.h>
#include <stddef.h>        // size_t
#include "cprocessing.h"   // CP_Vector, CP_Color
#include "zombie.h"        // MAX_ZOMBIES (piercing shots remember which zombies they already hit)

/* =========================================================
   Gun stats  (tweak balance here)
   Anything ending in _LV is a list of 4 numbers: { Lv1, Lv2, Lv3, Lv4 }.
   Lv3 is how strong every gun was before upgrades existed (Lv2 for the pistol).
   ========================================================= */

// ---- shared ----
#define MAX_WEAPON_LEVEL           4       // Lv4 = evolved
#define ZOMBIE_HIT_FLASH_TIME      0.08f   // white flash when a shot hits a zombie
#define MIN_HP_AFTER_SHOT          1.0f    // you can't shoot yourself below this much HP
#define START_FIRE_DELAY           0.3f    // short wait before the first shot of a new game
#define MAX_BULLET_HITS            4       // a piercing bullet remembers up to this many zombies it went through

// ---- [1] Pistol: cheap, flat cost, small fast bullets ----
// Lv4 TWIN FANG: fires 3 bullets in a spread, and each bullet goes through 1 zombie.
#define PISTOL_RGB                 255, 230, 120   // pistol colour: use as Col(PISTOL_RGB, alpha)
#define PISTOL_COST_LV             { 1.0f, 1.0f, 1.0f, 3.0f }   // HP per shot (Lv4: one shot = the whole 3-bullet spread)
#define PISTOL_DAMAGE_LV           { 10.0f, 17.0f, 26.0f, 30.0f }
#define PISTOL_FIRE_DELAY_LV       { 0.36f, 0.28f, 0.20f, 0.20f }   // seconds between shots
#define PISTOL_BULLET_SPEED        900.0f  // px per second
#define PISTOL_BULLET_RADIUS       5.0f
#define PISTOL_BULLET_LIFE         1.20f   // seconds before the bullet disappears
#define PISTOL_KNOCKBACK           4.0f    // px a zombie is shoved back when hit
#define PISTOL_EVO_BULLETS         3       // Lv4: bullets per shot
#define PISTOL_EVO_SPREAD          0.18f   // Lv4: radians between bullets in the spread (~10 degrees)
#define PISTOL_EVO_PIERCE          1       // Lv4: zombies each bullet goes through before stopping

// ---- [2] Health Laser: damage = your HP x multiplier, length = the filled part of your health bar, thickness = the bar's height ----
// (HEALTH_BAR_WIDTH / HEALTH_BAR_HEIGHT are in game.h, which includes this file before using these macros)
// The beam is the health bar's colour (green / yellow / red) when you fire, and the bar is hidden while it flies.
// You can't fire anything else until the beam has left the screen.
// Lv4 LIFELINE LANCE: much stronger, and every zombie the beam passes through gives you HP back.
#define LASER_RGB                  255, 50, 50     // barrel / crosshair colour: use as Col(LASER_RGB, alpha)
#define LASER_COST_PERCENT         0.25f   // each shot costs 25% of your current HP
#define LASER_FIRE_DELAY           1.0f    // minimum seconds between shots (the beam usually takes longer to cross the screen)
#define LASER_SPEED                750.0f  // px per second the beam travels (slow and heavy)
#define LASER_DAMAGE_MULTIPLIER_LV { 0.6f, 1.0f, 1.5f, 2.5f }   // damage = your HP x this
#define LASER_EVO_HEAL_PER_HIT     4.0f    // Lv4: HP you get back for each zombie the beam hits
#define LASER_SIZE_SCALE           0.9f    // beam is 90% of the health bar's size (length and thickness)
#define LASER_WIDTH                (HEALTH_BAR_HEIGHT * LASER_SIZE_SCALE)   // beam thickness (the hitbox is this wide too)
#define LASER_BORDER               3.0f    // black outline around the beam in px (drawing only, not part of the hitbox)
#define LASER_KNOCKBACK            6.0f    // px a zombie is shoved back when the beam passes through it

// ---- [3] Bloodrang: a boomerang made of your blood ----
// Throw it at the mouse: it holds the HP you paid. It flies out, curves back and homes in on you,
// hitting each zombie once per trip. Catch it to get its HP back (only part of it below Lv3, see RANG_REFUND_LV)
// AND a damage boost for every zombie it hit.
// It can only turn so sharply, so if you keep moving it overshoots and loops round again,
// bleeding HP each missed lap. If it bleeds out you lose the HP and your boost.
// Lv4 TWIN RANG: throws 2 at once (one each side of the mouse), and the boost can go twice as high.
#define BLOODRANG_RGB              200, 20, 50
#define RANG_COST                  15.0f   // HP stored inside each boomerang when you throw it
#define RANG_DAMAGE_LV             { 12.0f, 21.0f, 32.0f, 40.0f }   // damage per hit, before the boost
#define RANG_REFUND_LV             { 0.30f, 0.60f, 1.00f, 1.00f }   // share of the HP inside it you get back when you catch it
#define RANG_BOOST_PER_STACK       0.10f   // each stack = +10% damage on later throws
#define RANG_MAX_STACKS            5       // boost caps at +50%
#define RANG_EVO_MAX_STACKS        10      // Lv4: boost caps at +100%
#define RANG_EVO_COUNT             2       // Lv4: boomerangs per throw
#define RANG_EVO_SIDE_OFFSET       110.0f  // Lv4: each one flies to a spot this far to the side of the mouse
#define MAX_RANGS                  2
#define RANG_OUT_SPEED             850.0f  // px per second flying out
#define RANG_RETURN_SPEED          700.0f  // px per second flying back
#define RANG_TURN_RATE             4.5f    // how sharply it can turn on the way back (radians per second)
#define RANG_TURN_GROWTH           1.5f    // each missed lap it turns this much sharper, so it can't orbit you forever
#define RANG_LAP_DRAIN             3.0f    // HP it loses each lap it misses you
#define RANG_RADIUS                14.0f   // hitbox and catch size
#define RANG_KNOCKBACK             8.0f
#define RANG_FIRE_DELAY            0.3f
#define RANG_SPIN_SPEED            18.0f   // radians per second (just for looks)
#define RANG_MAX_LIFE              12.0f   // safety: it bleeds out after this many seconds no matter what

// ---- [4] Blood Bomb: hold to pour your HP into an orb, release to throw ----
// The more HP inside, the bigger the blast and the damage. It explodes on the first zombie it touches,
// or at the spot you aimed at. Zombies the blast kills can drop a blood heal (blood rain):
// 20% / 40% / 60% of them by level, every one at Lv4.
// Lv4 BLOOD CLUSTER: the blast throws out 4 mini bombs that explode again around it.
#define BLOOD_BOMB_RGB             230, 40, 110
#define BOMB_CHARGE_RATE           20.0f   // HP per second poured in while holding the mouse
#define BOMB_MAX_CHARGE_PERCENT    0.40f   // the orb holds at most 40% of the HP you had when you started charging
#define BOMB_DAMAGE_PER_HP_LV      { 1.0f, 1.7f, 2.5f, 3.0f }   // blast damage for each HP inside
#define BOMB_BLOOD_RAIN_LV         { 0.20f, 0.40f, 0.60f, 1.00f }   // chance each zombie the blast kills drops a blood heal
#define BOMB_BASE_RADIUS           40.0f   // blast radius with an almost empty orb
#define BOMB_RADIUS_PER_HP         3.0f    // ...plus this many px for each HP inside
#define BOMB_THROW_SPEED           700.0f  // px per second
#define BOMB_ORB_BASE_RADIUS       6.0f    // orb size (and hitbox) with nothing inside
#define BOMB_ORB_RADIUS_PER_HP     0.35f   // ...plus this many px for each HP inside
#define BOMB_KNOCKBACK             22.0f   // px zombies are thrown away from the blast
#define BOMB_FIRE_DELAY            0.5f    // seconds after a throw before you can start charging again
#define BOMB_CHARGE_MOVE_SPEED     0.6f    // you move at 60% speed while charging
#define BOMB_BLAST_SHOW_TIME       0.35f   // how long the explosion stays on screen
#define BOMB_EVO_MINI_COUNT        4       // Lv4: mini bombs thrown out by each blast
#define BOMB_EVO_MINI_SHARE        0.40f   // Lv4: each mini bomb counts as holding this share of the big bomb's HP
#define BOMB_EVO_MINI_DISTANCE     1.1f    // Lv4: mini bombs land this many blast-radii away from the centre
#define BOMB_EVO_MINI_SPEED        500.0f  // Lv4: px per second the mini bombs fly

// ---- [5] Last Stand: a revolver that gets stronger the LESS HP you have ----
// Damage climbs on an exponential curve from the min at full HP to the max near death: weak while your
// health bar is green (less than the pistol), a bit more than the pistol in the yellow, and it explodes
// upward in the red. Bullets turn deeper red AND get bigger (hitbox too) as you get closer to death.
// Once you're too low to pay for a shot (HP - cost would drop below MIN_HP_AFTER_SHOT), shots are free (every level).
// Lv4 DEATH'S DOOR: much higher max damage, and bullets go through zombies.
#define LAST_STAND_COST            3.0f    // HP per shot (flat)
#define LAST_STAND_FIRE_DELAY      0.333f  // 3 shots per second
#define LAST_STAND_MIN_DAMAGE_LV   { 5.0f, 8.0f, 10.0f, 10.0f }      // at full HP
#define LAST_STAND_MAX_DAMAGE_LV   { 38.0f, 68.0f, 120.0f, 200.0f }  // at (almost) 0 HP
#define LAST_STAND_CURVE           4.5f    // how sharply damage bends upward near death (bigger = flatter early, steeper late)
#define LAST_STAND_BULLET_SPEED    1000.0f
#define LAST_STAND_MIN_RADIUS      5.0f    // bullet size at full HP
#define LAST_STAND_MAX_RADIUS      12.0f   // bullet size at (almost) 0 HP
#define LAST_STAND_KNOCKBACK       6.0f
#define LAST_STAND_EVO_PIERCE      3       // Lv4: zombies each bullet goes through before stopping

// ---- pools ----
#define MAX_BULLETS                512     // pistol / last stand bullets on screen at once
#define MAX_LASERS                 8       // laser beams on screen at once
#define MAX_BOMBS                  24      // thrown blood bombs (and Lv4 mini bombs) in the air at once
#define MAX_BLASTS                 24      // explosion effects on screen at once

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

// Bloodrang: one at a time (two at Lv4).
typedef struct
{
	bool active;
	bool returning;              // false = flying out to the target, true = curving back to the player
	bool approaching;            // was it flying toward the player last frame? (used to spot a missed lap)
	CP_Vector pos, vel, target;
	float stored;                // HP inside it; you get this back when you catch it
	float damage;                // damage per hit (boost included), set when thrown
	float turnRate;              // grows every missed lap
	float spin;                  // spin angle, for drawing
	float life;                  // seconds left before it bleeds out (safety limit)
	int   hits;                  // zombies hit this throw (each one becomes a boost stack when you catch it)
	int   hitId[MAX_ZOMBIES];    // zombies already hit this trip (same idea as the laser)
} Bloodrang;

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
int         GetWeaponLevel(WeaponType w);           // 1..MAX_WEAPON_LEVEL (from player.gunLevel)
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
void UpdateShots(float deltaTime, ZombieKilledFn onZombieKilled);   // move shots, hit zombies, report kills
void DrawShots(void);
bool IsLaserActive(void);                                  // true while a health laser beam is still flying
