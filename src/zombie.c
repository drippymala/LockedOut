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

#define ZOMBIE_ATTACK_COOLDOWN     0.80f   // seconds between bites from the same zombie

Zombie     s_zombies[MAX_ZOMBIES];   // not static: shared through zombie.h
static int s_nextZombieId = 1;       // unique id per zombie

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

	float roll = CP_Random_RangeFloat(0.0f, 1.0f);

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

	// Pick zombie archetype
	if (roll < bruteChance)
	{
		z->type = Z_BRUTE;
		z->maxHp = 220.0f * hpMul;
		z->speed = 62.0f * spdMul;
		z->radius = 26.0f;
		z->damage = 22.0f * dmgMul;
	}
	else if (roll < bruteChance + runnerChance)
	{
		z->type = Z_RUNNER;
		z->maxHp = 22.0f * hpMul;
		z->speed = 190.0f * spdMul;
		z->radius = 12.0f;
		z->damage = 6.0f * dmgMul;
	}
	else
	{
		z->type = Z_WALKER;
		z->maxHp = 40.0f * hpMul;
		z->speed = 95.0f * spdMul;
		z->radius = 15.0f;
		z->damage = 10.0f * dmgMul;
	}

	z->hp = z->maxHp;
	z->attackTimer = 0.5f;

	// Spawn just outside a random edge (0 = top, 1 = right, 2 = bottom, 3 = left)
	const float margin = 40.0f;
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
	//check for active zombies and draw them on screen
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		const Zombie* z = &s_zombies[i];
		if (!z->active) continue;

		float x = z->pos.x;
		float y = z->pos.y;
		float r = z->radius;
		//set skin color
		CP_Color skinColor;
		float armW, armL, armOffsetY;

		//check zombie type
		if (z->type == Z_WALKER)
		{
			//check if hit if hit chang ecolor to white for a flash effect
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(95, 155, 85, 255);
			armW = 8.0f;          // Arm width (thickness)
			armL = r * 1.5f;      // Arm length
			armOffsetY = 0.0f;    // Vertical offset
		}
		else if (z->type == Z_RUNNER)
		{//if zombie is hit flash white else stay same color / yellowish color for runener zombie
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(220, 180, 50, 255);
			armW = 6.0f;
			armL = r * 1.8f;
			armOffsetY = -r * 0.2f;
		}
		else
		{
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(55, 95, 55, 255);
			armW = 16.0f;
			armL = r * 1.4f;
			armOffsetY = r * 0.2f;
		}

		//draw zombie arms
		G_Rect(x - r - armW * 0.5f, y - armL + armOffsetY, armW, armL, skinColor);
		G_Rect(x + r - armW * 0.5f, y - armL + armOffsetY, armW, armL, skinColor);

		//draw zombie arm outlines
		CP_Settings_RectMode(CP_POSITION_CORNER);
		CP_Settings_NoFill();
		CP_Settings_Stroke(Col(20, 35, 20, 255));
		CP_Settings_StrokeWeight(1.5f);
		//draw outlines for zombie arms
		CP_Graphics_DrawRect(x - r - armW * 0.5f, y - armL + armOffsetY, armW, armL);
		CP_Graphics_DrawRect(x + r - armW * 0.5f, y - armL + armOffsetY, armW, armL);
		CP_Settings_NoStroke();
		//zombie body
		G_Circle(x, y, r, skinColor);
		G_Ring(x, y, r, 2.0f, Col(20, 35, 20, 255));

		//draw zombie health bar if not full
		if (z->hp < z->maxHp)
		{
			//draw health bar above zombie
			float barW = r * 2.4f;
			//get ratio of current health to max health to draw how much health left
			float hpRatio = z->hp / z->maxHp;
			//draw health bar above zombie
			float barY = y - r - armL - 10.0f;

			G_Rect(x - barW * 0.5f, barY, barW, 5.0f, Col(0, 0, 0, 180));
			G_Rect(x - barW * 0.5f, barY, barW * hpRatio, 5.0f, Col(220, 50, 50, 255));
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
