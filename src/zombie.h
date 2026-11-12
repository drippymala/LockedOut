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

typedef enum { Z_WALKER, Z_RUNNER, Z_BRUTE } ZombieType;

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
void UpdateZombies(float deltaTime);  // chase the player and bite when touching
void DrawZombies(void);
