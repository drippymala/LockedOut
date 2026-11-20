//---------------------------------------------------------
// file:	zombie.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	Zombie types, the shared zombie array, and the
//			functions that spawn, move, draw and count zombies.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#pragma once
#include <stdbool.h>
#include "cprocessing.h"   // needed for CP_Vector

#define MAX_ZOMBIES          128

typedef enum { Z_WALKER, Z_RUNNER, Z_BRUTE, Z_BOSS } ZombieType;

// ---- wave 20 boss (just a huge, very tanky zombie for now: no special AI) ----
#define BOSS_NAME            "PATIENT ZERO"
#define BOSS_HP              9000.0f
#define BOSS_SPEED           58.0f
#define BOSS_RADIUS          62.0f
#define BOSS_DAMAGE          35.0f
#define BOSS_KNOCKBACK_SCALE 0.08f   // the boss only takes 8% of the normal knockback

typedef struct
{
	bool active;
	int id;
	ZombieType type;
	CP_Vector pos;
	float radius;
	float hp, maxHp;
	float speed, damage;
	float attackTimer, hitFlash;
} Zombie;

// "extern" = the array's memory is created in zombie.c,
// and every file that includes zombie.h shares that same array.
extern Zombie s_zombies[MAX_ZOMBIES];

void ResetZombies(void);              // clear every slot and restart the id counter (new game)
int  CountAliveZombies(void);         // how many zombies are on screen right now
void SpawnZombie(int wave);           // spawn one zombie at a screen edge, scaled to this wave
void SpawnBoss(void);                 // spawn the wave 20 boss at a screen edge
const Zombie* FindBoss(void);         // the boss if it is alive, otherwise NULL (for its health bar)
void UpdateZombies(float deltaTime);  // chase the player and bite when touching
void DrawZombies(void);
void SeparateZombies(void);           // push overlapping zombies apart (one pass)
