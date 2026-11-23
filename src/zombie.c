//---------------------------------------------------------
// file:	zombie.c
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	Zombie code: spawning, chasing, biting, drawing and
//			counting zombies. Wave progression stays in game.c.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#include <math.h>          // cosf, sinf (Tyrant summon ring)
#include "cprocessing.h"
#include "utils.h"
#include "game.h"     // player, windowWidth, windowHeight
#include "zombie.h"
#include "player.h"   // ZombieTouchingPlayer (player hitbox)
#include "sfx.h"      // player hurt sound

#define ZOMBIE_ATTACK_COOLDOWN 0.30f   // seconds between bites from the same zombie
#define MAX_RUNNER_SPEED       210.0f  // Cap runner speed so it never outruns the player
#define SPAWN_MIN_PLAYER_DIST  450.0f  // normal zombies never spawn closer than this to the player
#define SPAWN_TRIES            8       // random edge spots tried to find one that far away

Zombie     s_zombies[MAX_ZOMBIES];   // not static: shared through zombie.h
static int s_nextZombieId = 1;       // unique id per zombie

// For shooting boss (Wave 10)
// Represents a single projectile launched by boss entities toward the player
typedef struct {
	bool active;        // Whether this bullet slot is currently alive and in flight
	CP_Vector pos;      // Current (x, y) world position
	CP_Vector dir;      // Normalized unit vector pointing in movement trajectory
	float life;         // Remaining lifetime in seconds before despawning
} EnemyBullet;

// Static object pool for boss projectiles to avoid heap allocations
static EnemyBullet s_bossBullets[8];

// Clears every zombie slot and restarts the id counter. Called from Game_Init.
void ResetZombies(void)
{
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		s_zombies[i].active = false;
	}
	// Clear all active boss bullets
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
   // Attacks by locking a straight line toward the player and surging forward at high speed
static void chargerBoss(Zombie* z, CP_Vector moveDir, float dt)
{
	if (z->isDashing)
	{
		z->dashDuration -= dt;
		// Math: pos = pos + (dashDir * (speed * 5.5) * dt)
		// Accelerate 5.5x faster than base speed along the previously locked direction
		z->pos = CP_Vector_Add(z->pos, CP_Vector_Scale(z->dashDir, (z->speed * 5.5f) * dt));

		if (z->dashDuration <= 0.0f)
		{
			z->isDashing = false;
			z->abilityTimer = 3.0f; // Seconds before next dash
		}
	}
	else
	{
		// Regular chase movement while cooldown ticks down
		z->pos = CP_Vector_Add(z->pos, CP_Vector_Scale(moveDir, z->speed * dt));

		if (z->abilityTimer <= 0.0f)
		{
			z->isDashing = true;
			z->dashDir = moveDir;    // Lock straight toward player (snapshot current direction)
			z->dashDuration = 0.75f; // Dash lasts 0.75s
			z->hitFlash = 0.2f;      // Quick white flash visual cue
		}
	}
}

// Wave 10: Spitter Boss helpers
// Finds an unused slot in s_bossBullets and launches it directly at player position
static void ShootAtPlayer(CP_Vector fromPos, CP_Vector toPlayer)
{
	for (int i = 0; i < 8; ++i)
	{
		if (s_bossBullets[i].active) continue;

		s_bossBullets[i].active = true;
		s_bossBullets[i].pos = fromPos;
		// Math: Normalize vector from boss to player so speed remains uniform regardless of distance
		s_bossBullets[i].dir = GetNormalizedDirection(toPlayer);
		s_bossBullets[i].life = 3.5f;
		return; // Fired 1 bullet; stop search
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
// Sucks the player inward towards itself if within proximity radius
static void pullBoss(Zombie* z, CP_Vector toPlayer, CP_Vector moveDir, float dt)
{
	// 1. Slowly walk toward the player
	z->pos = CP_Vector_Add(z->pos, CP_Vector_Scale(moveDir, z->speed * dt));

	// 2. Gravitational pull if player is within 300px
	// Math: Pythagorean distance between boss and player: sqrt(dx^2 + dy^2)
	float dist = CP_Vector_Length(toPlayer);
	if (dist < 300.0f && dist > 15.0f)
	{
		// Math: moveDir points (Boss -> Player). Invert it to point (Player -> Boss)
		CP_Vector pullDir = CP_Vector_Scale(moveDir, -1.0f);

		// Math: Linear Falloff Suction:
		// When dist == 0, factor is 1.0 (Maximum suction force = 195 px/s).
		// When dist == 300, factor is 0.0 (Zero pull force at outer boundary).
		float pullForce = (1.0f - (dist / 300.0f)) * 195.0f;

		// Move player position directly towards the boss center
		player.position = CP_Vector_Add(player.position, CP_Vector_Scale(pullDir, pullForce * dt));
	}
}

//WAVE 20: final boss 
// Phase 0 = Dash, Phase 1 = Shoot, Phase 2 = Pull
//Boss activates power every 2 seconds, then runs the ability for 7 seconds before going back to cooldown 7 sec only applies to the pull and the shoot phase. 
static int   s_finalPhase = 0;         // Tracks current attack mode (0: Dash, 1: Shoot, 2: Pull)
static float s_finalPhaseTimer = 0.0f; // Countdown timer for 7-second active phases
static float s_finalShootTimer = 0.0f; // Interval between individual bullets during Phase 1

// Tyrant low-HP behaviour
#define TYRANT_SUMMON_HP        0.50f   // at or below this share of HP it starts summoning
#define TYRANT_SUMMON_INTERVAL  10.0f   // seconds between rings (the first ring comes as soon as it hits half HP)
#define TYRANT_SUMMON_COUNT     10      // zombies per ring (alternating walker / runner)
#define TYRANT_SUMMON_DISTANCE  80.0f   // how far outside its body the ring appears
#define TYRANT_ENRAGE_HP        0.25f   // at or below this share of HP it enrages
#define TYRANT_ENRAGE_SPEED     1.10f   // enraged: moves (and dashes) 10% faster
#define TYRANT_MINION_WAVE      20      // summoned zombies get wave 20 stats

static bool  s_finalEnraged = false;
static float s_finalSummonTimer = 0.0f;

static Zombie* SpawnZombieOfType(ZombieType type, int wave);   // spawning section below

// Spawns a ring of walkers and runners around the Tyrant (kept inside the screen).
// Math: Uses parametric circle equation: (x = cx + r*cos(theta), y = cy + r*sin(theta))
static void SummonRing(const Zombie* boss)
{
	float dist = boss->radius + TYRANT_SUMMON_DISTANCE;
	for (int i = 0; i < TYRANT_SUMMON_COUNT; ++i)
	{
		Zombie* m = SpawnZombieOfType((i % 2 == 0) ? Z_WALKER : Z_RUNNER, TYRANT_MINION_WAVE);
		if (!m) return;   // no free slots left

		// Math: 6.2831853f is 2*PI radians (360 degrees). Evenly distributes zombies along perimeter.
		float angle = 6.2831853f * (float)i / (float)TYRANT_SUMMON_COUNT;
		m->pos = CP_Vector_Set(
			CP_Math_ClampFloat(boss->pos.x + cosf(angle) * dist, m->radius, windowWidth - m->radius),
			CP_Math_ClampFloat(boss->pos.y + sinf(angle) * dist, m->radius, windowHeight - m->radius));
		m->hitFlash = 0.2f;   // flash so you can see them appear
	}
}

static void finalBoss(Zombie* z, CP_Vector toPlayer, CP_Vector moveDir, float dt)
{
	// 0. Low-HP behaviour (runs during every phase)
	if (!s_finalEnraged && z->hp <= z->maxHp * TYRANT_ENRAGE_HP)
	{
		s_finalEnraged = true;
		z->speed *= TYRANT_ENRAGE_SPEED;
		z->hitFlash = 0.3f;
	}
	if (z->hp <= z->maxHp * TYRANT_SUMMON_HP)
	{
		s_finalSummonTimer -= dt;
		if (s_finalSummonTimer <= 0.0f)
		{
			SummonRing(z);
			s_finalSummonTimer = TYRANT_SUMMON_INTERVAL;
		}
	}

	// 1. Cooldown between abilities (Walks normally for 2.0s)
	if (z->abilityTimer > 0.0f)
	{
		z->abilityTimer -= dt;
		z->pos = CP_Vector_Add(z->pos, CP_Vector_Scale(moveDir, z->speed * dt));

		// Cooldown finished -> switch to the next active phase
		if (z->abilityTimer <= 0.0f)
		{
			// Math: Modulo cycle 0 -> 1 -> 2 -> 0 -> ...
			s_finalPhase = (s_finalPhase + 1) % 3;

			if (s_finalPhase == 0) // Charger Dash
			{
				z->isDashing = true;
				z->dashDir = moveDir;    // Lock current angle
				z->dashDuration = 0.75f; // Sprint time
				z->hitFlash = 0.2f;      // Visual cue
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

   // Dedicated function to initialize and spawn milestone bosses
void SpawnBoss(int wave)
{
	// Linear scan to find first inactive zombie slot
	int slot = -1;
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		if (!s_zombies[i].active)
		{
			slot = i;
			break;
		}
	}
	if (slot < 0) return; // Pool full

	Zombie* z = &s_zombies[slot];
	z->active = true;
	z->id = s_nextZombieId++;
	z->attackTimer = 0.0f;
	z->hitFlash = 0.0f;
	z->isDashing = false;

	// Assign archetype baseline stats per boss milestone
	switch (wave)
	{
	case 20:
		z->type = Z_BOSS_FINAL; // Final Boss (The Tyrant)
		z->maxHp = 12000.0f;    // High health pool for solo showdown
		z->speed = 75.0f;
		z->radius = 45.0f;       // Largest boss size
		z->damage = 25.0f;
		z->abilityTimer = 2.0f;  // 2s pause before starting abilities
		s_finalPhase = 2;        // Starts with phase 0 (Dash) first because (2+1)%3 = 0
		s_finalEnraged = false;
		s_finalSummonTimer = 0.0f;   // first ring the moment it drops to half HP
		break;

	case 15:
		z->type = Z_BOSS_PULL;   // Pull / Vortex Boss
		z->maxHp = 1000.0f;      // High health tank
		z->speed = 55.0f;        // Slow walking speed
		z->radius = 40.0f;       // Giant body size
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
		z->maxHp = 550.0f;
		z->speed = 90.0f;
		z->radius = 35.0f;
		z->damage = 25.0f;
		z->abilityTimer = 2.0f;
		break;
	}

	z->hp = z->maxHp;
	// Bosses always enter from top center above the screen
	z->pos = CP_Vector_Set(windowWidth * 0.5f, -50.0f);
}

// Takes a free slot and sets it up as a normal zombie of this type, with this wave's scaling.
// Returns NULL if every slot is in use. The caller sets the position.
static Zombie* SpawnZombieOfType(ZombieType type, int wave)
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
	if (slot < 0) return NULL;

	// Multiplicative wave scaling
	// hp: +10% per wave | speed: +3% per wave (capped at +50%) | damage: +5% per wave
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

	z->type = type;
	if (type == Z_BRUTE)
	{
		z->maxHp = 200.0f;
		z->speed = 62.0f;
		z->radius = 26.0f;
		z->damage = 20.0f;
	}
	else if (type == Z_RUNNER)
	{
		z->maxHp = 17.0f;
		z->speed = 160.0f;
		z->radius = 12.0f;
		z->damage = 6.0f;
	}
	else
	{
		z->type = Z_WALKER;   // anything else (bosses use SpawnBoss) becomes a walker
		z->maxHp = 25.0f;
		z->speed = 95.0f;
		z->radius = 15.0f;
		z->damage = 8.0f;
	}

	// Apply wave scaling multipliers
	z->maxHp *= hpMul;
	z->speed *= spdMul;
	z->damage *= dmgMul;

	if (z->type == Z_RUNNER && z->speed > MAX_RUNNER_SPEED)
	{
		z->speed = MAX_RUNNER_SPEED;
	}

	z->hp = z->maxHp;
	z->attackTimer = 0.0f;
	return z;
}

void SpawnZombie(int wave)
{
	// Dynamic spawn chances based on current wave level
	// Wave 3 starts at 20% runner chance
	float runnerChance = (wave >= 3) ? (0.20f + 0.05f * (float)(wave - 3)) : 0.0f;
	if (runnerChance > 0.35f) runnerChance = 0.35f;

	float bruteChance = (wave >= 5) ? (0.04f * (float)(wave - 4)) : 0.0f;
	if (bruteChance > 0.18f) bruteChance = 0.18f;

	// Single random roll determines archetype bucket:
	// [0.0 to bruteChance)                   -> Z_BRUTE
	// [bruteChance to brute+runner)          -> Z_RUNNER
	// [brute+runner to 1.0)                  -> Z_WALKER
	float roll = CP_Random_RangeFloat(0.0f, 1.0f);

	ZombieType type = Z_WALKER;
	if (roll < bruteChance)
	{
		type = Z_BRUTE;
	}
	else if (roll < bruteChance + runnerChance)
	{
		type = Z_RUNNER;
	}

	Zombie* z = SpawnZombieOfType(type, wave);
	if (!z) return;

	// Choose spawn position along screen perimeter just out of player view,
	// but never right next to the player: try a few random spots and keep the first one far enough away
	// (if none are, use the farthest one tried)
	const float margin = 40.0f;
	CP_Vector best = CP_Vector_Set(0.0f, 0.0f);
	float bestDist = -1.0f;
	for (int attempt = 0; attempt < SPAWN_TRIES; ++attempt)
	{
		CP_Vector spot;
		int edge = (int)CP_Random_RangeFloat(0.0f, 4.0f);
		switch (edge)
		{
		case 0:  spot = CP_Vector_Set(CP_Random_RangeFloat(0.0f, windowWidth), -margin); break;
		case 1:  spot = CP_Vector_Set(windowWidth + margin, CP_Random_RangeFloat(0.0f, windowHeight)); break;
		case 2:  spot = CP_Vector_Set(CP_Random_RangeFloat(0.0f, windowWidth), windowHeight + margin); break;
		default: spot = CP_Vector_Set(-margin, CP_Random_RangeFloat(0.0f, windowHeight)); break;
		}

		float dist = CP_Vector_Distance(spot, player.position);
		if (dist > bestDist) { best = spot; bestDist = dist; }
		if (dist >= SPAWN_MIN_PLAYER_DIST) break;
	}
	z->pos = best;
}

/* =========================================================
   Render & Update
   ========================================================= */

void DrawZombies(void)
{
	// 1. Draw Boss Bullets
	// Multi-layer circles give projectiles a glowing energy ball aesthetic
	for (int i = 0; i < 8; ++i)
	{
		if (!s_bossBullets[i].active) continue;

		float bx = s_bossBullets[i].pos.x;
		float by = s_bossBullets[i].pos.y;

		G_Circle(bx, by, 10.0f, Col(130, 20, 200, 255));        // Outer purple aura
		G_Ring(bx, by, 10.0f, 2.0f, Col(40, 5, 70, 255));        // Dark perimeter ring
		G_Circle(bx, by, 5.0f, Col(220, 140, 255, 255));        // Bright lavender core
	}

	// 2. Draw Zombies & Bosses
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		const Zombie* z = &s_zombies[i];
		if (!z->active) continue;

		float r = z->radius;
		CP_Color skinColor;
		float armThickness, armLength;

		// Select visual skin colors and arm proportions by type (flashes pure white on hit)
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

			// Gravitational vortex danger ring: visual indicator of 300px suction boundary
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
			// Crimson red, or blazing orange-red once enraged
			CP_Color base = s_finalEnraged ? Col(255, 85, 20, 255) : Col(195, 25, 45, 255);
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : base;
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

		// Math: 2D Direction Vectors
		CP_Vector toPlayer = CP_Vector_Subtract(player.position, z->pos);
		CP_Vector fwd = GetNormalizedDirection(toPlayer);     // Unit forward vector toward player

		// Math: 2D Perpendicular / Normal Vector
		// Rotating a vector (x, y) by 90 degrees counter-clockwise yields (-y, x).
		// This creates a sideways axis relative to the zombie's facing angle.
		CP_Vector side = CP_Vector_Set(-fwd.y, fwd.x);

		// Math: Shoulder positions offset along the perpendicular sideways vector
		// Left Shoulder: Center - (side * radius)
		// Right Shoulder: Center + (side * radius)
		CP_Vector leftShoulder = CP_Vector_Subtract(z->pos, CP_Vector_Scale(side, r));
		CP_Vector rightShoulder = CP_Vector_Add(z->pos, CP_Vector_Scale(side, r));

		// Math: Hand positions extend forward along the forward vector: Shoulder + (fwd * reach)
		CP_Vector reach = CP_Vector_Scale(fwd, armLength);
		CP_Vector leftHand = CP_Vector_Add(leftShoulder, reach);
		CP_Vector rightHand = CP_Vector_Add(rightShoulder, reach);

		// Draw arms pointing towards player
		DrawColoredLine(leftShoulder.x, leftShoulder.y, leftHand.x, leftHand.y, armThickness, skinColor);
		DrawColoredLine(rightShoulder.x, rightShoulder.y, rightHand.x, rightHand.y, armThickness, skinColor);

		// Draw torso circle and dark contour ring
		G_Circle(z->pos.x, z->pos.y, r, skinColor);
		G_Ring(z->pos.x, z->pos.y, r, (z->type >= Z_BOSS_CHARGER ? 3.5f : 2.0f), Col(20, 35, 20, 255));

		// Draw floating health bar (only visible once zombie has taken damage)
		if (z->hp < z->maxHp)
		{
			float barW = r * 2.4f;
			// Ratio of remaining health [0.0, 1.0] determines red fill width
			float hpRatio = z->hp / z->maxHp;
			float barY = z->pos.y - r - 12.0f;

			G_Rect(z->pos.x - barW * 0.5f, barY, barW, 5.0f, Col(0, 0, 0, 180));                 // Background bar
			G_Rect(z->pos.x - barW * 0.5f, barY, barW * hpRatio, 5.0f, Col(220, 50, 50, 255));  // Red health bar
		}
	}
}

void UpdateZombies(float deltaTime)
{
	// 1. Move and update boss bullets
	for (int i = 0; i < 8; ++i)
	{
		if (!s_bossBullets[i].active) continue;

		// Math: pos = pos + (dir * speed * dt)
		s_bossBullets[i].pos = CP_Vector_Add(s_bossBullets[i].pos,
			CP_Vector_Scale(s_bossBullets[i].dir, 320.0f * deltaTime));
		s_bossBullets[i].life -= deltaTime;

		// Hit player check: circle-circle overlap formula: distance <= (bullet_radius + player_radius)
		if (CP_Vector_Distance(s_bossBullets[i].pos, player.position) <= 10.0f + PLAYER_RADIUS)
		{
			player.hp -= 18.0f;
			player.combatTimer = 0.0f;
			Sfx_Play(SFX_PLAYER_HURT);
			s_bossBullets[i].active = false;
			continue;
		}

		// Despawn bullet if lifetime expired or traveled off-screen beyond margins
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

		// Countdown hit flash timer
		if (z->hitFlash > 0.0f)
		{
			z->hitFlash -= deltaTime;
		}

		// Countdown melee bite attack cooldown
		if (z->attackTimer > 0.0f)
		{
			z->attackTimer -= deltaTime;
		}

		// Ability cooldown countdown (the Spitter and the Tyrant count their own timers down)
		if (z->abilityTimer > 0.0f && !z->isDashing && z->type != Z_BOSS_SHOOT && z->type != Z_BOSS_FINAL)
		{
			z->abilityTimer -= deltaTime;
		}

		// Calculate direction unit vector towards player
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
			// Default zombie pursuit: pos = pos + (moveDir * speed * dt)
			z->pos = CP_Vector_Add(z->pos, CP_Vector_Scale(moveDir, z->speed * deltaTime));
		}

		// Melee Bite Hit Detection
		// Math: Total collision distance accounts for player radius, zombie radius, and outstretched arms
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

// Zombie vs zombie: pushes every overlapping pair apart so zombies don't stack on each other.
void SeparateZombies(void)
{
	// O(N^2) pairwise collision check
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		if (!s_zombies[i].active) continue;

		for (int j = i + 1; j < MAX_ZOMBIES; ++j)
		{
			if (!s_zombies[j].active) continue;
			// PushApart resolves circle penetration equally (0.5f factor each)
			PushApart(&s_zombies[i].pos, s_zombies[i].radius, &s_zombies[j].pos, s_zombies[j].radius, 0.5f);
		}
	}
}

// Scans active zombies to find and return the current milestone boss (used for HUD boss health bar)
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