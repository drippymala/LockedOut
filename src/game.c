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
#define MAX_DELTA_TIME       0.033f // 30 fps cap: a 900 px/s bullet moves at most ~30 px/frame, less than the smallest hitbox (34 px)
#define MAX_ZOMBIES          128
#define MAX_BULLETS       512 // max amt of bullets

// ---- health cost per shot (every bullet costs the player's own blood) ----
#define PISTOL_COST                2.0f    // flat, cheap
#define PISTOL_KNOCKBACK           4.0f    // px a zombie is shoved back when hit
#define GHOST_DRAIN_RATE           70.0f   // how fast the pale "ghost" bar catches up (HP per second)

// ---- pickups dropped by zombies ----
#define MAX_PICKUPS           64
#define MAX_TEXTS             32       // floating "+30 HP" style pop-ups on screen at once
#define MEDKIT_HEAL           30.0f    // medkit: instant heal
#define BANDAGE_POOL          40.0f    // bandage: total HP healed over time...
#define BANDAGE_RATE          6.0f     //          ...at this many HP per second
#define BLOOD_DROP_HEAL       4.0f     // blood drop: small instant heal
#define BLOOD_MAGNET_RANGE    150.0f   //             ...blood drops fly to you inside this range...
#define BLOOD_MAGNET_SPEED    430.0f   //             ...at this speed (px/s)
#define BLOODLUST_TIME        12.0f    // bloodlust: seconds of kill-healing
#define BLOODLUST_KILL_HEAL   5.0f     //            ...HP healed per kill while it lasts
#define TWO_PI_F              6.2831853f

// ---- player hitbox / zombie contact damage ----
#define ZOMBIE_ATTACK_COOLDOWN     0.80f   // seconds between bites from the same zombie
#define ZOMBIE_HIT_FLASH_TIME      0.08f   // white flash when a bullet hits a zombie
#define OVERLAP_PASSES             4       // how many times per frame overlaps are pushed apart
#define PLAYER_HIGHLIGHT_OFFSET    4.0f    // offset of the shine spot on the player's body
#define CROSSHAIR_RING_RADIUS      11.0f
#define CROSSHAIR_TICK_START       14.0f
#define CROSSHAIR_TICK_END         20.0f
#define CROSSHAIR_THICKNESS        2.0f

typedef enum { Z_WALKER, Z_RUNNER, Z_BRUTE } ZombieType;
typedef enum { W_PISTOL, W_COUNT } WeaponType;

typedef enum {
	PICK_BLOOD,     // Small heal, flies toward the player
	PICK_MEDKIT,    // Large instant heal
	PICK_BANDAGE,   // Medium heal
	PICK_BLOODLUST, // Special buff item
	PICK_WEAPON     // Unlocks a new gun type
} PickupType;

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
	int   r, g, b;
} WeaponDef;


static const WeaponDef s_weapons[W_COUNT] =
{
	//   name             drain description                          delay    speed    rad    dmg    life   colour (R, G, B)
	{ "PISTOL",       "flat cost per shot",                      0.25f,   900.0f,  5.0f,  20.0f, 1.20f,  255, 230, 120 }
};

typedef struct
{
	bool active;
	WeaponType type;
	CP_Vector pos, vel; // velocity of the bullet
	float life, damage, radius;
} Bullet;

typedef struct
{
	CP_Vector position;
	CP_Vector aimDirection;   // unit vector from the player toward the mouse

	float hp;       // Player's current health
	float maxHp;    // Player's maximum health
	float ghostHp;  // Health value used to display the trailing ghost bar

	WeaponType weapon;//let player be able to use different weapons.
	bool owned[W_COUNT];

	float noFireFlash;  // red flash on the health bar when a shot is refused

	float regenPool;    // bandage: HP still waiting to be healed over time
	float bloodlust;    // bloodlust: seconds left (kills heal you while this is above 0)

	float combatTimer;  // tracks time since last shot or damage taken
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

typedef struct
{
	bool active;
	PickupType type;
	CP_Vector pos;
	float life;      // seconds left before it disappears
	float bob;       // used for the gentle up/down float
} Pickup;

typedef struct
{
	bool active;
	CP_Vector pos;
	float life;
	char text[32];
	int r, g, b;
} FloatText;

static Zombie   s_zombies[MAX_ZOMBIES];
static Pickup   s_pickups[MAX_PICKUPS];
static FloatText s_texts[MAX_TEXTS];
static int      s_textNext = 0;   // next slot to use for a floating text
static Pickup	s_pickups[MAX_PICKUPS]; // Hold all pickup slots
static CP_Color s_zombieColor;
static float    windowWidth = 1600.0f;
static float    windowHeight = 900.0f;
static Player   player;
static bool     s_paused = false;   // true while the pause menu is showing (syahmi)
static void     SpawnBullet(WeaponType type, CP_Vector pos, CP_Vector direction, float speedMul, float lifeMul);
static void     FireWeapon(void);
static void     SpawnZombie(void);
static int      CountAliveZombies(void);
static float    s_fireTimer = 0.0f;
static int      s_nextZombieId = 1;        // unique id per zombie
static float    s_zombieSpawnTimer = 0.0f; // Timer between spawns
static int      s_wave = 0;              // Current wave number
static int      s_toSpawn = 0;           // How many zombies left to spawn this wave
static float    s_spawnTimer = 0.0f;     // Delay between each spawn
static float    s_waveClearTimer = 0.0f; // Delay before the next wave begins

/* ---------- Logic & Rendering ---------- */

// How much HP the selected weapon would cost to fire right now.
static float GetShotCost(WeaponType w)
{
	switch (w)
	{
	case W_PISTOL: return PISTOL_COST;
	default:       return 1.0f;
	}
}

// WASD movement.
static void MovePlayer(float deltaTime)
{
	CP_Vector input = CP_Vector_Set(0.0f, 0.0f);
	if (CP_Input_KeyDown(KEY_W)) input.y -= 1.0f;
	if (CP_Input_KeyDown(KEY_S)) input.y += 1.0f;
	if (CP_Input_KeyDown(KEY_A)) input.x -= 1.0f;
	if (CP_Input_KeyDown(KEY_D)) input.x += 1.0f;

	// normalised so moving diagonally isn't faster
	CP_Vector direction = GetNormalizedDirection(input);
	player.position = CP_Vector_Add(player.position, CP_Vector_Scale(direction, PLAYER_SPEED * deltaTime));

	// keep the player inside the window
	player.position.x = CP_Math_ClampFloat(player.position.x, PLAYER_RADIUS, windowWidth - PLAYER_RADIUS);
	player.position.y = CP_Math_ClampFloat(player.position.y, PLAYER_RADIUS, windowHeight - PLAYER_RADIUS);
}

// Point the gun at the mouse.
static void AimPlayerAtMouse(void)
{
	CP_Vector mouse = CP_Vector_Set(CP_Input_GetMouseX(), CP_Input_GetMouseY());
	CP_Vector toMouse = CP_Vector_Subtract(mouse, player.position);

	// if the mouse is exactly on the player there is no direction, so keep the last aim
	if (CP_Vector_Length(toMouse) > 0.0001f)
	{
		player.aimDirection = CP_Vector_Normalize(toMouse);
	}
}

static void UpdatePlayerMovementAndAim(float deltaTime)
{
	MovePlayer(deltaTime);   // WASD, before aiming/shooting so bullets leave from where you are this frame
	AimPlayerAtMouse();      // must be worked out BEFORE firing

	//for guns
	if (CP_Input_KeyTriggered(KEY_1) && player.owned[W_PISTOL])       player.weapon = W_PISTOL;

	//for shooting / left click
	bool trigger = CP_Input_MouseDown(MOUSE_BUTTON_LEFT);

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
			player.combatTimer = 0.0f;          // shooting resets out-of-combat timer
		}

	}

	// refused-shot flash fades out
	player.noFireFlash = CP_Math_ClampFloat(player.noFireFlash - deltaTime, 0.0f, 1.0f);

	// bloodlust countdown
	player.bloodlust = CP_Math_ClampFloat(player.bloodlust - deltaTime, 0.0f, BLOODLUST_TIME);

	// bandage: heal a little every frame from the queued pool
	if (player.regenPool > 0.0f && player.hp < player.maxHp)
	{
		float heal = BANDAGE_RATE * deltaTime;
		if (heal > player.regenPool) heal = player.regenPool;                   // not more than is queued
		if (heal > player.maxHp - player.hp) heal = player.maxHp - player.hp;   // not more than the missing HP
		player.regenPool -= heal;
		player.hp += heal;
	}

	// the pale "ghost" bar trails behind the real bar so you can see how much a shot cost
	player.ghostHp = CP_Math_ClampFloat(player.ghostHp - GHOST_DRAIN_RATE * deltaTime, player.hp, player.maxHp);

	// --- OUT-OF-COMBAT HEALTH REGENERATION ---
	// If the player hasn't shot or taken damage, increase the timer
	player.combatTimer += deltaTime;

	// After 5 seconds of inactivity, slowly regenerate HP (e.g., 5 HP per second)
	if (player.combatTimer >= 5.0f && player.hp < player.maxHp)
	{
		float regenRate = 5.0f; // Amount of HP healed per second
		player.hp += regenRate * deltaTime;

		// Clamp so it never exceeds max HP
		if (player.hp > player.maxHp) {
			player.hp = player.maxHp;
		}

		// Keep ghost health bar in sync so it looks smooth
		if (player.ghostHp < player.hp) {
			player.ghostHp = player.hp;
		}
	}
}

// Player body + gun barrel.
static void DrawPlayerSprite(void)
{
	int alpha = 255;

	// the gun barrel takes the colour of the selected weapon
	const WeaponDef* weapon = &s_weapons[player.weapon];
	CP_Color gunColor       = Col(weapon->r, weapon->g, weapon->b, alpha);
	CP_Color bodyColor      = Col(60, 130, 220, alpha);
	CP_Color highlightColor = Col(120, 185, 255, alpha);

	// the barrel starts at the player's centre and points toward the mouse
	CP_Vector gunTip = CP_Vector_Add(player.position, CP_Vector_Scale(player.aimDirection,PLAYER_RADIUS + GUN_BARREL_LENGTH));

	DrawColoredLine(player.position.x, player.position.y, gunTip.x, gunTip.y, GUN_BARREL_THICKNESS, gunColor);
	DrawFilledCircle(player.position.x, player.position.y, PLAYER_RADIUS, bodyColor);
	DrawFilledCircle(player.position.x - PLAYER_HIGHLIGHT_OFFSET, player.position.y - PLAYER_HIGHLIGHT_OFFSET,
		PLAYER_RADIUS * 0.45f, highlightColor);
}

// Ring with four tick marks around the mouse position, coloured like the selected weapon.
static void DrawMouseCrosshair(void)
{
	float mouseX = CP_Input_GetMouseX();
	float mouseY = CP_Input_GetMouseY();
	const WeaponDef* weapon = &s_weapons[player.weapon];
	CP_Color color = Col(weapon->r, weapon->g, weapon->b, 230);

	float in  = CROSSHAIR_TICK_START;   // each tick starts this far from the centre...
	float out = CROSSHAIR_TICK_END;     // ...and ends this far

	DrawCircleOutline(mouseX, mouseY, CROSSHAIR_RING_RADIUS, CROSSHAIR_THICKNESS, color);
	DrawColoredLine(mouseX - out, mouseY, mouseX - in, mouseY, CROSSHAIR_THICKNESS, color);   // left
	DrawColoredLine(mouseX + in, mouseY, mouseX + out, mouseY, CROSSHAIR_THICKNESS, color);   // right
	DrawColoredLine(mouseX, mouseY - out, mouseX, mouseY - in, CROSSHAIR_THICKNESS, color);   // up
	DrawColoredLine(mouseX, mouseY + in, mouseX, mouseY + out, CROSSHAIR_THICKNESS, color);   // down
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
		snprintf(waveBuf, sizeof(waveBuf), "WAVE %d CLEARED!", s_wave);

		CP_Settings_TextSize(48.0f);
		CP_Settings_Fill(Col(120, 255, 150, 255));
		CP_Settings_TextAlignment(CP_TEXT_ALIGN_H_CENTER, CP_TEXT_ALIGN_V_MIDDLE);
		CP_Font_DrawText(waveBuf, screenW * 0.5f, screenH * 0.35f);
	}

	// ==========================================
	// 2. TOP-LEFT: HEALTH BAR (Syahmi)
	// ==========================================
	char buf[160];
	const Player* p = &player; // Pointer to the player struct

	// Define the positioning and dimensions of the health bar on the screen
	const float bx = 30.0f; // X-coordinate (left margin)
	const float by = 28.0f; // Y-coordinate (top margin)
	const float bw = 380.0f; // Total width of the health bar
	const float bh = 30.0f; // Height of the health bar

	float ratio = CP_Math_ClampFloat(p->hp / p->maxHp, 0.0f, 1.0f);           // Current active health ratio
	float ghost = CP_Math_ClampFloat(p->ghostHp / p->maxHp, 0.0f, 1.0f);     // Trailing ghost health ratio

	// Dynamically change health bar color based on remaining health percentage:
	// - Green (> 50%) for healthy state
	// - Yellow (25% - 50%) for warning state
	// - Red (< 25%) for critical danger state
	CP_Color hpCol = ratio > 0.5f ? Col(70, 200, 90, 255) :
		(ratio > 0.25f ? Col(230, 190, 50, 255) :
			Col(220, 50, 50, 255));

	// Draw a dark background bounding box/border behind the health bar for UI contrast
	G_Rect(bx - 4.0f, by - 4.0f, bw + 8.0f, bh + 8.0f, Col(0, 0, 0, 170));

	// Draw the white trailing "ghost" health bar underneath the active bar to visualize recent health costs
	G_Rect(bx, by, bw * ghost, bh, Col(255, 255, 255, 130));

	// Draw the main active health bar reflecting the player's exact current HP pool
	G_Rect(bx, by, bw * ratio, bh, hpCol);

	// Bandage: pale green segment showing how much healing is still queued
	float queued = player.regenPool;
	if (queued > player.maxHp - player.hp) queued = player.maxHp - player.hp;
	float pool = CP_Math_ClampFloat(queued / player.maxHp, 0.0f, 1.0f);
	if (pool > 0.0f) G_Rect(bx + bw * ratio, by, bw * pool, bh, Col(150, 255, 170, 110));

	// If the player tries to shoot without enough health, flash a red warning overlay over the health bar
	if (player.noFireFlash > 0.0f)
	{
		G_Rect(bx - 4.0f, by - 4.0f, bw + 8.0f, bh + 8.0f, Col(255, 0, 0, (int)(player.noFireFlash / 0.35f * 200.0f)));
	}

	// Render text over the health bar displaying exact numerical values (e.g., "HP 80 / 100")
	snprintf(buf, sizeof(buf), "HP %.0f / %.0f", player.hp, player.maxHp);
	G_Text(buf, bx + bw * 0.5f, by + bh * 0.5f, 20.0f, Col(255, 255, 255, 255), AL_CENTER);

	// status lines under the health bar
	float statusY = by + bh + 28.0f;
	if (player.bloodlust > 0.0f)
	{
		snprintf(buf, sizeof(buf), "BLOODLUST %.0fs  (kills heal +%.0f)", player.bloodlust, BLOODLUST_KILL_HEAL);
		G_Text(buf, bx, statusY, 22.0f, Col(230, 100, 255, 255), AL_LEFT);
		statusY += 26.0f;
	}
	if (player.regenPool > 0.5f)
	{
		snprintf(buf, sizeof(buf), "REGEN: %.0f HP queued", player.regenPool);
		G_Text(buf, bx, statusY, 22.0f, Col(150, 255, 190, 255), AL_LEFT);
	}

	// Render the active weapon name, its cost description, and the exact HP cost per shot at the bottom center of the screen
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

static void SpawnBullet(WeaponType type, CP_Vector pos, CP_Vector direction, float speedMul, float lifeMul)
{
	//check which weapons 
	const WeaponDef* def = &s_weapons[type];
	//speed of bullet is weapon speed * speed multiplier (for powerups)
	float spd = def->speed * speedMul;

	for (int i = 0; i < MAX_BULLETS; ++i) {
		// If this slot is already active (flying on screen), skip it
		if (M_bullets[i].active) continue;

		M_bullets[i] = (Bullet){
			.active = true,
			.type = type,
			.pos = pos,
			.vel = CP_Vector_Scale(direction, spd),
			.life = def->life * lifeMul,
			.damage = def->damage,
			.radius = def->radius
		};
		return;
	}
}

// Fires one bullet from the gun tip, straight toward the mouse.
static void FireWeapon(void)
{
	// Bullet emerges from the gun tip
	float barrelReach = PLAYER_RADIUS + GUN_BARREL_LENGTH;
	CP_Vector gunTipPos = CP_Vector_Add(player.position, CP_Vector_Scale(player.aimDirection, barrelReach));

	SpawnBullet(player.weapon, gunTipPos, player.aimDirection, 1.0f, 1.0f);
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

/* =========================================================
   Floating text (the "+30 HP" pop-ups)
   ========================================================= */
static void SpawnText(CP_Vector pos, const char* text, int r, int g, int b)
{
	s_texts[s_textNext] = (FloatText){ .active = true, .pos = pos, .life = 1.1f, .r = r, .g = g, .b = b };
	snprintf(s_texts[s_textNext].text, sizeof(s_texts[s_textNext].text), "%s", text);
	s_textNext = (s_textNext + 1) % MAX_TEXTS;   // wrap around, old texts have faded by then
}

static void UpdateFloatTexts(float deltaTime)
{
	for (int i = 0; i < MAX_TEXTS; ++i)
	{
		if (!s_texts[i].active) continue;

		s_texts[i].life -= deltaTime;
		if (s_texts[i].life <= 0.0f)
		{
			s_texts[i].active = false;
			continue;
		}
		s_texts[i].pos.y -= 40.0f * deltaTime;   // drift upward
	}
}

static void DrawFloatTexts(void)
{
	for (int i = 0; i < MAX_TEXTS; ++i)
	{
		if (!s_texts[i].active) continue;

		float alpha = CP_Math_ClampFloat(255.0f * s_texts[i].life, 0.0f, 255.0f);   // fades out over the last second
		G_Text(s_texts[i].text, s_texts[i].pos.x, s_texts[i].pos.y - 30.0f, 24.0f,
			Col(s_texts[i].r, s_texts[i].g, s_texts[i].b, (int)alpha), AL_CENTER);
	}
}

/* =========================================================
   Pickups
   ========================================================= */
   // Heals the player, never above max HP.
static void Heal(float amount)
{
	player.hp = CP_Math_ClampFloat(player.hp + amount, 0.0f, player.maxHp);
}

// How close the player has to be to collect it (blood drops are small).
static float PickupRadius(PickupType type)
{
	if (type == PICK_BLOOD) return 9.0f;
	return 18.0f;
}

static void SpawnPickup(PickupType type, CP_Vector pos)
{
	for (int i = 0; i < MAX_PICKUPS; ++i)
	{
		if (s_pickups[i].active) continue;   // slot in use, try the next one

		s_pickups[i] = (Pickup){
			.active = true,
			.type = type,
			// keep it on screen even if the zombie died at the edge
			.pos = CP_Vector_Set(CP_Math_ClampFloat(pos.x, 30.0f, windowWidth - 30.0f),
								 CP_Math_ClampFloat(pos.y, 30.0f, windowHeight - 30.0f)),
			.life = (type == PICK_BLOOD) ? 10.0f : 14.0f,
			.bob = CP_Random_RangeFloat(0.0f, TWO_PI_F)
		};
		return;
	}
}

// What happens when the player touches a pickup of this type.
static void CollectPickup(PickupType type)
{
	char buf[48];
	switch (type)
	{
	case PICK_BLOOD:
		Heal(BLOOD_DROP_HEAL);
		break;
	case PICK_MEDKIT:
		Heal(MEDKIT_HEAL);
		snprintf(buf, sizeof(buf), "+%.0f HP", MEDKIT_HEAL);
		SpawnText(player.position, buf, 120, 255, 140);
		break;
	case PICK_BANDAGE:
		player.regenPool += BANDAGE_POOL;     // healed gradually by the player update
		snprintf(buf, sizeof(buf), "REGEN +%.0f", BANDAGE_POOL);
		SpawnText(player.position, buf, 150, 255, 200);
		break;
	case PICK_BLOODLUST:
		player.bloodlust = BLOODLUST_TIME;    // kills heal you until this runs out
		SpawnText(player.position, "BLOODLUST!", 230, 80, 255);
		break;
	}
}

static void UpdatePickups(float deltaTime)
{
	for (int i = 0; i < MAX_PICKUPS; ++i)
	{
		if (!s_pickups[i].active) continue;

		s_pickups[i].bob += deltaTime;
		s_pickups[i].life -= deltaTime;
		if (s_pickups[i].life <= 0.0f)         // nobody collected it in time
		{
			s_pickups[i].active = false;
			continue;
		}

		CP_Vector toPlayer = CP_Vector_Subtract(player.position, s_pickups[i].pos);
		float distance = CP_Vector_Length(toPlayer);

		// blood drops fly toward the player once they are close enough
		if (s_pickups[i].type == PICK_BLOOD && distance < BLOOD_MAGNET_RANGE)
		{
			s_pickups[i].pos = CP_Vector_Add(s_pickups[i].pos,
				CP_Vector_Scale(GetNormalizedDirection(toPlayer), BLOOD_MAGNET_SPEED * deltaTime));
		}

		// touching the player = collected
		if (distance < PLAYER_RADIUS + PickupRadius(s_pickups[i].type))
		{
			CollectPickup(s_pickups[i].type);
			s_pickups[i].active = false;
		}
	}
}

static void DrawPickups(void)
{
	for (int i = 0; i < MAX_PICKUPS; ++i)
	{
		if (!s_pickups[i].active) continue;

		// blink during the last 3 seconds so the player knows it is about to vanish
		if (s_pickups[i].life < 3.0f && ((int)(s_pickups[i].life * 10.0f) % 2) == 0) continue;

		float x = s_pickups[i].pos.x;
		float y = s_pickups[i].pos.y + sinf(s_pickups[i].bob * 3.0f) * 3.0f;   // gentle up/down float

		switch (s_pickups[i].type)
		{
		case PICK_BLOOD:
			G_Circle(x, y, 8.0f, Col(200, 20, 40, 255));
			G_Circle(x - 2.0f, y - 2.0f, 3.0f, Col(255, 140, 150, 255));
			break;
		case PICK_MEDKIT:
			G_Rect(x - 15.0f, y - 15.0f, 30.0f, 30.0f, Col(240, 240, 240, 255));
			G_Rect(x - 9.0f, y - 3.0f, 18.0f, 6.0f, Col(220, 30, 40, 255));    // red cross, horizontal bar
			G_Rect(x - 3.0f, y - 9.0f, 6.0f, 18.0f, Col(220, 30, 40, 255));    // red cross, vertical bar
			break;
		case PICK_BANDAGE:
			G_Circle(x, y, 17.0f, Col(235, 200, 150, 255));
			G_Ring(x, y, 17.0f, 3.0f, Col(120, 255, 170, 255));
			G_Text("REGEN", x, y - 28.0f, 16.0f, Col(150, 255, 200, 255), AL_CENTER);
			break;
		case PICK_BLOODLUST:
			G_Circle(x, y, 17.0f, Col(150, 40, 190, 255));
			G_Circle(x, y, 8.0f, Col(255, 60, 90, 255));
			G_Text("BLOODLUST", x, y - 28.0f, 16.0f, Col(230, 120, 255, 255), AL_CENTER);
			break;
		}
	}
}

/* =========================================================
   Loot: what a zombie leaves behind when it dies
   ========================================================= */
static void DropLoot(CP_Vector pos, ZombieType type)
{
	float roll = CP_Random_RangeFloat(0.0f, 1.0f);

	// brutes always drop a real heal (medkit or bandage), nudged aside so it doesn't sit on top of the normal drop
	if (type == Z_BRUTE)
	{
		CP_Vector nudged = CP_Vector_Set(pos.x + CP_Random_RangeFloat(-14.0f, 14.0f),
			pos.y + CP_Random_RangeFloat(-14.0f, 14.0f));
		SpawnPickup((CP_Random_RangeFloat(0.0f, 1.0f) < 0.5f) ? PICK_MEDKIT : PICK_BANDAGE, nudged);
	}

	// drop chances (tune these later for the Drop Rates task)
	if (roll < 0.03f) SpawnPickup(PICK_BLOODLUST, pos);   //  3%  rare
	else if (roll < 0.08f) SpawnPickup(PICK_MEDKIT, pos);      //  5%
	else if (roll < 0.14f) SpawnPickup(PICK_BANDAGE, pos);     //  6%
	else if (roll < 0.54f) SpawnPickup(PICK_BLOOD, pos);       // 40%  most common
	// otherwise nothing drops
}

// Called the moment a zombie dies (index = its slot in s_zombies): drops loot and applies bloodlust.
static void OnZombieKilled(int index)
{
	DropLoot(s_zombies[index].pos, s_zombies[index].type);

	if (player.bloodlust > 0.0f)
	{
		Heal(BLOODLUST_KILL_HEAL);
		SpawnText(s_zombies[index].pos, "+5", 230, 80, 255);
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

		// 3. Bullet hitbox vs zombies: do the bullet circle and zombie circle overlap?
		// (Safe because MAX_DELTA_TIME keeps a bullet's step per frame smaller than the smallest hitbox.)
		for (int z = 0; z < MAX_ZOMBIES; ++z)
		{
			Zombie* zombie = &s_zombies[z];
			if (!zombie->active) continue;

			if (!AreCirclesIntersecting(b->pos.x, b->pos.y, b->radius,
			                            zombie->pos.x, zombie->pos.y, zombie->radius)) continue;

			// Hurt zombie & trigger hit flash
			zombie->hp -= b->damage;
			zombie->hitFlash = ZOMBIE_HIT_FLASH_TIME;

			// Knockback along the bullet's direction
			zombie->pos = CP_Vector_Add(zombie->pos, CP_Vector_Scale(GetNormalizedDirection(b->vel), PISTOL_KNOCKBACK));

			// Check if zombie died
			if (zombie->hp <= 0.0f)
			{
				zombie->active = false;
				OnZombieKilled(z);   // loot drop + bloodlust heal
			}

			// Bullet disappears on hit
			b->active = false;
			break; // Stop checking other zombies for this bullet
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
	float runnerChance = (s_wave >= 3) ? (0.07f * (float)(s_wave - 2)) : 0.0f;
	if (runnerChance > 0.35f) runnerChance = 0.35f;

	//chance for brute starts at wave 5, with a max of 18% chance at wave 9 (increase by 4% per wave)
	float bruteChance = (s_wave >= 5) ? (0.04f * (float)(s_wave - 4)) : 0.0f;
	if (bruteChance > 0.18f) bruteChance = 0.18f;

	float roll = CP_Random_RangeFloat(0.0f, 1.0f);

	//health scales by 10% per wave, speed scales by 3% per wave (max 50% faster), damage scales by 5% per wave
	float hpMul = 1.0f + 0.10f * (float)(s_wave - 1);
	float spdMul = 1.0f + 0.03f * (float)(s_wave - 1);
	//caps speed at 1.5 times of based speed
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

static void DrawZombies(void)
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
		float hitDist = PLAYER_RADIUS + z->radius;
		float dist = CP_Vector_Distance(player.position, z->pos);

		if (dist <= hitDist)
		{
			z->attackTimer -= deltaTime;
			if (z->attackTimer <= 0.0f)
			{
				player.hp -= z->damage;
				z->attackTimer = 0.8f; // Attack interval

				player.combatTimer = 0.0f;      // taking damage resets out-of-combat timer
			}
		}
	}
}


// Pushes circle A out of circle B so they are exactly touching.
// shareA = how much of the push A takes: 0.5 = both move half, 1.0 = only A moves.
static void PushApart(CP_Vector* a, float radiusA, CP_Vector* b, float radiusB, float shareA)
{
	CP_Vector fromBToA = CP_Vector_Subtract(*a, *b);
	float distance = CP_Vector_Length(fromBToA);
	float overlap = (radiusA + radiusB) - distance;
	if (overlap <= 0.0f) return;                               // not touching

	// direction to push along (if both are exactly on top of each other, pick any)
	CP_Vector direction = (distance > 0.0001f) ? CP_Vector_Scale(fromBToA, 1.0f / distance) : CP_Vector_Set(1.0f, 0.0f);

	*a = CP_Vector_Add(*a, CP_Vector_Scale(direction, overlap * shareA));
	*b = CP_Vector_Subtract(*b, CP_Vector_Scale(direction, overlap * (1.0f - shareA)));
}

// Pushes circles apart until nothing overlaps: zombie vs zombie, and zombie vs player.
// Runs once per frame after everything has moved, so overlaps never reach the screen.
static void ResolveOverlaps(void)
{
	// a few passes, because fixing one overlap can create another in a crowd
	for (int pass = 0; pass < OVERLAP_PASSES; ++pass)
	{
		// zombie vs zombie: each one moves half of the overlap
		for (int i = 0; i < MAX_ZOMBIES; ++i)
		{
			if (!s_zombies[i].active) continue;

			for (int j = i + 1; j < MAX_ZOMBIES; ++j)
			{
				if (!s_zombies[j].active) continue;
				PushApart(&s_zombies[i].pos, s_zombies[i].radius, &s_zombies[j].pos, s_zombies[j].radius, 0.5f);
			}
		}

		// zombie vs player: done last so no zombie is left inside the player.
		// shareA = 1.0, so the player never moves and the zombie takes the whole push
		for (int i = 0; i < MAX_ZOMBIES; ++i)
		{
			if (!s_zombies[i].active) continue;
			PushApart(&s_zombies[i].pos, s_zombies[i].radius, &player.position, PLAYER_RADIUS, 1.0f);
		}
	}
}


static void Render(void)
{
	DrawBackgroundGrid();
	DrawPickups();          // under the zombies and the player
	DrawZombies();
	DrawBullets();
	DrawPlayerSprite();
	DrawFloatTexts();       // "+30 HP" pop-ups above everything in the world
	DrawHud();

	if (s_paused) DrawPauseOverlay();
	else          DrawMouseCrosshair();   // hide the crosshair while paused
}

static void GoToMenu(void)
{
	CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
}

/* =========================================================
   [GAME OVER] START  -- everything for the game over screen
   ========================================================= */

   // ---- colours (R, G, B) -- copied from mainmenu.c so both screens match ----
#define GO_TITLE         200, 30, 30     // "GAME OVER" colour (same red as the "BLEED OUT" title)
#define GO_TITLE_BRIGHT  255, 30, 30     // the title pulses between GO_TITLE and this (same as the menu)
#define GO_BORDER_OFF    90, 20, 20      // button border: normal      (menu button border)
#define GO_BORDER_ON     255, 230, 120   // button border: mouse over  (menu hover yellow)
#define GO_FILL_OFF      60, 12, 12      // button body: normal        (menu button body)
#define GO_FILL_ON       170, 30, 30     // button body: mouse over    (menu hover red)
#define GO_GLOW          255, 230, 120   // soft glow around a hovered button
#define GO_STAT_TEXT     230, 230, 230   // wave / kills / score text  (menu subtitle grey)
#define GO_BEST_TEXT     255, 230, 120   // "best" text                (menu best-score yellow)

#define GO_BTN_W 380.0f             // button width
#define GO_BTN_H 70.0f              // button height

static bool  s_gameOver = false;    // true while the game over screen is showing
static float s_goTimer = 0.0f;      // seconds since the player died
static float s_goTryGlow = 0.0f;    // TRY AGAIN button: 0 = dark, 1 = lit up
static float s_goMenuGlow = 0.0f;   // MAIN MENU button: 0 = dark, 1 = lit up

// ---- scoring (same points as the original BLEED OUT game) ----
static int   s_score = 0;           // points this run
static int   s_kills = 0;           // zombies killed this run
static int   s_goWave = 0;          // wave the player died on
static int   s_bestScore = 0;       // best score since the program started (NOT reset on restart)
static int   s_bestWave = 0;        // best wave since the program started (NOT reset on restart)

// kill tracking: remembers which zombie was in each slot last frame
static int        s_goSeenId[MAX_ZOMBIES];     // zombie id seen in each slot (0 = empty slot)
static ZombieType s_goSeenType[MAX_ZOMBIES];   // and what type it was
static float      s_goPrevClear = 0.0f;        // last frame's s_waveClearTimer (to spot "wave cleared")

// clears the score and tracking. Called from Game_Init, so every new run starts from 0
static void GameOver_Reset(void)
{
	s_gameOver = false;
	s_score = 0;
	s_kills = 0;
	s_goPrevClear = 0.0f;
	for (int i = 0; i < MAX_ZOMBIES; ++i) s_goSeenId[i] = 0;
}

// Works out the score WITHOUT touching the team's zombie / bullet code.
// Called once per frame: a zombie that was in a slot last frame but is gone now = a kill.
static void GameOver_TrackScore(void)
{
	// 1. kills: Walker = 10, Runner = 15, Brute = 50 points
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		// was a zombie here last frame, but now the slot is empty (or holds a newer zombie)?
		if (s_goSeenId[i] != 0 && (!s_zombies[i].active || s_zombies[i].id != s_goSeenId[i]))
		{
			s_kills++;
			if (s_goSeenType[i] == Z_BRUTE)       s_score += 50;
			else if (s_goSeenType[i] == Z_RUNNER) s_score += 15;
			else                                  s_score += 10;
		}

		// remember what is in this slot now, for next frame
		if (s_zombies[i].active)
		{
			s_goSeenId[i] = s_zombies[i].id;
			s_goSeenType[i] = s_zombies[i].type;
		}
		else
		{
			s_goSeenId[i] = 0;
		}
	}

	// 2. wave cleared bonus: 100 x wave (the team's s_waveClearTimer jumps from 0 to 3 when a wave is cleared)
	if (s_goPrevClear <= 0.0f && s_waveClearTimer > 0.0f) s_score += 100 * s_wave;
	s_goPrevClear = s_waveClearTimer;
}

// blends colour 1 -> colour 2 (t = 0 gives colour 1, t = 1 gives colour 2)
static CP_Color GO_MixColor(int r1, int g1, int b1, int r2, int g2, int b2, float t, int a)
{
	return Col((int)(r1 + (r2 - r1) * t), (int)(g1 + (g2 - g1) * t), (int)(b1 + (b2 - b1) * t), a);
}

// top-left corner of each button (worked out from the window size)
static float GO_TryX(void) { return (windowWidth - GO_BTN_W) * 0.5f; }
static float GO_TryY(void) { return windowHeight * 0.55f; }
static float GO_MenuX(void) { return GO_TryX(); }
static float GO_MenuY(void) { return GO_TryY() + 90.0f; }

// is the mouse inside the button that starts at (x, y)?
static bool GO_MouseOver(float x, float y)
{
	float mx = CP_Input_GetMouseX();
	float my = CP_Input_GetMouseY();
	return (mx >= x && mx <= x + GO_BTN_W && my >= y && my <= y + GO_BTN_H);
}

// draw one button. glow: 0 = normal colours, 1 = lit up with a glow around it
static void GO_DrawButton(float x, float y, const char* label, float glow)
{
	// soft glow around the button (invisible when glow = 0)
	for (int i = 5; i >= 1; i--)
	{
		float grow = 4.0f + 5.0f * i;
		G_Rect(x - grow, y - grow, GO_BTN_W + grow * 2.0f, GO_BTN_H + grow * 2.0f,
			Col(GO_GLOW, (int)(glow * (60 - 10 * i))));
	}
	// border
	G_Rect(x - 4.0f, y - 4.0f, GO_BTN_W + 8.0f, GO_BTN_H + 8.0f,
		GO_MixColor(GO_BORDER_OFF, GO_BORDER_ON, glow, 255));
	// button body
	G_Rect(x, y, GO_BTN_W, GO_BTN_H, GO_MixColor(GO_FILL_OFF, GO_FILL_ON, glow, 255));
	// label
	G_Text(label, x + GO_BTN_W * 0.5f, y + GO_BTN_H * 0.5f, 42.0f, Col(255, 255, 255, 255), AL_CENTER);
}

// call this once, the moment the player dies
static void GameOver_Start(void)
{
	s_gameOver = true;
	s_goTimer = 0.0f;
	s_goTryGlow = 0.0f;
	s_goMenuGlow = 0.0f;
	s_goWave = s_wave;

	if (s_score > s_bestScore) s_bestScore = s_score;   // remember the bests
	if (s_goWave > s_bestWave) s_bestWave = s_goWave;
}

// call every frame while the game over screen is showing (draws on top of Render())
static void GameOver_UpdateAndDraw(float dt)
{
	s_goTimer += dt;

	// 1. hover: the glow fades up when the mouse is on a button, and fades down when it leaves
	float target = GO_MouseOver(GO_TryX(), GO_TryY()) ? 1.0f : 0.0f;
	s_goTryGlow += (target - s_goTryGlow) * 12.0f * dt;
	target = GO_MouseOver(GO_MenuX(), GO_MenuY()) ? 1.0f : 0.0f;
	s_goMenuGlow += (target - s_goMenuGlow) * 12.0f * dt;

	// 2. clicks (ignored for the first 0.5s so a click made while dying doesn't skip the screen)
	if (s_goTimer > 0.5f && CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT))
	{
		if (GO_MouseOver(GO_TryX(), GO_TryY()))
		{
			Game_Init();   // TRY AGAIN = reset everything (hp, zombies, bullets, wave, score) and hide this screen
			return;
		}
		if (GO_MouseOver(GO_MenuX(), GO_MenuY()))
		{
			GoToMenu();   // MAIN MENU
			return;
		}
	}

	// 3. draw: dark overlay that fades in
	float fade = s_goTimer / 0.4f;
	if (fade > 1.0f) fade = 1.0f;
	int a = (int)(255 * fade);
	G_Rect(0.0f, 0.0f, windowWidth, windowHeight, Col(0, 0, 0, (int)(170 * fade)));

	// title "GAME OVER" (pulses slowly, with a shadow behind it)
	float pulse = 0.5f + 0.5f * sinf(s_goTimer * 2.5f);
	float titleY = windowHeight * 0.28f;
	G_Text("GAME OVER", windowWidth * 0.5f + 4.0f, titleY + 4.0f, 120.0f, Col(0, 0, 0, (int)(200 * fade)), AL_CENTER);
	G_Text("GAME OVER", windowWidth * 0.5f, titleY, 120.0f, GO_MixColor(GO_TITLE, GO_TITLE_BRIGHT, pulse, a), AL_CENTER);

	// wave, kills and score
	char text[100];
	snprintf(text, sizeof(text), "Reached wave %d          Kills: %d          Score: %d", s_goWave, s_kills, s_score);
	G_Text(text, windowWidth * 0.5f, titleY + 90.0f, 36.0f, Col(GO_STAT_TEXT, a), AL_CENTER);
	snprintf(text, sizeof(text), "Best this session:  wave %d  /  score %d", s_bestWave, s_bestScore);
	G_Text(text, windowWidth * 0.5f, titleY + 140.0f, 30.0f, Col(GO_BEST_TEXT, a), AL_CENTER);

	// buttons (shown once the overlay is mostly visible)
	if (fade > 0.5f)
	{
		GO_DrawButton(GO_TryX(), GO_TryY(), "TRY AGAIN", s_goTryGlow);
		GO_DrawButton(GO_MenuX(), GO_MenuY(), "MAIN MENU", s_goMenuGlow);
	}

	// same crosshair (same colour) as in the gameplay, drawn on top of the overlay
	DrawMouseCrosshair();
}
/* [GAME OVER] END */

/* =========================================================
   CProcessing Engine Callbacks
   ========================================================= */
void Game_Init(void)
{
	Font_EnsureLoaded();   // the menu frees its font on exit, so reload it for the pause text
	s_paused = false;      // always start a new game unpaused
	GameOver_Reset();      // [GAME OVER] new run = score back to 0, game over screen hidden

	windowWidth = (float)CP_System_GetWindowWidth();
	windowHeight = (float)CP_System_GetWindowHeight();

	player.position = CP_Vector_Set(windowWidth * 0.5f, windowHeight * 0.5f);
	player.aimDirection = CP_Vector_Set(1.0f, 0.0f);   // start aiming right

	// Initialize player health
	player.hp = 100.0f;       // Player starts with 100 HP
	player.maxHp = 100.0f;    // Maximum player health is 100 HP
	player.ghostHp = 100.0f;  // Ghost health starts at maximum HP
	player.noFireFlash = 0.0f;

	player.regenPool = 0.0f;
	player.bloodlust = 0.0f;

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

	//clear pickups and floating texts
	for (int i = 0; i < MAX_PICKUPS; ++i)
	{
		s_pickups[i].active = false;
	}
	for (int i = 0; i < MAX_TEXTS; ++i)
	{
		s_texts[i].active = false;
	}
	s_textNext = 0;

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

	// [GAME OVER] START ---------------------------------------------------------
	if (!s_gameOver)
	{
		GameOver_TrackScore();                        // count kills + score
		if (player.hp <= 0.0f) GameOver_Start();      // player died -> show the game over screen
	}
	if (s_gameOver)
	{
		Render();                                     // draw the frozen game
		GameOver_UpdateAndDraw(deltaTime);            // then the game over screen on top
		return;                                       // skip the normal game update
	}
	// [GAME OVER] END -----------------------------------------------------------

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
		UpdatePickups(deltaTime); // Call the pickup update loop here every frame
		UpdateFloatTexts(deltaTime);
		UpdateWave(deltaTime);   // <-- Manages spawning & wave progression
		ResolveOverlaps();      // last, so knockback and fresh spawns can't leave anything overlapping
	}

	Render();
}

void Game_Exit(void)
{
}