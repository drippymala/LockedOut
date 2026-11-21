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
#include "utils.h"       
#include "game.h"        // Matches the Game_Init / Game_Update / Game_Exit declarations
#include "mainmenu.h"
#include "zombie.h"      // zombie spawning, chasing, drawing (zombie.c)
#include "player.h"      // Sean's player code (movement, aim, sprite, hitboxes)
#include "weapon.h"      // gun stats, firing, bullets and the health laser (weapon.c)
#include "sfx.h"         // sound effects (sfx.c)
#include <math.h>
#include <stdbool.h>

#define GRID_CELL_SIZE       100.0f
#define OVERLAP_PASSES       4      // how many times per frame overlaps are pushed apart
#define MAX_DELTA_TIME       0.025f // 40 fps cap: bullet (900 px/s) + fastest runner (285 px/s) close at most ~30 px/frame, less than the smallest hitbox (34 px)

// (every gun stat — cost, fire delay, speed, damage — is in weapon.h)
#define GHOST_DRAIN_RATE          70.0f   // how fast the pale "ghost" bar catches up (HP per second)
#define WAVE_INTRO_TIME      1.4f   // seconds the "WAVE N" banner shows before zombies start spawning
#define WAVE_BREAK_TIME      1.5f   // seconds of "WAVE N CLEARED!" before the next wave (or the upgrade cards)
#define CHEAT_SKIP_BREAK     0.25f  // Ctrl + N: much shorter break...
#define CHEAT_SKIP_INTRO     0.35f  //           ...and wave banner
#define FINAL_WAVE           20     // the boss wave: kill the boss to win
#define UPGRADE_EVERY_WAVES  3      // after clearing every 3rd wave you pick one gun to upgrade
#define UPGRADE_CLICK_DELAY  0.6f   // seconds before the upgrade cards can be clicked (so a shooting click doesn't pick one)


// ---- pickups dropped by zombies ---- (syahmi
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
// ---- drop chances when a zombie dies ----
// Medkits and bandages are rarer now, and only go back up to their old rate when you're at DROP_CRITICAL_HP or lower.
// Blood drops go up when you're at DROP_LOW_HP or lower.
#define DROP_CRITICAL_HP           15.0f
#define DROP_LOW_HP                30.0f
#define DROP_BLOODLUST             0.03f    //  3%  rare, always
#define DROP_MEDKIT                0.02f    //  2%  normally...
#define DROP_MEDKIT_CRITICAL       0.05f    //  5%  ...at 15 HP or lower
#define DROP_BANDAGE               0.025f   //  2.5% normally...
#define DROP_BANDAGE_CRITICAL      0.06f    //  6%  ...at 15 HP or lower
#define DROP_BLOOD                 0.40f    // 40%  normally...
#define DROP_BLOOD_LOW             0.60f    // 60%  ...at 30 HP or lower
#define DROP_BRUTE_HEAL            0.40f    // brutes: chance of an extra medkit / bandage normally...
#define DROP_BRUTE_HEAL_CRITICAL   1.00f    //         ...always at 15 HP or lower
#define WEAPON_PICKUP_LIFE   20.0f    // weapon pickups (cheat keys only now) stay on the ground longer than heals
#define TWO_PI_F              6.2831853f

typedef enum {
	PICK_BLOOD,     // Small heal, flies toward the player
	PICK_MEDKIT,    // Large instant heal
	PICK_BANDAGE,   // Medium heal
	PICK_BLOODLUST, // Special buff item
	PICK_WEAPON     // Unlocks a new gun type (only spawned by the Ctrl + 1-5 cheat; zombies don't drop guns)
} PickupType;

// Bullet and Player structs are in game.h, Zombie is in zombie.h

// syahmi
typedef struct
{
	bool active;
	PickupType type;
	WeaponType weapon;   // which gun it unlocks (only used by PICK_WEAPON)
	CP_Vector pos;
	float life;      // seconds left before it disappears
	float bob;       // used for the gentle up/down float
} Pickup;

// syahmi
typedef struct
{
	bool active;
	CP_Vector pos;
	float life;
	char text[32];
	int r, g, b;
} FloatText;

// not static: shared with player.c through the "extern" lines in game.h
float           windowWidth = 1600.0f;
float           windowHeight = 900.0f;
Player          player;

static Pickup   s_pickups[MAX_PICKUPS]; // Hold all pickup slots
static FloatText s_texts[MAX_TEXTS];
static int      s_textNext = 0;   // next slot to use for a floating text
static bool     s_paused = false;   // true while the pause menu is showing (syahmi)
static int      s_wave = 0;              // Current wave number
static int      s_toSpawn = 0;           // How many zombies left to spawn this wave
static float    s_spawnTimer = 0.0f;     // Delay between each spawn
static float    s_waveClearTimer = 0.0f; // Delay before the next wave begins
static float    s_waveIntroTimer = 0.0f; // counts down while the wave banner is showing
static bool     s_cheatSkipping = false; // Ctrl + N was used: short break and short banner for this transition
static bool     s_upgradeOpen = false;   // true while the "choose an upgrade" cards are showing
static float    s_upgradeTimer = 0.0f;   // seconds since the upgrade cards opened

static void GameOver_Start(bool won);    // [GAME OVER] section below (won = beat the boss)
static void StartWave(void);             // waves section below
static bool OpenUpgradeScreen(bool startWaveAfter);   // upgrade section below

/* ---------- Logic & Rendering ---------- */

static void UpdatePlayerMovementAndAim(float deltaTime)
{
	MovePlayer(deltaTime);   // WASD (player.c), before aiming/shooting so bullets leave from where you are this frame
	AimPlayerAtMouse();      // player.c, must be worked out BEFORE firing
	HandleGunSwitch();       // player.c

	// Left mouse: fire / charge / throw the current gun, paying HP (weapon.c)
	HandleShooting(deltaTime);

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
		float regenRate = 10.0f; // Amount of HP healed per second
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

// syahmi

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

// Health bar colour based on remaining health percentage:
// - Green (> 50%) for healthy state
// - Yellow (25% - 50%) for warning state
// - Red (< 25%) for critical danger state
// Not static: weapon.c uses it so the health laser is the same colour as the bar.
CP_Color GetHealthBarColor(float hp, float maxHp)
{
	float ratio = CP_Math_ClampFloat(hp / maxHp, 0.0f, 1.0f);
	if (ratio > 0.5f)  return Col(70, 200, 90, 255);
	if (ratio > 0.25f) return Col(230, 190, 50, 255);
	return Col(220, 50, 50, 255);
}

// Stats panel under the health bar: the current gun's cost and damage, updated live.
// (x, y) = top-left corner, w = width. Returns the y of the panel's bottom edge.
static float DrawGunStatsPanel(float x, float y, float w)
{
	WeaponStats st;
	GetWeaponStats(player.weapon, &st);   // weapon.c: the numbers as they are right now

	const float rowH = 28.0f;
	const float panelH = rowH * 4.0f + 14.0f;
	const float valueX = x + 108.0f;      // values start here, labels sit to the left of it
	CP_Color gunColor = GetWeaponColor(player.weapon, 255);
	CP_Color labelColor = Col(160, 160, 160, 255);

	G_Rect(x - 4.0f, y, w + 8.0f, panelH, Col(0, 0, 0, 150));   // dark background
	G_Rect(x - 4.0f, y, 5.0f, panelH, gunColor);                // accent bar in the gun's colour

	float rowY = y + 7.0f + rowH * 0.5f;
	G_Text(st.title, x + 14.0f, rowY, 24.0f, gunColor, AL_LEFT);
	rowY += rowH;

	G_Text("COST", x + 14.0f, rowY, 18.0f, labelColor, AL_LEFT);
	if (st.cantAfford) G_Text("NOT ENOUGH HP", valueX, rowY, 22.0f, Col(255, 70, 70, 255), AL_LEFT);
	else               G_Text(st.cost, valueX, rowY, 22.0f, Col(255, 150, 150, 255), AL_LEFT);
	rowY += rowH;

	G_Text("DAMAGE", x + 14.0f, rowY, 18.0f, labelColor, AL_LEFT);
	G_Text(st.damage, valueX, rowY, 22.0f, Col(255, 230, 120, 255), AL_LEFT);
	rowY += rowH;

	G_Text(st.extraLabel, x + 14.0f, rowY, 18.0f, labelColor, AL_LEFT);
	G_Text(st.extraValue, valueX, rowY, 22.0f, Col(215, 215, 215, 255), AL_LEFT);

	return y + panelH;
}

static void DrawHud(void)
{
	float screenW = (float)CP_System_GetWindowWidth();
	float screenH = (float)CP_System_GetWindowHeight();

	char waveBuf[64];
	int alive = CountAliveZombies();

	// Main Wave text
	snprintf(waveBuf, sizeof(waveBuf), "WAVE %d / %d", s_wave, FINAL_WAVE);
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
		if (s_wave >= FINAL_WAVE) snprintf(waveBuf, sizeof(waveBuf), "%s IS DEAD!", BOSS_NAME);
		else                      snprintf(waveBuf, sizeof(waveBuf), "WAVE %d CLEARED!", s_wave);

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
	const float bw = HEALTH_BAR_WIDTH; // Total width of the health bar
	const float bh = HEALTH_BAR_HEIGHT; // Height of the health bar

	float ratio = CP_Math_ClampFloat(p->hp / p->maxHp, 0.0f, 1.0f);           // Current active health ratio
	float ghost = CP_Math_ClampFloat(p->ghostHp / p->maxHp, 0.0f, 1.0f);     // Trailing ghost health ratio

	CP_Color hpCol = GetHealthBarColor(p->hp, p->maxHp);   // green / yellow / red

	// While a health laser is flying, the health bar is "out there" as the beam, so the bar left behind
	// is only a faint grey shadow of itself (still there, but hard to read)
	if (IsLaserActive())
	{
		G_Rect(bx - 4.0f, by - 4.0f, bw + 8.0f, bh + 8.0f, Col(0, 0, 0, 40));               // faint border
		G_Rect(bx, by, bw * ratio, bh, Col(150, 150, 150, 45));                             // grey, see-through bar
		snprintf(buf, sizeof(buf), "HP %.0f / %.0f", player.hp, player.maxHp);
		G_Text(buf, bx + bw * 0.5f, by + bh * 0.5f, 20.0f, Col(200, 200, 200, 55), AL_CENTER);
	}
	else
	{
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
	}

	// gun stats panel under the health bar, then the status lines under the panel
	float statusY = DrawGunStatsPanel(bx, by + bh + 16.0f, bw) + 22.0f;
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

		// ----- bottom: gun slots (1-5), unowned guns show as ??? -----
	const float slotW = 200.0f, slotH = 56.0f, slotGap = 10.0f;
	float slotX = (screenW - (slotW * W_COUNT + slotGap * (W_COUNT - 1))) * 0.5f;
	float slotY = screenH - 100.0f;

	for (int w = 0; w < W_COUNT; ++w)
	{
		float x = slotX + (slotW + slotGap) * (float)w;
		bool owned = player.owned[w];
		bool selected = (player.weapon == (WeaponType)w);

		// slot background (a bit darker when not selected)
		G_Rect(x, slotY, slotW, slotH, Col(0, 0, 0, selected ? 200 : 140));

		// coloured underline on the gun you're holding
		if (selected)
		{
			G_Rect(x, slotY + slotH - 5.0f, slotW, 5.0f, GetWeaponColor((WeaponType)w, 255));
		}

		// label: "[2] HEALTH LASER" in the gun's colour, or "[2] ???" in grey if not unlocked yet
		if (owned) snprintf(buf, sizeof(buf), "[%d] %s", w + 1, GetWeaponName((WeaponType)w));
		else       snprintf(buf, sizeof(buf), "[%d] ???", w + 1);

		G_Text(buf, x + slotW * 0.5f, slotY + slotH * 0.5f - (owned ? 10.0f : 3.0f), 21.0f,
			owned ? GetWeaponColor((WeaponType)w, 255) : Col(120, 120, 120, 255), AL_CENTER);

		// level under the name: "Lv2", or "Lv4 MAX" in gold once evolved
		if (owned)
		{
			int level = GetWeaponLevel((WeaponType)w);
			if (level >= MAX_WEAPON_LEVEL) snprintf(buf, sizeof(buf), "Lv%d MAX", level);
			else                           snprintf(buf, sizeof(buf), "Lv%d", level);
			G_Text(buf, x + slotW * 0.5f, slotY + slotH * 0.5f + 13.0f, 16.0f,
				(level >= MAX_WEAPON_LEVEL) ? Col(255, 210, 80, 255) : Col(190, 190, 190, 255), AL_CENTER);
		}
	}

	// ----- top centre: boss health bar (wave 20) -----
	const Zombie* boss = FindBoss();
	if (boss)
	{
		const float bossW = 640.0f, bossH = 24.0f;
		float bossX = (screenW - bossW) * 0.5f;
		float bossY = 46.0f;
		float bossRatio = CP_Math_ClampFloat(boss->hp / boss->maxHp, 0.0f, 1.0f);

		G_Text(BOSS_NAME, screenW * 0.5f, bossY - 20.0f, 26.0f, Col(220, 150, 255, 255), AL_CENTER);
		G_Rect(bossX - 4.0f, bossY - 4.0f, bossW + 8.0f, bossH + 8.0f, Col(0, 0, 0, 200));
		G_Rect(bossX, bossY, bossW * bossRatio, bossH, Col(170, 40, 200, 255));
		snprintf(buf, sizeof(buf), "%.0f / %.0f", boss->hp, boss->maxHp);
		G_Text(buf, screenW * 0.5f, bossY + bossH * 0.5f, 18.0f, Col(255, 255, 255, 255), AL_CENTER);
	}

	// Render the active weapon name, its cost description, and the exact HP cost per shot at the bottom center of the screen
	GetWeaponHudText(player.weapon, buf, sizeof(buf));
	G_Text(buf, screenW * 0.5f, screenH - 22.0f, 22.0f, Col(230, 230, 230, 255), AL_CENTER);
}

static void DrawPauseOverlay(void)
{
	// dim the whole screen, then draw the pause text on top
	G_Rect(0.0f, 0.0f, windowWidth, windowHeight, Col(0, 0, 0, 150));
	G_Text("PAUSED", windowWidth * 0.5f, windowHeight * 0.42f, 100.0f, Col(255, 255, 255, 255), AL_CENTER);
	G_Text("ESC - Resume      M - Main Menu", windowWidth * 0.5f, windowHeight * 0.42f + 90.0f, 34.0f, Col(220, 220, 220, 255), AL_CENTER);
}

// (bullets and lasers are stored, moved and drawn in weapon.c)

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

// Returns the new pickup (so the caller can fill in extra info), or NULL if every slot is full.
static Pickup* SpawnPickup(PickupType type, CP_Vector pos)
{
	for (int i = 0; i < MAX_PICKUPS; ++i)
	{
		if (s_pickups[i].active) continue;   // slot in use, try the next one

		s_pickups[i] = (Pickup){
			.active = true, // mark this slot as in use
			.type = type,
			// keep it on screen even if the zombie died at the edge
			.pos = CP_Vector_Set(CP_Math_ClampFloat(pos.x, 30.0f, windowWidth - 30.0f),
								 CP_Math_ClampFloat(pos.y, 30.0f, windowHeight - 30.0f)),
			.life = (type == PICK_BLOOD) ? 10.0f :          // blood drops vanish faster than other pickups
			        (type == PICK_WEAPON) ? WEAPON_PICKUP_LIFE : 14.0f,
			.bob = CP_Random_RangeFloat(0.0f, TWO_PI_F) // up/down float
		};
		return &s_pickups[i]; // stop after spawning one pickup
	}
	return NULL;
}

// What happens when the player touches this pickup.
static void CollectPickup(const Pickup* pickup)
{
	char buf[48];   // scratch space for building the pop-up text
	CP_Color gunColor;

	switch (pickup->type)   // jump to the case that matches this pickup's type
	{
	case PICK_BLOOD:
		Heal(BLOOD_DROP_HEAL);      // small instant heal (+4 HP) 
		break;

	case PICK_MEDKIT:
		Heal(MEDKIT_HEAL);          // big instant heal (+30 HP), Heal() stops it going above max HP
		snprintf(buf, sizeof(buf), "+%.0f HP", MEDKIT_HEAL);   // build the text "+30 HP"
		SpawnText(player.position, buf, 120, 255, 140);        // show it above the player in green
		break;

	case PICK_BANDAGE:
		player.regenPool += BANDAGE_POOL;   // queue 40 HP of healing; the player update heals it slowly (6 HP per second)
		snprintf(buf, sizeof(buf), "REGEN +%.0f", BANDAGE_POOL);   // build the text "REGEN +40"
		SpawnText(player.position, buf, 150, 255, 200);            // show it above the player in light green
		break;

	case PICK_BLOODLUST:
		player.bloodlust = BLOODLUST_TIME;   // start the 12 second timer; kills heal you until it runs out
		SpawnText(player.position, "BLOODLUST!", 230, 80, 255);    // show it above the player in purple
		break;

	case PICK_WEAPON:
		player.owned[pickup->weapon] = true;     // unlock it for good (its number key)...
		player.weapon = pickup->weapon;          // ...and equip it straight away
		snprintf(buf, sizeof(buf), "%s! [%d]", GetWeaponName(pickup->weapon), (int)pickup->weapon + 1);
		gunColor = GetWeaponColor(pickup->weapon, 255);
		SpawnText(player.position, buf, gunColor.r, gunColor.g, gunColor.b);
		break;
	}
}

static void UpdatePickups(float deltaTime)
{
	for (int i = 0; i < MAX_PICKUPS; ++i)          // go through every slot in the pickup array
	{
		if (!s_pickups[i].active) continue;        // empty slot, skip to the next one

		s_pickups[i].bob += deltaTime;             // advance the float animation timer (used by DrawPickups)
		s_pickups[i].life -= deltaTime;            // count down the seconds before it disappears
		if (s_pickups[i].life <= 0.0f)             // nobody collected it in time
		{
			s_pickups[i].active = false;           // free the slot so it can be reused
			continue;                              // nothing more to do for this pickup this frame
		}

		// arrow from the pickup to the player, and how long that arrow is
		CP_Vector toPlayer = CP_Vector_Subtract(player.position, s_pickups[i].pos);
		float distance = CP_Vector_Length(toPlayer);

		// blood drops fly toward the player once they are close enough
		if (s_pickups[i].type == PICK_BLOOD && distance < BLOOD_MAGNET_RANGE)
		{
			s_pickups[i].pos = CP_Vector_Add(s_pickups[i].pos,                     // move the pickup...
				CP_Vector_Scale(GetNormalizedDirection(toPlayer),                  // ...in the direction of the player...
					BLOOD_MAGNET_SPEED * deltaTime));                              // ...by this frame's share of the speed
		}

		// touching the player = collected (two circles overlap when distance < sum of radii)
		if (distance < PLAYER_RADIUS + PickupRadius(s_pickups[i].type))
		{
			CollectPickup(&s_pickups[i]);          // apply the effect (heal, bandage, bloodlust, new gun)
			s_pickups[i].active = false;           // remove the pickup
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
		float y = s_pickups[i].pos.y + sinf(s_pickups[i].bob * 3.0f) *
			3.0f;   // gentle up/down float

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
		case PICK_WEAPON:
			// a short glowing rod in the gun's colour, with its name above
			DrawColoredLine(x - 18.0f, y, x + 18.0f, y, 18.0f, GetWeaponColor(s_pickups[i].weapon, 80));
			DrawColoredLine(x - 18.0f, y, x + 18.0f, y, 8.0f, GetWeaponColor(s_pickups[i].weapon, 255));
			DrawColoredLine(x - 18.0f, y, x + 18.0f, y, 3.0f, Col(255, 230, 230, 255));
			G_Text(GetWeaponName(s_pickups[i].weapon), x, y - 28.0f, 16.0f, GetWeaponColor(s_pickups[i].weapon, 255), AL_CENTER);
			break;
		}
	}
}

/* =========================================================
   Loot: what a zombie leaves behind when it dies
   ========================================================= */
// (zombies no longer drop guns: new guns are picked on the upgrade screen every 3 waves)
static void DropLoot(CP_Vector pos, ZombieType type)
{
	float roll = CP_Random_RangeFloat(0.0f, 1.0f);

	// heals drop more when you're in trouble (see the DROP_ defines at the top)
	bool critical = (player.hp <= DROP_CRITICAL_HP);   // medkits / bandages back to full rate
	bool low      = (player.hp <= DROP_LOW_HP);        // more blood drops

	// brutes may drop a real heal (medkit or bandage), nudged aside so it doesn't sit on top of the normal drop
	float bruteHealChance = critical ? DROP_BRUTE_HEAL_CRITICAL : DROP_BRUTE_HEAL;
	if (type == Z_BRUTE && CP_Random_RangeFloat(0.0f, 1.0f) < bruteHealChance)
	{
		CP_Vector nudged = CP_Vector_Set(pos.x + CP_Random_RangeFloat(-14.0f, 14.0f),
			pos.y + CP_Random_RangeFloat(-14.0f, 14.0f));
		SpawnPickup((CP_Random_RangeFloat(0.0f, 1.0f) < 0.5f) ? PICK_MEDKIT : PICK_BANDAGE, nudged);
	}

	// one roll picks at most one normal drop; each chance stacks on top of the one before it
	float bloodlustUpTo = DROP_BLOODLUST;
	float medkitUpTo    = bloodlustUpTo + (critical ? DROP_MEDKIT_CRITICAL : DROP_MEDKIT);
	float bandageUpTo   = medkitUpTo + (critical ? DROP_BANDAGE_CRITICAL : DROP_BANDAGE);
	float bloodUpTo     = bandageUpTo + (low ? DROP_BLOOD_LOW : DROP_BLOOD);

	if (roll < bloodlustUpTo)    SpawnPickup(PICK_BLOODLUST, pos);
	else if (roll < medkitUpTo)  SpawnPickup(PICK_MEDKIT, pos);
	else if (roll < bandageUpTo) SpawnPickup(PICK_BANDAGE, pos);
	else if (roll < bloodUpTo)   SpawnPickup(PICK_BLOOD, pos);
	// otherwise nothing drops
}

/* =========================================================
   TEMP cheat keys for playtesting (remove before release)
     Ctrl + 1-5  drop that gun's pickup just in front of you
     Ctrl + L    unlock every gun and equip the health laser
     Ctrl + H    refill your HP to max
     Ctrl + =    level up the gun you're holding (Ctrl + - levels it down)
     Ctrl + U    open the "choose an upgrade" screen now
     Ctrl + N    clear this wave (counts as beaten: reward + upgrade cards every 3rd wave, with a very short break)
     Ctrl + B    jump straight to the wave 20 boss (no upgrade cards on the way)
   ========================================================= */
#define CHEAT_DROP_DISTANCE   70.0f   // how far in front of you a cheat gun pickup lands

// Ctrl + N: wipes out this wave (zombies on screen and the ones still to spawn), so the normal
// "wave cleared" step runs: the reward and, every 3rd wave, the upgrade cards.
// The break and the next wave banner are cut very short, so you can tap it to race through waves.
static void CheatClearWave(void)
{
	s_cheatSkipping = true;
	if (s_waveClearTimer > 0.0f)   // already in the break: just cut it short
	{
		if (s_waveClearTimer > CHEAT_SKIP_BREAK) s_waveClearTimer = CHEAT_SKIP_BREAK;
		return;
	}
	for (int i = 0; i < MAX_ZOMBIES; ++i) s_zombies[i].active = false;
	s_toSpawn = 0;
	s_waveIntroTimer = 0.0f;
	SpawnText(player.position, "WAVE SKIPPED", 255, 255, 255);
}

// Ctrl + B: removes every zombie and starts wave "wave" straight away (skips any upgrade cards in between).
static void CheatJumpToWave(int wave)
{
	ResetZombies();
	s_wave = wave - 1;   // StartWave adds 1
	StartWave();
}

static void CheatChangeGunLevel(int change)
{
	WeaponType w = player.weapon;
	int level = player.gunLevel[w] + change;
	if (level < 1) level = 1;
	if (level > MAX_WEAPON_LEVEL) level = MAX_WEAPON_LEVEL;
	player.gunLevel[w] = level;

	char buf[48];
	snprintf(buf, sizeof(buf), "%s Lv%d", GetWeaponName(w), level);
	SpawnText(player.position, buf, 255, 255, 255);
}

static void HandleCheatKeys(void)
{
	bool ctrlHeld = CP_Input_KeyDown(KEY_LEFT_CONTROL) || CP_Input_KeyDown(KEY_RIGHT_CONTROL);
	if (!ctrlHeld) return;

	// Ctrl + number -> that gun's pickup, in the direction you're aiming (walk into it to grab it)
	const CP_KEY gunKeys[W_COUNT] = { KEY_1, KEY_2, KEY_3, KEY_4, KEY_5 };
	for (int w = 0; w < W_COUNT; ++w)
	{
		if (!CP_Input_KeyTriggered(gunKeys[w])) continue;

		CP_Vector spot = CP_Vector_Add(player.position, CP_Vector_Scale(player.aimDirection, CHEAT_DROP_DISTANCE));
		Pickup* gun = SpawnPickup(PICK_WEAPON, spot);
		if (gun) gun->weapon = (WeaponType)w;
	}

	if (CP_Input_KeyTriggered(KEY_L))
	{
		for (int w = 0; w < W_COUNT; ++w) player.owned[w] = true;
		player.weapon = W_HEALTH_LASER;
		SpawnText(player.position, "ALL GUNS", 255, 255, 255);
	}

	if (CP_Input_KeyTriggered(KEY_H))
	{
		player.hp = player.maxHp;
		SpawnText(player.position, "FULL HP", 120, 255, 140);
	}

	if (CP_Input_KeyTriggered(KEY_EQUAL) || CP_Input_KeyTriggered(KEY_KP_ADD))      CheatChangeGunLevel(+1);
	if (CP_Input_KeyTriggered(KEY_MINUS) || CP_Input_KeyTriggered(KEY_KP_SUBTRACT)) CheatChangeGunLevel(-1);

	if (CP_Input_KeyTriggered(KEY_U) && !OpenUpgradeScreen(false))
	{
		SpawnText(player.position, "NOTHING TO UPGRADE", 200, 200, 200);
	}

	if (CP_Input_KeyTriggered(KEY_N) && s_wave < FINAL_WAVE) CheatClearWave();
	if (CP_Input_KeyTriggered(KEY_B))                         CheatJumpToWave(FINAL_WAVE);
}

// Called the moment a zombie dies (index = its slot in s_zombies, killedBy = the gun that did it):
// drops loot and applies bloodlust.
static void OnZombieKilled(int index, WeaponType killedBy)
{
	DropLoot(s_zombies[index].pos, s_zombies[index].type);

	// blood rain: zombies a blood bomb kills may drop a blood heal (20% / 40% / 60% / 100% by bomb level)
	if (killedBy == W_BLOOD_BOMB && CP_Random_RangeFloat(0.0f, 1.0f) < GetBombBloodRainChance())
	{
		SpawnPickup(PICK_BLOOD, CP_Vector_Set(s_zombies[index].pos.x + CP_Random_RangeFloat(-10.0f, 10.0f),
			s_zombies[index].pos.y + CP_Random_RangeFloat(-10.0f, 10.0f)));
	}

	if (player.bloodlust > 0.0f)
	{
		Heal(BLOODLUST_KILL_HEAL);
		SpawnText(s_zombies[index].pos, "+5", 230, 80, 255);
	}
}

//weapon HUD

//WAVES (zombie spawning / drawing / chasing is in zombie.c)

// Begins the next wave with more zombies
static void StartWave(void)
{
	s_wave++;
	// Wave 1: 8 zombies, Wave 2: 12 zombies, Wave 3: 16 zombies...
	// The final wave is just the boss.
	s_toSpawn = (s_wave >= FINAL_WAVE) ? 1 : 4 + (4 * s_wave);
	s_spawnTimer = 0.0f;
	s_waveClearTimer = 0.0f;
	s_waveIntroTimer = s_cheatSkipping ? CHEAT_SKIP_INTRO : WAVE_INTRO_TIME;
	s_cheatSkipping = false;
}

static void UpdateWave(float deltaTime)
{
	// wave banner is showing: the player can already move and aim, but nothing spawns yet
	if (s_waveIntroTimer > 0.0f)
	{
		s_waveIntroTimer -= deltaTime;
		return;
	}

	int alive = CountAliveZombies();

	// 1. If we still have zombies to spawn in this wave
	if (s_toSpawn > 0)
	{
		s_spawnTimer -= deltaTime;
		if (s_spawnTimer <= 0.0f)
		{
			if (s_wave >= FINAL_WAVE) SpawnBoss();      // zombie.c: wave 20 is the boss
			else                      SpawnZombie(s_wave);   // zombie.c, scaled to the current wave
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
			s_waveClearTimer = s_cheatSkipping ? CHEAT_SKIP_BREAK : WAVE_BREAK_TIME; // short break before next wave
		}
		else
		{
			s_waveClearTimer -= deltaTime;
			if (s_waveClearTimer <= 0.0f)
			{
				if (s_wave >= FINAL_WAVE) GameOver_Start(true);   // boss is dead: you win!
				else if (s_wave % UPGRADE_EVERY_WAVES == 0 && OpenUpgradeScreen(true)) { /* the next wave starts once a gun is picked */ }
				else StartWave(); // Start next wave!
			}
		}
	}
}

// Pushes circles apart until nothing overlaps: zombie vs zombie, and zombie vs player.
// Runs once per frame after everything has moved, so overlaps never reach the screen.
static void ResolveOverlaps(void)
{
	// a few passes, because fixing one overlap can create another in a crowd
	for (int pass = 0; pass < OVERLAP_PASSES; ++pass)
	{
		SeparateZombies();          // zombie.c: zombie vs zombie, each moves half
		PushZombiesOutOfPlayer();   // player.c: zombie vs player, done last so no zombie is left inside the player
	}
}

// "WAVE N" banner that fades out during the intro
static void DrawWaveIntro(void)
{
	if (s_waveIntroTimer <= 0.0f) return;

	// full strength at first, fades out over the second half of the intro
	float fade = CP_Math_ClampFloat(s_waveIntroTimer / WAVE_INTRO_TIME * 2.0f, 0.0f, 1.0f);
	int alpha = (int)(fade * 255.0f);

	char buf[64];
	if (s_wave >= FINAL_WAVE) snprintf(buf, sizeof(buf), "FINAL WAVE");
	else                      snprintf(buf, sizeof(buf), "WAVE %d", s_wave);
	G_Text(buf, windowWidth * 0.5f, windowHeight * 0.28f, 110.0f, Col(255, 230, 120, alpha), AL_CENTER);

	if (s_wave >= FINAL_WAVE)
	{
		snprintf(buf, sizeof(buf), "%s is coming. Kill it to escape.", BOSS_NAME);
		G_Text(buf, windowWidth * 0.5f, windowHeight * 0.28f + 80.0f, 30.0f, Col(220, 150, 255, alpha), AL_CENTER);
	}
	else if (s_wave == 1)
	{
		G_Text("Every bullet costs your blood. Kill zombies and grab drops to live.",
			windowWidth * 0.5f, windowHeight * 0.28f + 80.0f, 30.0f, Col(230, 230, 230, alpha), AL_CENTER);
	}
	else
	{
		G_Text("They're getting stronger...",
			windowWidth * 0.5f, windowHeight * 0.28f + 80.0f, 30.0f, Col(230, 230, 230, alpha), AL_CENTER);
	}
}

static void Render(void)
{
	DrawBackgroundGrid();
	DrawPickups();          // under the zombies and the player
	DrawZombies();
	DrawShots();            // pistol bullets + health laser beams (weapon.c)
	DrawPlayerSprite();
	DrawFloatTexts();       // "+30 HP" pop-ups above everything in the world
	DrawHud();
	DrawWaveIntro();   // "WAVE N" banner at the start of each wave

	if (s_paused) DrawPauseOverlay();
	else          DrawMouseCrosshair();   // hide the crosshair while paused
}

static void GoToMenu(void)
{
	CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
}

/* =========================================================
   Upgrade choice: after every 3rd wave you get ONE pick:
     - an UPGRADE card for each gun you own (raises it one level, Lv4 = evolved), or
     - a NEW GUN card for each gun you don't own yet (unlocks it at Lv1).
   Upgrade cards are on the left, new gun cards on the right.
   The game is frozen while the cards are showing.
   ========================================================= */
#define UPGRADE_CARD_W       270.0f
#define UPGRADE_CARD_H       230.0f
#define UPGRADE_CARD_GAP     20.0f
#define UPGRADE_GROUP_GAP    50.0f   // extra space between the upgrade cards and the new gun cards
#define NEW_GUN_RGB          90, 200, 255   // border / labels of the new gun cards

static bool s_upgradeStartsWave = false;   // true = start the next wave once a card is picked (false when opened by the cheat key)
static WeaponType s_upgradeChoices[W_COUNT];   // owned guns first, then the ones you don't own
static int        s_upgradeChoiceCount = 0;
static int        s_upgradeOwnedCount = 0;     // how many of the choices are guns you own (upgrade cards)

// Opens the cards. Returns false (and doesn't open) if there is nothing to pick:
// every gun is owned and maxed.
static bool OpenUpgradeScreen(bool startWaveAfter)
{
	s_upgradeChoiceCount = 0;
	bool anyPick = false;
	for (int w = 0; w < W_COUNT; ++w)   // upgrade cards first...
	{
		if (!player.owned[w]) continue;
		s_upgradeChoices[s_upgradeChoiceCount++] = (WeaponType)w;
		if (GetWeaponLevel((WeaponType)w) < MAX_WEAPON_LEVEL) anyPick = true;
	}
	s_upgradeOwnedCount = s_upgradeChoiceCount;
	for (int w = 0; w < W_COUNT; ++w)   // ...then new gun cards
	{
		if (player.owned[w]) continue;
		s_upgradeChoices[s_upgradeChoiceCount++] = (WeaponType)w;
		anyPick = true;
	}
	if (!anyPick) return false;

	CancelCharging();   // weapon.c: don't leave a blood bomb charging under the cards
	s_upgradeOpen = true;
	s_upgradeTimer = 0.0f;
	s_upgradeStartsWave = startWaveAfter;
	return true;
}

static bool IsNewGunCard(int i) { return i >= s_upgradeOwnedCount; }

// The extra group gap only exists when there are both upgrade cards and new gun cards.
static float UpgradeGroupGap(void)
{
	return (s_upgradeOwnedCount > 0 && s_upgradeOwnedCount < s_upgradeChoiceCount) ? UPGRADE_GROUP_GAP : 0.0f;
}

// Top-left corner of card number i.
static float UpgradeCardX(int i)
{
	float totalW = UPGRADE_CARD_W * (float)s_upgradeChoiceCount + UPGRADE_CARD_GAP * (float)(s_upgradeChoiceCount - 1) + UpgradeGroupGap();
	float x = (windowWidth - totalW) * 0.5f + (UPGRADE_CARD_W + UPGRADE_CARD_GAP) * (float)i;
	if (IsNewGunCard(i)) x += UpgradeGroupGap();
	return x;
}
static float UpgradeCardY(void) { return windowHeight * 0.38f; }

static bool MouseOverUpgradeCard(int i)
{
	float mx = CP_Input_GetMouseX(), my = CP_Input_GetMouseY();
	float x = UpgradeCardX(i), y = UpgradeCardY();
	return mx >= x && mx <= x + UPGRADE_CARD_W && my >= y && my <= y + UPGRADE_CARD_H;
}

static void DrawUpgradeScreen(void)
{
	G_Rect(0.0f, 0.0f, windowWidth, windowHeight, Col(0, 0, 0, 175));
	G_Text("CHOOSE ONE", windowWidth * 0.5f, windowHeight * 0.20f, 80.0f, Col(255, 230, 120, 255), AL_CENTER);
	G_Text("Upgrade a gun you have (Lv4 evolves it), or take a new gun.", windowWidth * 0.5f, windowHeight * 0.20f + 62.0f, 26.0f, Col(220, 220, 220, 255), AL_CENTER);

	// group labels above each set of cards
	float labelY = UpgradeCardY() - 30.0f;
	if (s_upgradeOwnedCount > 0)
	{
		float left = UpgradeCardX(0), right = UpgradeCardX(s_upgradeOwnedCount - 1) + UPGRADE_CARD_W;
		G_Text("UPGRADE", (left + right) * 0.5f, labelY, 26.0f, Col(255, 230, 120, 255), AL_CENTER);
	}
	if (s_upgradeOwnedCount < s_upgradeChoiceCount)
	{
		float left = UpgradeCardX(s_upgradeOwnedCount), right = UpgradeCardX(s_upgradeChoiceCount - 1) + UPGRADE_CARD_W;
		G_Text("NEW GUN", (left + right) * 0.5f, labelY, 26.0f, Col(NEW_GUN_RGB, 255), AL_CENTER);
	}

	char buf[64];
	char lines[UPGRADE_TEXT_LINES][UPGRADE_TEXT_LEN];
	for (int i = 0; i < s_upgradeChoiceCount; ++i)
	{
		WeaponType w = s_upgradeChoices[i];
		bool isNew = IsNewGunCard(i);
		int level = GetWeaponLevel(w);
		bool maxed = !isNew && (level >= MAX_WEAPON_LEVEL);
		bool hover = !maxed && MouseOverUpgradeCard(i);
		float x = UpgradeCardX(i), y = UpgradeCardY();
		float cx = x + UPGRADE_CARD_W * 0.5f;
		CP_Color gunColor = maxed ? Col(110, 110, 110, 255) : GetWeaponColor(w, 255);
		CP_Color borderColor = isNew ? Col(NEW_GUN_RGB, 255) : gunColor;   // new gun cards always have a blue border

		// card: glow + coloured border when hovered (new gun cards keep a dim blue border so they read as a separate group)
		if (hover) G_Rect(x - 10.0f, y - 10.0f, UPGRADE_CARD_W + 20.0f, UPGRADE_CARD_H + 20.0f, isNew ? Col(NEW_GUN_RGB, 60) : GetWeaponColor(w, 60));
		G_Rect(x - 4.0f, y - 4.0f, UPGRADE_CARD_W + 8.0f, UPGRADE_CARD_H + 8.0f,
			hover ? borderColor : (isNew ? Col(40, 90, 120, 255) : Col(70, 70, 70, 255)));
		G_Rect(x, y, UPGRADE_CARD_W, UPGRADE_CARD_H, Col(20, 22, 20, 255));

		G_Text(GetWeaponName(w), cx, y + 32.0f, 28.0f, gunColor, AL_CENTER);

		if (isNew)
		{
			snprintf(buf, sizeof(buf), "NEW  -  Lv1  [%d]", (int)w + 1);
			G_Text(buf, cx, y + 76.0f, 24.0f, Col(NEW_GUN_RGB, 255), AL_CENTER);
			GetNewWeaponText(w, lines);
			for (int l = 0; l < UPGRADE_TEXT_LINES; ++l)
			{
				if (lines[l][0] == '\0') continue;
				G_Text(lines[l], cx, y + 124.0f + 30.0f * (float)l, 19.0f, Col(210, 210, 210, 255), AL_CENTER);
			}
			continue;
		}

		if (maxed)
		{
			G_Text("MAX LEVEL", cx, y + 80.0f, 26.0f, Col(255, 210, 80, 255), AL_CENTER);
			continue;
		}

		bool evolves = (level + 1 == MAX_WEAPON_LEVEL);
		snprintf(buf, sizeof(buf), "Lv%d  ->  Lv%d", level, level + 1);
		G_Text(buf, cx, y + 76.0f, 26.0f, evolves ? Col(255, 210, 80, 255) : Col(255, 255, 255, 255), AL_CENTER);

		GetUpgradeText(w, level + 1, lines);
		for (int l = 0; l < UPGRADE_TEXT_LINES; ++l)
		{
			if (lines[l][0] == '\0') continue;
			bool headline = evolves && l == 0;   // "EVOLVES: ..." stands out
			G_Text(lines[l], cx, y + 124.0f + 30.0f * (float)l, headline ? 21.0f : 19.0f,
				headline ? Col(255, 210, 80, 255) : Col(210, 210, 210, 255), AL_CENTER);
		}
	}

	DrawMouseCrosshair();
}

static void UpdateUpgradeScreen(float deltaTime)
{
	s_upgradeTimer += deltaTime;
	if (s_upgradeTimer < UPGRADE_CLICK_DELAY || !CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT)) return;

	for (int i = 0; i < s_upgradeChoiceCount; ++i)
	{
		WeaponType w = s_upgradeChoices[i];
		bool isNew = IsNewGunCard(i);
		if (!MouseOverUpgradeCard(i)) continue;
		if (!isNew && GetWeaponLevel(w) >= MAX_WEAPON_LEVEL) continue;   // maxed cards can't be picked

		char buf[48];
		if (isNew)
		{
			player.owned[w] = true;     // unlock it for good (its number key)
			player.gunLevel[w] = 1;
			snprintf(buf, sizeof(buf), "%s! [%d]", GetWeaponName(w), (int)w + 1);
		}
		else
		{
			player.gunLevel[w] = GetWeaponLevel(w) + 1;
			snprintf(buf, sizeof(buf), "%s Lv%d!", GetWeaponName(w), player.gunLevel[w]);
		}
		player.weapon = w;   // equip the gun you just picked

		CP_Color c = GetWeaponColor(w, 255);
		SpawnText(player.position, buf, c.r, c.g, c.b);

		s_upgradeOpen = false;
		if (s_upgradeStartsWave) StartWave();
		return;
	}
}

/* =========================================================
   [GAME OVER] START  -- everything for the game over screen
   ========================================================= */

   
#define GO_BUTTON_W        380.0f   // button width in pixels
#define GO_BUTTON_H        70.0f    // button height in pixels
#define GO_CLICK_DELAY     0.5f     // seconds to wait before clicks work (so you can't skip the screen by accident)
#define GO_POINTS_WALKER   10       // points for killing a walker
#define GO_POINTS_RUNNER   15       // points for killing a runner
#define GO_POINTS_BRUTE    50       // points for killing a brute
#define GO_POINTS_BOSS     2000     // points for killing the wave 20 boss
#define GO_WAVE_BONUS      100      // wave cleared bonus = this number x the wave number

// ---- the game over screen ----
static bool  s_gameOver = false;    // true while the game over screen is showing
static bool  s_won = false;         // true = it's the victory screen (the boss is dead), not a death
static float s_goTimer = 0.0f;      // seconds since the player died

// where the two buttons are (x, y = top-left corner). Worked out once, in GameOver_Start
static float s_goTryX, s_goTryY;    // TRY AGAIN button
static float s_goMenuX, s_goMenuY;  // MAIN MENU button

// ---- the score ----
static int s_score = 0;             // points this run
static int s_kills = 0;             // zombies killed this run
static int s_goWave = 0;            // the wave the player died on
static int s_bestScore = 0;         // best score so far (NOT reset when you restart)
static int s_bestWave = 0;          // best wave so far (NOT reset when you restart)

// ---- memory of the last frame (to spot a killed zombie) ----
static int        s_goLastId[MAX_ZOMBIES];     // id of the zombie that was in each slot last frame (0 = empty)
static ZombieType s_goLastType[MAX_ZOMBIES];   // what type that zombie was
static float      s_goLastClearTimer = 0.0f;   // last frame's s_waveClearTimer

// Puts the score and the screen back to the start. Called from Game_Init, so every new run starts clean.
static void GameOver_Reset(void)
{
	s_gameOver = false;
	s_won = false;
	s_score = 0;
	s_kills = 0;
	s_goLastClearTimer = 0.0f;
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		s_goLastId[i] = 0;
	}
}

// Works out the score. Called every frame while the game is running.
static void GameOver_CountScore(void)
{
	// PART 1: a zombie that was in a slot last frame but is gone now = it was killed
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		bool hadZombieLastFrame = (s_goLastId[i] != 0);
		bool sameZombieStillHere = s_zombies[i].active && (s_zombies[i].id == s_goLastId[i]);

		if (hadZombieLastFrame && !sameZombieStillHere)
		{
			s_kills++;
			if (s_goLastType[i] == Z_BOSS)        s_score += GO_POINTS_BOSS;
			else if (s_goLastType[i] == Z_BRUTE)  s_score += GO_POINTS_BRUTE;
			else if (s_goLastType[i] == Z_RUNNER) s_score += GO_POINTS_RUNNER;
			else                                  s_score += GO_POINTS_WALKER;
		}

		// remember what is in this slot, ready for the next frame
		if (s_zombies[i].active)
		{
			s_goLastId[i] = s_zombies[i].id;
			s_goLastType[i] = s_zombies[i].type;
		}
		else
		{
			s_goLastId[i] = 0;
		}
	}

	// PART 2: wave cleared bonus.  s_waveClearTimer jumps from 0 to 3 at the moment a wave is cleared
	bool waveJustCleared = (s_goLastClearTimer <= 0.0f && s_waveClearTimer > 0.0f);
	if (waveJustCleared)
	{
		s_score += GO_WAVE_BONUS * s_wave;
	}
	s_goLastClearTimer = s_waveClearTimer;
}

// Is the mouse inside the button whose top-left corner is (x, y)?
static bool GameOver_MouseIsOver(float x, float y)
{
	float mouseX = CP_Input_GetMouseX();
	float mouseY = CP_Input_GetMouseY();

	bool insideX = (mouseX >= x) && (mouseX <= x + GO_BUTTON_W);   // between the left and right edges
	bool insideY = (mouseY >= y) && (mouseY <= y + GO_BUTTON_H);   // between the top and bottom edges
	return insideX && insideY;
}

// Draws one button. It lights up when the mouse is on it.
static void GameOver_DrawButton(float x, float y, const char* label)
{
	if (GameOver_MouseIsOver(x, y))
	{
		// LIT UP: a soft yellow glow, a yellow border and a bright red body
		G_Rect(x - 10.0f, y - 10.0f, GO_BUTTON_W + 20.0f, GO_BUTTON_H + 20.0f, Col(255, 230, 120, 60));   // glow
		G_Rect(x - 4.0f, y - 4.0f, GO_BUTTON_W + 8.0f, GO_BUTTON_H + 8.0f, Col(255, 230, 120, 255));     // border
		G_Rect(x, y, GO_BUTTON_W, GO_BUTTON_H, Col(170, 30, 30, 255));                                   // body
	}
	else
	{
		// NORMAL: a dark red border and a very dark red body
		G_Rect(x - 4.0f, y - 4.0f, GO_BUTTON_W + 8.0f, GO_BUTTON_H + 8.0f, Col(90, 20, 20, 255));        // border
		G_Rect(x, y, GO_BUTTON_W, GO_BUTTON_H, Col(60, 12, 12, 255));                                    // body
	}

	// the words, in the middle of the button
	G_Text(label, x + GO_BUTTON_W / 2.0f, y + GO_BUTTON_H / 2.0f, 42.0f, Col(255, 255, 255, 255), AL_CENTER);
}

// Called once, the moment the player dies (won = false) or the boss is killed (won = true).
static void GameOver_Start(bool won)
{
	s_gameOver = true;
	s_won = won;
	s_goTimer = 0.0f;
	Sfx_StopBombCharge();    // died mid-charge: don't let the charge-up keep playing over the game over screen
	s_goWave = s_wave;       // remember which wave we died on

	if (s_score > s_bestScore) s_bestScore = s_score;
	if (s_goWave > s_bestWave) s_bestWave = s_goWave;

	// work out where the buttons go (both centred left-to-right, one above the other)
	s_goTryX = (windowWidth - GO_BUTTON_W) / 2.0f;
	s_goTryY = windowHeight * 0.55f;
	s_goMenuX = s_goTryX;
	s_goMenuY = s_goTryY + 90.0f;
}

// Draws the game over screen. It is drawn ON TOP of the frozen game.
static void GameOver_Draw(void)
{
	float centerX = windowWidth / 2.0f;     // the middle of the screen, left to right
	float titleY = windowHeight * 0.28f;    // the title is 28% of the way down

	// 1. a dark see-through layer over the whole game
	G_Rect(0.0f, 0.0f, windowWidth, windowHeight, Col(0, 0, 0, 170));

	// 2. the title: a black shadow first, then the words on top of it (red for a death, gold for a win)
	const char* title = s_won ? "YOU SURVIVED" : "GAME OVER";
	G_Text(title, centerX + 4.0f, titleY + 4.0f, 120.0f, Col(0, 0, 0, 200), AL_CENTER);
	G_Text(title, centerX, titleY, 120.0f, s_won ? Col(255, 210, 80, 255) : Col(220, 30, 30, 255), AL_CENTER);

	// 3. wave, kills and score (the %d is replaced by each number, in order)
	char text[100];
	if (s_won) snprintf(text, sizeof(text), "%s is dead!          Kills: %d          Score: %d", BOSS_NAME, s_kills, s_score);
	else       snprintf(text, sizeof(text), "Reached wave %d          Kills: %d          Score: %d", s_goWave, s_kills, s_score);
	G_Text(text, centerX, titleY + 90.0f, 36.0f, Col(230, 230, 230, 255), AL_CENTER);

	snprintf(text, sizeof(text), "Best this session:  wave %d  /  score %d", s_bestWave, s_bestScore);
	G_Text(text, centerX, titleY + 140.0f, 30.0f, Col(255, 230, 120, 255), AL_CENTER);

	// 4. the two buttons
	GameOver_DrawButton(s_goTryX, s_goTryY, "TRY AGAIN");
	GameOver_DrawButton(s_goMenuX, s_goMenuY, "MAIN MENU");

	// 5. the same crosshair as in the game, drawn last so it is on top
	DrawMouseCrosshair();
}

// Checks if the player clicked a button. Called every frame while the game over screen is showing.
static void GameOver_CheckClicks(float deltaTime)
{
	s_goTimer += deltaTime;    // count the seconds since the player died

	bool waitIsOver = (s_goTimer >= GO_CLICK_DELAY);
	bool clicked = CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT);   // true only on the frame the button is pressed

	if (waitIsOver && clicked)
	{
		if (GameOver_MouseIsOver(s_goTryX, s_goTryY))
		{
			Game_Init();     // TRY AGAIN: Game_Init resets the whole game (and GameOver_Reset hides this screen)
		}
		else if (GameOver_MouseIsOver(s_goMenuX, s_goMenuY))
		{
			GoToMenu();      // MAIN MENU
		}
	}
}

/* [GAME OVER] END */

/* =========================================================
   CProcessing Engine Callbacks
   ========================================================= */
void Game_Init(void)
{
	Font_EnsureLoaded();   // the menu frees its font on exit, so reload it for the pause text
	Sfx_Load();            // sfx.c: loads the sound effects (skips any already loaded, e.g. on TRY AGAIN)
	s_paused = false;      // always start a new game unpaused
	s_upgradeOpen = false; // no upgrade cards left over from the last run
	s_cheatSkipping = false;
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


	//weapon ownership and selection
	for (int i = 0; i < W_COUNT; ++i)
	{
		player.owned[i] = false;
		player.gunLevel[i] = 1;   // every gun starts at Lv1 (upgraded every 3 waves)
	}
	player.owned[W_PISTOL] = true;   // Starter weapon
	player.weapon = W_PISTOL;

	ResetWeapons();   // weapon.c: clear every bullet and laser

	ResetZombies();   // zombie.c: clear every zombie slot and restart the id counter

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
		GameOver_CountScore();                        // keep the score up to date
		if (player.hp <= 0.0f) GameOver_Start(false); // HP is 0 -> the player died -> show the game over screen
	}
	if (s_gameOver)
	{
		Render();                                     // 1. draw the frozen game
		GameOver_Draw();                              // 2. draw the game over screen on top
		GameOver_CheckClicks(deltaTime);              // 3. check if a button was clicked
		return;                                       // 4. skip the normal update (this is what freezes the game)
	}
	// [GAME OVER] END -----------------------------------------------------------

	// "choose an upgrade" cards: the game stays frozen until a gun is picked
	if (s_upgradeOpen)
	{
		Render();
		DrawUpgradeScreen();
		UpdateUpgradeScreen(deltaTime);
		return;
	}

	// ESC toggles pause; while paused, M goes back to the main menu
	if (CP_Input_KeyTriggered(KEY_ESCAPE))
	{
		s_paused = !s_paused;
		Sfx_SetPaused(s_paused);   // sfx.c: freeze sounds (e.g. the bomb charge-up) along with the game
	}
	if (s_paused && CP_Input_KeyTriggered(KEY_M))
	{
		GoToMenu();
		return;
	}

	// only advance the game when not paused (Render below still draws the frozen frame)
	if (!s_paused)
	{
		HandleCheatKeys();       // TEMP playtesting cheats (Ctrl + 1-5 / L / H / = / - / U / N / B)
		UpdatePlayerMovementAndAim(deltaTime);
		UpdateZombies(deltaTime);
		UpdateShots(deltaTime, OnZombieKilled);   // weapon.c: move bullets/lasers, hit zombies, OnZombieKilled handles each kill
		UpdatePickups(deltaTime); // Call the pickup update loop here every frame
		UpdateFloatTexts(deltaTime);
		UpdateWave(deltaTime);   // <-- Manages spawning & wave progression
		ResolveOverlaps();      // last, so knockback and fresh spawns can't leave anything overlapping
	}

	Render();
}

void Game_Exit(void)
{
	Sfx_Free();   // sfx.c: stop and free every sound effect
}