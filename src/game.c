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
#define SPAWN_DELAY_START    1.0f   // seconds between spawns on wave 1
#define SPAWN_DELAY_PER_WAVE 0.035f // ...minus this much every wave (wave 10 = 0.69s, wave 19 = 0.37s)
#define SPAWN_DELAY_MIN      0.35f  // never spawns faster than this
#define SPAWN_GROUP2_WAVE    8      // from this wave, zombies spawn 2 at a time
#define SPAWN_GROUP3_WAVE    14     // from this wave, 3 at a time
#define SPAWN_ALIVE_BASE     8      // at most this + the wave number zombies alive at once (wave 5: 13, wave 15: 23)
#define TYRANT_MEDKIT_TIME   10.0f  // during the final boss fight, a medkit drops at a random spot this often (seconds)
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
static int      s_waveTotal = 0;         // How many normal zombies this wave started with
static bool     s_bossPending = false;   // a mini boss (wave 5 / 10 / 15) is waiting to spawn once 25% of the wave is out
static float    s_tyrantMedkitTimer = 0.0f;   // counts down to the next medkit drop in the Tyrant fight
static float    s_spawnTimer = 0.0f;     // Delay between each spawn
static float    s_waveClearTimer = 0.0f; // Delay before the next wave begins
static float    s_waveIntroTimer = 0.0f; // counts down while the wave banner is showing
static bool     s_cheatSkipping = false; // Ctrl + N was used: short break and short banner for this transition
static bool     s_upgradeOpen = false;   // true while the "choose an upgrade" cards are showing
static float    s_upgradeTimer = 0.0f;   // seconds since the upgrade cards opened

static void GameOver_Start(bool won);    // [GAME OVER] section below (won = beat the boss)
static void StartWave(void);             // waves section below
static bool OpenUpgradeScreen(bool startWaveAfter);   // upgrade section below
static void OpenBossReward(void);                     // boss reward section below
static void DrawPauseOverlay(void);      // forward declaration so Render() can call it

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
		if (s_wave >= FINAL_WAVE) snprintf(waveBuf, sizeof(waveBuf), "BOSS IS DEAD!");
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

	// Q / E hints on either side of the slots: previous / next gun
	G_Text("< Q", slotX - 38.0f, slotY + slotH * 0.5f, 22.0f, Col(200, 200, 200, 255), AL_CENTER);
	G_Text("E >", slotX + slotW * (float)W_COUNT + slotGap * (float)(W_COUNT - 1) + 38.0f, slotY + slotH * 0.5f, 22.0f, Col(200, 200, 200, 255), AL_CENTER);

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

		// level under the name: "Lv2", or "Lv4 MAX" (Lv5 for the pistol) in gold once evolved
		if (owned)
		{
			int level = GetWeaponLevel((WeaponType)w);
			if (level >= GetMaxWeaponLevel((WeaponType)w)) snprintf(buf, sizeof(buf), "Lv%d MAX", level);
			else                           snprintf(buf, sizeof(buf), "Lv%d", level);
			G_Text(buf, x + slotW * 0.5f, slotY + slotH * 0.5f + 13.0f, 16.0f,
				(level >= GetMaxWeaponLevel((WeaponType)w)) ? Col(255, 210, 80, 255) : Col(190, 190, 190, 255), AL_CENTER);
		}
	}

	// ----- top centre: boss health bar (wave 5, 10, 15, 20) -----
	const Zombie* boss = FindBoss();
	if (boss)
	{
		const float bossW = 640.0f, bossH = 24.0f;
		float bossX = (screenW - bossW) * 0.5f;
		float bossY = 46.0f;
		float bossRatio = CP_Math_ClampFloat(boss->hp / boss->maxHp, 0.0f, 1.0f);

		const char* bName = (boss->type == Z_BOSS_CHARGER) ? "CHARGER" :
			(boss->type == Z_BOSS_SHOOT) ? "SPITTER" :
			(boss->type == Z_BOSS_PULL) ? "PULL" : "TYRANT";

		G_Text(bName, screenW * 0.5f, bossY - 20.0f, 26.0f, Col(220, 150, 255, 255), AL_CENTER);
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
static void Heal(float amount)
{
	player.hp = CP_Math_ClampFloat(player.hp + amount, 0.0f, player.maxHp);
}

static float PickupRadius(PickupType type)
{
	if (type == PICK_BLOOD) return 9.0f;
	return 18.0f;
}

static Pickup* SpawnPickup(PickupType type, CP_Vector pos)
{
	for (int i = 0; i < MAX_PICKUPS; ++i)
	{
		if (s_pickups[i].active) continue;

		s_pickups[i] = (Pickup){
			.active = true,
			.type = type,
			.pos = CP_Vector_Set(CP_Math_ClampFloat(pos.x, 30.0f, windowWidth - 30.0f),
								 CP_Math_ClampFloat(pos.y, 30.0f, windowHeight - 30.0f)),
			.life = (type == PICK_BLOOD) ? 10.0f :
					(type == PICK_WEAPON) ? WEAPON_PICKUP_LIFE : 14.0f,
			.bob = CP_Random_RangeFloat(0.0f, TWO_PI_F)
		};
		return &s_pickups[i];
	}
	return NULL;
}

static void CollectPickup(const Pickup* pickup)
{
	char buf[48];
	CP_Color gunColor;

	switch (pickup->type)
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
		player.regenPool += BANDAGE_POOL;
		snprintf(buf, sizeof(buf), "REGEN +%.0f", BANDAGE_POOL);
		SpawnText(player.position, buf, 150, 255, 200);
		break;

	case PICK_BLOODLUST:
		player.bloodlust = BLOODLUST_TIME;
		SpawnText(player.position, "BLOODLUST!", 230, 80, 255);
		break;

	case PICK_WEAPON:
		player.owned[pickup->weapon] = true;
		player.weapon = pickup->weapon;
		snprintf(buf, sizeof(buf), "%s! [%d]", GetWeaponName(pickup->weapon), (int)pickup->weapon + 1);
		gunColor = GetWeaponColor(pickup->weapon, 255);
		SpawnText(player.position, buf, gunColor.r, gunColor.g, gunColor.b);
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
		if (s_pickups[i].life <= 0.0f)
		{
			s_pickups[i].active = false;
			continue;
		}

		CP_Vector toPlayer = CP_Vector_Subtract(player.position, s_pickups[i].pos);
		float distance = CP_Vector_Length(toPlayer);

		if (s_pickups[i].type == PICK_BLOOD && distance < BLOOD_MAGNET_RANGE)
		{
			s_pickups[i].pos = CP_Vector_Add(s_pickups[i].pos,
				CP_Vector_Scale(GetNormalizedDirection(toPlayer),
					BLOOD_MAGNET_SPEED * deltaTime));
		}

		if (distance < PLAYER_RADIUS + PickupRadius(s_pickups[i].type))
		{
			CollectPickup(&s_pickups[i]);
			s_pickups[i].active = false;
		}
	}
}

static void DrawPickups(void)
{
	for (int i = 0; i < MAX_PICKUPS; ++i)
	{
		if (!s_pickups[i].active) continue;

		if (s_pickups[i].life < 3.0f && ((int)(s_pickups[i].life * 10.0f) % 2) == 0) continue;

		float x = s_pickups[i].pos.x;
		float y = s_pickups[i].pos.y + sinf(s_pickups[i].bob * 3.0f) * 3.0f;

		switch (s_pickups[i].type)
		{
		case PICK_BLOOD:
			G_Circle(x, y, 8.0f, Col(200, 20, 40, 255));
			G_Circle(x - 2.0f, y - 2.0f, 3.0f, Col(255, 140, 150, 255));
			break;
		case PICK_MEDKIT:
			G_Rect(x - 15.0f, y - 15.0f, 30.0f, 30.0f, Col(240, 240, 240, 255));
			G_Rect(x - 9.0f, y - 3.0f, 18.0f, 6.0f, Col(220, 30, 40, 255));
			G_Rect(x - 3.0f, y - 9.0f, 6.0f, 18.0f, Col(220, 30, 40, 255));
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
static void DropLoot(CP_Vector pos, ZombieType type)
{
	float roll = CP_Random_RangeFloat(0.0f, 1.0f);

	bool critical = (player.hp <= DROP_CRITICAL_HP);
	bool low = (player.hp <= DROP_LOW_HP);

	float bruteHealChance = critical ? DROP_BRUTE_HEAL_CRITICAL : DROP_BRUTE_HEAL;
	if (type == Z_BRUTE && CP_Random_RangeFloat(0.0f, 1.0f) < bruteHealChance)
	{
		CP_Vector nudged = CP_Vector_Set(pos.x + CP_Random_RangeFloat(-14.0f, 14.0f),
			pos.y + CP_Random_RangeFloat(-14.0f, 14.0f));
		SpawnPickup((CP_Random_RangeFloat(0.0f, 1.0f) < 0.5f) ? PICK_MEDKIT : PICK_BANDAGE, nudged);
	}

	float bloodlustUpTo = DROP_BLOODLUST;
	float medkitUpTo = bloodlustUpTo + (critical ? DROP_MEDKIT_CRITICAL : DROP_MEDKIT);
	float bandageUpTo = medkitUpTo + (critical ? DROP_BANDAGE_CRITICAL : DROP_BANDAGE);
	float bloodUpTo = bandageUpTo + (low ? DROP_BLOOD_LOW : DROP_BLOOD);

	if (roll < bloodlustUpTo)    SpawnPickup(PICK_BLOODLUST, pos);
	else if (roll < medkitUpTo)  SpawnPickup(PICK_MEDKIT, pos);
	else if (roll < bandageUpTo) SpawnPickup(PICK_BANDAGE, pos);
	else if (roll < bloodUpTo)   SpawnPickup(PICK_BLOOD, pos);
}

/* =========================================================
   TEMP cheat keys for playtesting
   ========================================================= */
#define CHEAT_DROP_DISTANCE   70.0f

static void CheatClearWave(void)
{
	s_cheatSkipping = true;
	if (s_waveClearTimer > 0.0f)
	{
		if (s_waveClearTimer > CHEAT_SKIP_BREAK) s_waveClearTimer = CHEAT_SKIP_BREAK;
		return;
	}

	// skipping a mini boss you haven't beaten yet (still waiting to spawn, or alive) still gives its reward
	const Zombie* boss = FindBoss();
	bool skippedBoss = s_bossPending || (boss && boss->type != Z_BOSS_FINAL);

	for (int i = 0; i < MAX_ZOMBIES; ++i) s_zombies[i].active = false;
	s_toSpawn = 0;
	s_bossPending = false;
	s_waveIntroTimer = 0.0f;
	SpawnText(player.position, "WAVE SKIPPED", 255, 255, 255);

	if (skippedBoss) OpenBossReward();
}

static void CheatJumpToWave(int wave)
{
	ResetZombies();
	s_wave = wave - 1;
	StartWave();
}

static void CheatChangeGunLevel(int change)
{
	WeaponType w = player.weapon;
	int level = player.gunLevel[w] + change;
	if (level < 1) level = 1;
	if (level > GetMaxWeaponLevel(w)) level = GetMaxWeaponLevel(w);
	player.gunLevel[w] = level;

	char buf[48];
	snprintf(buf, sizeof(buf), "%s Lv%d", GetWeaponName(w), level);
	SpawnText(player.position, buf, 255, 255, 255);
}

static void HandleCheatKeys(void)
{
	bool ctrlHeld = CP_Input_KeyDown(KEY_LEFT_CONTROL) || CP_Input_KeyDown(KEY_RIGHT_CONTROL);
	if (!ctrlHeld) return;

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

static void OnZombieKilled(int index, WeaponType killedBy)
{
	DropLoot(s_zombies[index].pos, s_zombies[index].type);

	// a milestone boss (wave 5, 10, 15) died: pause and pick a reward. The final boss ends the game instead.
	ZombieType type = s_zombies[index].type;
	if (type >= Z_BOSS_CHARGER && type != Z_BOSS_FINAL) OpenBossReward();

	// the Tyrant died: its summoned minions die with it, so the win comes straight away
	if (type == Z_BOSS_FINAL)
	{
		for (int i = 0; i < MAX_ZOMBIES; ++i)
			if (i != index) s_zombies[i].active = false;
	}

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

// Begins the next wave with more zombies
static void StartWave(void)
{
	s_wave++;
	s_spawnTimer = 0.0f;
	s_waveClearTimer = 0.0f;
	s_tyrantMedkitTimer = TYRANT_MEDKIT_TIME;   // the first medkit of the Tyrant fight comes 10s in
	Music_Play(s_wave == FINAL_WAVE ? MUSIC_BOSS : MUSIC_GAME);   // boss music for the Tyrant, game music otherwise
	s_waveIntroTimer = s_cheatSkipping ? CHEAT_SKIP_INTRO : WAVE_INTRO_TIME;
	s_cheatSkipping = false;

	// Final Boss Wave (Solo Boss, no normal zombies)
	if (s_wave == 20)
	{
		ResetZombies();      // Clear any leftover zombies
		SpawnBoss(20);       // Spawn the Final Boss (Tyrant)
		s_toSpawn = 0;       // Stop all regular zombie spawns
		s_bossPending = false;
	}
	else
	{
		// Normal zombie count for non-final waves
		s_toSpawn = 4 + (4 * s_wave);
		s_waveTotal = s_toSpawn;

		// Milestone Boss (Wave 5, 10, 15): joins once 25% of the wave has spawned (see UpdateWave)
		s_bossPending = (s_wave % 5 == 0);
	}
}
static void UpdateWave(float deltaTime)
{
	if (s_waveIntroTimer > 0.0f)
	{
		s_waveIntroTimer -= deltaTime;
		return;
	}

	// final boss fight: a medkit drops at a random spot every TYRANT_MEDKIT_TIME seconds while the Tyrant is alive
	const Zombie* tyrant = FindBoss();
	if (s_wave == FINAL_WAVE && tyrant && tyrant->type == Z_BOSS_FINAL)
	{
		s_tyrantMedkitTimer -= deltaTime;
		if (s_tyrantMedkitTimer <= 0.0f)
		{
			const float margin = 80.0f;   // keep it away from the edges
			SpawnPickup(PICK_MEDKIT, CP_Vector_Set(CP_Random_RangeFloat(margin, windowWidth - margin),
			                                       CP_Random_RangeFloat(margin, windowHeight - margin)));
			s_tyrantMedkitTimer = TYRANT_MEDKIT_TIME;
		}
	}

	int alive = CountAliveZombies();

	if (s_toSpawn > 0)
	{
		s_spawnTimer -= deltaTime;
		// never more than SPAWN_ALIVE_BASE + wave zombies alive at once: spawning waits until you kill some
		int aliveCap = SPAWN_ALIVE_BASE + s_wave;
		if (s_spawnTimer <= 0.0f && alive < aliveCap)
		{
			// later waves spawn in small groups, so zombies come from several sides at once
			int group = (s_wave >= SPAWN_GROUP3_WAVE) ? 3 : (s_wave >= SPAWN_GROUP2_WAVE) ? 2 : 1;
			for (int g = 0; g < group && s_toSpawn > 0 && alive + g < aliveCap; ++g)
			{
				SpawnZombie(s_wave);
				s_toSpawn--;
			}

			// mini boss arrives once a quarter of the wave's zombies have spawned
			if (s_bossPending && s_toSpawn <= s_waveTotal - s_waveTotal / 4)
			{
				SpawnBoss(s_wave);
				s_bossPending = false;
			}

			// ...and the wait between spawns shrinks each wave
			float delay = SPAWN_DELAY_START - SPAWN_DELAY_PER_WAVE * (float)(s_wave - 1);
			s_spawnTimer = (delay < SPAWN_DELAY_MIN) ? SPAWN_DELAY_MIN : delay;
		}
	}
	else if (alive == 0)
	{
		if (s_waveClearTimer <= 0.0f)
		{
			player.maxHp += 5.0f;
			player.hp = CP_Math_ClampFloat(player.hp + 25.0f, 0.0f, player.maxHp);
			s_waveClearTimer = s_cheatSkipping ? CHEAT_SKIP_BREAK : WAVE_BREAK_TIME;
		}
		else
		{
			s_waveClearTimer -= deltaTime;
			if (s_waveClearTimer <= 0.0f)
			{
				if (s_wave >= FINAL_WAVE) GameOver_Start(true);
				else if (s_wave % UPGRADE_EVERY_WAVES == 0 && OpenUpgradeScreen(true)) {}
				else StartWave();
			}
		}
	}
}

static void ResolveOverlaps(void)
{
	for (int pass = 0; pass < OVERLAP_PASSES; ++pass)
	{
		SeparateZombies();
		PushZombiesOutOfPlayer();
	}
}

static void DrawWaveIntro(void)
{
	if (s_waveIntroTimer <= 0.0f) return;

	float fade = CP_Math_ClampFloat(s_waveIntroTimer / WAVE_INTRO_TIME * 2.0f, 0.0f, 1.0f);
	int alpha = (int)(fade * 255.0f);

	char buf[64];
	if (s_wave >= FINAL_WAVE) snprintf(buf, sizeof(buf), "FINAL WAVE");
	else                      snprintf(buf, sizeof(buf), "WAVE %d", s_wave);
	G_Text(buf, windowWidth * 0.5f, windowHeight * 0.28f, 110.0f, Col(255, 230, 120, alpha), AL_CENTER);

	if (s_wave % 5 == 0)
	{
		G_Text("A powerful threat approaches!", windowWidth * 0.5f, windowHeight * 0.28f + 80.0f, 30.0f, Col(220, 150, 255, alpha), AL_CENTER);
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
	DrawPickups();
	DrawZombies();
	DrawShots();
	DrawPlayerSprite();
	DrawFloatTexts();
	DrawHud();
	DrawWaveIntro();

	if (s_paused) DrawPauseOverlay();
	else          DrawMouseCrosshair();
}

static void GoToMenu(void)
{
	CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
}

/* =========================================================
   Upgrade Screen
   ========================================================= */
#define UPGRADE_CARD_W       270.0f
#define UPGRADE_CARD_H       230.0f
#define UPGRADE_CARD_GAP     20.0f
#define UPGRADE_GROUP_GAP    50.0f
#define NEW_GUN_RGB          90, 200, 255

static bool s_upgradeStartsWave = false;
static WeaponType s_upgradeChoices[W_COUNT];
static int        s_upgradeChoiceCount = 0;
static int        s_upgradeOwnedCount = 0;

static bool OpenUpgradeScreen(bool startWaveAfter)
{
	s_upgradeChoiceCount = 0;
	bool anyPick = false;
	for (int w = 0; w < W_COUNT; ++w)
	{
		if (!player.owned[w]) continue;
		s_upgradeChoices[s_upgradeChoiceCount++] = (WeaponType)w;
		if (GetWeaponLevel((WeaponType)w) < GetMaxWeaponLevel((WeaponType)w)) anyPick = true;
	}
	s_upgradeOwnedCount = s_upgradeChoiceCount;
	for (int w = 0; w < W_COUNT; ++w)
	{
		if (player.owned[w]) continue;
		s_upgradeChoices[s_upgradeChoiceCount++] = (WeaponType)w;
		anyPick = true;
	}
	if (!anyPick) return false;

	CancelCharging();
	s_upgradeOpen = true;
	s_upgradeTimer = 0.0f;
	s_upgradeStartsWave = startWaveAfter;
	return true;
}

static bool IsNewGunCard(int i) { return i >= s_upgradeOwnedCount; }

static float UpgradeGroupGap(void)
{
	return (s_upgradeOwnedCount > 0 && s_upgradeOwnedCount < s_upgradeChoiceCount) ? UPGRADE_GROUP_GAP : 0.0f;
}

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
	G_Text("Upgrade a gun you have (its last level evolves it), or take a new gun.", windowWidth * 0.5f, windowHeight * 0.20f + 62.0f, 26.0f, Col(220, 220, 220, 255), AL_CENTER);

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
		bool maxed = !isNew && (level >= GetMaxWeaponLevel(w));
		bool hover = !maxed && MouseOverUpgradeCard(i);
		float x = UpgradeCardX(i), y = UpgradeCardY();
		float cx = x + UPGRADE_CARD_W * 0.5f;
		CP_Color gunColor = maxed ? Col(110, 110, 110, 255) : GetWeaponColor(w, 255);
		CP_Color borderColor = isNew ? Col(NEW_GUN_RGB, 255) : gunColor;

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

		bool evolves = (level + 1 == GetMaxWeaponLevel(w));
		snprintf(buf, sizeof(buf), "Lv%d  ->  Lv%d", level, level + 1);
		G_Text(buf, cx, y + 76.0f, 26.0f, evolves ? Col(255, 210, 80, 255) : Col(255, 255, 255, 255), AL_CENTER);

		GetUpgradeText(w, level + 1, lines);
		for (int l = 0; l < UPGRADE_TEXT_LINES; ++l)
		{
			if (lines[l][0] == '\0') continue;
			bool headline = evolves && l == 0;
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
		if (!isNew && GetWeaponLevel(w) >= GetMaxWeaponLevel(w)) continue;

		char buf[48];
		if (isNew)
		{
			player.owned[w] = true;
			player.gunLevel[w] = 1;
			snprintf(buf, sizeof(buf), "%s! [%d]", GetWeaponName(w), (int)w + 1);
		}
		else
		{
			player.gunLevel[w] = GetWeaponLevel(w) + 1;
			snprintf(buf, sizeof(buf), "%s Lv%d!", GetWeaponName(w), player.gunLevel[w]);
		}
		player.weapon = w;

		CP_Color c = GetWeaponColor(w, 255);
		SpawnText(player.position, buf, c.r, c.g, c.b);

		s_upgradeOpen = false;
		BlockFireUntilRelease();   // the click that picked this card shouldn't also fire the gun
		if (s_upgradeStartsWave) StartWave();
		return;
	}
}

/* =========================================================
   Boss Reward  (after the wave 5 / 10 / 15 boss dies)
   Two cards: upgrade the pistol, or heal to full + a permanent max HP boost.
   ========================================================= */
#define BOSS_REWARD_HP_BOOST   0.15f   // max HP goes up by 15% of what it is now
#define BOSS_REWARD_CARDS      2
#define HEAL_CARD_RGB          120, 255, 140

static bool  s_bossRewardOpen  = false;
static float s_bossRewardTimer = 0.0f;   // seconds since the cards opened (clicks are ignored for UPGRADE_CLICK_DELAY)

static void OpenBossReward(void)
{
	CancelCharging();
	s_bossRewardOpen  = true;
	s_bossRewardTimer = 0.0f;
}

static bool PistolMaxed(void) { return GetWeaponLevel(W_PISTOL) >= GetMaxWeaponLevel(W_PISTOL); }

static float BossRewardCardX(int i)
{
	float totalW = UPGRADE_CARD_W * BOSS_REWARD_CARDS + UPGRADE_CARD_GAP * (BOSS_REWARD_CARDS - 1);
	return (windowWidth - totalW) * 0.5f + (UPGRADE_CARD_W + UPGRADE_CARD_GAP) * (float)i;
}

static bool MouseOverBossRewardCard(int i)
{
	float mx = CP_Input_GetMouseX(), my = CP_Input_GetMouseY();
	float x = BossRewardCardX(i), y = UpgradeCardY();
	return mx >= x && mx <= x + UPGRADE_CARD_W && my >= y && my <= y + UPGRADE_CARD_H;
}

static void DrawBossReward(void)
{
	G_Rect(0.0f, 0.0f, windowWidth, windowHeight, Col(0, 0, 0, 175));
	G_Text("BOSS DEFEATED!", windowWidth * 0.5f, windowHeight * 0.20f, 80.0f, Col(220, 150, 255, 255), AL_CENTER);
	G_Text("Choose your reward.", windowWidth * 0.5f, windowHeight * 0.20f + 62.0f, 26.0f, Col(220, 220, 220, 255), AL_CENTER);

	char buf[64];
	char lines[UPGRADE_TEXT_LINES][UPGRADE_TEXT_LEN];
	float y = UpgradeCardY();

	for (int i = 0; i < BOSS_REWARD_CARDS; ++i)
	{
		bool isPistol = (i == 0);
		bool disabled = isPistol && PistolMaxed();
		bool hover = !disabled && MouseOverBossRewardCard(i);
		float x = BossRewardCardX(i);
		float cx = x + UPGRADE_CARD_W * 0.5f;
		CP_Color color = disabled ? Col(110, 110, 110, 255) : (isPistol ? GetWeaponColor(W_PISTOL, 255) : Col(HEAL_CARD_RGB, 255));

		if (hover) G_Rect(x - 10.0f, y - 10.0f, UPGRADE_CARD_W + 20.0f, UPGRADE_CARD_H + 20.0f, Col(color.r, color.g, color.b, 60));
		G_Rect(x - 4.0f, y - 4.0f, UPGRADE_CARD_W + 8.0f, UPGRADE_CARD_H + 8.0f, hover ? color : Col(70, 70, 70, 255));
		G_Rect(x, y, UPGRADE_CARD_W, UPGRADE_CARD_H, Col(20, 22, 20, 255));

		if (isPistol)
		{
			G_Text(GetWeaponName(W_PISTOL), cx, y + 32.0f, 28.0f, color, AL_CENTER);
			if (disabled)
			{
				G_Text("MAX LEVEL", cx, y + 80.0f, 26.0f, Col(255, 210, 80, 255), AL_CENTER);
				continue;
			}
			int level = GetWeaponLevel(W_PISTOL);
			bool evolves = (level + 1 == GetMaxWeaponLevel(W_PISTOL));
			snprintf(buf, sizeof(buf), "Lv%d  ->  Lv%d", level, level + 1);
			G_Text(buf, cx, y + 76.0f, 26.0f, evolves ? Col(255, 210, 80, 255) : Col(255, 255, 255, 255), AL_CENTER);
			GetUpgradeText(W_PISTOL, level + 1, lines);
			for (int l = 0; l < UPGRADE_TEXT_LINES; ++l)
			{
				if (lines[l][0] == '\0') continue;
				G_Text(lines[l], cx, y + 124.0f + 30.0f * (float)l, 19.0f, Col(210, 210, 210, 255), AL_CENTER);
			}
		}
		else
		{
			float boost = player.maxHp * BOSS_REWARD_HP_BOOST;
			G_Text("VITALITY", cx, y + 32.0f, 28.0f, color, AL_CENTER);
			snprintf(buf, sizeof(buf), "Max HP %.0f  ->  %.0f", player.maxHp, player.maxHp + boost);
			G_Text(buf, cx, y + 76.0f, 24.0f, Col(255, 255, 255, 255), AL_CENTER);
			G_Text("Heal to full HP", cx, y + 124.0f, 19.0f, Col(210, 210, 210, 255), AL_CENTER);
			snprintf(buf, sizeof(buf), "+%.0f%% max HP, forever", BOSS_REWARD_HP_BOOST * 100.0f);
			G_Text(buf, cx, y + 154.0f, 19.0f, Col(210, 210, 210, 255), AL_CENTER);
		}
	}

	DrawMouseCrosshair();
}

static void UpdateBossReward(float deltaTime)
{
	s_bossRewardTimer += deltaTime;
	if (s_bossRewardTimer < UPGRADE_CLICK_DELAY || !CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT)) return;

	char buf[48];

	if (MouseOverBossRewardCard(0) && !PistolMaxed())
	{
		player.gunLevel[W_PISTOL] = GetWeaponLevel(W_PISTOL) + 1;
		player.weapon = W_PISTOL;
		snprintf(buf, sizeof(buf), "%s Lv%d!", GetWeaponName(W_PISTOL), player.gunLevel[W_PISTOL]);
		CP_Color c = GetWeaponColor(W_PISTOL, 255);
		SpawnText(player.position, buf, c.r, c.g, c.b);
	}
	else if (MouseOverBossRewardCard(1))
	{
		player.maxHp += player.maxHp * BOSS_REWARD_HP_BOOST;
		player.hp = player.maxHp;
		snprintf(buf, sizeof(buf), "MAX HP %.0f!", player.maxHp);
		SpawnText(player.position, buf, HEAL_CARD_RGB);
	}
	else return;   // clicked outside the cards

	s_bossRewardOpen = false;
	BlockFireUntilRelease();   // the click that picked this card shouldn't also fire the gun
}

/* =========================================================
   [GAME OVER]
   ========================================================= */
#define GO_BUTTON_W        380.0f
#define GO_BUTTON_H        70.0f
#define GO_CLICK_DELAY     0.5f
#define GO_POINTS_WALKER   10
#define GO_POINTS_RUNNER   15
#define GO_POINTS_BRUTE    50
#define GO_POINTS_BOSS     2000
#define GO_WAVE_BONUS      100

static bool  s_gameOver = false;
static bool  s_won = false;
static float s_goTimer = 0.0f;

static float s_goTryX, s_goTryY;
static float s_goMenuX, s_goMenuY;

static int s_score = 0;
static int s_kills = 0;
static int s_goWave = 0;
static int s_bestScore = 0;
static int s_bestWave = 0;

static int        s_goLastId[MAX_ZOMBIES];
static ZombieType s_goLastType[MAX_ZOMBIES];
static float      s_goLastClearTimer = 0.0f;

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

static void GameOver_CountScore(void)
{
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		bool hadZombieLastFrame = (s_goLastId[i] != 0);
		bool sameZombieStillHere = s_zombies[i].active && (s_zombies[i].id == s_goLastId[i]);

		if (hadZombieLastFrame && !sameZombieStillHere)
		{
			s_kills++;
			// Fixed: check >= Z_BOSS_CHARGER to handle any milestone boss without needing Z_BOSS
			if (s_goLastType[i] >= Z_BOSS_CHARGER) s_score += GO_POINTS_BOSS;
			else if (s_goLastType[i] == Z_BRUTE)    s_score += GO_POINTS_BRUTE;
			else if (s_goLastType[i] == Z_RUNNER)   s_score += GO_POINTS_RUNNER;
			else                                    s_score += GO_POINTS_WALKER;
		}

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

	bool waveJustCleared = (s_goLastClearTimer <= 0.0f && s_waveClearTimer > 0.0f);
	if (waveJustCleared)
	{
		s_score += GO_WAVE_BONUS * s_wave;
	}
	s_goLastClearTimer = s_waveClearTimer;
}

static bool GameOver_MouseIsOver(float x, float y)
{
	float mouseX = CP_Input_GetMouseX();
	float mouseY = CP_Input_GetMouseY();

	bool insideX = (mouseX >= x) && (mouseX <= x + GO_BUTTON_W);
	bool insideY = (mouseY >= y) && (mouseY <= y + GO_BUTTON_H);
	return insideX && insideY;
}

static void GameOver_DrawButton(float x, float y, const char* label)
{
	if (GameOver_MouseIsOver(x, y))
	{
		G_Rect(x - 10.0f, y - 10.0f, GO_BUTTON_W + 20.0f, GO_BUTTON_H + 20.0f, Col(255, 230, 120, 60));
		G_Rect(x - 4.0f, y - 4.0f, GO_BUTTON_W + 8.0f, GO_BUTTON_H + 8.0f, Col(255, 230, 120, 255));
		G_Rect(x, y, GO_BUTTON_W, GO_BUTTON_H, Col(170, 30, 30, 255));
	}
	else
	{
		G_Rect(x - 4.0f, y - 4.0f, GO_BUTTON_W + 8.0f, GO_BUTTON_H + 8.0f, Col(90, 20, 20, 255));
		G_Rect(x, y, GO_BUTTON_W, GO_BUTTON_H, Col(60, 12, 12, 255));
	}

	G_Text(label, x + GO_BUTTON_W / 2.0f, y + GO_BUTTON_H / 2.0f, 42.0f, Col(255, 255, 255, 255), AL_CENTER);
}

static void GameOver_Start(bool won)
{
	s_gameOver = true;
	s_won = won;
	s_goTimer = 0.0f;
	Music_Stop();
	Sfx_StopBombCharge();
	s_goWave = s_wave;

	if (s_score > s_bestScore) s_bestScore = s_score;
	if (s_goWave > s_bestWave) s_bestWave = s_goWave;

	s_goTryX = (windowWidth - GO_BUTTON_W) / 2.0f;
	s_goTryY = windowHeight * 0.55f;
	s_goMenuX = s_goTryX;
	s_goMenuY = s_goTryY + 90.0f;
}

static void GameOver_Draw(void)
{
	float centerX = windowWidth / 2.0f;
	float titleY = windowHeight * 0.28f;

	G_Rect(0.0f, 0.0f, windowWidth, windowHeight, Col(0, 0, 0, 170));

	const char* title = s_won ? "YOU SURVIVED" : "GAME OVER";
	G_Text(title, centerX + 4.0f, titleY + 4.0f, 120.0f, Col(0, 0, 0, 200), AL_CENTER);
	G_Text(title, centerX, titleY, 120.0f, s_won ? Col(255, 210, 80, 255) : Col(220, 30, 30, 255), AL_CENTER);

	char text[100];
	if (s_won) snprintf(text, sizeof(text), "The Horde is Defeated!   Kills: %d   Score: %d", s_kills, s_score);
	else       snprintf(text, sizeof(text), "Reached wave %d          Kills: %d          Score: %d", s_goWave, s_kills, s_score);
	G_Text(text, centerX, titleY + 90.0f, 36.0f, Col(230, 230, 230, 255), AL_CENTER);

	snprintf(text, sizeof(text), "Best this session:  wave %d  /  score %d", s_bestWave, s_bestScore);
	G_Text(text, centerX, titleY + 140.0f, 30.0f, Col(255, 230, 120, 255), AL_CENTER);

	GameOver_DrawButton(s_goTryX, s_goTryY, "TRY AGAIN");
	GameOver_DrawButton(s_goMenuX, s_goMenuY, "MAIN MENU");

	DrawMouseCrosshair();
}

static void GameOver_CheckClicks(float deltaTime)
{
	s_goTimer += deltaTime;

	bool waitIsOver = (s_goTimer >= GO_CLICK_DELAY);
	bool clicked = CP_Input_MouseTriggered(MOUSE_BUTTON_LEFT);

	if (waitIsOver && clicked)
	{
		if (GameOver_MouseIsOver(s_goTryX, s_goTryY))
		{
			Game_Init();
		}
		else if (GameOver_MouseIsOver(s_goMenuX, s_goMenuY))
		{
			GoToMenu();
		}
	}
}

/* =========================================================
   CProcessing Engine Callbacks
   ========================================================= */
void Game_Init(void)
{
	Font_EnsureLoaded();
	Sfx_Load();
	s_paused = false;
	s_upgradeOpen = false;
	s_bossRewardOpen = false;
	s_cheatSkipping = false;
	GameOver_Reset();

	windowWidth = (float)CP_System_GetWindowWidth();
	windowHeight = (float)CP_System_GetWindowHeight();

	player.position = CP_Vector_Set(windowWidth * 0.5f, windowHeight * 0.5f);
	player.aimDirection = CP_Vector_Set(1.0f, 0.0f);

	player.hp = 100.0f;
	player.maxHp = 100.0f;
	player.ghostHp = 100.0f;
	player.noFireFlash = 0.0f;

	player.regenPool = 0.0f;
	player.bloodlust = 0.0f;

	for (int i = 0; i < W_COUNT; ++i)
	{
		player.owned[i] = false;
		player.gunLevel[i] = 1;
	}
	player.owned[W_PISTOL] = true;
	player.weapon = W_PISTOL;

	ResetWeapons();
	ResetZombies();

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
	s_bossPending = false;
	StartWave();
}

void Game_Update(void)
{
	float deltaTime = CP_System_GetDt();
	if (deltaTime > MAX_DELTA_TIME)
	{
		deltaTime = MAX_DELTA_TIME;
	}

	if (!s_gameOver)
	{
		GameOver_CountScore();
		if (player.hp <= 0.0f) GameOver_Start(false);
	}
	if (s_gameOver)
	{
		Render();
		GameOver_Draw();
		GameOver_CheckClicks(deltaTime);
		return;
	}

	if (s_upgradeOpen)
	{
		Render();
		DrawUpgradeScreen();
		UpdateUpgradeScreen(deltaTime);
		return;
	}

	if (s_bossRewardOpen)
	{
		Render();
		DrawBossReward();
		UpdateBossReward(deltaTime);
		return;
	}

	if (CP_Input_KeyTriggered(KEY_ESCAPE))
	{
		s_paused = !s_paused;
		Sfx_SetPaused(s_paused);
	}
	if (s_paused && CP_Input_KeyTriggered(KEY_M))
	{
		GoToMenu();
		return;
	}

	if (!s_paused)
	{
		HandleCheatKeys();
		UpdatePlayerMovementAndAim(deltaTime);
		UpdateZombies(deltaTime);
		UpdateShots(deltaTime, OnZombieKilled);
		UpdatePickups(deltaTime);
		UpdateFloatTexts(deltaTime);
		UpdateWave(deltaTime);
		ResolveOverlaps();
	}

	Render();
}

void Game_Exit(void)
{
	Sfx_Free();
}