//---------------------------------------------------------
// file:	practicerange.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	TEMPORARY playtest stage: the Practice Range.
//			Every gun, infinite health, three standing dummies
//			(runner / walker / brute sized) and a live DPS readout.
//
//			Open it from the main menu by typing the secret code
//			(PR_CODE below, one key after another, at most
//			PR_CODE_GAP seconds apart).
//
//			Everything lives in this one header (no .c file, nothing
//			to add to the project). It is only included by mainmenu.c.
//
//			TO REMOVE IT:
//			  1. delete this file
//			  2. in mainmenu.c, delete  #include "practicerange.h"
//			  3. in mainmenu.c, delete the  PracticeRange_CheckCode  block
//			     at the top of Main_Menu_Update
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#pragma once
#include <stdio.h>
#include "cprocessing.h"
#include "game.h"       // player, windowWidth / windowHeight, Game_* (not used), health bar colour
#include "player.h"     // movement, aim, gun switching, player sprite, crosshair
#include "zombie.h"     // s_zombies, ResetZombies, DrawZombies (the dummies are zombies that never move)
#include "weapon.h"     // shooting, shots, gun names / colours / levels
#include "utils.h"      // G_Text, G_Rect, Col, DrawColoredLine
#include "sfx.h"        // Sfx_Load / Sfx_Free
#include "mainmenu.h"   // Font_EnsureLoaded, Main_Menu_* (to go back)

/* ---------- settings ---------- */
static const CP_KEY PR_CODE[] = { KEY_8, KEY_P, KEY_2, KEY_K };   // secret code typed on the main menu
#define PR_CODE_LENGTH   (int)(sizeof(PR_CODE) / sizeof(PR_CODE[0]))
#define PR_CODE_GAP      1.5f       // max seconds between two keys of the code
#define PR_DUMMY_HP      1000000.0f // dummies can't die: their HP is topped up every frame
#define PR_DUMMY_COUNT   3
#define PR_WINDOW        3.0f       // DPS = damage dealt in the last 3 seconds
#define PR_BUCKET_TIME   0.1f       // the window is kept as 30 buckets of 0.1 s
#define PR_BUCKETS       30
#define PR_MAX_DT        0.05f

/* ---------- state ---------- */
typedef struct
{
	const char* name;
	ZombieType  type;
	float       radius;
	CP_Vector   pos;
	float       buckets[PR_BUCKETS];   // damage per 0.1 s slice, newest in [0]
	float       total;                 // damage since the last reset
} PrDummy;

static PrDummy s_prDummies[PR_DUMMY_COUNT];
static float   s_prBucketTimer = 0.0f;
static float   s_prFightTime   = 0.0f;   // seconds since the first hit in the current window (caps the DPS divisor)
static float   s_prPeakDps     = 0.0f;
static int     s_prHpStep      = 0;      // which entry of s_prHpSteps the player's HP is locked to
static const float s_prHpSteps[] = { 1.00f, 0.50f, 0.25f, 0.10f, 0.0f };   // 0 = locked at 1 HP
#define PR_HP_STEP_COUNT (int)(sizeof(s_prHpSteps) / sizeof(s_prHpSteps[0]))

static int   s_prCodeIndex = 0;   // how much of the code has been typed so far
static float s_prCodeTimer = 0.0f;

/* ---------- helpers ---------- */
static float PrLockedHp(void)
{
	float share = s_prHpSteps[s_prHpStep];
	return (share <= 0.0f) ? MIN_HP_AFTER_SHOT : player.maxHp * share;
}

static float PrWindowDamage(const PrDummy* d)
{
	float sum = 0.0f;
	for (int b = 0; b < PR_BUCKETS; ++b) sum += d->buckets[b];
	return sum;
}

static float PrDivisor(void)
{
	// right after the first hit the window isn't full yet, so divide by the time actually fought
	float t = (s_prFightTime < PR_WINDOW) ? s_prFightTime : PR_WINDOW;
	return (t < PR_BUCKET_TIME) ? PR_BUCKET_TIME : t;
}

static void PrResetStats(void)
{
	for (int i = 0; i < PR_DUMMY_COUNT; ++i)
	{
		for (int b = 0; b < PR_BUCKETS; ++b) s_prDummies[i].buckets[b] = 0.0f;
		s_prDummies[i].total = 0.0f;
	}
	s_prBucketTimer = 0.0f;
	s_prFightTime = 0.0f;
	s_prPeakDps = 0.0f;
}

// Puts dummy i into its zombie slot (same sizes as the real zombies), full HP, back on its spot.
static void PrPlaceDummy(int i)
{
	Zombie* z = &s_zombies[i];
	z->active       = true;
	z->id           = 900000 + i;   // ids far away from real zombies (piercing shots remember ids)
	z->type         = s_prDummies[i].type;
	z->radius       = s_prDummies[i].radius;
	z->pos          = s_prDummies[i].pos;
	z->maxHp        = PR_DUMMY_HP;
	z->hp           = PR_DUMMY_HP;
	z->speed        = 0.0f;
	z->damage       = 0.0f;
	z->attackTimer  = 0.0f;
	z->abilityTimer = 0.0f;
	z->isDashing    = false;
	// (hitFlash is left alone so the white hit flash still shows; PrFadeHitFlash counts it down)
}

// The real zombie update counts the hit flash down, but the dummies don't use it, so do it here.
static void PrFadeHitFlash(float dt)
{
	for (int i = 0; i < PR_DUMMY_COUNT; ++i)
	{
		Zombie* z = &s_zombies[i];
		z->hitFlash = (z->hitFlash > dt) ? z->hitFlash - dt : 0.0f;
	}
}

// The dummies are solid: push the player out of any dummy they walk into (the dummy doesn't budge).
static void PrPushPlayerOutOfDummies(void)
{
	for (int i = 0; i < PR_DUMMY_COUNT; ++i)
	{
		CP_Vector dummyPos = s_prDummies[i].pos;
		float minDist = PLAYER_RADIUS + s_prDummies[i].radius;
		CP_Vector away = CP_Vector_Subtract(player.position, dummyPos);
		float dist = CP_Vector_Length(away);
		if (dist >= minDist) continue;

		CP_Vector dir = (dist > 0.001f) ? CP_Vector_Scale(away, 1.0f / dist) : CP_Vector_Set(0.0f, 1.0f);
		player.position = CP_Vector_Add(dummyPos, CP_Vector_Scale(dir, minDist));
	}
}

/* ---------- game state ---------- */
static void PracticeRange_Init(void)
{
	Font_EnsureLoaded();
	Sfx_Load();

	windowWidth  = (float)CP_System_GetWindowWidth();
	windowHeight = (float)CP_System_GetWindowHeight();

	player.position     = CP_Vector_Set(windowWidth * 0.5f, windowHeight * 0.75f);
	player.aimDirection = CP_Vector_Set(0.0f, -1.0f);
	player.maxHp        = 100.0f;
	player.hp           = player.maxHp;
	player.ghostHp      = player.maxHp;
	player.noFireFlash  = 0.0f;
	player.regenPool    = 0.0f;
	player.bloodlust    = 0.0f;
	player.combatTimer  = 0.0f;
	for (int w = 0; w < W_COUNT; ++w)
	{
		player.owned[w]    = true;   // every gun
		player.gunLevel[w] = 1;
	}
	player.weapon = W_PISTOL;

	ResetWeapons();
	ResetZombies();

	// runner / walker / brute sized dummies in a row across the top half
	float y = windowHeight * 0.32f;
	s_prDummies[0] = (PrDummy){ .name = "RUNNER", .type = Z_RUNNER, .radius = 12.0f, .pos = CP_Vector_Set(windowWidth * 0.30f, y) };
	s_prDummies[1] = (PrDummy){ .name = "WALKER", .type = Z_WALKER, .radius = 15.0f, .pos = CP_Vector_Set(windowWidth * 0.50f, y) };
	s_prDummies[2] = (PrDummy){ .name = "BRUTE",  .type = Z_BRUTE,  .radius = 26.0f, .pos = CP_Vector_Set(windowWidth * 0.70f, y) };
	for (int i = 0; i < PR_DUMMY_COUNT; ++i)
	{
		s_zombies[i].hitFlash = 0.0f;
		PrPlaceDummy(i);
	}

	s_prHpStep = 0;
	PrResetStats();
}

static void PracticeRange_Update(void)
{
	float dt = CP_System_GetDt();
	if (dt > PR_MAX_DT) dt = PR_MAX_DT;

	if (CP_Input_KeyTriggered(KEY_ESCAPE) || CP_Input_KeyTriggered(KEY_Q))
	{
		CP_Engine_SetNextGameState(Main_Menu_Init, Main_Menu_Update, Main_Menu_Exit);
		return;
	}

	// ---- controls ----
	WeaponType w = player.weapon;
	if (CP_Input_KeyTriggered(KEY_EQUAL) || CP_Input_KeyTriggered(KEY_KP_ADD))
	{
		if (player.gunLevel[w] < GetMaxWeaponLevel(w)) player.gunLevel[w]++;
		PrResetStats();
	}
	if (CP_Input_KeyTriggered(KEY_MINUS) || CP_Input_KeyTriggered(KEY_KP_SUBTRACT))
	{
		if (player.gunLevel[w] > 1) player.gunLevel[w]--;
		PrResetStats();
	}
	if (CP_Input_KeyTriggered(KEY_H))
	{
		s_prHpStep = (s_prHpStep + 1) % PR_HP_STEP_COUNT;
		player.hp = PrLockedHp();   // with Last Stand this is also how you refill
	}
	if (CP_Input_KeyTriggered(KEY_R)) PrResetStats();

	// Last Stand is about how low your HP is, so with it equipped HP really drains (from the H value).
	// Every other gun gets infinite health: HP is held at the H value.
	bool drainHp = (player.weapon == W_LAST_STAND);

	// ---- player and shots (the real game code) ----
	if (!drainHp) player.hp = PrLockedHp();   // set BEFORE shooting so the laser uses the locked value
	MovePlayer(dt);
	PrPushPlayerOutOfDummies();
	AimPlayerAtMouse();
	WeaponType before = player.weapon;
	HandleGunSwitch();
	if (player.weapon != before)
	{
		PrResetStats();
		player.hp = PrLockedHp();   // switching to or from Last Stand starts again from the H value
		drainHp = (player.weapon == W_LAST_STAND);
	}
	HandleShooting(dt);
	UpdateShots(dt, NULL);   // no kill callback: dummies never die and drop nothing
	if (!drainHp) player.hp = PrLockedHp();   // infinite health: whatever the shot cost comes straight back
	player.ghostHp = player.hp;

	PrFadeHitFlash(dt);

	// ---- read the damage off each dummy, then reset it ----
	bool anyDamage = false;
	for (int i = 0; i < PR_DUMMY_COUNT; ++i)
	{
		Zombie* z = &s_zombies[i];
		float dealt = PR_DUMMY_HP - (z->active ? z->hp : 0.0f);
		if (dealt > 0.0f)
		{
			s_prDummies[i].buckets[0] += dealt;
			s_prDummies[i].total += dealt;
			anyDamage = true;
		}
		PrPlaceDummy(i);   // full HP, knocked back into place
	}

	// ---- slide the DPS window ----
	float windowSum = 0.0f;
	for (int i = 0; i < PR_DUMMY_COUNT; ++i) windowSum += PrWindowDamage(&s_prDummies[i]);
	if (windowSum > 0.0f || anyDamage) s_prFightTime += dt;
	else                               s_prFightTime = 0.0f;   // nothing in the window: next hit starts a new fight

	s_prBucketTimer += dt;
	while (s_prBucketTimer >= PR_BUCKET_TIME)
	{
		s_prBucketTimer -= PR_BUCKET_TIME;
		for (int i = 0; i < PR_DUMMY_COUNT; ++i)
		{
			for (int b = PR_BUCKETS - 1; b > 0; --b) s_prDummies[i].buckets[b] = s_prDummies[i].buckets[b - 1];
			s_prDummies[i].buckets[0] = 0.0f;
		}
	}

	float totalDps = windowSum / PrDivisor();
	if (s_prFightTime >= 1.0f && totalDps > s_prPeakDps) s_prPeakDps = totalDps;   // ignore the noisy first second

	// ---- draw ----
	CP_Graphics_ClearBackground(Col(24, 28, 34, 255));
	for (float x = 0.0f; x <= windowWidth; x += 64.0f)  DrawColoredLine(x, 0.0f, x, windowHeight, 1.0f, Col(255, 255, 255, 10));
	for (float y = 0.0f; y <= windowHeight; y += 64.0f) DrawColoredLine(0.0f, y, windowWidth, y, 1.0f, Col(255, 255, 255, 10));

	DrawZombies();
	DrawShots();
	DrawPlayerSprite();

	char buf[96];
	for (int i = 0; i < PR_DUMMY_COUNT; ++i)
	{
		const PrDummy* d = &s_prDummies[i];
		float labelY = d->pos.y + d->radius + 24.0f;
		G_Text(d->name, d->pos.x, labelY, 20.0f, Col(200, 200, 200, 255), AL_CENTER);
		snprintf(buf, sizeof(buf), "%.0f DPS", PrWindowDamage(d) / PrDivisor());
		G_Text(buf, d->pos.x, labelY + 26.0f, 24.0f, Col(255, 230, 120, 255), AL_CENTER);
		snprintf(buf, sizeof(buf), "total %.0f", d->total);
		G_Text(buf, d->pos.x, labelY + 50.0f, 16.0f, Col(150, 150, 150, 255), AL_CENTER);
	}

	// top: title + combined DPS
	G_Text("PRACTICE RANGE", windowWidth * 0.5f, 34.0f, 34.0f, Col(120, 200, 255, 255), AL_CENTER);
	snprintf(buf, sizeof(buf), "DPS (last %.0fs): %.0f      peak: %.0f", PR_WINDOW, totalDps, s_prPeakDps);
	G_Text(buf, windowWidth * 0.5f, 76.0f, 28.0f, Col(255, 255, 255, 255), AL_CENTER);

	// bottom left: gun, level, HP
	w = player.weapon;
	snprintf(buf, sizeof(buf), "[%d] %s  Lv%d / %d", (int)w + 1, GetWeaponName(w), GetWeaponLevel(w), GetMaxWeaponLevel(w));
	G_Text(buf, 24.0f, windowHeight - 92.0f, 26.0f, GetWeaponColor(w, 255), AL_LEFT);
	if (drainHp)
		snprintf(buf, sizeof(buf), "HP %.0f / %.0f  -  drains with Last Stand (H refills to %.0f)", player.hp, player.maxHp, PrLockedHp());
	else if (s_prHpSteps[s_prHpStep] <= 0.0f)
		snprintf(buf, sizeof(buf), "HP locked at 1  (infinite health)");
	else
		snprintf(buf, sizeof(buf), "HP locked at %.0f%%  (infinite health)", s_prHpSteps[s_prHpStep] * 100.0f);
	G_Text(buf, 24.0f, windowHeight - 60.0f, 20.0f, GetHealthBarColor(player.hp, player.maxHp), AL_LEFT);

	G_Text("1-5 gun    +/- level    H cycle HP    R reset stats    ESC / Q menu",
		24.0f, windowHeight - 28.0f, 18.0f, Col(160, 160, 160, 255), AL_LEFT);

	DrawMouseCrosshair();
}

static void PracticeRange_Exit(void)
{
	ResetWeapons();
	ResetZombies();
	Sfx_Free();
}

/* ---------- secret code (called from Main_Menu_Update) ---------- */
// Returns true on the frame the code is finished (the next state is already set).
static bool PracticeRange_CheckCode(float dt)
{
	if (s_prCodeIndex > 0)
	{
		s_prCodeTimer += dt;
		if (s_prCodeTimer > PR_CODE_GAP) s_prCodeIndex = 0;   // too slow: start again
	}

	if (CP_Input_KeyTriggered(PR_CODE[s_prCodeIndex]))
	{
		s_prCodeIndex++;
		s_prCodeTimer = 0.0f;
		if (s_prCodeIndex >= PR_CODE_LENGTH)
		{
			s_prCodeIndex = 0;
			CP_Engine_SetNextGameState(PracticeRange_Init, PracticeRange_Update, PracticeRange_Exit);
			return true;
		}
	}
	else if (s_prCodeIndex > 0 && CP_Input_KeyTriggered(PR_CODE[0]))
	{
		s_prCodeIndex = 1;   // a wrong key that is the first key of the code restarts it from there
		s_prCodeTimer = 0.0f;
	}
	return false;
}
