#pragma once
#include <stdbool.h>
#include "cprocessing.h"

#define MAX_ZOMBIES          128
#define BOSS_KNOCKBACK_SCALE 0.20f

typedef enum {
    Z_WALKER,
    Z_RUNNER,
    Z_BRUTE,
    Z_BOSS_CHARGER,     // Wave 5
    Z_BOSS_SHOOT,        // Wave 10
	Z_BOSS_PULL,         // Wave 15
	Z_BOSS_FINAL 	  // Wave 20
} ZombieType;

typedef struct {
    bool active;
    int id;
    ZombieType type;
    CP_Vector pos;
    float radius;
    float hp, maxHp;
    float speed, damage;
    float attackTimer, hitFlash;

    // Ability timers
    float abilityTimer;  // Charger dash cooldown OR Spitter shoot timer
    bool  isDashing;
    float dashDuration;
    CP_Vector dashDir;
} Zombie;

extern Zombie s_zombies[MAX_ZOMBIES];

void ResetZombies(void);
int  CountAliveZombies(void);
void SpawnZombie(int wave);
void SpawnBoss(int wave);
const Zombie* FindBoss(void);
void UpdateZombies(float deltaTime);
void DrawZombies(void);
void SeparateZombies(void);