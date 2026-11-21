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

#define MAX_ZOMBIES             128
#define BOSS_KNOCKBACK_SCALE    0.20f     // Bosses only take 20% bullet knockback
#define BOSS_NAME               "CHARGER" // Default boss display name

typedef enum {
	// Regular Mobs
	Z_WALKER,           // Basic horde zombie
	Z_RUNNER,           // Fast, fragile rusher
	Z_BRUTE,            // Slow, high HP tank

	// Milestone Bosses (every 5 waves)
	Z_BOSS_CHARGER,     // Wave 5:  Dash charge
	Z_BOSS_SHOOT,       // Wave 10: Shoots projectiles
	Z_BOSS_SLOW,        // Wave 15: Melee/Slime slows player speed
	Z_BOSS_FINAL        // Wave 20: Final Boss (Dash + Shoots Back + Slows on hit)
} ZombieType;

typedef struct
{
	bool active;
	int id;
	ZombieType type;
	CP_Vector pos;
	float radius;
	float hp, maxHp;
	float speed, damage;
	float attackTimer, swayPhase, swayAmp, hitFlash;

	// Ability and dash tracking
	float abilityTimer;
	bool  isDashing;
	float dashDuration;
	CP_Vector dashDir;
} Zombie;

// Shared array across files
extern Zombie s_zombies[MAX_ZOMBIES];

void ResetZombies(void);              // Clear every slot and restart id counter
int  CountAliveZombies(void);         // Active zombies currently alive
void SpawnZombie(int wave);           // Spawn standard wave zombie
void SpawnBoss(int wave);             // Spawn milestone boss for this wave
const Zombie* FindBoss(void);         // Returns active boss pointer or NULL (for health bar)
void UpdateZombies(float deltaTime);  // Movement and attack logic
void DrawZombies(void);
void SeparateZombies(void);           // Zombie-vs-zombie separation