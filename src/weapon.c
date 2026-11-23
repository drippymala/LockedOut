//---------------------------------------------------------
// file:	weapon.c
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	How the guns work: paying HP to shoot, fire delay,
//			spawning, moving, hit-testing and drawing every shot
//			(pistol / last stand bullets, health laser beams,
//			the bloodrang and blood bombs). All the numbers live
//			in weapon.h. Each gun's stats depend on its level
//			(player.gunLevel); the last level (Lv5 pistol, Lv4 others) adds an evolved ability.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#include <stdio.h>
#include <math.h>
#include "cprocessing.h"
#include "utils.h"
#include "game.h"      // player, windowWidth / windowHeight, PLAYER_RADIUS, health bar helpers
#include "zombie.h"    // s_zombies
#include "weapon.h"
#include "sfx.h"       // gun / hit / explosion sounds

#define BULLET_OFFSCREEN_MARGIN   30.0f   // a bullet this far past the edge is removed
#define LASER_OFFSCREEN_MARGIN    60.0f   // the laser keeps going a bit past the edge so it still hits zombies spawning there
#define BOMB_OFFSCREEN_MARGIN     40.0f
#define PI_F                      3.14159265f

// per-level stat tables, filled from the _LV lists in weapon.h
static const float s_pistolDamage[MAX_WEAPON_LEVEL]     = PISTOL_DAMAGE_LV;
static const float s_pistolDelay[MAX_WEAPON_LEVEL]      = PISTOL_FIRE_DELAY_LV;
static const float s_pistolCost[MAX_WEAPON_LEVEL]       = PISTOL_COST_LV;
static const float s_laserMultiplier[MAX_WEAPON_LEVEL]  = LASER_DAMAGE_MULTIPLIER_LV;
static const float s_rangDamage[MAX_WEAPON_LEVEL]       = RANG_DAMAGE_LV;
static const float s_rangRefund[MAX_WEAPON_LEVEL]       = RANG_REFUND_LV;
static const float s_rangRadius[MAX_WEAPON_LEVEL]       = RANG_RADIUS_LV;
static const float s_bombDamagePerHp[MAX_WEAPON_LEVEL]  = BOMB_DAMAGE_PER_HP_LV;
static const float s_bombBloodRain[MAX_WEAPON_LEVEL]    = BOMB_BLOOD_RAIN_LV;
static const float s_lastStandMin[MAX_WEAPON_LEVEL]     = LAST_STAND_MIN_DAMAGE_LV;
static const float s_lastStandMax[MAX_WEAPON_LEVEL]     = LAST_STAND_MAX_DAMAGE_LV;
static const float s_lastStandCurve[MAX_WEAPON_LEVEL]   = LAST_STAND_CURVE_LV;

static Bullet    s_bullets[MAX_BULLETS];
static Laser     s_lasers[MAX_LASERS];
static Bloodrang s_rangs[MAX_RANGS];      // one boomerang at a time (two at Lv4)
static int       s_rangStacks = 0;        // damage boost stacks earned by catching the bloodrang
static BloodBomb s_bombs[MAX_BOMBS];
static Blast     s_blasts[MAX_BLASTS];
static BloodPool s_pools[MAX_POOLS];      // Lv4 blood pools left by big blasts
static float     s_fireTimer = 0.0f;      // seconds until the gun can fire again
static bool      s_waitForRelease = false; // true after a UI click: no shooting until the mouse button is let go

// blood bomb being charged on the gun tip (not thrown yet)
static bool  s_charging = false;
static float s_chargeStored = 0.0f;       // HP poured in so far
static float s_chargeCap = 0.0f;          // the most it can hold (40% of HP when charging started)

/* =========================================================
   Levels
   ========================================================= */

int GetMaxWeaponLevel(WeaponType w)
{
	return (w == W_PISTOL) ? PISTOL_MAX_LEVEL : GUN_MAX_LEVEL;
}

int GetWeaponLevel(WeaponType w)
{
	if (w < 0 || w >= W_COUNT) return 1;
	int level = player.gunLevel[w];
	if (level < 1) level = 1;
	if (level > GetMaxWeaponLevel(w)) level = GetMaxWeaponLevel(w);
	return level;
}

static int  LevelIndex(WeaponType w) { return GetWeaponLevel(w) - 1; }   // 0-based, for the stat tables
static bool IsEvolved(WeaponType w)  { return GetWeaponLevel(w) >= GetMaxWeaponLevel(w); }

/* =========================================================
   Small helpers
   ========================================================= */

// 0 at full HP, 1 at no HP. Used by Last Stand.
static float MissingHpFraction(void)
{
	return 1.0f - CP_Math_ClampFloat(player.hp / player.maxHp, 0.0f, 1.0f);
}

static float LastStandMinDamage(void) { return s_lastStandMin[LevelIndex(W_LAST_STAND)]; }
static float LastStandMaxDamage(void) { return s_lastStandMax[LevelIndex(W_LAST_STAND)]; }

// Exponential curve: 0 at full HP -> 1 near death, staying low for most of the bar and shooting up at the end.
// (e^(curve * t) - 1) / (e^curve - 1), where t = how much HP is missing (0..1).
// Curve 3.5 (Lv1-3): half HP = 0.15, 25% HP = 0.40, 10% HP = 0.72.  Curve 4.5 (Lv4): half HP = 0.10, 25% HP = 0.32, 10% HP = 0.64.
static float LastStandPower(void)
{
	float curve = s_lastStandCurve[LevelIndex(W_LAST_STAND)];
	return (expf(curve * MissingHpFraction()) - 1.0f) / (expf(curve) - 1.0f);
}

static float LastStandDamage(void)
{
	return LastStandMinDamage() + (LastStandMaxDamage() - LastStandMinDamage()) * LastStandPower();
}

// small at full HP -> big near death (on the same curve, so the bullets visibly swell in the red)
static float LastStandRadius(void)
{
	return LAST_STAND_MIN_RADIUS + (LAST_STAND_MAX_RADIUS - LAST_STAND_MIN_RADIUS) * LastStandPower();
}

// pale pink at full HP -> deep blood red near death
static CP_Color LastStandColor(int alpha)
{
	float t = MissingHpFraction();
	return Col((int)(255.0f - 55.0f * t), (int)(190.0f - 190.0f * t), (int)(170.0f - 150.0f * t), alpha);
}

static float PistolDamage(void)     { return s_pistolDamage[LevelIndex(W_PISTOL)]; }
static float LaserMultiplier(void)  { return s_laserMultiplier[LevelIndex(W_HEALTH_LASER)]; }
static float BombDamagePerHp(void)  { return s_bombDamagePerHp[LevelIndex(W_BLOOD_BOMB)]; }
static float BombMaxChargePercent(void) { return IsEvolved(W_BLOOD_BOMB) ? BOMB_EVO_MAX_CHARGE_PERCENT : BOMB_MAX_CHARGE_PERCENT; }
static float RangRefund(void)       { return s_rangRefund[LevelIndex(W_BLOODRANG)]; }
static float RangRadius(void)       { return s_rangRadius[LevelIndex(W_BLOODRANG)]; }
static int   RangMaxStacks(void)   { return IsEvolved(W_BLOODRANG) ? RANG_EVO_MAX_STACKS : RANG_MAX_STACKS; }
static int   RangThrowCount(void)   { return IsEvolved(W_BLOODRANG) ? RANG_EVO_COUNT : 1; }

// stacks earned, but never more than this level allows (matters if the level is lowered by a cheat)
static int RangStacks(void)
{
	return (s_rangStacks > RangMaxStacks()) ? RangMaxStacks() : s_rangStacks;
}

static float RangDamageWithBoost(void)
{
	return s_rangDamage[LevelIndex(W_BLOODRANG)] * (1.0f + RANG_BOOST_PER_STACK * (float)RangStacks());
}

static float BombBlastRadius(float stored) { return BOMB_BASE_RADIUS + stored * BOMB_RADIUS_PER_HP; }
static float BombOrbRadius(float stored) { return BOMB_ORB_BASE_RADIUS + stored * BOMB_ORB_RADIUS_PER_HP; }

static CP_Vector GunTip(void)
{
	float barrelReach = PLAYER_RADIUS + GUN_BARREL_LENGTH;
	return CP_Vector_Add(player.position, CP_Vector_Scale(player.aimDirection, barrelReach));
}

static CP_Vector MousePos(void)
{
	return CP_Vector_Set(CP_Input_GetMouseX(), CP_Input_GetMouseY());
}

static void HealPlayer(float amount)
{
	player.hp = CP_Math_ClampFloat(player.hp + amount, 0.0f, player.maxHp);
}

// direction turned by "angle" radians
static CP_Vector RotateDirection(CP_Vector dir, float angle)
{
	float c = cosf(angle), s = sinf(angle);
	return CP_Vector_Set(dir.x * c - dir.y * s, dir.x * s + dir.y * c);
}

static bool AnyRangActive(void)
{
	for (int i = 0; i < MAX_RANGS; ++i)
	{
		if (s_rangs[i].active) return true;
	}
	return false;
}

/* =========================================================
   Gun info
   ========================================================= */

// You can fire if the shot is free, or you have any HP to spare above the minimum.
// (If you can't pay the full cost, the shot still fires and drops you to MIN_HP_AFTER_SHOT.)
static bool CanAffordShot(float cost)
{
	return cost <= 0.0f || player.hp - MIN_HP_AFTER_SHOT > LAST_SHOT_MIN_SPARE;
}

// How much HP the gun would cost to fire right now.
float GetShotCost(WeaponType w)
{
	switch (w)
	{
	case W_PISTOL:       return s_pistolCost[LevelIndex(W_PISTOL)];
	case W_HEALTH_LASER: return LASER_COST_FLAT + player.hp * LASER_COST_PERCENT;   // flat part + a share of what you have now
	case W_BLOODRANG:    return RANG_COST * (float)RangThrowCount();
	case W_BLOOD_BOMB:   return 0.0f;                             // paid while charging instead
	case W_LAST_STAND:
		// at 1 HP the shot is free (so it never locks up near death)
		if (player.hp - MIN_HP_AFTER_SHOT <= LAST_SHOT_MIN_SPARE) return 0.0f;
		return LAST_STAND_COST;
	default:             return 1.0f;
	}
}

float GetFireDelay(WeaponType w)
{
	switch (w)
	{
	case W_PISTOL:       return s_pistolDelay[LevelIndex(W_PISTOL)];
	case W_HEALTH_LASER: return LASER_FIRE_DELAY;
	case W_BLOODRANG:    return RANG_FIRE_DELAY;
	case W_BLOOD_BOMB:   return BOMB_FIRE_DELAY;
	case W_LAST_STAND:   return LAST_STAND_FIRE_DELAY;
	default:             return 0.25f;
	}
}

const char* GetWeaponName(WeaponType w)
{
	bool evolved = IsEvolved(w);
	switch (w)
	{
	case W_PISTOL:       return evolved ? "TWIN FANG"      : "PISTOL";
	case W_HEALTH_LASER: return evolved ? "LIFELINE LANCE" : "HEALTH LASER";
	case W_BLOODRANG:    return evolved ? "TWIN RANG"      : "BLOODRANG";
	case W_BLOOD_BOMB:   return evolved ? "BLOOD CLUSTER"  : "BLOOD BOMB";
	case W_LAST_STAND:   return evolved ? "DEATH'S DOOR"   : "LAST STAND";
	default:             return "???";
	}
}

CP_Color GetWeaponColor(WeaponType w, int alpha)
{
	switch (w)
	{
	case W_HEALTH_LASER: return Col(LASER_RGB, alpha);
	case W_BLOODRANG:    return Col(BLOODRANG_RGB, alpha);
	case W_BLOOD_BOMB:   return Col(BLOOD_BOMB_RGB, alpha);
	case W_LAST_STAND:   return LastStandColor(alpha);
	case W_PISTOL:
	default:             return Col(PISTOL_RGB, alpha);
	}
}

// One line for the bottom of the HUD: what the gun does and what it costs right now.
void GetWeaponHudText(WeaponType w, char* buf, size_t bufSize)
{
	int level = GetWeaponLevel(w);
	const char* name = GetWeaponName(w);

	switch (w)
	{
	case W_HEALTH_LASER:
		snprintf(buf, bufSize, "[2] %s Lv%d: damage = HP x %.2g, costs %.0f + %.0f%% HP%s  -  costs %.1f HP, deals %.0f now",
			name, level, LaserMultiplier(), LASER_COST_FLAT, LASER_COST_PERCENT * 100.0f,
			IsEvolved(w) ? ", heals per hit" : "", GetShotCost(w), player.hp * LaserMultiplier());
		break;
	case W_BLOODRANG:
		if (AnyRangActive())
		{
			float stored = 0.0f;
			int hits = 0;
			for (int i = 0; i < MAX_RANGS; ++i)
			{
				if (!s_rangs[i].active) continue;
				stored += s_rangs[i].stored;
				hits += s_rangs[i].hits;
			}
			snprintf(buf, bufSize, "[3] %s Lv%d: catch it!  gives back %.0f HP, hit %d zombie(s)  -  boost +%.0f%%",
				name, level, stored * RangRefund(), hits, RANG_BOOST_PER_STACK * 100.0f * (float)RangStacks());
		}
		else
			snprintf(buf, bufSize, "[3] %s Lv%d: costs %.0f HP, catch it to get %.0f%% back  -  boost +%.0f%% (%d/%d), deals %.0f",
				name, level, GetShotCost(w), RangRefund() * 100.0f, RANG_BOOST_PER_STACK * 100.0f * (float)RangStacks(), RangStacks(), RangMaxStacks(), RangDamageWithBoost());
		break;
	case W_BLOOD_BOMB:
		if (s_charging)
			snprintf(buf, bufSize, "[4] %s Lv%d: charging %.0f / %.0f HP  -  blast deals %.0f",
				name, level, s_chargeStored, s_chargeCap, s_chargeStored * BombDamagePerHp());
		else
			snprintf(buf, bufSize, "[4] %s Lv%d: hold to pour HP in (up to %.0f%% of max HP), release to throw  -  %.1f damage per HP",
				name, level, BombMaxChargePercent() * 100.0f, BombDamagePerHp());
		break;
	case W_LAST_STAND:
		if (GetShotCost(w) <= 0.0f)
			snprintf(buf, bufSize, "[5] %s Lv%d: FREE SHOTS at death's door  -  deals %.0f now", name, level, LastStandDamage());
		else
			snprintf(buf, bufSize, "[5] %s Lv%d: stronger the lower your HP (up to %.0f)  -  costs %.0f HP, deals %.0f now",
				name, level, LastStandMaxDamage(), LAST_STAND_COST, LastStandDamage());
		break;
	case W_PISTOL:
	default:
		snprintf(buf, bufSize, "[1] %s Lv%d: %.0f damage, flat cost per shot  -  costs %.1f HP now",
			name, level, PistolDamage(), GetShotCost(w));
		break;
	}
}

// Live numbers for the stats panel next to the health bar (game.c draws it).
void GetWeaponStats(WeaponType w, WeaponStats* out)
{
	float cost = GetShotCost(w);

	snprintf(out->title, sizeof(out->title), "%s  Lv%d%s", GetWeaponName(w), GetWeaponLevel(w), IsEvolved(w) ? " MAX" : "");
	out->cantAfford = !CanAffordShot(cost);   // same check HandleShooting uses before it fires

	switch (w)
	{
	case W_HEALTH_LASER:
		snprintf(out->cost, sizeof(out->cost), "%.1f HP  (%.0f + %.0f%% of HP)", cost, LASER_COST_FLAT, LASER_COST_PERCENT * 100.0f);
		snprintf(out->damage, sizeof(out->damage), "%.0f  (HP x %.2g)", player.hp * LaserMultiplier(), LaserMultiplier());
		if (IsEvolved(w))
		{
			snprintf(out->extraLabel, sizeof(out->extraLabel), "HEAL");
			snprintf(out->extraValue, sizeof(out->extraValue), "+%.0f HP per zombie hit", LASER_EVO_HEAL_PER_HIT);
		}
		else
		{
			snprintf(out->extraLabel, sizeof(out->extraLabel), "BEAM");
			snprintf(out->extraValue, sizeof(out->extraValue), "pierces every zombie");
		}
		break;

	case W_BLOODRANG:
		if (AnyRangActive())
		{
			float stored = 0.0f;
			for (int i = 0; i < MAX_RANGS; ++i)
			{
				if (s_rangs[i].active) stored += s_rangs[i].stored;
			}
			snprintf(out->cost, sizeof(out->cost), "catch it!  +%.0f HP back", stored * RangRefund());
			out->cantAfford = false;   // it's out already: you can't throw another one anyway
		}
		else
		{
			snprintf(out->cost, sizeof(out->cost), "%.0f HP  (%.0f%% back)", cost, RangRefund() * 100.0f);
		}
		snprintf(out->damage, sizeof(out->damage), "%.0f", RangDamageWithBoost());
		snprintf(out->extraLabel, sizeof(out->extraLabel), "BOOST");
		snprintf(out->extraValue, sizeof(out->extraValue), "+%.0f%%  (%d/%d)",
			RANG_BOOST_PER_STACK * 100.0f * (float)RangStacks(), RangStacks(), RangMaxStacks());
		break;

	case W_BLOOD_BOMB:
		if (s_charging)
		{
			snprintf(out->cost, sizeof(out->cost), "%.0f / %.0f in  (paid %.0f HP)", s_chargeStored, s_chargeCap, s_chargeStored * BOMB_HP_COST_SHARE);
			snprintf(out->damage, sizeof(out->damage), "%.0f", s_chargeStored * BombDamagePerHp());
			snprintf(out->extraLabel, sizeof(out->extraLabel), "RADIUS");
			snprintf(out->extraValue, sizeof(out->extraValue), "%.0f", BombBlastRadius(s_chargeStored));
			out->cantAfford = false;
		}
		else
		{
			float cap = player.maxHp * BombMaxChargePercent();   // the most the orb can hold (a share of your MAX HP)
			snprintf(out->cost, sizeof(out->cost), "up to %.0f HP  (holds %.0f)", cap * BOMB_HP_COST_SHARE, cap);
			snprintf(out->damage, sizeof(out->damage), "%.1f per HP poured", BombDamagePerHp());
			snprintf(out->extraLabel, sizeof(out->extraLabel), "MAX");
			snprintf(out->extraValue, sizeof(out->extraValue), "%.0f dmg, radius %.0f", cap * BombDamagePerHp(), BombBlastRadius(cap));
			out->cantAfford = (player.hp - MIN_HP_AFTER_SHOT <= LAST_SHOT_MIN_SPARE);   // same check UpdateBombCharge uses
		}
		break;

	case W_LAST_STAND:
		if (cost <= 0.0f) snprintf(out->cost, sizeof(out->cost), "FREE");
		else              snprintf(out->cost, sizeof(out->cost), "%.0f HP per shot", cost);
		snprintf(out->damage, sizeof(out->damage), "%.0f  (max %.0f)", LastStandDamage(), LastStandMaxDamage());
		snprintf(out->extraLabel, sizeof(out->extraLabel), "RATE");
		snprintf(out->extraValue, sizeof(out->extraValue), "%.1f shots/sec", 1.0f / GetFireDelay(w));
		break;

	case W_PISTOL:
	default:
		snprintf(out->cost, sizeof(out->cost), "%.1f HP per shot", cost);
		if (IsEvolved(W_PISTOL)) snprintf(out->damage, sizeof(out->damage), "%.0f x %d bullets", PistolDamage(), PISTOL_EVO_BULLETS);
		else                     snprintf(out->damage, sizeof(out->damage), "%.0f", PistolDamage());
		snprintf(out->extraLabel, sizeof(out->extraLabel), "RATE");
		snprintf(out->extraValue, sizeof(out->extraValue), "%.1f shots/sec", 1.0f / GetFireDelay(w));
		break;
	}
}

// What upgrading gun w to toLevel changes, as up to 3 short lines (for the upgrade cards).
void GetUpgradeText(WeaponType w, int toLevel, char lines[UPGRADE_TEXT_LINES][UPGRADE_TEXT_LEN])
{
	for (int i = 0; i < UPGRADE_TEXT_LINES; ++i) lines[i][0] = '\0';
	if (toLevel < 2 || toLevel > GetMaxWeaponLevel(w)) return;

	int from = toLevel - 2;   // index of the current level in the stat tables
	int to = toLevel - 1;
	bool evolves = (toLevel == GetMaxWeaponLevel(w));

	switch (w)
	{
	case W_PISTOL:
		if (evolves)
		{
			snprintf(lines[0], UPGRADE_TEXT_LEN, "EVOLVES: TWIN FANG");
			snprintf(lines[1], UPGRADE_TEXT_LEN, "%d bullets, %.0f dmg, %.0f HP/shot", PISTOL_EVO_BULLETS, s_pistolDamage[to], s_pistolCost[to]);
			snprintf(lines[2], UPGRADE_TEXT_LEN, "Pierce %d (%.0f%% dmg), %.0f%% life steal", PISTOL_EVO_PIERCE, PISTOL_EVO_PIERCE_FALLOFF * 100.0f, PISTOL_EVO_LIFESTEAL_CHANCE * 100.0f);
		}
		else
		{
			snprintf(lines[0], UPGRADE_TEXT_LEN, "Damage %.0f -> %.0f", s_pistolDamage[from], s_pistolDamage[to]);
			snprintf(lines[1], UPGRADE_TEXT_LEN, "Shots/sec %.1f -> %.1f", 1.0f / s_pistolDelay[from], 1.0f / s_pistolDelay[to]);
		}
		break;
	case W_HEALTH_LASER:
		if (evolves)
		{
			snprintf(lines[0], UPGRADE_TEXT_LEN, "EVOLVES: LIFELINE LANCE");
			snprintf(lines[1], UPGRADE_TEXT_LEN, "Damage = HP x %.2g", s_laserMultiplier[to]);
			snprintf(lines[2], UPGRADE_TEXT_LEN, "+%.0f HP per zombie hit", LASER_EVO_HEAL_PER_HIT);
		}
		else
		{
			snprintf(lines[0], UPGRADE_TEXT_LEN, "Damage = HP x");
			snprintf(lines[1], UPGRADE_TEXT_LEN, "%.2g -> %.2g", s_laserMultiplier[from], s_laserMultiplier[to]);
		}
		break;
	case W_BLOODRANG:
		if (evolves)
		{
			snprintf(lines[0], UPGRADE_TEXT_LEN, "EVOLVES: TWIN RANG");
			snprintf(lines[1], UPGRADE_TEXT_LEN, "Throws %d, %.0f dmg each", RANG_EVO_COUNT, s_rangDamage[to]);
			snprintf(lines[2], UPGRADE_TEXT_LEN, "Boost cap +%.0f%%", RANG_BOOST_PER_STACK * 100.0f * (float)RANG_EVO_MAX_STACKS);
		}
		else
		{
			snprintf(lines[0], UPGRADE_TEXT_LEN, "Damage %.0f -> %.0f", s_rangDamage[from], s_rangDamage[to]);
			snprintf(lines[1], UPGRADE_TEXT_LEN, "Catch refund %.0f%% -> %.0f%%", s_rangRefund[from] * 100.0f, s_rangRefund[to] * 100.0f);
			if (s_rangRadius[to] > s_rangRadius[from]) snprintf(lines[2], UPGRADE_TEXT_LEN, "Bigger boomerang");
		}
		break;
	case W_BLOOD_BOMB:
		if (evolves)
		{
			snprintf(lines[0], UPGRADE_TEXT_LEN, "EVOLVES: BLOOD CLUSTER");
			snprintf(lines[1], UPGRADE_TEXT_LEN, "%.1f dmg/HP, orb holds %.0f%% HP", s_bombDamagePerHp[to], BOMB_EVO_MAX_CHARGE_PERCENT * 100.0f);
			snprintf(lines[2], UPGRADE_TEXT_LEN, "%d minis + pool, 100%% blood rain", BOMB_EVO_MINI_COUNT);
		}
		else
		{
			snprintf(lines[0], UPGRADE_TEXT_LEN, "Damage per HP %.1f -> %.1f", s_bombDamagePerHp[from], s_bombDamagePerHp[to]);
			if (s_bombBloodRain[to] > s_bombBloodRain[from])
				snprintf(lines[1], UPGRADE_TEXT_LEN, "Blood rain %.0f%% -> %.0f%%", s_bombBloodRain[from] * 100.0f, s_bombBloodRain[to] * 100.0f);
		}
		break;
	case W_LAST_STAND:
		if (evolves)
		{
			snprintf(lines[0], UPGRADE_TEXT_LEN, "EVOLVES: DEATH'S DOOR");
			snprintf(lines[1], UPGRADE_TEXT_LEN, "Max damage %.0f", s_lastStandMax[to]);
			snprintf(lines[2], UPGRADE_TEXT_LEN, "Pierce %d zombies (-50%% each)", LAST_STAND_EVO_PIERCE);
		}
		else
		{
			snprintf(lines[0], UPGRADE_TEXT_LEN, "Max damage %.0f -> %.0f", s_lastStandMax[from], s_lastStandMax[to]);
			snprintf(lines[1], UPGRADE_TEXT_LEN, "Min damage %.0f -> %.0f", s_lastStandMin[from], s_lastStandMin[to]);
		}
		break;
	default:
		break;
	}
}

// What a gun you don't own yet does at Lv1, as up to 3 short lines (for the new weapon cards).
void GetNewWeaponText(WeaponType w, char lines[UPGRADE_TEXT_LINES][UPGRADE_TEXT_LEN])
{
	for (int i = 0; i < UPGRADE_TEXT_LINES; ++i) lines[i][0] = '\0';

	switch (w)
	{
	case W_PISTOL:
		snprintf(lines[0], UPGRADE_TEXT_LEN, "Cheap, fast bullets");
		snprintf(lines[1], UPGRADE_TEXT_LEN, "%.0f dmg, %.0f HP per shot", s_pistolDamage[0], s_pistolCost[0]);
		break;
	case W_HEALTH_LASER:
		snprintf(lines[0], UPGRADE_TEXT_LEN, "Fires your health bar");
		snprintf(lines[1], UPGRADE_TEXT_LEN, "Damage = HP x %.2g", s_laserMultiplier[0]);
		snprintf(lines[2], UPGRADE_TEXT_LEN, "Costs %.0f + %.0f%% HP, pierces", LASER_COST_FLAT, LASER_COST_PERCENT * 100.0f);
		break;
	case W_BLOODRANG:
		snprintf(lines[0], UPGRADE_TEXT_LEN, "Blood boomerang, %.0f dmg", s_rangDamage[0]);
		snprintf(lines[1], UPGRADE_TEXT_LEN, "Catch it: %.0f%% HP back", s_rangRefund[0] * 100.0f);
		snprintf(lines[2], UPGRADE_TEXT_LEN, "+ damage boost per hit");
		break;
	case W_BLOOD_BOMB:
		snprintf(lines[0], UPGRADE_TEXT_LEN, "Hold to pour HP in, release");
		snprintf(lines[1], UPGRADE_TEXT_LEN, "Kills drop heals %.0f%% of the time", s_bombBloodRain[0] * 100.0f);
		snprintf(lines[2], UPGRADE_TEXT_LEN, "%.1f damage per HP", s_bombDamagePerHp[0]);
		break;
	case W_LAST_STAND:
		snprintf(lines[0], UPGRADE_TEXT_LEN, "Stronger at low HP");
		snprintf(lines[1], UPGRADE_TEXT_LEN, "%.0f -> %.0f damage", s_lastStandMin[0], s_lastStandMax[0]);
		snprintf(lines[2], UPGRADE_TEXT_LEN, "%.0f HP/shot, free near death", LAST_STAND_COST);
		break;
	default:
		break;
	}
}

float GetBombBloodRainChance(void)
{
	return s_bombBloodRain[LevelIndex(W_BLOOD_BOMB)];
}

float GetMoveSpeedMultiplier(void)
{
	return s_charging ? BOMB_CHARGE_MOVE_SPEED : 1.0f;
}

/* =========================================================
   Math helpers
   ========================================================= */

// Shortest distance from point p to the line segment a-b.
// Works by sliding along a->b to the spot closest to p (t = 0 is a, t = 1 is b).
static float DistancePointToSegment(CP_Vector p, CP_Vector a, CP_Vector b)
{
	CP_Vector ab = CP_Vector_Subtract(b, a);
	CP_Vector ap = CP_Vector_Subtract(p, a);
	float lengthSq = ab.x * ab.x + ab.y * ab.y;

	if (lengthSq < 0.0001f) return CP_Vector_Length(ap);   // the segment is just a point

	float t = (ap.x * ab.x + ap.y * ab.y) / lengthSq;      // how far along a->b the closest spot is
	t = CP_Math_ClampFloat(t, 0.0f, 1.0f);                 // stay on the segment, not the endless line

	CP_Vector closest = CP_Vector_Add(a, CP_Vector_Scale(ab, t));
	return CP_Vector_Distance(p, closest);
}

static bool IsOffScreen(CP_Vector p, float margin)
{
	return p.x < -margin || p.x > windowWidth + margin ||
	       p.y < -margin || p.y > windowHeight + margin;
}

// Damages a zombie, flashes it and shoves it along dir.
// Returns true if that hit killed it (the zombie is then removed and reported).
static bool DamageZombie(int index, float damage, CP_Vector dir, float knockback,
                         ZombieKilledFn onZombieKilled, WeaponType killedBy)
{
	Zombie* zombie = &s_zombies[index];

	if (zombie->type >= Z_BOSS_CHARGER) knockback *= BOSS_KNOCKBACK_SCALE;;   // the boss barely budges

	Sfx_Play(SFX_ZOMBIE_HIT);
	zombie->hp -= damage;
	zombie->hitFlash = ZOMBIE_HIT_FLASH_TIME;
	zombie->pos = CP_Vector_Add(zombie->pos, CP_Vector_Scale(dir, knockback));

	if (zombie->hp > 0.0f) return false;

	zombie->active = false;
	if (onZombieKilled) onZombieKilled(index, killedBy);   // loot drop + bloodlust heal (game.c)
	return true;
}

/* =========================================================
   Bullets: [1] Pistol and [5] Last Stand
   ========================================================= */
static void SpawnBullet(WeaponType type, CP_Vector pos, CP_Vector direction, float speed,
                        float damage, float radius, float knockback, CP_Color color, int pierce)
{
	for (int i = 0; i < MAX_BULLETS; ++i)
	{
		if (s_bullets[i].active) continue;   // slot in use (flying on screen), skip it

		s_bullets[i] = (Bullet){
			.active     = true,
			.type       = type,
			.pos        = pos,
			.vel        = CP_Vector_Scale(direction, speed),
			.life       = (type == W_LAST_STAND) ? LAST_STAND_BULLET_LIFE : PISTOL_BULLET_LIFE,
			.damage     = damage,
			.radius     = radius,
			.knockback  = knockback,
			.color      = color,
			.pierceLeft = pierce,
			.hitCount   = 0
		};
		return;
	}
}

static bool BulletAlreadyHit(const Bullet* b, int zombieId)
{
	for (int h = 0; h < b->hitCount; ++h)
	{
		if (b->hitId[h] == zombieId) return true;
	}
	return false;
}

static void UpdateBullets(float deltaTime, ZombieKilledFn onZombieKilled)
{
	for (int i = 0; i < MAX_BULLETS; ++i)
	{
		Bullet* b = &s_bullets[i];
		if (!b->active) continue;

		// 1. Move (scaled by delta time so speed doesn't depend on frame rate)
		b->pos = CP_Vector_Add(b->pos, CP_Vector_Scale(b->vel, deltaTime));
		b->life -= deltaTime;

		// 2. Despawn if lifetime runs out or it leaves the screen
		if (b->life <= 0.0f || IsOffScreen(b->pos, BULLET_OFFSCREEN_MARGIN))
		{
			b->active = false;
			continue;
		}

		// 3. Hitbox: bullet circle vs zombie circle. The bullet stops at the first zombie it hits,
		//    unless it can still pierce (then it remembers that zombie and keeps going).
		for (int z = 0; z < MAX_ZOMBIES; ++z)
		{
			Zombie* zombie = &s_zombies[z];
			if (!zombie->active) continue;
			if (BulletAlreadyHit(b, zombie->id)) continue;

			if (!AreCirclesIntersecting(b->pos.x, b->pos.y, b->radius,
			                            zombie->pos.x, zombie->pos.y, zombie->radius)) continue;

			if (b->hitCount < MAX_BULLET_HITS) b->hitId[b->hitCount++] = zombie->id;
			DamageZombie(z, b->damage, GetNormalizedDirection(b->vel), b->knockback, onZombieKilled, b->type);

			// Twin Fang: each hit has a small chance to steal life
			if (b->type == W_PISTOL && IsEvolved(W_PISTOL) && CP_Random_RangeFloat(0.0f, 1.0f) < PISTOL_EVO_LIFESTEAL_CHANCE)
				HealPlayer(PISTOL_EVO_LIFESTEAL_HEAL);

			if (b->pierceLeft > 0 && b->hitCount < MAX_BULLET_HITS)
			{
				b->pierceLeft--;
				if (b->type == W_LAST_STAND) b->damage *= LAST_STAND_PIERCE_FALLOFF;   // each zombie it goes through halves the damage
				else if (b->type == W_PISTOL) b->damage *= PISTOL_EVO_PIERCE_FALLOFF;  // Twin Fang: the pierced zombie only takes a quarter
				continue;
			}
			b->active = false;
			break;
		}
	}
}

/* =========================================================
   [2] Health Laser
   ========================================================= */

// Back end of the beam. While the beam is still coming out of the gun it is only as long as it has travelled.
static CP_Vector LaserTail(const Laser* l)
{
	float visibleLength = (l->traveled < l->length) ? l->traveled : l->length;
	return CP_Vector_Subtract(l->head, CP_Vector_Scale(l->dir, visibleLength));
}

static void SpawnLaser(CP_Vector pos, CP_Vector direction)
{
	for (int i = 0; i < MAX_LASERS; ++i)
	{
		if (s_lasers[i].active) continue;

		Laser* l = &s_lasers[i];
		l->active   = true;
		l->head     = pos;
		l->dir      = direction;
		l->traveled = 0.0f;
		// as long as the filled part of the health bar, and the same colour as it
		l->length   = HEALTH_BAR_WIDTH * CP_Math_ClampFloat(player.hp / player.maxHp, 0.0f, 1.0f) * LASER_SIZE_SCALE;
		l->color    = GetHealthBarColor(player.hp, player.maxHp);
		l->damage   = player.hp * LaserMultiplier();   // hits harder the more HP you have
		l->healPerHit = IsEvolved(W_HEALTH_LASER) ? LASER_EVO_HEAL_PER_HIT : 0.0f;
		for (int z = 0; z < MAX_ZOMBIES; ++z) l->hitId[z] = 0;
		return;
	}
}

static void UpdateLasers(float deltaTime, ZombieKilledFn onZombieKilled)
{
	for (int i = 0; i < MAX_LASERS; ++i)
	{
		Laser* l = &s_lasers[i];
		if (!l->active) continue;

		// 1. Move the head forward. Remember where the tail was, so the hitbox covers
		//    everything the beam swept over this frame (a short beam can't skip past a zombie).
		CP_Vector oldTail = LaserTail(l);
		float step = LASER_SPEED * deltaTime;
		l->head = CP_Vector_Add(l->head, CP_Vector_Scale(l->dir, step));
		l->traveled += step;

		// 2. Gone once the back end has left the screen (the whole beam is off screen then)
		if (IsOffScreen(LaserTail(l), LASER_OFFSCREEN_MARGIN))
		{
			l->active = false;
			continue;
		}

		// 3. Hitbox: a thick line from oldTail to head. A zombie is hit when its centre is
		//    closer to that line than (its radius + half the beam width). Pierces: keeps going.
		float halfWidth = LASER_WIDTH * 0.5f;
		for (int z = 0; z < MAX_ZOMBIES; ++z)
		{
			Zombie* zombie = &s_zombies[z];
			if (!zombie->active) continue;
			if (l->hitId[z] == zombie->id) continue;   // already hurt this zombie with this shot

			float dist = DistancePointToSegment(zombie->pos, oldTail, l->head);
			if (dist > zombie->radius + halfWidth) continue;

			l->hitId[z] = zombie->id;
			if (l->healPerHit > 0.0f) HealPlayer(l->healPerHit);   // Lv4: every zombie hit gives HP back
			DamageZombie(z, l->damage, l->dir, LASER_KNOCKBACK, onZombieKilled, W_HEALTH_LASER);
			l->damage *= LASER_PIERCE_FALLOFF;   // each zombie it goes through takes a little less than the one before
		}
	}
}

// Draws a filled rectangle lying along the beam: from tail to head, halfWidth to each side,
// grown by "grow" px on every side (used to draw the black border bigger than the coloured beam).
static void DrawBeamRect(CP_Vector tail, CP_Vector head, CP_Vector dir, float halfWidth, float grow, CP_Color color)
{
	CP_Vector side = CP_Vector_Set(-dir.y, dir.x);                         // 90 degrees from the beam direction
	CP_Vector back = CP_Vector_Subtract(tail, CP_Vector_Scale(dir, grow)); // stretch past both ends
	CP_Vector front = CP_Vector_Add(head, CP_Vector_Scale(dir, grow));
	CP_Vector offset = CP_Vector_Scale(side, halfWidth + grow);

	CP_Vector a = CP_Vector_Add(back, offset);
	CP_Vector b = CP_Vector_Add(front, offset);
	CP_Vector c = CP_Vector_Subtract(front, offset);
	CP_Vector d = CP_Vector_Subtract(back, offset);

	CP_Settings_NoStroke();
	CP_Settings_Fill(color);
	CP_Graphics_DrawQuad(a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);
}

static void DrawLasers(void)
{
	for (int i = 0; i < MAX_LASERS; ++i)
	{
		const Laser* l = &s_lasers[i];
		if (!l->active) continue;

		CP_Vector tail = LaserTail(l);
		float halfWidth = LASER_WIDTH * 0.5f;

		// black border first, then the solid health-bar-coloured beam on top
		DrawBeamRect(tail, l->head, l->dir, halfWidth, LASER_BORDER, Col(0, 0, 0, 255));
		DrawBeamRect(tail, l->head, l->dir, halfWidth, 0.0f, l->color);
		// Lv4: a bright white core so the evolved beam stands out
		if (l->healPerHit > 0.0f) DrawBeamRect(tail, l->head, l->dir, halfWidth * 0.3f, 0.0f, Col(255, 255, 255, 200));
	}
}

/* =========================================================
   [3] Bloodrang
   ========================================================= */
static void ClearRangHits(Bloodrang* r)
{
	for (int z = 0; z < MAX_ZOMBIES; ++z) r->hitId[z] = 0;
}

// Point on the outward curve at t (0 = start, 1 = target): a quadratic Bezier bent toward r->control.
static CP_Vector RangOutPoint(const Bloodrang* r, float t)
{
	float u = 1.0f - t;
	CP_Vector p = CP_Vector_Scale(r->start, u * u);
	p = CP_Vector_Add(p, CP_Vector_Scale(r->control, 2.0f * u * t));
	return CP_Vector_Add(p, CP_Vector_Scale(r->target, t * t));
}

// Throws one boomerang from pos to target. bend = how far (px, + or - for the side) the path bows out
// halfway there; 0 = a straight throw. Either way it arrives exactly at target.
static void ThrowRang(Bloodrang* r, CP_Vector pos, CP_Vector target, float bend)
{
	r->active      = true;
	r->returning   = false;
	r->approaching = false;
	r->pos         = pos;
	// fly to the target, kept inside the screen
	r->target      = CP_Vector_Set(CP_Math_ClampFloat(target.x, RangRadius(), windowWidth - RangRadius()),
	                               CP_Math_ClampFloat(target.y, RangRadius(), windowHeight - RangRadius()));

	// the curve: a Bezier's middle sits halfway between the chord and the control point, so the control
	// point goes out 2x the bend from the midpoint, sideways to the throw
	CP_Vector chord = CP_Vector_Subtract(r->target, pos);
	CP_Vector dir   = GetNormalizedDirection(chord);
	CP_Vector mid   = CP_Vector_Add(pos, CP_Vector_Scale(chord, 0.5f));
	r->start   = pos;
	r->control = CP_Vector_Add(mid, CP_Vector_Scale(CP_Vector_Set(-dir.y, dir.x), 2.0f * bend));
	r->outT    = 0.0f;
	// length estimate: average of the straight line and the control polygon (close enough for speed)
	float chordLen = CP_Vector_Length(chord);
	float polyLen  = CP_Vector_Distance(pos, r->control) + CP_Vector_Distance(r->control, r->target);
	r->outLength   = 0.5f * (chordLen + polyLen);

	r->vel         = CP_Vector_Scale(GetNormalizedDirection(CP_Vector_Subtract(r->control, pos)), RANG_OUT_SPEED);
	r->stored      = RANG_COST;
	r->damage      = RangDamageWithBoost();
	r->turnRate    = RANG_TURN_RATE;
	r->spin        = 0.0f;
	r->life        = RANG_MAX_LIFE;
	r->hits        = 0;
	ClearRangHits(r);
}

// Throws one boomerang at the mouse, or two on mirrored arcs that cross at the mouse at Lv4.
static void ThrowRangs(CP_Vector pos, CP_Vector direction)
{
	(void)direction;   // both throws aim at the mouse itself
	CP_Vector mouse = MousePos();
	if (RangThrowCount() == 1)
	{
		ThrowRang(&s_rangs[0], pos, mouse, 0.0f);
		return;
	}

	// Twin Rang "scissor": two mirrored arcs that bow out to either side and cross exactly at the mouse
	float bend = CP_Vector_Distance(pos, mouse) * RANG_EVO_ARC_SHARE;
	if (bend > RANG_EVO_ARC_WIDTH) bend = RANG_EVO_ARC_WIDTH;
	ThrowRang(&s_rangs[0], pos, mouse,  bend);
	ThrowRang(&s_rangs[1], pos, mouse, -bend);
}

// Caught: get its HP back (30% / 60% / 100% by level), and every zombie it hit becomes a boost stack.
static void CatchRang(Bloodrang* r)
{
	Sfx_Play(SFX_RANG_CATCH);
	HealPlayer(r->stored * RangRefund());
	s_rangStacks += r->hits;
	if (s_rangStacks > RangMaxStacks()) s_rangStacks = RangMaxStacks();
	r->active = false;
}

// Bled out: the HP is gone and so is your boost.
static void LoseRang(Bloodrang* r)
{
	s_rangStacks = 0;
	r->active = false;
}

static void UpdateRang(Bloodrang* r, float deltaTime, ZombieKilledFn onZombieKilled)
{
	if (!r->active) return;

	r->spin += RANG_SPIN_SPEED * deltaTime;
	r->life -= deltaTime;
	if (r->life <= 0.0f) { LoseRang(r); return; }

	if (!r->returning)
	{
		// ---- flying OUT: along its curve (straight for a single throw) to the target ----
		float stepT = (r->outLength > 1.0f) ? RANG_OUT_SPEED * deltaTime / r->outLength : 1.0f;
		r->outT += stepT;

		if (r->outT >= 1.0f)
		{
			// reached it: start coming back. It keeps the direction the curve ends on, so it swings round
			CP_Vector endDir = GetNormalizedDirection(CP_Vector_Subtract(r->target, r->control));
			r->vel = CP_Vector_Scale(endDir, RANG_OUT_SPEED);
			r->pos = r->target;
			r->returning = true;
			r->approaching = false;
			ClearRangHits(r);       // new trip, so zombies hit on the way out can be hit again on the way back
		}
		else
		{
			CP_Vector next = RangOutPoint(r, r->outT);
			if (deltaTime > 0.0f)
				r->vel = CP_Vector_Scale(CP_Vector_Subtract(next, r->pos), 1.0f / deltaTime);   // keeps the heading right for the hand-off
			r->pos = next;
		}
	}
	else
	{
		// ---- flying BACK: turn toward the player, but only up to turnRate radians per second ----
		CP_Vector toPlayer = CP_Vector_Subtract(player.position, r->pos);
		float heading = atan2f(r->vel.y, r->vel.x);          // the angle it is flying at now
		float wanted  = atan2f(toPlayer.y, toPlayer.x);      // the angle that points at the player
		float turn    = wanted - heading;
		while (turn >  PI_F) turn -= 2.0f * PI_F;           // take the short way round
		while (turn < -PI_F) turn += 2.0f * PI_F;
		float maxTurn = r->turnRate * deltaTime;
		turn = CP_Math_ClampFloat(turn, -maxTurn, maxTurn);
		heading += turn;

		r->vel = CP_Vector_Set(cosf(heading) * RANG_RETURN_SPEED, sinf(heading) * RANG_RETURN_SPEED);
		r->pos = CP_Vector_Add(r->pos, CP_Vector_Scale(r->vel, deltaTime));

		// caught?
		if (CP_Vector_Distance(r->pos, player.position) <= PLAYER_RADIUS + RangRadius())
		{
			CatchRang(r);
			return;
		}

		// missed a lap? It was flying toward you last frame and now it's flying away = it overshot.
		toPlayer = CP_Vector_Subtract(player.position, r->pos);
		bool approachingNow = (r->vel.x * toPlayer.x + r->vel.y * toPlayer.y) > 0.0f;
		if (r->approaching && !approachingNow)
		{
			r->stored   -= RANG_LAP_DRAIN;     // bleeds a little each lap
			r->turnRate *= RANG_TURN_GROWTH;   // and homes in harder, so it can't orbit you forever
			ClearRangHits(r);                  // new lap = new trip
			if (r->stored <= 0.0f) { LoseRang(r); return; }
		}
		r->approaching = approachingNow;
	}

	// hitbox: circle vs every zombie, each zombie once per trip
	for (int z = 0; z < MAX_ZOMBIES; ++z)
	{
		Zombie* zombie = &s_zombies[z];
		if (!zombie->active) continue;
		if (r->hitId[z] == zombie->id) continue;

		if (!AreCirclesIntersecting(r->pos.x, r->pos.y, RangRadius(),
		                            zombie->pos.x, zombie->pos.y, zombie->radius)) continue;

		r->hitId[z] = zombie->id;
		r->hits++;
		DamageZombie(z, r->damage, GetNormalizedDirection(r->vel), RANG_KNOCKBACK, onZombieKilled, W_BLOODRANG);
	}
}

static void DrawRang(const Bloodrang* r)
{
	if (!r->active) return;

	// a spinning blood "X": black outline first, then the red blades. It fades as it bleeds out.
	int alpha = (int)(120.0f + 135.0f * CP_Math_ClampFloat(r->stored / RANG_COST, 0.0f, 1.0f));
	CP_Color color = Col(BLOODRANG_RGB, alpha);
	for (int blade = 0; blade < 2; ++blade)
	{
		float angle = r->spin + (float)blade * (PI_F * 0.5f);
		CP_Vector arm = CP_Vector_Set(cosf(angle) * RangRadius(), sinf(angle) * RangRadius());
		CP_Vector a = CP_Vector_Subtract(r->pos, arm);
		CP_Vector b = CP_Vector_Add(r->pos, arm);
		DrawColoredLine(a.x, a.y, b.x, b.y, 9.0f, Col(0, 0, 0, alpha));
		DrawColoredLine(a.x, a.y, b.x, b.y, 5.0f, color);
	}
	G_Circle(r->pos.x, r->pos.y, 4.0f, Col(255, 180, 180, alpha));
}

/* =========================================================
   [4] Blood Bomb
   ========================================================= */
static void SpawnBlast(CP_Vector pos, float radius)
{
	for (int i = 0; i < MAX_BLASTS; ++i)
	{
		if (s_blasts[i].active) continue;
		s_blasts[i] = (Blast){ .active = true, .pos = pos, .radius = radius, .life = BOMB_BLAST_SHOW_TIME };
		return;
	}
}

// Lv4: leaves a blood pool where a big bomb went off (the oldest one is replaced if all are in use).
static void SpawnPool(CP_Vector pos, float blastRadius, float blastDamage)
{
	int slot = 0;
	for (int i = 0; i < MAX_POOLS; ++i)
	{
		if (!s_pools[i].active) { slot = i; break; }
		if (s_pools[i].life < s_pools[slot].life) slot = i;
	}
	s_pools[slot] = (BloodPool){
		.active        = true,
		.pos           = pos,
		.radius        = blastRadius * BOMB_EVO_POOL_SIZE,
		.life          = BOMB_EVO_POOL_TIME,
		.tickTimer     = BOMB_EVO_POOL_TICK,
		.damagePerTick = blastDamage * BOMB_EVO_POOL_DPS_SHARE * BOMB_EVO_POOL_TICK
	};
}

static void UpdatePools(float deltaTime, ZombieKilledFn onZombieKilled)
{
	for (int i = 0; i < MAX_POOLS; ++i)
	{
		BloodPool* p = &s_pools[i];
		if (!p->active) continue;

		p->life -= deltaTime;
		if (p->life <= 0.0f) { p->active = false; continue; }

		p->tickTimer -= deltaTime;
		if (p->tickTimer > 0.0f) continue;
		p->tickTimer += BOMB_EVO_POOL_TICK;

		// pulse: every zombie standing in it takes damage (no knockback, so they stay in the pool)
		for (int z = 0; z < MAX_ZOMBIES; ++z)
		{
			Zombie* zombie = &s_zombies[z];
			if (!zombie->active) continue;
			if (CP_Vector_Distance(zombie->pos, p->pos) > p->radius + zombie->radius) continue;
			DamageZombie(z, p->damagePerTick, CP_Vector_Set(0.0f, 0.0f), 0.0f, onZombieKilled, W_BLOOD_BOMB);
		}
	}
}

static void DrawPools(void)
{
	for (int i = 0; i < MAX_POOLS; ++i)
	{
		const BloodPool* p = &s_pools[i];
		if (!p->active) continue;
		float t = p->life / BOMB_EVO_POOL_TIME;   // 1 -> 0, fades out
		G_Circle(p->pos.x, p->pos.y, p->radius, Col(120, 10, 40, (int)(150.0f * t)));
		G_Ring(p->pos.x, p->pos.y, p->radius, 3.0f, Col(BLOOD_BOMB_RGB, (int)(200.0f * t)));
	}
}

// Puts a bomb in the air. Returns false if every slot is full.
static bool SpawnBomb(CP_Vector pos, CP_Vector vel, CP_Vector target, float stored, float damagePerHp, bool mini)
{
	for (int i = 0; i < MAX_BOMBS; ++i)
	{
		if (s_bombs[i].active) continue;
		s_bombs[i] = (BloodBomb){
			.active      = true,
			.mini        = mini,
			.pos         = pos,
			.vel         = vel,
			.target      = target,
			.stored      = stored,
			.damagePerHp = damagePerHp
		};
		return true;
	}
	return false;
}

static void ThrowBomb(CP_Vector pos, CP_Vector direction, CP_Vector target, float stored)
{
	if (!SpawnBomb(pos, CP_Vector_Scale(direction, BOMB_THROW_SPEED), target, stored, BombDamagePerHp(), false))
	{
		HealPlayer(stored);   // no free slot (very unlikely): give the HP back instead of losing it
	}
}

// Every zombie inside the blast takes the damage and is thrown away from the centre.
// Lv4: a big bomb also throws out mini bombs in a ring that explode around it.
// directHit = slot of the zombie the orb burst on (it takes BOMB_DIRECT_HIT_BONUS), or -1 if it went off on its own.
static void ExplodeBomb(BloodBomb* bomb, ZombieKilledFn onZombieKilled, int directHit)
{
	BloodBomb b = *bomb;   // copy: the slot is freed first so the mini bombs can reuse it
	bomb->active = false;

	float radius = BombBlastRadius(b.stored);
	float damage = b.stored * b.damagePerHp;

	Sfx_Play(SFX_BOMB_EXPLOSION);

	for (int z = 0; z < MAX_ZOMBIES; ++z)
	{
		Zombie* zombie = &s_zombies[z];
		if (!zombie->active) continue;

		CP_Vector away = CP_Vector_Subtract(zombie->pos, b.pos);
		if (CP_Vector_Length(away) > radius + zombie->radius) continue;

		float hit = (z == directHit) ? damage * BOMB_DIRECT_HIT_BONUS : damage;   // the zombie it burst on takes extra
		DamageZombie(z, hit, GetNormalizedDirection(away), BOMB_KNOCKBACK, onZombieKilled, W_BLOOD_BOMB);
	}

	SpawnBlast(b.pos, radius);

	if (!b.mini && IsEvolved(W_BLOOD_BOMB))
	{
		SpawnPool(b.pos, radius, damage);

		float miniStored = b.stored * BOMB_EVO_MINI_SHARE;
		float startAngle = CP_Random_RangeFloat(0.0f, 2.0f * PI_F);
		for (int m = 0; m < BOMB_EVO_MINI_COUNT; ++m)
		{
			float angle = startAngle + (float)m * (2.0f * PI_F / (float)BOMB_EVO_MINI_COUNT);
			CP_Vector dir = CP_Vector_Set(cosf(angle), sinf(angle));
			CP_Vector target = CP_Vector_Add(b.pos, CP_Vector_Scale(dir, radius * BOMB_EVO_MINI_DISTANCE));
			SpawnBomb(b.pos, CP_Vector_Scale(dir, BOMB_EVO_MINI_SPEED), target, miniStored, b.damagePerHp * BOMB_EVO_MINI_DAMAGE_SCALE, true);
		}
	}
}

static void UpdateBombs(float deltaTime, ZombieKilledFn onZombieKilled)
{
	for (int i = 0; i < MAX_BOMBS; ++i)
	{
		BloodBomb* bomb = &s_bombs[i];
		if (!bomb->active) continue;

		// reached the spot you aimed at -> explode there
		float step = CP_Vector_Length(bomb->vel) * deltaTime;
		if (CP_Vector_Distance(bomb->pos, bomb->target) <= step)
		{
			bomb->pos = bomb->target;
			ExplodeBomb(bomb, onZombieKilled, -1);
			continue;
		}

		bomb->pos = CP_Vector_Add(bomb->pos, CP_Vector_Scale(bomb->vel, deltaTime));
		if (IsOffScreen(bomb->pos, BOMB_OFFSCREEN_MARGIN))
		{
			ExplodeBomb(bomb, onZombieKilled, -1);
			continue;
		}

		// touched a zombie -> explode on it (mini bombs start inside the crowd, so they only explode where they land)
		if (bomb->mini) continue;
		float orbRadius = BombOrbRadius(bomb->stored);
		for (int z = 0; z < MAX_ZOMBIES; ++z)
		{
			Zombie* zombie = &s_zombies[z];
			if (!zombie->active) continue;
			if (!AreCirclesIntersecting(bomb->pos.x, bomb->pos.y, orbRadius,
			                            zombie->pos.x, zombie->pos.y, zombie->radius)) continue;
			ExplodeBomb(bomb, onZombieKilled, z);   // burst on this zombie: it takes the direct-hit bonus
			break;
		}
	}

	for (int i = 0; i < MAX_BLASTS; ++i)
	{
		if (!s_blasts[i].active) continue;
		s_blasts[i].life -= deltaTime;
		if (s_blasts[i].life <= 0.0f) s_blasts[i].active = false;
	}

	UpdatePools(deltaTime, onZombieKilled);
}

static void DrawBombOrb(CP_Vector pos, float stored)
{
	float r = BombOrbRadius(stored);
	G_Circle(pos.x, pos.y, r + 3.0f, Col(0, 0, 0, 255));
	G_Circle(pos.x, pos.y, r, Col(BLOOD_BOMB_RGB, 255));
	G_Circle(pos.x - r * 0.3f, pos.y - r * 0.3f, r * 0.3f, Col(255, 170, 200, 255));   // shine
}

static void DrawBombs(void)
{
	DrawPools();   // under the blasts and orbs

	// blasts: a quick flash that grows to full size and fades out
	for (int i = 0; i < MAX_BLASTS; ++i)
	{
		const Blast* b = &s_blasts[i];
		if (!b->active) continue;
		float t = b->life / BOMB_BLAST_SHOW_TIME;          // 1 -> 0
		float r = b->radius * (1.0f - 0.4f * t);           // grows from 60% to 100%
		G_Circle(b->pos.x, b->pos.y, r, Col(BLOOD_BOMB_RGB, (int)(110.0f * t)));
		G_Ring(b->pos.x, b->pos.y, r, 4.0f, Col(0, 0, 0, (int)(255.0f * t)));
	}

	for (int i = 0; i < MAX_BOMBS; ++i)
	{
		if (s_bombs[i].active) DrawBombOrb(s_bombs[i].pos, s_bombs[i].stored);
	}

	// the orb you are charging sits on the gun tip
	if (s_charging) DrawBombOrb(GunTip(), s_chargeStored);
}

// Stop charging (switched guns, wave ended...): pour the HP back into the player.
void CancelCharging(void)
{
	if (!s_charging) return;
	HealPlayer(s_chargeStored * BOMB_HP_COST_SHARE);   // give back what you actually paid
	s_charging = false;
	s_chargeStored = 0.0f;
	Sfx_StopBombCharge();
}

// Hold = drain HP into the orb (up to the cap). Release = throw it at the mouse.
static void UpdateBombCharge(float deltaTime, bool held)
{
	if (held && !s_charging)
	{
		if (s_fireTimer > 0.0f || IsLaserActive()) return;
		if (player.hp - MIN_HP_AFTER_SHOT <= LAST_SHOT_MIN_SPARE)   // nothing left to give
		{
			player.noFireFlash = 0.35f;
			s_fireTimer = 0.2f;
			return;
		}
		s_charging = true;
		s_chargeStored = 0.0f;
		s_chargeCap = player.maxHp * BombMaxChargePercent();   // share of MAX HP; pouring still stops at 1 HP
		// rising charge-up, stretched to last until this orb is full (cut off on release / full / 1 HP)
		float canPour = (player.hp - MIN_HP_AFTER_SHOT) / BOMB_HP_COST_SHARE;   // the most you can pour before 1 HP
		float fullOrb = (s_chargeCap < canPour) ? s_chargeCap : canPour;
		Sfx_PlayBombCharge(fullOrb / BOMB_CHARGE_RATE);
	}
	if (!s_charging) return;

	if (held)
	{
		// "pour" is what goes INTO the orb; you only pay BOMB_HP_COST_SHARE of it
		float pour = BOMB_CHARGE_RATE * deltaTime;
		if (pour > s_chargeCap - s_chargeStored) pour = s_chargeCap - s_chargeStored;   // orb is full
		float maxPour = (player.hp - MIN_HP_AFTER_SHOT) / BOMB_HP_COST_SHARE;
		if (pour > maxPour) pour = maxPour;                                            // never below 1 HP
		if (pour < 0.0f) pour = 0.0f;

		player.hp -= pour * BOMB_HP_COST_SHARE;
		s_chargeStored += pour;
		player.combatTimer = 0.0f;

		// orb full (or no HP left to give): the charge-up sound stops instead of playing on
		if (pour <= 0.0f) Sfx_StopBombCharge();
	}
	else
	{
		Sfx_StopBombCharge();
		ThrowBomb(GunTip(), player.aimDirection, MousePos(), s_chargeStored);
		s_charging = false;
		s_chargeStored = 0.0f;
		s_fireTimer = BOMB_FIRE_DELAY;
	}
}

/* =========================================================
   Public
   ========================================================= */
bool IsLaserActive(void)
{
	for (int i = 0; i < MAX_LASERS; ++i)
	{
		if (s_lasers[i].active) return true;
	}
	return false;
}

void ResetWeapons(void)
{
	for (int i = 0; i < MAX_BULLETS; ++i) s_bullets[i].active = false;
	for (int i = 0; i < MAX_LASERS; ++i)  s_lasers[i].active = false;
	for (int i = 0; i < MAX_BOMBS; ++i)   s_bombs[i].active = false;
	for (int i = 0; i < MAX_BLASTS; ++i)  s_blasts[i].active = false;
	for (int i = 0; i < MAX_POOLS; ++i)   s_pools[i].active = false;
	for (int i = 0; i < MAX_RANGS; ++i)   s_rangs[i].active = false;
	s_rangStacks = 0;
	s_charging = false;
	s_chargeStored = 0.0f;
	s_fireTimer = START_FIRE_DELAY;
	s_waitForRelease = false;
	Sfx_StopBombCharge();
}

// Pistol shot: one bullet, or a piercing 3-bullet spread at Lv5 (Twin Fang).
static void FirePistol(CP_Vector gunTip, CP_Vector direction)
{
	CP_Color color = Col(PISTOL_RGB, 255);
	if (!IsEvolved(W_PISTOL))
	{
		SpawnBullet(W_PISTOL, gunTip, direction, PISTOL_BULLET_SPEED, PistolDamage(),
			PISTOL_BULLET_RADIUS, PISTOL_KNOCKBACK, color, 0);
		return;
	}

	float firstAngle = -PISTOL_EVO_SPREAD * (float)(PISTOL_EVO_BULLETS - 1) * 0.5f;
	for (int i = 0; i < PISTOL_EVO_BULLETS; ++i)
	{
		CP_Vector dir = RotateDirection(direction, firstAngle + PISTOL_EVO_SPREAD * (float)i);
		SpawnBullet(W_PISTOL, gunTip, dir, PISTOL_BULLET_SPEED, PistolDamage(),
			PISTOL_BULLET_RADIUS, PISTOL_KNOCKBACK, color, PISTOL_EVO_PIERCE);
	}
}

// Spawns the shot for a "click to fire" gun. Called BEFORE the HP cost is taken (the laser uses your HP).
static void FireWeapon(WeaponType w, CP_Vector gunTip, CP_Vector direction)
{
	switch (w)
	{
	case W_HEALTH_LASER: SpawnLaser(gunTip, direction); Sfx_Play(SFX_HEALTH_LASER); break;
	case W_BLOODRANG:    ThrowRangs(gunTip, direction); Sfx_Play(SFX_RANG_THROW);   break;
	case W_LAST_STAND:
		SpawnBullet(W_LAST_STAND, gunTip, direction, LAST_STAND_BULLET_SPEED, LastStandDamage(),
			LastStandRadius(), LAST_STAND_KNOCKBACK, LastStandColor(255),
			IsEvolved(W_LAST_STAND) ? LAST_STAND_EVO_PIERCE : 0);
		Sfx_Play(SFX_LAST_STAND);
		break;
	case W_PISTOL:
	default:
		FirePistol(gunTip, direction);
		Sfx_Play(SFX_PISTOL);
		break;
	}
}

void BlockFireUntilRelease(void)
{
	if (s_charging) CancelCharging();   // refund a half-charged bomb instead of throwing it on release
	s_waitForRelease = true;
}

void HandleShooting(float deltaTime)
{
	s_fireTimer -= deltaTime;
	bool held = CP_Input_MouseDown(MOUSE_BUTTON_LEFT);

	// the click that picked an upgrade card is still held: don't let it fire the gun too
	if (s_waitForRelease)
	{
		if (held) return;
		s_waitForRelease = false;
	}

	// switched away from the bomb mid-charge: refund it
	if (s_charging && player.weapon != W_BLOOD_BOMB) CancelCharging();

	// the blood bomb charges while held and throws on release, so it works differently
	if (player.weapon == W_BLOOD_BOMB)
	{
		UpdateBombCharge(deltaTime, held);
		return;
	}

	if (!held || s_fireTimer > 0.0f) return;
	if (IsLaserActive()) return;                                 // nothing fires until the laser has left the screen
	if (player.weapon == W_BLOODRANG && AnyRangActive()) return; // one throw at a time

	float cost = GetShotCost(player.weapon);
	if (!CanAffordShot(cost))                   // already at 1 HP: you can't shoot yourself to death
	{
		player.noFireFlash = 0.35f;             // flash the health bar red
		s_fireTimer = 0.2f;                     // try again shortly
		return;
	}

	// Not enough for the full cost? It still fires one last time and takes you down to 1 HP.
	float spare = player.hp - MIN_HP_AFTER_SHOT;
	float paid  = (cost > spare) ? spare : cost;

	FireWeapon(player.weapon, GunTip(), player.aimDirection);   // fire FIRST: the laser's damage and length use your HP before the cost
	if (player.weapon == W_BLOODRANG && paid < cost)
	{
		// the boomerang only holds the HP you actually paid, so catching it can't give back more than that
		for (int i = 0; i < MAX_RANGS; ++i)
			if (s_rangs[i].active) s_rangs[i].stored *= paid / cost;
	}
	player.hp -= paid;                                          // every shot costs health
	s_fireTimer = GetFireDelay(player.weapon);
	player.combatTimer = 0.0f;                                  // shooting resets the out-of-combat timer
}

void UpdateShots(float deltaTime, ZombieKilledFn onZombieKilled)
{
	UpdateBullets(deltaTime, onZombieKilled);
	UpdateLasers(deltaTime, onZombieKilled);
	for (int i = 0; i < MAX_RANGS; ++i) UpdateRang(&s_rangs[i], deltaTime, onZombieKilled);
	UpdateBombs(deltaTime, onZombieKilled);
}

void DrawShots(void)
{
	for (int i = 0; i < MAX_BULLETS; ++i)
	{
		const Bullet* b = &s_bullets[i];
		if (!b->active) continue;
		G_Circle(b->pos.x, b->pos.y, b->radius, b->color);
	}
	DrawLasers();
	DrawBombs();
	for (int i = 0; i < MAX_RANGS; ++i) DrawRang(&s_rangs[i]);
}
