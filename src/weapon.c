//---------------------------------------------------------
// file:	weapon.c
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	How the guns work: paying HP to shoot, fire delay,
//			spawning, moving, hit-testing and drawing every shot
//			(pistol / last stand bullets, health laser beams,
//			the bloodrang and blood bombs). All the numbers live
//			in weapon.h.
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

static Bullet    s_bullets[MAX_BULLETS];
static Laser     s_lasers[MAX_LASERS];
static Bloodrang s_rang;                  // only one boomerang can be out at a time
static int       s_rangStacks = 0;        // damage boost stacks earned by catching the bloodrang
static BloodBomb s_bombs[MAX_BOMBS];
static Blast     s_blasts[MAX_BLASTS];
static float     s_fireTimer = 0.0f;      // seconds until the gun can fire again

// blood bomb being charged on the gun tip (not thrown yet)
static bool  s_charging = false;
static float s_chargeStored = 0.0f;       // HP poured in so far
static float s_chargeCap = 0.0f;          // the most it can hold (40% of HP when charging started)

/* =========================================================
   Small helpers
   ========================================================= */

// 0 at full HP, 1 at no HP. Used by Last Stand.
static float MissingHpFraction(void)
{
	return 1.0f - CP_Math_ClampFloat(player.hp / player.maxHp, 0.0f, 1.0f);
}

static float LastStandDamage(void)
{
	return LAST_STAND_MIN_DAMAGE + (LAST_STAND_MAX_DAMAGE - LAST_STAND_MIN_DAMAGE) * MissingHpFraction();
}

// small at full HP -> big near death
static float LastStandRadius(void)
{
	return LAST_STAND_MIN_RADIUS + (LAST_STAND_MAX_RADIUS - LAST_STAND_MIN_RADIUS) * MissingHpFraction();
}

// pale pink at full HP -> deep blood red near death
static CP_Color LastStandColor(int alpha)
{
	float t = MissingHpFraction();
	return Col((int)(255.0f - 55.0f * t), (int)(190.0f - 190.0f * t), (int)(170.0f - 150.0f * t), alpha);
}

static float RangDamageWithBoost(void)
{
	return RANG_DAMAGE * (1.0f + RANG_BOOST_PER_STACK * (float)s_rangStacks);
}

static float BombDamage(float stored) { return stored * BOMB_DAMAGE_PER_HP; }
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

/* =========================================================
   Gun info
   ========================================================= */

// How much HP the gun would cost to fire right now.
float GetShotCost(WeaponType w)
{
	switch (w)
	{
	case W_PISTOL:       return PISTOL_COST;
	case W_HEALTH_LASER: return player.hp * LASER_COST_PERCENT;   // a quarter of whatever you have now
	case W_BLOODRANG:    return RANG_COST;
	case W_BLOOD_BOMB:   return 0.0f;                             // paid while charging instead
	case W_LAST_STAND:   return LAST_STAND_COST;
	default:             return 1.0f;
	}
}

float GetFireDelay(WeaponType w)
{
	switch (w)
	{
	case W_PISTOL:       return PISTOL_FIRE_DELAY;
	case W_HEALTH_LASER: return LASER_FIRE_DELAY;
	case W_BLOODRANG:    return RANG_FIRE_DELAY;
	case W_BLOOD_BOMB:   return BOMB_FIRE_DELAY;
	case W_LAST_STAND:   return LAST_STAND_FIRE_DELAY;
	default:             return PISTOL_FIRE_DELAY;
	}
}

const char* GetWeaponName(WeaponType w)
{
	switch (w)
	{
	case W_PISTOL:       return "PISTOL";
	case W_HEALTH_LASER: return "HEALTH LASER";
	case W_BLOODRANG:    return "BLOODRANG";
	case W_BLOOD_BOMB:   return "BLOOD BOMB";
	case W_LAST_STAND:   return "LAST STAND";
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
	switch (w)
	{
	case W_HEALTH_LASER:
		snprintf(buf, bufSize, "[2] HEALTH LASER: damage = HP x %.1f, length = your health bar, costs %.0f%% HP  -  costs %.1f HP, deals %.0f now",
			LASER_DAMAGE_MULTIPLIER, LASER_COST_PERCENT * 100.0f, GetShotCost(w), player.hp * LASER_DAMAGE_MULTIPLIER);
		break;
	case W_BLOODRANG:
		if (s_rang.active)
			snprintf(buf, bufSize, "[3] BLOODRANG: catch it!  holds %.0f HP, hit %d zombie(s)  -  boost +%.0f%%",
				s_rang.stored, s_rang.hits, RANG_BOOST_PER_STACK * 100.0f * (float)s_rangStacks);
		else
			snprintf(buf, bufSize, "[3] BLOODRANG: costs %.0f HP, catch it to get it back  -  boost +%.0f%% (%d/%d), deals %.0f",
				RANG_COST, RANG_BOOST_PER_STACK * 100.0f * (float)s_rangStacks, s_rangStacks, RANG_MAX_STACKS, RangDamageWithBoost());
		break;
	case W_BLOOD_BOMB:
		if (s_charging)
			snprintf(buf, bufSize, "[4] BLOOD BOMB: charging %.0f / %.0f HP  -  blast deals %.0f", s_chargeStored, s_chargeCap, BombDamage(s_chargeStored));
		else
			snprintf(buf, bufSize, "[4] BLOOD BOMB: hold to pour HP in (up to %.0f%%), release to throw  -  %.1f damage per HP",
				BOMB_MAX_CHARGE_PERCENT * 100.0f, BOMB_DAMAGE_PER_HP);
		break;
	case W_LAST_STAND:
		snprintf(buf, bufSize, "[5] LAST STAND: stronger the lower your HP  -  costs %.0f HP, deals %.0f now", LAST_STAND_COST, LastStandDamage());
		break;
	case W_PISTOL:
	default:
		snprintf(buf, bufSize, "[1] PISTOL: flat cost per shot  -  costs %.1f HP now", GetShotCost(w));
		break;
	}
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
                        float damage, float radius, float knockback, CP_Color color)
{
	for (int i = 0; i < MAX_BULLETS; ++i)
	{
		if (s_bullets[i].active) continue;   // slot in use (flying on screen), skip it

		s_bullets[i] = (Bullet){
			.active    = true,
			.type      = type,
			.pos       = pos,
			.vel       = CP_Vector_Scale(direction, speed),
			.life      = PISTOL_BULLET_LIFE,
			.damage    = damage,
			.radius    = radius,
			.knockback = knockback,
			.color     = color
		};
		return;
	}
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

		// 3. Hitbox: bullet circle vs zombie circle. The bullet stops at the first zombie it hits.
		for (int z = 0; z < MAX_ZOMBIES; ++z)
		{
			Zombie* zombie = &s_zombies[z];
			if (!zombie->active) continue;

			if (!AreCirclesIntersecting(b->pos.x, b->pos.y, b->radius,
			                            zombie->pos.x, zombie->pos.y, zombie->radius)) continue;

			DamageZombie(z, b->damage, GetNormalizedDirection(b->vel), b->knockback, onZombieKilled, b->type);
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
		l->damage   = player.hp * LASER_DAMAGE_MULTIPLIER;   // hits harder the more HP you have
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
			DamageZombie(z, l->damage, l->dir, LASER_KNOCKBACK, onZombieKilled, W_HEALTH_LASER);
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
	}
}

/* =========================================================
   [3] Bloodrang
   ========================================================= */
static void ClearRangHits(void)
{
	for (int z = 0; z < MAX_ZOMBIES; ++z) s_rang.hitId[z] = 0;
}

static void ThrowRang(CP_Vector pos, CP_Vector direction)
{
	s_rang.active      = true;
	s_rang.returning   = false;
	s_rang.approaching = false;
	s_rang.pos         = pos;
	s_rang.vel         = CP_Vector_Scale(direction, RANG_OUT_SPEED);
	// fly to the mouse, kept inside the screen
	CP_Vector mouse    = MousePos();
	s_rang.target      = CP_Vector_Set(CP_Math_ClampFloat(mouse.x, RANG_RADIUS, windowWidth - RANG_RADIUS),
	                                   CP_Math_ClampFloat(mouse.y, RANG_RADIUS, windowHeight - RANG_RADIUS));
	s_rang.stored      = RANG_COST;
	s_rang.damage      = RangDamageWithBoost();
	s_rang.turnRate    = RANG_TURN_RATE;
	s_rang.spin        = 0.0f;
	s_rang.life        = RANG_MAX_LIFE;
	s_rang.hits        = 0;
	ClearRangHits();
}

// Caught: get its HP back, and every zombie it hit becomes a boost stack.
static void CatchRang(void)
{
	Sfx_Play(SFX_RANG_CATCH);
	HealPlayer(s_rang.stored);
	s_rangStacks += s_rang.hits;
	if (s_rangStacks > RANG_MAX_STACKS) s_rangStacks = RANG_MAX_STACKS;
	s_rang.active = false;
}

// Bled out: the HP is gone and so is your boost.
static void LoseRang(void)
{
	s_rangStacks = 0;
	s_rang.active = false;
}

static void UpdateRang(float deltaTime, ZombieKilledFn onZombieKilled)
{
	Bloodrang* r = &s_rang;
	if (!r->active) return;

	r->spin += RANG_SPIN_SPEED * deltaTime;
	r->life -= deltaTime;
	if (r->life <= 0.0f) { LoseRang(); return; }

	if (!r->returning)
	{
		// ---- flying OUT: straight to the target ----
		CP_Vector toTarget = CP_Vector_Subtract(r->target, r->pos);
		float dist = CP_Vector_Length(toTarget);
		float step = RANG_OUT_SPEED * deltaTime;

		if (dist <= step)
		{
			r->pos = r->target;     // reached it: start coming back (it keeps its outward velocity, so it swings round in a curve)
			r->returning = true;
			r->approaching = false;
			ClearRangHits();        // new trip, so zombies hit on the way out can be hit again on the way back
		}
		else
		{
			r->vel = CP_Vector_Scale(CP_Vector_Scale(toTarget, 1.0f / dist), RANG_OUT_SPEED);
			r->pos = CP_Vector_Add(r->pos, CP_Vector_Scale(r->vel, deltaTime));
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
		if (CP_Vector_Distance(r->pos, player.position) <= PLAYER_RADIUS + RANG_RADIUS)
		{
			CatchRang();
			return;
		}

		// missed a lap? It was flying toward you last frame and now it's flying away = it overshot.
		toPlayer = CP_Vector_Subtract(player.position, r->pos);
		bool approachingNow = (r->vel.x * toPlayer.x + r->vel.y * toPlayer.y) > 0.0f;
		if (r->approaching && !approachingNow)
		{
			r->stored   -= RANG_LAP_DRAIN;     // bleeds a little each lap
			r->turnRate *= RANG_TURN_GROWTH;   // and homes in harder, so it can't orbit you forever
			ClearRangHits();                   // new lap = new trip
			if (r->stored <= 0.0f) { LoseRang(); return; }
		}
		r->approaching = approachingNow;
	}

	// hitbox: circle vs every zombie, each zombie once per trip
	for (int z = 0; z < MAX_ZOMBIES; ++z)
	{
		Zombie* zombie = &s_zombies[z];
		if (!zombie->active) continue;
		if (r->hitId[z] == zombie->id) continue;

		if (!AreCirclesIntersecting(r->pos.x, r->pos.y, RANG_RADIUS,
		                            zombie->pos.x, zombie->pos.y, zombie->radius)) continue;

		r->hitId[z] = zombie->id;
		r->hits++;
		DamageZombie(z, r->damage, GetNormalizedDirection(r->vel), RANG_KNOCKBACK, onZombieKilled, W_BLOODRANG);
	}
}

static void DrawRang(void)
{
	if (!s_rang.active) return;

	// a spinning blood "X": black outline first, then the red blades. It fades as it bleeds out.
	int alpha = (int)(120.0f + 135.0f * CP_Math_ClampFloat(s_rang.stored / RANG_COST, 0.0f, 1.0f));
	CP_Color color = Col(BLOODRANG_RGB, alpha);
	for (int blade = 0; blade < 2; ++blade)
	{
		float angle = s_rang.spin + (float)blade * (PI_F * 0.5f);
		CP_Vector arm = CP_Vector_Set(cosf(angle) * RANG_RADIUS, sinf(angle) * RANG_RADIUS);
		CP_Vector a = CP_Vector_Subtract(s_rang.pos, arm);
		CP_Vector b = CP_Vector_Add(s_rang.pos, arm);
		DrawColoredLine(a.x, a.y, b.x, b.y, 9.0f, Col(0, 0, 0, alpha));
		DrawColoredLine(a.x, a.y, b.x, b.y, 5.0f, color);
	}
	G_Circle(s_rang.pos.x, s_rang.pos.y, 4.0f, Col(255, 180, 180, alpha));
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

static void ThrowBomb(CP_Vector pos, CP_Vector direction, CP_Vector target, float stored)
{
	for (int i = 0; i < MAX_BOMBS; ++i)
	{
		if (s_bombs[i].active) continue;
		s_bombs[i] = (BloodBomb){
			.active = true,
			.pos    = pos,
			.vel    = CP_Vector_Scale(direction, BOMB_THROW_SPEED),
			.target = target,
			.stored = stored
		};
		return;
	}
	HealPlayer(stored);   // no free slot (very unlikely): give the HP back instead of losing it
}

// Every zombie inside the blast takes the damage and is thrown away from the centre.
static void ExplodeBomb(BloodBomb* bomb, ZombieKilledFn onZombieKilled)
{
	float radius = BombBlastRadius(bomb->stored);
	float damage = BombDamage(bomb->stored);

	Sfx_Play(SFX_BOMB_EXPLOSION);

	for (int z = 0; z < MAX_ZOMBIES; ++z)
	{
		Zombie* zombie = &s_zombies[z];
		if (!zombie->active) continue;

		CP_Vector away = CP_Vector_Subtract(zombie->pos, bomb->pos);
		if (CP_Vector_Length(away) > radius + zombie->radius) continue;

		DamageZombie(z, damage, GetNormalizedDirection(away), BOMB_KNOCKBACK, onZombieKilled, W_BLOOD_BOMB);
	}

	SpawnBlast(bomb->pos, radius);
	bomb->active = false;
}

static void UpdateBombs(float deltaTime, ZombieKilledFn onZombieKilled)
{
	for (int i = 0; i < MAX_BOMBS; ++i)
	{
		BloodBomb* bomb = &s_bombs[i];
		if (!bomb->active) continue;

		// reached the spot you aimed at -> explode there
		float step = BOMB_THROW_SPEED * deltaTime;
		if (CP_Vector_Distance(bomb->pos, bomb->target) <= step)
		{
			bomb->pos = bomb->target;
			ExplodeBomb(bomb, onZombieKilled);
			continue;
		}

		bomb->pos = CP_Vector_Add(bomb->pos, CP_Vector_Scale(bomb->vel, deltaTime));
		if (IsOffScreen(bomb->pos, BOMB_OFFSCREEN_MARGIN))
		{
			ExplodeBomb(bomb, onZombieKilled);
			continue;
		}

		// touched a zombie -> explode on it
		float orbRadius = BombOrbRadius(bomb->stored);
		for (int z = 0; z < MAX_ZOMBIES; ++z)
		{
			Zombie* zombie = &s_zombies[z];
			if (!zombie->active) continue;
			if (!AreCirclesIntersecting(bomb->pos.x, bomb->pos.y, orbRadius,
			                            zombie->pos.x, zombie->pos.y, zombie->radius)) continue;
			ExplodeBomb(bomb, onZombieKilled);
			break;
		}
	}

	for (int i = 0; i < MAX_BLASTS; ++i)
	{
		if (!s_blasts[i].active) continue;
		s_blasts[i].life -= deltaTime;
		if (s_blasts[i].life <= 0.0f) s_blasts[i].active = false;
	}
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

// Switched guns while charging: pour the HP back into the player.
static void CancelBombCharge(void)
{
	HealPlayer(s_chargeStored);
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
		if (player.hp - MIN_HP_AFTER_SHOT < 0.5f)   // nothing left to give
		{
			player.noFireFlash = 0.35f;
			s_fireTimer = 0.2f;
			return;
		}
		s_charging = true;
		s_chargeStored = 0.0f;
		s_chargeCap = player.hp * BOMB_MAX_CHARGE_PERCENT;
		Sfx_Play(SFX_BOMB_CHARGE);   // rising charge-up, cut off on release
	}
	if (!s_charging) return;

	if (held)
	{
		float pour = BOMB_CHARGE_RATE * deltaTime;
		if (pour > s_chargeCap - s_chargeStored) pour = s_chargeCap - s_chargeStored;   // orb is full
		if (pour > player.hp - MIN_HP_AFTER_SHOT) pour = player.hp - MIN_HP_AFTER_SHOT; // never below 1 HP
		if (pour < 0.0f) pour = 0.0f;

		player.hp -= pour;
		s_chargeStored += pour;
		player.combatTimer = 0.0f;
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
	s_rang.active = false;
	s_rangStacks = 0;
	s_charging = false;
	s_chargeStored = 0.0f;
	s_fireTimer = START_FIRE_DELAY;
	Sfx_StopBombCharge();
}

// Spawns the shot for a "click to fire" gun. Called BEFORE the HP cost is taken (the laser uses your HP).
static void FireWeapon(WeaponType w, CP_Vector gunTip, CP_Vector direction)
{
	switch (w)
	{
	case W_HEALTH_LASER: SpawnLaser(gunTip, direction); Sfx_Play(SFX_HEALTH_LASER); break;
	case W_BLOODRANG:    ThrowRang(gunTip, direction);  Sfx_Play(SFX_RANG_THROW);   break;
	case W_LAST_STAND:
		SpawnBullet(W_LAST_STAND, gunTip, direction, LAST_STAND_BULLET_SPEED, LastStandDamage(),
			LastStandRadius(), LAST_STAND_KNOCKBACK, LastStandColor(255));
		Sfx_Play(SFX_LAST_STAND);
		break;
	case W_PISTOL:
	default:
		SpawnBullet(W_PISTOL, gunTip, direction, PISTOL_BULLET_SPEED, PISTOL_BULLET_DAMAGE,
			PISTOL_BULLET_RADIUS, PISTOL_KNOCKBACK, Col(PISTOL_RGB, 255));
		Sfx_Play(SFX_PISTOL);
		break;
	}
}

void HandleShooting(float deltaTime)
{
	s_fireTimer -= deltaTime;
	bool held = CP_Input_MouseDown(MOUSE_BUTTON_LEFT);

	// switched away from the bomb mid-charge: refund it
	if (s_charging && player.weapon != W_BLOOD_BOMB) CancelBombCharge();

	// the blood bomb charges while held and throws on release, so it works differently
	if (player.weapon == W_BLOOD_BOMB)
	{
		UpdateBombCharge(deltaTime, held);
		return;
	}

	if (!held || s_fireTimer > 0.0f) return;
	if (IsLaserActive()) return;                                 // nothing fires until the laser has left the screen
	if (player.weapon == W_BLOODRANG && s_rang.active) return;   // one bloodrang at a time

	float cost = GetShotCost(player.weapon);
	if (player.hp - cost < MIN_HP_AFTER_SHOT)   // you can't shoot yourself to death
	{
		player.noFireFlash = 0.35f;             // flash the health bar red
		s_fireTimer = 0.2f;                     // try again shortly
		return;
	}

	FireWeapon(player.weapon, GunTip(), player.aimDirection);   // fire FIRST: the laser's damage and length use your HP before the cost
	player.hp -= cost;                                          // every shot costs health
	s_fireTimer = GetFireDelay(player.weapon);
	player.combatTimer = 0.0f;                                  // shooting resets the out-of-combat timer
}

void UpdateShots(float deltaTime, ZombieKilledFn onZombieKilled)
{
	UpdateBullets(deltaTime, onZombieKilled);
	UpdateLasers(deltaTime, onZombieKilled);
	UpdateRang(deltaTime, onZombieKilled);
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
	DrawRang();
}
