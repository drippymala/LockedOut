//---------------------------------------------------------
// file:    game.c
// author:  LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:   Source file containing the implementation of the 
//          game state, including initialization, game updates, and 
//          exit functions.
//
// Copyright © 2026 DigiPen, All rights reserved
//---------------------------------------------------------

#include <stdio.h>
#include "cprocessing.h"
#include "utils.h"       // Fixes C4013 'IsAreaClicked' undefined
#include "game.h"        // Matches the Game_Init / Game_Update / Game_Exit declarations
#include "mainmenu.h"
#include <math.h>
#include <stdbool.h>

#define PLAYER_RADIUS        18.0f
#define PLAYER_SPEED         290.0f
#define GUN_BARREL_LENGTH    14.0f
#define GUN_BARREL_THICKNESS 9.0f
#define GRID_CELL_SIZE       100.0f
#define MAX_DELTA_TIME       0.05f
#define MAX_ZOMBIES          128
#define MAX_BULLETS       512 // max amt of bullets
#define MAX_PIERCE_TRACK   16 // to prevent hitting the same zombie multiple times with piercing bullets

// ---- health cost per shot (every bullet costs the player's own blood) ----
#define PISTOL_COST                2.0f    // flat, cheap
#define TARGET_LOCK_COST           9.0f    // placeholder until the charge mechanic exists
#define PARASITE_COST              4.0f    // paid up front...
#define PARASITE_REFUND            5.0f    // ...and refunded per hit
#define MINIGUN_COST_MIN           0.4f    // cost per bullet when you start firing
#define MINIGUN_COST_MAX           1.6f    // ...and after MINIGUN_RAMP_TIME of holding
#define MINIGUN_RAMP_TIME          3.0f
#define HEALTH_LASER_COST_PERCENT  0.20f   // placeholder: % of CURRENT hp per shot
#define HEALTH_LASER_COST_MIN      5.0f
#define GHOST_DRAIN_RATE           70.0f   // how fast the pale "ghost" bar catches up (HP per second)

// ---- player hitbox / zombie contact damage ----
#define PLAYER_HIT_INVULN          0.30f   // grace time after a zombie hit (player blinks)
#define PLAYER_HIT_FLASH_TIME      0.35f   // red screen flash when bitten
#define ZOMBIE_ATTACK_COOLDOWN     0.80f   // seconds between bites from the same zombie
#define ZOMBIE_HIT_FLASH_TIME      0.08f   // white flash when a bullet hits a zombie
#define DEG_TO_RAD                 0.01745329f

typedef enum { Z_WALKER, Z_RUNNER, Z_BRUTE } ZombieType;
typedef enum { W_PISTOL, W_TARGET_LOCK, W_PARASITE, W_MINIGUN, W_HEALTH_LASER, W_COUNT} WeaponType;

//weapons
typedef struct
{
	const char* name;
	const char* drain;      // text describing how it drains health
	float fireDelay;        // seconds between shots
	float speed;            // bullet speed (px/s)
	float radius;
	float damage;
	float life;             // seconds before the bullet disappears
	int   pellets;
	float spreadDeg;        // total cone angle
	int   pierce;           // extra zombies a bullet can go through
	int   r, g, b;
} WeaponDef;

static const WeaponDef s_weapons[W_COUNT] =
{
	//   name             drain description                          delay    speed    rad    dmg    life   pel spread pierce   colour (R, G, B)
	{ "PISTOL",       "flat cost per shot",                      0.25f,   900.0f,  5.0f,  20.0f, 1.20f,  1,  1.5f,   0,   255, 230, 120 },
	{ "TARGET LOCK",  "health bar charges a homing shot",        0.60f,   850.0f,  6.0f,  35.0f, 1.50f,  1,  0.0f,   0,   255, 150,  60 },
	{ "PARASITE",     "pay up front, refunded on hit",           0.40f,   750.0f,  6.0f,  12.0f, 1.20f,  1,  2.0f,   0,   110, 255, 140 },
	{ "MINIGUN",      "cost increases while held",               0.085f, 1000.0f,  4.0f,   9.0f, 1.00f,  1,  5.0f,   0,   120, 200, 255 },
	{ "HEALTH LASER", "health bar extends into a laser beam",    1.10f,  2400.0f,  4.0f, 120.0f, 0.80f,  1,  0.0f,  99,   220, 120, 255 }
};

typedef struct
{
	bool active;
	WeaponType type;
	CP_Vector pos, vel; //velocity of the bullet
	float life, damage, radius;
	int pierceLeft;
	int hitIds[MAX_PIERCE_TRACK]; 
	int hitCount;
} Bullet;

typedef struct
{
	CP_Vector position;
	float     aimAngleRadians;

	float hp;       // Player's current health
	float maxHp;    // Player's maximum health
	float ghostHp;  // Health value used to display the trailing ghost bar

	WeaponType weapon;//let player be able to use different weapons.
	bool owned[W_COUNT];

	float rampTime;     // minigun: how long the trigger has been held
	float noFireFlash;  // red flash on the health bar when a shot is refused
	float invuln;       // seconds of invulnerability left after being bitten
	float hitFlash;     // seconds left on the red "you got hit" screen flash
} Player;

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
} Zombie;

static Zombie   s_zombies[MAX_ZOMBIES];
static CP_Color s_zombieColor;
static float    windowWidth = 1600.0f;
static float    windowHeight = 900.0f;
static Player   player;
static bool     s_paused = false;   // true while the pause menu is showing
static void SpawnBullet(WeaponType type, CP_Vector pos, float angle, float speedMul, float lifeMul);
static void FireWeapon(void);
static void SpawnZombie(void); 
static float s_fireTimer = 0.0f;
static int   s_nextZombieId = 1;        // unique id per zombie so piercing bullets don't double-hit
static float s_zombieSpawnTimer = 0.0f; // Timer between spawns
static int   s_wave = 0;              // Current wave number
static int   s_toSpawn = 0;           // How many zombies left to spawn this wave
static float s_spawnTimer = 0.0f;     // Delay between each spawn
static float s_waveClearTimer = 0.0f; // Delay before the next wave begins

/* ---------- Logic & Rendering ---------- */

// How much HP the selected weapon would cost to fire right now.
static float GetShotCost(WeaponType w)
{
	switch (w)
	{
	case W_PISTOL:      return PISTOL_COST;
	case W_TARGET_LOCK: return TARGET_LOCK_COST;
	case W_PARASITE:    return PARASITE_COST;
	case W_MINIGUN:     // starts cheap, gets pricier the longer you hold
	{
		float t = CP_Math_ClampFloat(player.rampTime / MINIGUN_RAMP_TIME, 0.0f, 1.0f);
		return MINIGUN_COST_MIN + (MINIGUN_COST_MAX - MINIGUN_COST_MIN) * t;
	}
	case W_HEALTH_LASER: // scales with how healthy you are
	{
		float cost = player.hp * HEALTH_LASER_COST_PERCENT;
		return (cost > HEALTH_LASER_COST_MIN) ? cost : HEALTH_LASER_COST_MIN;
	}
	default: return 1.0f;
	}
}

static void UpdatePlayerMovementAndAim(float deltaTime)
{
	CP_Vector inputDirection = CP_Vector_Set(0.0f, 0.0f);

	if (CP_Input_KeyDown(KEY_W)) inputDirection.y -= 1.0f;
	if (CP_Input_KeyDown(KEY_S)) inputDirection.y += 1.0f;
	if (CP_Input_KeyDown(KEY_A)) inputDirection.x -= 1.0f;
	if (CP_Input_KeyDown(KEY_D)) inputDirection.x += 1.0f;

	// --- movement (WASD) --- done before aiming/shooting so bullets leave from where you are this frame
	CP_Vector moveDirection = GetNormalizedDirection(inputDirection);
	CP_Vector velocity = CP_Vector_Scale(moveDirection, PLAYER_SPEED * deltaTime);
	player.position = CP_Vector_Add(player.position, velocity);

	player.position.x = CP_Math_ClampFloat(player.position.x, PLAYER_RADIUS, windowWidth - PLAYER_RADIUS);
	player.position.y = CP_Math_ClampFloat(player.position.y, PLAYER_RADIUS, windowHeight - PLAYER_RADIUS);

	// --- mouse aim --- must be worked out BEFORE firing, otherwise the bullet uses last frame's angle
	float mouseDeltaX = CP_Input_GetMouseX() - player.position.x;
	float mouseDeltaY = CP_Input_GetMouseY() - player.position.y;
	player.aimAngleRadians = atan2f(mouseDeltaY, mouseDeltaX);

	//for guns
	if (CP_Input_KeyTriggered(KEY_1) && player.owned[W_PISTOL])       player.weapon = W_PISTOL;
	if (CP_Input_KeyTriggered(KEY_2) && player.owned[W_TARGET_LOCK])  player.weapon = W_TARGET_LOCK;
	if (CP_Input_KeyTriggered(KEY_3) && player.owned[W_PARASITE])     player.weapon = W_PARASITE;
	if (CP_Input_KeyTriggered(KEY_4) && player.owned[W_MINIGUN])      player.weapon = W_MINIGUN;
	if (CP_Input_KeyTriggered(KEY_5) && player.owned[W_HEALTH_LASER]) player.weapon = W_HEALTH_LASER;

	//for shooting / left click
	bool trigger = CP_Input_MouseDown(MOUSE_BUTTON_LEFT);

	// Minigun ramp-up: the longer the trigger is held, the more each bullet costs
	if (trigger && player.weapon == W_MINIGUN)
	{
		player.rampTime += deltaTime;
	}
	else
	{
		player.rampTime -= deltaTime * 1.5f;     // cools down faster than it heats up
		if (player.rampTime < 0.0f) player.rampTime = 0.0f;
	}

	// Decrease fire rate cooldown
	s_fireTimer -= deltaTime;

	// Shoot when holding or clicking Left Mouse Button
	if (trigger && s_fireTimer <= 0.0f)
	{
		float cost = GetShotCost(player.weapon);

		if (player.hp - cost < 1.0f)             // you can't shoot yourself to death
		{
			player.noFireFlash = 0.35f;          // flash the health bar red
			s_fireTimer = 0.2f;                  // try again shortly
		}
		else
		{
			player.hp -= cost;                   // every shot costs health
			FireWeapon();                        // spawns the bullet(s) at the gun tip, using the gun's stats
			s_fireTimer = s_weapons[player.weapon].fireDelay; // reset fire cooldown
		}
	}

	// refused-shot flash fades out
	player.noFireFlash = CP_Math_ClampFloat(player.noFireFlash - deltaTime, 0.0f, 1.0f);

	// player hitbox timers: invulnerability window + red screen flash after a bite
	player.invuln   = CP_Math_ClampFloat(player.invuln - deltaTime, 0.0f, PLAYER_HIT_INVULN);
	player.hitFlash = CP_Math_ClampFloat(player.hitFlash - deltaTime, 0.0f, PLAYER_HIT_FLASH_TIME);

	// the pale "ghost" bar trails behind the real bar so you can see how much a shot cost
	player.ghostHp = CP_Math_ClampFloat(player.ghostHp - GHOST_DRAIN_RATE * deltaTime, player.hp, player.maxHp);
}

static void DrawBackgroundGrid(void)
{
	CP_Color backgroundColor = Col(28, 34, 26, 255);
	CP_Color gridLineColor = Col(255, 255, 255, 10);

	CP_Graphics_ClearBackground(backgroundColor);

	for (float x = 0.0f; x <= windowWidth; x += GRID_CELL_SIZE)
	{
		DrawColoredLine(x, 0.0f, x, windowHeight, 1.0f, gridLineColor);
	}
	for (float y = 0.0f; y <= windowHeight; y += GRID_CELL_SIZE)
	{
		DrawColoredLine(0.0f, y, windowWidth, y, 1.0f, gridLineColor);
	}
}

static void DrawPlayerSprite(void)
{
	// blink while invulnerable after a zombie hit, so the player can see they got bitten
	int alpha = (player.invuln > 0.0f && ((int)(CP_System_GetSeconds() * 30.0f) % 2) == 0) ? 110 : 255;

	// the gun barrel takes the colour of the selected weapon
	const WeaponDef* def = &s_weapons[player.weapon];
	CP_Color gunColor = Col(def->r, def->g, def->b, alpha);
	CP_Color bodyColor = Col(60, 130, 220, alpha);
	CP_Color highlightColor = Col(120, 185, 255, alpha);

	CP_Vector aimDirection = CP_Vector_Set(cosf(player.aimAngleRadians), sinf(player.aimAngleRadians));
	float     barrelReach = PLAYER_RADIUS + GUN_BARREL_LENGTH;
	CP_Vector gunTipPos = CP_Vector_Add(player.position, CP_Vector_Scale(aimDirection, barrelReach));

	DrawColoredLine(player.position.x, player.position.y, gunTipPos.x, gunTipPos.y, GUN_BARREL_THICKNESS, gunColor);
	DrawFilledCircle(player.position.x, player.position.y, PLAYER_RADIUS, bodyColor);
	DrawFilledCircle(player.position.x - 4.0f, player.position.y - 4.0f, PLAYER_RADIUS * 0.45f, highlightColor);
}

static void DrawMouseCrosshair(void)
{
	float    mouseX = CP_Input_GetMouseX();
	float    mouseY = CP_Input_GetMouseY();
	const WeaponDef* def = &s_weapons[player.weapon];
	CP_Color crosshairColor = Col(def->r, def->g, def->b, 230);

	DrawCircleOutline(mouseX, mouseY, 11.0f, 2.0f, crosshairColor);
	DrawColoredLine(mouseX - 20.0f, mouseY, mouseX - 14.0f, mouseY, 2.0f, crosshairColor);
	DrawColoredLine(mouseX + 14.0f, mouseY, mouseX + 20.0f, mouseY, 2.0f, crosshairColor);
	DrawColoredLine(mouseX, mouseY - 20.0f, mouseX, mouseY - 14.0f, 2.0f, crosshairColor);
	DrawColoredLine(mouseX, mouseY + 14.0f, mouseX, mouseY + 20.0f, 2.0f, crosshairColor);
}

static void DrawHud(void)
{
	float screenW = (float)CP_System_GetWindowWidth();
	float screenH = (float)CP_System_GetWindowHeight();

	char waveBuf[64];
	int alive = CountAliveZombies();

	// Main Wave text
	snprintf(waveBuf, sizeof(waveBuf), "WAVE %d", s_wave);
	CP_Settings_TextSize(36.0f);
	CP_Settings_Fill(Col(255, 230, 120, 255));
	CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_RIGHT, CP_TEXT_ALIGN_V_TOP);
	CP_Font_DrawText(waveBuf, screenW - 30.0f, 25.0f);

	// Subtitle: Zombies remaining in this wave
	snprintf(waveBuf, sizeof(waveBuf), "Zombies Left: %d", s_toSpawn + alive);
	CP_Settings_TextSize(20.0f);
	CP_Settings_Fill(Col(200, 200, 200, 255));
	CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_RIGHT, CP_TEXT_ALIGN_V_TOP);
	CP_Font_DrawText(waveBuf, screenW - 30.0f, 68.0f);

	// Wave cleared announcement in the center of the screen
	if (s_waveClearTimer > 0.0f)
	{
		CP_Settings_TextSize(48.0f);
		CP_Settings_Fill(Col(120, 255, 150, 255));
		CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
		CP_Font_DrawText("WAVE CLEARED!", screenW * 0.5f, screenH * 0.35f);
	}

	// ==========================================
	// 2. TOP-LEFT: HEALTH BAR
	// ==========================================
	char buf[160];
	const Player* p = &player; // Fixed syntax error here

	const float bx = 30.0f;
	const float by = 28.0f;
	const float bw = 380.0f;
	const float bh = 30.0f;

	float ratio = CP_Math_ClampFloat(p->hp / p->maxHp, 0.0f, 1.0f);
	float ghost = CP_Math_ClampFloat(p->ghostHp / p->maxHp, 0.0f, 1.0f);

	CP_Color hpCol = ratio > 0.5f ? Col(70, 200, 90, 255) :
		(ratio > 0.25f ? Col(230, 190, 50, 255) :
			Col(220, 50, 50, 255));

	// Background backing
	G_Rect(bx - 4.0f, by - 4.0f, bw + 8.0f, bh + 8.0f, Col(0, 0, 0, 170));

	// Ghost health bar
	G_Rect(bx, by, bw * ghost, bh, Col(255, 255, 255, 130));

	// Active health bar
	G_Rect(bx, by, bw * ratio, bh, hpCol);

	// Red flash if shot refused due to low health
	if (player.noFireFlash > 0.0f)
	{
		G_Rect(bx - 4.0f, by - 4.0f, bw + 8.0f, bh + 8.0f, Col(255, 0, 0, (int)(player.noFireFlash / 0.35f * 200.0f)));
	}

	// Health text on the bar
	snprintf(buf, sizeof(buf), "HP %.0f / %.0f", player.hp, player.maxHp);
	G_Text(buf, bx + bw * 0.5f, by + bh * 0.5f, 20.0f, Col(255, 255, 255, 255), AL_CENTER);

	// Active weapon drain label at bottom center
	snprintf(buf, sizeof(buf), "%s: %s  -  costs %.1f HP now", s_weapons[player.weapon].name, s_weapons[player.weapon].drain, GetShotCost(player.weapon));
	G_Text(buf, screenW * 0.5f, screenH - 22.0f, 22.0f, Col(230, 230, 230, 255), AL_CENTER);
}

static void DrawPauseOverlay(void)
{
	// dim the whole screen, then draw the pause text on top
	G_Rect(0.0f, 0.0f, windowWidth, windowHeight, Col(0, 0, 0, 150));
	G_Text("PAUSED", windowWidth * 0.5f, windowHeight * 0.42f, 100.0f, Col(255, 255, 255, 255), AL_CENTER);
	G_Text("ESC - Resume      M - Main Menu", windowWidth * 0.5f, windowHeight * 0.42f + 90.0f, 34.0f, Col(220, 220, 220, 255), AL_CENTER);
}

//BULLETS
static Bullet M_bullets[MAX_BULLETS];

static void SpawnBullet(WeaponType type, CP_Vector pos, float angle, float speedMul, float lifeMul)
{
	//check which weapons 
	const WeaponDef* def = &s_weapons[type];
	//speed of bullet is weapon speed * speed multiplier (for powerups)
	float spd = def->speed * speedMul;

	for (int i = 0; i < MAX_BULLETS; ++i){
		// If this slot is already active (flying on screen), skip it
		if (M_bullets[i].active) continue;

		M_bullets[i] = (Bullet){
			.active = true,
			.type = type,
			.pos = pos,
			.vel = CP_Vector_Set(cosf(angle) * spd, sinf(angle) * spd),
			.life = def->life * lifeMul,
			.damage = def->damage,
			.radius = def->radius,
			.pierceLeft = def->pierce
		};
		return;
	}
}

// Fires the currently selected weapon using its stats from s_weapons:
// pellets = how many bullets per shot, spreadDeg = random cone the bullets fly in.
static void FireWeapon(void)
{
	const WeaponDef* def = &s_weapons[player.weapon];

	// Bullet emerges from the gun tip
	CP_Vector aimDir = CP_Vector_Set(cosf(player.aimAngleRadians), sinf(player.aimAngleRadians));
	float barrelReach = PLAYER_RADIUS + GUN_BARREL_LENGTH;
	CP_Vector gunTipPos = CP_Vector_Add(player.position, CP_Vector_Scale(aimDir, barrelReach));

	float halfSpread = def->spreadDeg * 0.5f * DEG_TO_RAD;
	for (int i = 0; i < def->pellets; ++i)
	{
		float angle = player.aimAngleRadians + CP_Random_RangeFloat(-halfSpread, halfSpread);
		// multi-pellet guns get slightly uneven speed/range so the spread looks natural
		float speedMul = (def->pellets > 1) ? CP_Random_RangeFloat(0.85f, 1.10f) : 1.0f;
		float lifeMul  = (def->pellets > 1) ? CP_Random_RangeFloat(0.80f, 1.10f) : 1.0f;
		SpawnBullet(player.weapon, gunTipPos, angle, speedMul, lifeMul);
	}
}

// Squared distance from point p to the line segment a-b.
// Used for the bullet hitbox: we test the whole path the bullet travelled this frame,
// not just where it ended up, so fast bullets (e.g. 2400 px/s Health Laser) can't skip through a zombie.
static float PointSegDistSq(CP_Vector p, CP_Vector a, CP_Vector b)
{
	CP_Vector ab = CP_Vector_Subtract(b, a);
	float len2 = ab.x * ab.x + ab.y * ab.y;
	float t = 0.0f;
	if (len2 > 0.0001f)
	{
		CP_Vector ap = CP_Vector_Subtract(p, a);
		t = CP_Math_ClampFloat((ap.x * ab.x + ap.y * ab.y) / len2, 0.0f, 1.0f);
	}
	CP_Vector closest = CP_Vector_Add(a, CP_Vector_Scale(ab, t));
	float dx = p.x - closest.x;
	float dy = p.y - closest.y;
	return dx * dx + dy * dy;
}

//set true false for if bullet has hit a zombie.
//let bullet pierce thru zombie once and only damage once until bullet leaves zombie.
static bool BulletAlreadyHit(const Bullet* b, int zombieId)
{
	int n;
	if (b->hitCount < MAX_PIERCE_TRACK){
		n = b->hitCount;
	}else{
		n = MAX_PIERCE_TRACK;
	}for (int i = 0; i < n; ++i){
		if (b->hitIds[i] == zombieId) return true; // Already damaged this zombie, skip!
	}
	return false;
}

static void DrawBullets(void)
{
	for (int i = 0; i < MAX_BULLETS; ++i)
	{
		const Bullet* b = &M_bullets[i];
		if (!b->active) continue;
		//color of buller based on weapon type
		const WeaponDef* def = &s_weapons[b->type];
		G_Circle(b->pos.x, b->pos.y, b->radius, Col(def->r, def->g, def->b, 255));
	}
}

static void UpdateBullets(float dt)
{
	for (int i = 0; i < MAX_BULLETS; ++i)
	{
		Bullet* b = &M_bullets[i];
		//check if bullet is active
		if (!b->active) continue;

		// 1. Move the bullet, bullet vector is scaled by delta time to ensure consistent movement regardless of frame rate
		CP_Vector prevPos = b->pos;   // remembered for the swept hitbox check below
		b->pos = CP_Vector_Add(b->pos, CP_Vector_Scale(b->vel, dt));
		b->life -= dt;

		// 2. Despawn if lifetime runs out or it leaves the screen
		if (b->life <= 0.0f ||
			b->pos.x < -30.0f || b->pos.x > windowWidth + 30.0f ||
			b->pos.y < -30.0f || b->pos.y > windowHeight + 30.0f)
		{
			//cahnge bullet to inactive so slot can be used for another bullet
			b->active = false;
			continue;
		}

		// 3. Bullet hitbox vs zombies (swept along the path travelled this frame)
		for (int z = 0; z < MAX_ZOMBIES; ++z)
		{
			Zombie* zombie = &s_zombies[z];
			if (!zombie->active) continue;

			// circles touch somewhere along prevPos -> pos ?
			float hitDist = b->radius + zombie->radius;
			if (PointSegDistSq(zombie->pos, prevPos, b->pos) > hitDist * hitDist) continue;

			// piercing bullets only damage each zombie once
			if (BulletAlreadyHit(b, zombie->id)) continue;

			// Hurt zombie & trigger hit flash
			zombie->hp -= b->damage;
			zombie->hitFlash = ZOMBIE_HIT_FLASH_TIME;

			// Knockback along the bullet's direction (heavier weapons shove harder)
			float knock = (b->type == W_TARGET_LOCK) ? 9.0f : (b->type == W_HEALTH_LASER ? 14.0f : 4.0f);
			zombie->pos = CP_Vector_Add(zombie->pos, CP_Vector_Scale(GetNormalizedDirection(b->vel), knock));

			// Parasite weapon heals player
			if (b->type == W_PARASITE)
			{
				player.hp = CP_Math_ClampFloat(player.hp + PARASITE_REFUND, 0.0f, player.maxHp);
			}

			// Check if zombie died
			if (zombie->hp <= 0.0f)
			{
				zombie->active = false;
			}

			// No pierce left -> bullet disappears on hit
			if (b->pierceLeft <= 0)
			{
				b->active = false;
				break; // Stop checking other zombies for this bullet
			}

			// Piercing bullet (e.g. Health Laser): keeps flying, remember who it already hit
			b->pierceLeft--;
			b->hitIds[b->hitCount % MAX_PIERCE_TRACK] = zombie->id;
			b->hitCount++;
		}
	}
}
//weapon HUD

//ZOMBIES

//for zombie wave spawning
// Counts how many zombies are currently alive on screen
static int CountAliveZombies(void)
{
	int count = 0;
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		if (s_zombies[i].active) count++;
	}
	return count;
}

// Begins the next wave with more zombies
static void StartWave(void)
{
	s_wave++;
	// Wave 1: 8 zombies, Wave 2: 12 zombies, Wave 3: 16 zombies...
	s_toSpawn = 4 + (4 * s_wave);
	s_spawnTimer = 0.0f;
	s_waveClearTimer = 0.0f;
}

static void UpdateWave(float deltaTime)
{
	int alive = CountAliveZombies();

	// 1. If we still have zombies to spawn in this wave
	if (s_toSpawn > 0)
	{
		s_spawnTimer -= deltaTime;
		if (s_spawnTimer <= 0.0f)
		{
			SpawnZombie();
			s_toSpawn--;
			s_spawnTimer = 1.0f; // 1 second between each zombie spawn
		}
	}
	// 2. All zombies spawned AND all zombies killed -> Wave Cleared!
	else if (alive == 0)
	{
		if (s_waveClearTimer <= 0.0f)
		{
			// Reward player for completing the wave
			player.maxHp += 5.0f;
			player.hp = CP_Math_ClampFloat(player.hp + 25.0f, 0.0f, player.maxHp);
			s_waveClearTimer = 3.0f; // 3 seconds break before next wave
		}
		else
		{
			s_waveClearTimer -= deltaTime;
			if (s_waveClearTimer <= 0.0f)
			{
				StartWave(); // Start next wave!
			}
		}
	}
}

//spawn zombies
static void SpawnZombie(void)
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
	if (slot < 0) return; // All slots full

	// Wave difficulty scaling
	float runnerChance = (s_wave >= 3) ? (0.07f * (float)(s_wave - 2)) : 0.0f;
	if (runnerChance > 0.35f) runnerChance = 0.35f;

	float bruteChance = (s_wave >= 5) ? (0.04f * (float)(s_wave - 4)) : 0.0f;
	if (bruteChance > 0.18f) bruteChance = 0.18f;

	float roll = CP_Random_RangeFloat(0.0f, 1.0f);

	float hpMul = 1.0f + 0.10f * (float)(s_wave - 1);
	float spdMul = 1.0f + 0.03f * (float)(s_wave - 1);
	if (spdMul > 1.5f) spdMul = 1.5f;

	float dmgMul = 1.0f + 0.05f * (float)(s_wave - 1);

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

static void DrawZombies(void)
{
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		const Zombie* z = &s_zombies[i];
		if (!z->active) continue;

		float x = z->pos.x;
		float y = z->pos.y;
		float r = z->radius;

		CP_Color skinColor;
		float armW, armL, armOffsetY;

		//check zombie type
		if (z->type == Z_WALKER)
		{
			//check if hit if hit chang ecolor to white for a flash effect
			skinColor = (z->hitFlash > 0.0f) ? Col(255, 255, 255, 255) : Col(95, 155, 85, 255);
			//move arms to replicate getting hit
			armW = 8.0f;
			armL = r * 1.5f;
			armOffsetY = 0.0f;
		}
		else if (z->type == Z_RUNNER)
		{
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
		CP_Graphics_DrawRect(x - r - armW * 0.5f, y - armL + armOffsetY, armW, armL);
		CP_Graphics_DrawRect(x + r - armW * 0.5f, y - armL + armOffsetY, armW, armL);
		CP_Settings_NoStroke();
		//zombie body
		G_Circle(x, y, r, skinColor);
		G_Ring(x, y, r, 2.0f, Col(20, 35, 20, 255));

		//draw zombie health bar if not full
		if (z->hp < z->maxHp)
		{
			float barW = r * 2.4f;
			float hpRatio = z->hp / z->maxHp;
			float barY = y - r - armL - 10.0f;

			G_Rect(x - barW * 0.5f, barY, barW, 5.0f, Col(0, 0, 0, 180));
			G_Rect(x - barW * 0.5f, barY, barW * hpRatio, 5.0f, Col(220, 50, 50, 255));
		}
	}
}

static void UpdateZombies(float deltaTime)
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
		//hit box from prev code s
		float hitDist = PLAYER_RADIUS + z->radius;
		if (CP_Vector_Distance(player.position, z->pos) <= hitDist)
		{
			z->attackTimer -= deltaTime;
			if (z->attackTimer <= 0.0f)
			{
				player.hp -= z->damage;
				z->attackTimer = 0.8f; // Attack interval
			}
		}
	}
}


static void Render(void)
{
	DrawBackgroundGrid();
	DrawZombies();
	DrawBullets();
	DrawPlayerSprite();
	DrawHud();

	// red flash over the screen when a zombie bites you
	if (player.hitFlash > 0.0f)
	{
		G_Rect(0.0f, 0.0f, windowWidth, windowHeight, Col(200, 0, 0, (int)(player.hitFlash / PLAYER_HIT_FLASH_TIME * 90.0f)));
	}

	if (s_paused) DrawPauseOverlay();
	else          DrawMouseCrosshair();   // hide the crosshair while paused
}

static void GoToMenu(void)
{
	CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
}

/* =========================================================
   CProcessing Engine Callbacks
   ========================================================= */
void Game_Init(void)
{
	Font_EnsureLoaded();   // the menu frees its font on exit, so reload it for the pause text
	s_paused = false;      // always start a new game unpaused

	windowWidth = (float)CP_System_GetWindowWidth();
	windowHeight = (float)CP_System_GetWindowHeight();

	player.position = CP_Vector_Set(windowWidth * 0.5f, windowHeight * 0.5f);
	player.aimAngleRadians = 0.0f;

	// Initialize player health
	player.hp = 100.0f;       // Player starts with 100 HP
	player.maxHp = 100.0f;    // Maximum player health is 100 HP
	player.ghostHp = 100.0f;  // Ghost health starts at maximum HP
	player.rampTime = 0.0f;
	player.noFireFlash = 0.0f;
	player.invuln = 0.0f;
	player.hitFlash = 0.0f;
	s_fireTimer = 0.3f;

	//weapon ownership and selection
	for (int i = 0; i < W_COUNT; ++i)
	{
		player.owned[i] = false;
	}
	player.owned[W_PISTOL] = true;   // Starter weapon
	player.weapon = W_PISTOL;

	//clear starting bullets
	for (int i = 0; i < MAX_BULLETS; ++i)
	{
		M_bullets[i].active = false;
	}

	s_zombieColor = Col(95, 155, 85, 255);

	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		s_zombies[i].active = false;
	}
	s_nextZombieId = 1;
	s_wave = 0;
	StartWave();
}

void Game_Update(void)
{
	float deltaTime = CP_System_GetDt();
	if (deltaTime > MAX_DELTA_TIME)
	{
		deltaTime = MAX_DELTA_TIME;
	}

	if (CP_Input_KeyTriggered(KEY_Q))
	{
		CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
		return;
	}

	// ESC toggles pause; while paused, M goes back to the main menu
	if (CP_Input_KeyTriggered(KEY_ESCAPE)) s_paused = !s_paused;
	if (s_paused && CP_Input_KeyTriggered(KEY_M))
	{
		GoToMenu();
		return;
	}

	// only advance the game when not paused (Render below still draws the frozen frame)
	if (!s_paused)
	{
		UpdatePlayerMovementAndAim(deltaTime);
		UpdateZombies(deltaTime);
		UpdateBullets(deltaTime);
		UpdateWave(deltaTime);   // <-- Manages spawning & wave progression
	}

	Render();
}

void Game_Exit(void)
{
}