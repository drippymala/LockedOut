//---------------------------------------------------------
// file:	zombie.c
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	Zombie code: spawning, chasing, biting, drawing and
//			counting zombies. Wave progression stays in game.c.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#include "cprocessing.h"
#include "utils.h"
#include "game.h"     // player, windowWidth, windowHeight
#include "zombie.h"
#include "player.h"   // ZombieTouchingPlayer (player hitbox)
#include "sfx.h"      // player hurt sound

#define ZOMBIE_ATTACK_COOLDOWN 0.30f   // seconds between bites from the same zombie
#define MAX_RUNNER_SPEED       210.0f  // Cap runner speed so it never outruns the player

Zombie     s_zombies[MAX_ZOMBIES];   // not static: shared through zombie.h
static int s_nextZombieId = 1;       // unique id per zombie

// For shooting boss (Wave 10)
typedef struct {
	bool active;
	CP_Vector pos;
	CP_Vector dir;
	float life;
} EnemyBullet;

static EnemyBullet s_bossBullets[8];

// Clears every zombie slot and restarts the id counter. Called from Game_Init.
void ResetZombies(void)
{
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		s_zombies[i].active = false;
	}
	for (int i = 0; i < 8; ++i)
	{
		s_bossBullets[i].active = false;
	}
	s_nextZombieId = 1;
}

// Counts how many zombies are currently alive on screen
int CountAliveZombies(void)
{
	int count = 0;
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		if (s_zombies[i].active) count++;
	}
	return count;
}

/* =========================================================
   Boss AI Functions
   ========================================================= */

   // Wave 5: Charger Boss
static void chargerBoss(Zombie* z, CP_Vector moveDir, float dt)
{
	if (z->isDashing)
	{
		z->dashDuration -= dt;
		z->pos = CP_Vector_Add(z->pos, CP_Vector_Scale(z->dashDir, (z->speed * 5.5f) * dt));

		if (z->dashDuration <= 0.0f)
		{
			z->isDashing = false;
			z->abilityTimer = 3.0f; // Seconds before next dash
		}
	}
	else
	{
		z->pos = CP_Vector_Add(z->pos, CP_Vector_Scale(moveDir, z->speed * dt));

		if (z->abilityTimer <= 0.0f)
		{
			z->isDashing = true;
			z->dashDir = moveDir;    // Lock straight toward player
			z->dashDuration = 0.75f; // Dash lasts 0.75s
			z->hitFlash = 0.2f;      // Quick white flash visual cue
		}
	}
}

// Wave 10: Spitter Boss helpers
static void ShootAtPlayer(CP_Vector fromPos, CP_Vector toPlayer)
{
	for (int i = 0; i < 8; ++i)
	{
		if (s_bossBullets[i].active) continue;

		s_bossBullets[i].active = true;
		s_bossBullets[i].pos = fromPos;
		s_bossBullets[i].dir = GetNormalizedDirection(toPlayer);
		s_bossBullets[i].life = 3.5f;
		return;
	}
}

static void shootBoss(Zombie* z, CP_Vector toPlayer, CP_Vector moveDir, float dt)
{
	// 1. Walk toward player
	z->pos = CP_Vector_Add(z->pos, CP_Vector_Scale(moveDir, z->speed * dt));

	// 2. Count down and shoot
	z->abilityTimer -= dt;
	if (z->abilityTimer <= 0.0f)
	{
		ShootAtPlayer(z->pos, toPlayer);
		z->abilityTimer = 1.0f; // Shoot cooldown
		z->hitFlash = 0.15f;
	}
}

// Wave 15: Pull Boss (Gravitational vortex suction)
static void pullBoss(Zombie* z, CP_Vector toPlayer, CP_Vector moveDir, float dt)
{
	// 1. Slowly walk toward the player
	z->pos = CP_Vector_Add(z->pos, CP_Vector_Scale(moveDir, z->speed * dt));

	// 2. Gravitational pull if player is within 300px
	float dist = CP_Vector_Length(toPlayer);
	if (dist < 300.0f && dist > 15.0f)
	{
		// Direction pulling toward the boss
		CP_Vector pullDir = CP_Vector_Scale(moveDir, -1.0f);
		// Force increases the closer the player gets
		float pullForce = (1.0f - (dist / 300.0f)) * 195.0f;

		player.position = CP_Vector_Add(player.position, CP_Vector_Scale(pullDir, pullForce * dt));
	}
}

//WAVE 20: final boss 
// Phase 0 = Dash, Phase 1 = Shoot, Phase 2 = Pull
//Boss activates power every 2 seconds, then runs the ability for 7 seconds before going back to cooldown 7 sec only applies to the pull and the shoot phase. 
static int   s_finalPhase = 0;
static float s_finalPhaseTimer = 0.0f;
static float s_finalShootTimer = 0.0f; // Interval between bullets during Phase 1

static void finalBoss(Zombie* z, CP_Vector toPlayer, CP_Vector moveDir, float dt)
{
	// 1. Cooldown between abilities (Walks normally for 2.0s)
	if (z->abilityTimer > 0.0f)
	{
		z->abilityTimer -= dt;
		z->pos = CP_Vector_Add(z->pos, CP_Vector_Scale(moveDir, z->speed * dt));

		// Cooldown finished -> switch to the next active phase
		if (z->abilityTimer <= 0.0f)
		{
			s_finalPhase = (s_finalPhase + 1) % 3; // Cycle: Dash (0) -> Shoot (1) -> Pull (2)

			if (s_finalPhase == 0) // Charger Dash
			{
				z->isDashing = true;
				z->dashDir = moveDir;
				z->dashDuration = 0.75f;
				z->hitFlash = 0.2f;
			}
			else if (s_finalPhase == 1) // Shoot Phase (Runs for 7 seconds)
			{
				s_finalPhaseTimer = 7.0f;
				s_finalShootTimer = 0.0f; // Fire first shot immediately
				z->hitFlash = 0.2f;
			}
			else if (s_finalPhase == 2) // Pull Phase (Runs for 7 seconds)
			{
				s_finalPhaseTimer = 7.0f;
				z->hitFlash = 0.2f;
			}
		}
		return;
	}

	// 2. Active Ability Phase
	switch (s_finalPhase)
	{
	case 0: // Charger Dash
		chargerBoss(z, moveDir, dt);
		// When dash finishes, snap cooldown to 2.0s before shooting phase
		if (!z->isDashing && z->abilityTimer > 0.0f)
		{
			z->abilityTimer = 2.0f;
		}
		break;

	case 1: // Spitter Shoot: Fires continuously throughout the 7 seconds
		z->pos = CP_Vector_Add(z->pos, CP_Vector_Scale(moveDir, z->speed * dt));
		s_finalPhaseTimer -= dt;

		// Fire a projectile every 1.2 seconds without touching z->abilityTimer
		s_finalShootTimer -= dt;
		if (s_finalShootTimer <= 0.0f)
		{
			ShootAtPlayer(z->pos, toPlayer);
			s_finalShootTimer = 1.2f; // Time between shots (adjust this if you want faster/slower bullets)
			z->hitFlash = 0.15f;
		}

		// When the 7 seconds run out, begin 2.0s cooldown before pulling
		if (s_finalPhaseTimer <= 0.0f)
		{
			z->abilityTimer = 2.0f;
		}
		break;

	case 2: // Vortex Pull: Pulls continuously for 7 seconds
		pullBoss(z, toPlayer, moveDir, dt);
		s_finalPhaseTimer -= dt;

		// When the 7 seconds run out, begin 2.0s cooldown before dashing
		if (s_finalPhaseTimer <= 0.0f)
		{
			z->abilityTimer = 2.0f;
		}
		break;
	}
}

/* =========================================================
   Spawning
   ========================================================= */

void SpawnBoss(int wave)
{
	int slot = -1;
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		if (!s_zombies[i].active)
		{
			slot = i;
			break;
		}
	}
	if (slot < 0) return;

	Zombie* z = &s_zombies[slot];
	z->active = true;
	z->id = s_nextZombieId++;
	z->attackTimer = 0.0f;
	z->hitFlash = 0.0f;
	z->isDashing = false;

	switch (wave)
	{
	case 20:
		z->type = Z_BOSS_FINAL; // Final Boss (The Tyrant)
		z->maxHp = 2000.0f;      // High health pool for solo showdown
		z->speed = 75.0f;
		z->radius = 45.0f;        // Largest boss size
		z->damage = 25.0f;
		z->abilityTimer = 2.0f;   // 2s pause before starting abilities
		s_finalPhase = 2;         // Starts with phase 0 (Dash) first
		break;
	case 15:
		z->type = Z_BOSS_PULL;   // Pull / Vortex Boss
		z->maxHp = 1000.0f;       // High health tank
		z->speed = 55.0f;         // Slow walking speed
		z->radius = 40.0f;         // Giant body size
		z->damage = 25.0f;
		z->abilityTimer = 0.0f;
		break;

	case 10:
		z->type = Z_BOSS_SHOOT;  // Spitter Boss
		z->maxHp = 750.0f;
		z->speed = 75.0f;
		z->radius = 32.0f;
		z->damage = 18.0f;
		z->abilityTimer = 2.0f;
		break;

	case 5:
	default:
		z->type = Z_BOSS_CHARGER;// Charger Boss
		z->maxHp = 450.0f;
		z->speed = 90.0f;
		z->radius = 35.0f;
		z->damage = 25.0f;
		z->abilityTimer = 2.0f;
		break;
	}

	z->hp = z->maxHp;
	z->pos = CP_Vector_Set(windowWidth * 0.5f, -50.0f); // Top center
}

void SpawnZombie(int wave)
{
	int slot = -1;
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		if (!s_zombies[i].active)
		{
			slot = i;
			break;
		}
	}
	if (slot < 0) return;

	float runnerChance = (wave >= 3) ? (0.07f * (float)(wave - 2)) : 0.0f;
	if (runnerChance > 0.35f) runnerChance = 0.35f;

	float bruteChance = (wave >= 5) ? (0.04f * (float)(wave - 4)) : 0.0f;
	if (bruteChance > 0.18f) bruteChance = 0.18f;

	float hpMul = 1.0f + 0.10f * (float)(wave - 1);
	float spdMul = 1.0f + 0.03f * (float)(wave - 1);
	if (spdMul > 1.5f) spdMul = 1.5f;

	float dmgMul = 1.0f + 0.05f * (float)(wave - 1);

	Zombie* z = &s_zombies[slot];
	z->active = true;
	z->id = s_nextZombieId++;
	z->hitFlash = 0.0f;
	z->abilityTimer = 0.0f;
	z->isDashing = false;

	if (CP_Random_RangeFloat(0.0f, 1.0f) < bruteChance)
	{
		z->type = Z_BRUTE;
		z->maxHp = 200.0f;
		z->speed = 62.0f;
		z->radius = 26.0f;
		z->damage = 20.0f;
	}
	else if (CP_Random_RangeFloat(0.0f, 1.0f) < runnerChance)
	{
		z->type = Z_RUNNER;
		z->maxHp = 17.0f;
		z->speed = 160.0f;
		z->radius = 12.0f;
		z->damage = 6.0f;
	}
	else
	{
		z->type = Z_WALKER;
		z->maxHp = 25.0f;
		z->speed = 95.0f;
		z->radius = 15.0f;
		z->damage = 8.0f;
	}

	z->maxHp *= hpMul;
	z->speed *= spdMul;
	z->damage *= dmgMul;

	if (z->type == Z_RUNNER && z->speed > MAX_RUNNER_SPEED)
	{
		z->speed = MAX_RUNNER_SPEED;
	}

	z->hp = z->maxHp;
	z->attackTimer = 0.0f;

	const float margin = 40.0f;
	int edge = (int)CP_Random_RangeFloat(0.0f, 4.0f);
	switch (edge)
	{
	case 0:  z->pos = CP_Vector_Set(CP_Random_RangeFloat(0.0f, windowWidth), -margin); break;
	case 1:  z->pos = CP_Vector_Set(windowWidth + margin, CP_Random_RangeFloat(0.0f, windowHeight)); break;
	case 2:  z->pos = CP_Vector_Set(CP_Random_RangeFloat(0.0f, windowWidth), windowHeight + margin); break;
	default: z->pos = CP_Vector_Set(-margin, CP_Random_RangeFloat(0.0f, windowHeight)); break;
	}
}

/* =========================================================
   Render & Update
   ========================================================= */

void DrawZombies(void)
{
	// 1. Draw Boss Bullets
	for (int i = 0; i < 8; ++i)
	{
		if (!s_bossBullets[i].active) continue;

		float bx = s_bossBullets[i].pos.x;
		float by = s_bossBullets[i].pos.y;

		G_Circle(bx, by, 10.0f, Col(130, 20, 200, 255));
		G_Ring(bx, by, 10.0f, 2.0f, Col(40, 5, 70, 255));
		G_Circle(bx, by, 5.0f, Col(220, 140, 255, 255));
	}

	// 2. Draw Zombies & Bosses
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		const Zombie* z = &s_zombies[i];
		if (!z->active) continue;

		float r = z->radius;
		CP_Color skinColor;
		float armThickness, armLength;

		if (z->type == Z_BOSS_CHARGER)
		{
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(240, 120, 30, 255); // Orange
			armThickness = 14.0f;
			armLength = r * 1.3f;
		}
		else if (z->type == Z_BOSS_SHOOT)
		{
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(125, 30, 185, 255); // Deep Purple
			armThickness = 12.0f;
			armLength = r * 1.3f;
		}
		else if (z->type == Z_BOSS_PULL)
		{
			// Pull Boss: Dark teal/vortex blue with heavy arms
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(30, 160, 180, 255);
			armThickness = 18.0f;
			armLength = r * 1.4f;

			// Gravitational vortex danger ring
			G_Ring(z->pos.x, z->pos.y, 300.0f, 2.0f, Col(30, 160, 180, 80));
		}
		else if (z->type == Z_WALKER)
		{
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(95, 155, 85, 255);
			armThickness = 8.0f;
			armLength = r * 1.5f;
		}
		else if (z->type == Z_RUNNER)
		{
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(220, 180, 50, 255);
			armThickness = 6.0f;
			armLength = r * 1.8f;
		}
		else if (z->type == Z_BOSS_FINAL)
		{
			// Crimson Red
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(195, 25, 45, 255);
			armThickness = 20.0f;
			armLength = r * 1.5f;

			// Draw suction ring only when Phase 2 (Pull) is actively running
			if (z->abilityTimer <= 0.0f && s_finalPhase == 2)
			{
				G_Ring(z->pos.x, z->pos.y, 300.0f, 2.5f, Col(195, 25, 45, 80));
			}
		}
		else // Z_BRUTE
		{
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(55, 95, 55, 255);
			armThickness = 16.0f;
			armLength = r * 1.4f;
		}

		CP_Vector toPlayer = CP_Vector_Subtract(player.position, z->pos);
		CP_Vector fwd = GetNormalizedDirection(toPlayer);
		CP_Vector side = CP_Vector_Set(-fwd.y, fwd.x);

		CP_Vector leftShoulder = CP_Vector_Subtract(z->pos, CP_Vector_Scale(side, r));
		CP_Vector rightShoulder = CP_Vector_Add(z->pos, CP_Vector_Scale(side, r));

		CP_Vector reach = CP_Vector_Scale(fwd, armLength);
		CP_Vector leftHand = CP_Vector_Add(leftShoulder, reach);
		CP_Vector rightHand = CP_Vector_Add(rightShoulder, reach);

		DrawColoredLine(leftShoulder.x, leftShoulder.y, leftHand.x, leftHand.y, armThickness, skinColor);
		DrawColoredLine(rightShoulder.x, rightShoulder.y, rightHand.x, rightHand.y, armThickness, skinColor);

		G_Circle(z->pos.x, z->pos.y, r, skinColor);
		G_Ring(z->pos.x, z->pos.y, r, (z->type >= Z_BOSS_CHARGER ? 3.5f : 2.0f), Col(20, 35, 20, 255));

		if (z->hp < z->maxHp)
		{
			float barW = r * 2.4f;
			float hpRatio = z->hp / z->maxHp;
			float barY = z->pos.y - r - 12.0f;

			G_Rect(z->pos.x - barW * 0.5f, barY, barW, 5.0f, Col(0, 0, 0, 180));
			G_Rect(z->pos.x - barW * 0.5f, barY, barW * hpRatio, 5.0f, Col(220, 50, 50, 255));
		}
	}
}

void UpdateZombies(float deltaTime)
{
	// 1. Move and update boss bullets
	for (int i = 0; i < 8; ++i)
	{
		if (!s_bossBullets[i].active) continue;

		s_bossBullets[i].pos = CP_Vector_Add(s_bossBullets[i].pos,
			CP_Vector_Scale(s_bossBullets[i].dir, 320.0f * deltaTime));
		s_bossBullets[i].life -= deltaTime;

		if (CP_Vector_Distance(s_bossBullets[i].pos, player.position) <= 10.0f + PLAYER_RADIUS)
		{
			player.hp -= 18.0f;
			player.combatTimer = 0.0f;
			Sfx_Play(SFX_PLAYER_HURT);
			s_bossBullets[i].active = false;
			continue;
		}

		if (s_bossBullets[i].life <= 0.0f ||
			s_bossBullets[i].pos.x < -30.0f || s_bossBullets[i].pos.x > windowWidth + 30.0f ||
			s_bossBullets[i].pos.y < -30.0f || s_bossBullets[i].pos.y > windowHeight + 30.0f)
		{
			s_bossBullets[i].active = false;
		}
	}

	// 2. Update zombies
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		Zombie* z = &s_zombies[i];
		if (!z->active) continue;

		if (z->hitFlash > 0.0f)
		{
			z->hitFlash -= deltaTime;
		}

		if (z->attackTimer > 0.0f)
		{
			z->attackTimer -= deltaTime;
		}

		if (z->abilityTimer > 0.0f && !z->isDashing && z->type != Z_BOSS_SHOOT)
		{
			z->abilityTimer -= deltaTime;
		}

		CP_Vector toPlayer = CP_Vector_Subtract(player.position, z->pos);
		CP_Vector moveDir = GetNormalizedDirection(toPlayer);

		// Boss Behaviors & Regular Movement
		if (z->type == Z_BOSS_CHARGER)
		{
			chargerBoss(z, moveDir, deltaTime);
		}
		else if (z->type == Z_BOSS_SHOOT)
		{
			shootBoss(z, toPlayer, moveDir, deltaTime);
		}
		else if (z->type == Z_BOSS_PULL)
		{
			pullBoss(z, toPlayer, moveDir, deltaTime);
		}
		else if (z->type == Z_BOSS_FINAL)
		{
			finalBoss(z, toPlayer, moveDir, deltaTime);
		}
		else
		{
			z->pos = CP_Vector_Add(z->pos, CP_Vector_Scale(moveDir, z->speed * deltaTime));
		}

		// Melee Bite Hit Detection
		float reach = z->radius * 0.8f;
		float touchDist = PLAYER_RADIUS + z->radius + reach;

		if (CP_Vector_Distance(player.position, z->pos) <= touchDist)
		{
			if (z->attackTimer <= 0.0f)
			{
				player.hp -= z->damage;
				Sfx_Play(SFX_PLAYER_HURT);
				z->attackTimer = ZOMBIE_ATTACK_COOLDOWN;
				player.combatTimer = 0.0f;
			}
		}
	}
}

void SeparateZombies(void)
{
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		if (!s_zombies[i].active) continue;

		for (int j = i + 1; j < MAX_ZOMBIES; ++j)
		{
			if (!s_zombies[j].active) continue;
			PushApart(&s_zombies[i].pos, s_zombies[i].radius, &s_zombies[j].pos, s_zombies[j].radius, 0.5f);
		}
	}
}

const Zombie* FindBoss(void)
{
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		if (s_zombies[i].active && s_zombies[i].type >= Z_BOSS_CHARGER)
		{
			return &s_zombies[i];
		}
	}
	return NULL;
}