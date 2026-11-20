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

#define ZOMBIE_ATTACK_COOLDOWN     0.80f   // seconds between bites from the same zombie

Zombie     s_zombies[MAX_ZOMBIES];   // not static: shared through zombie.h
static int s_nextZombieId = 1;       // unique id per zombie

static void PlaceAtRandomEdge(Zombie* z, float margin);

// Clears every zombie slot and restarts the id counter. Called from Game_Init.
void ResetZombies(void)
{
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		s_zombies[i].active = false;
	}
	s_nextZombieId = 1;
}

//for zombie wave spawning
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

//spawn zombies
void SpawnZombie(int wave)
{
	//find inactive slot in zombie arr to spawn new zombie
	int slot = -1;
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		if (!s_zombies[i].active)
		{
			slot = i;
			break;
		}
	}
	if (slot < 0) return; // All slots full

	// Wave difficulty scaling
	//chance for runner starts at wave 3,with a max of 35% chance at wave 6 (increase by 7% per wave)
	float runnerChance = (wave >= 3) ? (0.07f * (float)(wave - 2)) : 0.0f;
	if (runnerChance > 0.35f) runnerChance = 0.35f;

	//chance for brute starts at wave 5, with a max of 18% chance at wave 9 (increase by 4% per wave)
	float bruteChance = (wave >= 5) ? (0.04f * (float)(wave - 4)) : 0.0f;
	if (bruteChance > 0.18f) bruteChance = 0.18f;

	//health scales by 10% per wave, speed scales by 3% per wave (max 50% faster), damage scales by 5% per wave
	float hpMul = 1.0f + 0.10f * (float)(wave - 1);
	float spdMul = 1.0f + 0.03f * (float)(wave - 1);
	//caps speed at 1.5 times of based speed
	if (spdMul > 1.5f) spdMul = 1.5f;

	float dmgMul = 1.0f + 0.05f * (float)(wave - 1);

	Zombie* z = &s_zombies[slot];
	z->active = true;
	z->id = s_nextZombieId++;
	z->hitFlash = 0.0f;

	// Pick zombie archetype: check brute first, then runner, else walker
	if (CP_Random_RangeFloat(0.0f, 1.0f) < bruteChance)
	{
		z->type = Z_BRUTE;
		z->maxHp = 220.0f;
		z->speed = 62.0f;
		z->radius = 26.0f;
		z->damage = 22.0f;
	}
	else if (CP_Random_RangeFloat(0.0f, 1.0f) < runnerChance)
	{
		z->type = Z_RUNNER;
		z->maxHp = 22.0f;
		z->speed = 190.0f;
		z->radius = 12.0f;
		z->damage = 6.0f;
	}
	else
	{
		z->type = Z_WALKER;
		z->maxHp = 40.0f;
		z->speed = 95.0f;
		z->radius = 15.0f;
		z->damage = 10.0f;
	}

	z->maxHp *= hpMul;
	z->speed *= spdMul;
	z->damage *= dmgMul;
	z->hp = z->maxHp;
	z->attackTimer = 0.5f;

	PlaceAtRandomEdge(z, 40.0f);
}

// The wave 20 boss: one huge, very tanky zombie. No special AI yet, it just chases you.
void SpawnBoss(void)
{
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		if (s_zombies[i].active) continue;

		Zombie* z = &s_zombies[i];
		z->active = true;
		z->id = s_nextZombieId++;
		z->type = Z_BOSS;
		z->maxHp = BOSS_HP;
		z->hp = BOSS_HP;
		z->speed = BOSS_SPEED;
		z->radius = BOSS_RADIUS;
		z->damage = BOSS_DAMAGE;
		z->attackTimer = 0.5f;
		z->hitFlash = 0.0f;
		PlaceAtRandomEdge(z, BOSS_RADIUS + 20.0f);
		return;
	}
}

const Zombie* FindBoss(void)
{
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		if (s_zombies[i].active && s_zombies[i].type == Z_BOSS) return &s_zombies[i];
	}
	return NULL;
}

// Puts a zombie just outside a random screen edge (0 = top, 1 = right, 2 = bottom, 3 = left).
static void PlaceAtRandomEdge(Zombie* z, float margin)
{
	int edge = (int)CP_Random_RangeFloat(0.0f, 4.0f);
	//where to spawn zombie based on edge
	switch (edge)
	{
	case 0:  // Top
		z->pos = CP_Vector_Set(CP_Random_RangeFloat(0.0f, windowWidth), -margin);
		break;
	case 1:  // Right
		z->pos = CP_Vector_Set(windowWidth + margin, CP_Random_RangeFloat(0.0f, windowHeight));
		break;
	case 2:  // Bottom
		z->pos = CP_Vector_Set(CP_Random_RangeFloat(0.0f, windowWidth), windowHeight + margin);
		break;
	default: // Left
		z->pos = CP_Vector_Set(-margin, CP_Random_RangeFloat(0.0f, windowHeight));
		break;
	}
}

void DrawZombies(void)
{
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		const Zombie* z = &s_zombies[i];
		if (!z->active) continue;

		float r = z->radius;
		CP_Color skinColor;
		float armThickness, armLength;

		// 1. Pick colors and arm size based on archetype
		if (z->type == Z_WALKER)
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
		else if (z->type == Z_BRUTE)
		{
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(55, 95, 55, 255);
			armThickness = 16.0f;
			armLength = r * 1.4f;
		}
		else // Z_BOSS: a huge sickly purple zombie (its health bar is drawn at the top of the screen)
		{
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 220, 220, 255) : Col(110, 60, 120, 255);
			armThickness = 30.0f;
			armLength = r * 1.3f;
		}

		// 2. Vector Math: Find forward direction and sideways direction
		CP_Vector toPlayer = CP_Vector_Subtract(player.position, z->pos);
		//this gets direction from zombie to player, then normalizes it to a unit vector
		CP_Vector fwd = GetNormalizedDirection(toPlayer);     // Points straight at player

		//this for postioning shoulders since using shoulders to point zombie to the correct direction 
		//when zombie facing norht since using right shoulder therefore -y axis is used to get the right shoulder position
		CP_Vector side = CP_Vector_Set(-fwd.y, fwd.x);        // 90 degrees sideways

		// 3. Find the left and right shoulder positions (offset sideways by radius r)
		CP_Vector leftShoulder  = CP_Vector_Subtract(z->pos, CP_Vector_Scale(side, r));
		CP_Vector rightShoulder = CP_Vector_Add(z->pos, CP_Vector_Scale(side, r));

		// 4. Find the hand positions (reach forward from shoulders toward player)
		CP_Vector reach = CP_Vector_Scale(fwd, armLength);
		CP_Vector leftHand  = CP_Vector_Add(leftShoulder, reach);
		CP_Vector rightHand = CP_Vector_Add(rightShoulder, reach);

		// 5. Draw the arms as thick lines reaching forward
		DrawColoredLine(leftShoulder.x, leftShoulder.y, leftHand.x, leftHand.y, armThickness, skinColor);
		DrawColoredLine(rightShoulder.x, rightShoulder.y, rightHand.x, rightHand.y, armThickness, skinColor);

		// 6. Draw the head circle
		G_Circle(z->pos.x, z->pos.y, r, skinColor);
		G_Ring(z->pos.x, z->pos.y, r, (z->type == Z_BOSS) ? 5.0f : 2.0f, Col(20, 35, 20, 255));
		if (z->type == Z_BOSS)
		{
			// glowing red eyes so it reads as "the big one"
			CP_Vector eyeBase = CP_Vector_Add(z->pos, CP_Vector_Scale(fwd, r * 0.45f));
			CP_Vector eyeSide = CP_Vector_Scale(side, r * 0.32f);
			CP_Vector leftEye = CP_Vector_Subtract(eyeBase, eyeSide);
			CP_Vector rightEye = CP_Vector_Add(eyeBase, eyeSide);
			G_Circle(leftEye.x, leftEye.y, 8.0f, Col(255, 40, 40, 255));
			G_Circle(rightEye.x, rightEye.y, 8.0f, Col(255, 40, 40, 255));
		}

		// 7. Draw floating health bar (if damaged; the boss has its own bar at the top of the screen)
		if (z->hp < z->maxHp && z->type != Z_BOSS)
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
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		Zombie* z = &s_zombies[i];
		if (!z->active) continue;

		// Reduce hit flash timer
		if (z->hitFlash > 0.0f)
		{
			z->hitFlash -= deltaTime;
		}

		// Steer toward player
		//make zombie chase player after getting diretion.
		CP_Vector toPlayer = CP_Vector_Subtract(player.position, z->pos);
		//the direction zombie should move in to the player
		CP_Vector moveDir = GetNormalizedDirection(toPlayer);

		z->pos = CP_Vector_Add(z->pos, CP_Vector_Scale(moveDir, z->speed * deltaTime));

		// Damage player if touching
		if (ZombieTouchingPlayer(z))   // player hitbox (player.c)
		{
			z->attackTimer -= deltaTime;
			if (z->attackTimer <= 0.0f)
			{
				player.hp -= z->damage;
				Sfx_Play(SFX_PLAYER_HURT);
				z->attackTimer = ZOMBIE_ATTACK_COOLDOWN; // Attack interval

				player.combatTimer = 0.0f;      // taking damage resets out-of-combat timer
			}
		}
	}
}

// Zombie vs zombie: pushes every overlapping pair apart so zombies don't stack on each other.
// One pass; game.c runs it a few times per frame (together with the player push in player.c).
void SeparateZombies(void)
{
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		if (!s_zombies[i].active) continue;

		// j starts at i + 1 so each pair is checked once, and never a zombie against itself
		for (int j = i + 1; j < MAX_ZOMBIES; ++j)
		{
			if (!s_zombies[j].active) continue;
			PushApart(&s_zombies[i].pos, s_zombies[i].radius, &s_zombies[j].pos, s_zombies[j].radius, 0.5f);   // each moves half
		}
	}
}
