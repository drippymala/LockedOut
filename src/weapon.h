//---------------------------------------------------------
// file:	weapon.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	Every gun's stats in one place, plus the functions
//			that fire, move, hit-test and draw shots.
//			Every gun runs on the player's own HP.
//			Guns: [1] Pistol  [2] Health Laser  [3] Bloodrang
//			      [4] Blood Bomb  [5] Last Stand
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
   ========================================================= */

// ---- shared ----
#define ZOMBIE_HIT_FLASH_TIME      0.08f   // white flash when a shot hits a zombie
#define MIN_HP_AFTER_SHOT          1.0f    // you can't shoot yourself below this much HP
#define START_FIRE_DELAY           0.3f    // short wait before the first shot of a new game

// ---- [1] Pistol: cheap, flat cost, small fast bullets ----
#define PISTOL_RGB                 255, 230, 120   // pistol colour: use as Col(PISTOL_RGB, alpha)
#define PISTOL_COST                2.0f    // HP per shot (flat)
#define PISTOL_FIRE_DELAY          0.25f   // seconds between shots
#define PISTOL_BULLET_SPEED        900.0f  // px per second
#define PISTOL_BULLET_RADIUS       5.0f
#define PISTOL_BULLET_DAMAGE       20.0f
#define PISTOL_BULLET_LIFE         1.20f   // seconds before the bullet disappears
#define PISTOL_KNOCKBACK           4.0f    // px a zombie is shoved back when hit

// ---- [2] Health Laser: damage = your HP x 1.5, length = the filled part of your health bar, thickness = the bar's height ----
// (HEALTH_BAR_WIDTH / HEALTH_BAR_HEIGHT are in game.h, which includes this file before using these macros)
// The beam is the health bar's colour (green / yellow / red) when you fire, and the bar is hidden while it flies.
// You can't fire anything else until the beam has left the screen.
#define LASER_RGB                  255, 50, 50     // barrel / crosshair colour: use as Col(LASER_RGB, alpha)
#define LASER_COST_PERCENT         0.25f   // each shot costs 25% of your current HP
#define LASER_FIRE_DELAY           1.0f    // minimum seconds between shots (the beam usually takes longer to cross the screen)
#define LASER_SPEED                750.0f  // px per second the beam travels (slow and heavy)
#define LASER_DAMAGE_MULTIPLIER    1.5f    // damage = your HP x this
#define LASER_SIZE_SCALE           0.9f    // beam is 90% of the health bar's size (length and thickness)
#define LASER_WIDTH                (HEALTH_BAR_HEIGHT * LASER_SIZE_SCALE)   // beam thickness (the hitbox is this wide too)
#define LASER_BORDER               3.0f    // black outline around the beam in px (drawing only, not part of the hitbox)
#define LASER_KNOCKBACK            6.0f    // px a zombie is shoved back when the beam passes through it

// ---- [3] Bloodrang: a boomerang made of your blood ----
// Throw it at the mouse: it holds the HP you paid. It flies out, curves back and homes in on you,
// hitting each zombie once per trip. Catch it to get its HP back AND a damage boost for every zombie it hit.
// It can only turn so sharply, so if you keep moving it overshoots and loops round again,
// bleeding HP each missed lap. If it bleeds out you lose the HP and your boost.
#define BLOODRANG_RGB              200, 20, 50
#define RANG_COST                  15.0f   // HP stored inside the boomerang when you throw it
#define RANG_DAMAGE                32.0f   // damage per hit, before the boost
#define RANG_BOOST_PER_STACK       0.10f   // each stack = +10% damage on later throws
#define RANG_MAX_STACKS            5       // boost caps at +50%
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
// or at the spot you aimed at. Every zombie the blast kills drops a blood heal (blood rain).
#define BLOOD_BOMB_RGB             230, 40, 110
#define BOMB_CHARGE_RATE           20.0f   // HP per second poured in while holding the mouse
#define BOMB_MAX_CHARGE_PERCENT    0.40f   // the orb holds at most 40% of the HP you had when you started charging
#define BOMB_DAMAGE_PER_HP         2.5f    // blast damage for each HP inside
#define BOMB_BASE_RADIUS           40.0f   // blast radius with an almost empty orb
#define BOMB_RADIUS_PER_HP         3.0f    // ...plus this many px for each HP inside
#define BOMB_THROW_SPEED           700.0f  // px per second
#define BOMB_ORB_BASE_RADIUS       6.0f    // orb size (and hitbox) with nothing inside
#define BOMB_ORB_RADIUS_PER_HP     0.35f   // ...plus this many px for each HP inside
#define BOMB_KNOCKBACK             22.0f   // px zombies are thrown away from the blast
#define BOMB_FIRE_DELAY            0.5f    // seconds after a throw before you can start charging again
#define BOMB_CHARGE_MOVE_SPEED     0.6f    // you move at 60% speed while charging
#define BOMB_BLAST_SHOW_TIME       0.35f   // how long the explosion stays on screen

// ---- [5] Last Stand: a revolver that gets stronger the LESS HP you have ----
// Damage rises smoothly from 10 at full HP to 120 near death. Bullets turn deeper red AND get bigger
// (hitbox too) as you get closer to death.
#define LAST_STAND_COST            3.0f    // HP per shot (flat)
#define LAST_STAND_FIRE_DELAY      0.333f  // 3 shots per second
#define LAST_STAND_MIN_DAMAGE      10.0f   // at full HP
#define LAST_STAND_MAX_DAMAGE      120.0f  // at (almost) 0 HP
#define LAST_STAND_BULLET_SPEED    1000.0f
#define LAST_STAND_MIN_RADIUS      5.0f    // bullet size at full HP
#define LAST_STAND_MAX_RADIUS      12.0f   // bullet size at (almost) 0 HP
#define LAST_STAND_KNOCKBACK       6.0f

// ---- pools ----
#define MAX_BULLETS                512     // pistol / last stand bullets on screen at once
#define MAX_LASERS                 8       // laser beams on screen at once
#define MAX_BOMBS                  4       // thrown blood bombs in the air at once
#define MAX_BLASTS                 8       // explosion effects on screen at once

/* =========================================================
   Types
   ========================================================= */
typedef enum { W_PISTOL, W_HEALTH_LASER, W_BLOODRANG, W_BLOOD_BOMB, W_LAST_STAND, W_COUNT } WeaponType;

// Bullet (pistol and last stand): a small circle that stops at the first zombie it hits.
typedef struct
{
	bool active;
	WeaponType type;
	CP_Vector pos, vel; // velocity of the bullet
	float life, damage, radius, knockback;
	CP_Color color;
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
	int   hitId[MAX_ZOMBIES];    // id of the zombie in each slot this beam already hurt (0 = none), so each zombie is hit once per shot
} Laser;

// Bloodrang: only one can be out at a time.
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
	CP_Vector pos, vel, target;  // target = where you aimed; it explodes there if it hits nothing first
	float stored;                // HP inside it
} BloodBomb;

// Explosion effect (drawing only).
typedef struct
{
	bool active;
	CP_Vector pos;
	float radius, life;
} Blast;

// Called by UpdateShots when a shot kills a zombie (index = its slot in s_zombies, killedBy = the gun that did it).
typedef void (*ZombieKilledFn)(int index, WeaponType killedBy);

/* =========================================================
   Gun info (used by the shooting code and the HUD)
   ========================================================= */
float       GetShotCost(WeaponType w);              // HP the gun would cost to fire right now
float       GetFireDelay(WeaponType w);             // seconds between shots
const char* GetWeaponName(WeaponType w);
CP_Color    GetWeaponColor(WeaponType w, int alpha); // barrel / crosshair / shot / pickup colour
void        GetWeaponHudText(WeaponType w, char* buf, size_t bufSize);
float       GetMoveSpeedMultiplier(void);           // 1.0 normally, slower while charging a blood bomb

/* =========================================================
   Shots
   ========================================================= */
void ResetWeapons(void);                                   // clear every shot and charge (new game)
void HandleShooting(float deltaTime);                      // left mouse: fire / charge / throw the current gun, paying HP
void UpdateShots(float deltaTime, ZombieKilledFn onZombieKilled);   // move shots, hit zombies, report kills
void DrawShots(void);
bool IsLaserActive(void);                                  // true while a health laser beam is still flying
