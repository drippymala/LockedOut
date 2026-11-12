//---------------------------------------------------------
// file:	player.c
// author:	Sean
//
// brief:	Player code: WASD movement, mouse aim, gun switching,
//			player sprite, crosshair, bullet hitbox and player hitbox.
//			Uses the shared player / s_zombies globals from game.h.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#include "cprocessing.h"
#include "utils.h"
#include "game.h"
#include "zombie.h"
#include "player.h"

#define PLAYER_SPEED               290.0f  // px per second
#define GUN_BARREL_THICKNESS       9.0f
#define PLAYER_HIGHLIGHT_OFFSET    4.0f    // offset of the shine spot on the player's body
#define CROSSHAIR_RING_RADIUS      11.0f
#define CROSSHAIR_TICK_START       14.0f
#define CROSSHAIR_TICK_END         20.0f
#define CROSSHAIR_THICKNESS        2.0f
#define PISTOL_KNOCKBACK           4.0f    // px a zombie is shoved back when hit
#define ZOMBIE_HIT_FLASH_TIME      0.08f   // white flash when a bullet hits a zombie

/* =========================================================
   WASD Movement
   ========================================================= */
void MovePlayer(float deltaTime)
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

/* =========================================================
   Mouse Aim
   ========================================================= */
// Point the gun at the mouse.
void AimPlayerAtMouse(void)
{
	CP_Vector mouse = CP_Vector_Set(CP_Input_GetMouseX(), CP_Input_GetMouseY());
	CP_Vector toMouse = CP_Vector_Subtract(mouse, player.position); // Vector from player to mouse (mouse - player)

	// if the mouse is exactly on the player there is no direction, so keep the last aim	
	if (CP_Vector_Length(toMouse) > 0.0001f)
	{
		player.aimDirection = CP_Vector_Normalize(toMouse);
	}
}

// Ring with four tick marks around the mouse position, in the pistol colour.
void DrawMouseCrosshair(void)
{
	float mouseX = CP_Input_GetMouseX();
	float mouseY = CP_Input_GetMouseY();
	CP_Color color = Col(PISTOL_RGB, 230);

	float in  = CROSSHAIR_TICK_START;   // each tick starts this far from the centre
	float out = CROSSHAIR_TICK_END;     // and ends this far

	DrawCircleOutline(mouseX, mouseY, CROSSHAIR_RING_RADIUS, CROSSHAIR_THICKNESS, color);
	DrawColoredLine(mouseX - out, mouseY, mouseX - in, mouseY, CROSSHAIR_THICKNESS, color);   // left
	DrawColoredLine(mouseX + in, mouseY, mouseX + out, mouseY, CROSSHAIR_THICKNESS, color);   // right
	DrawColoredLine(mouseX, mouseY - out, mouseX, mouseY - in, CROSSHAIR_THICKNESS, color);   // up
	DrawColoredLine(mouseX, mouseY + in, mouseX, mouseY + out, CROSSHAIR_THICKNESS, color);   // down
}

/* =========================================================
   Gun Switching
   ========================================================= */
void HandleGunSwitch(void)
{
	//for gun switching in the future, currently only one gun
	if (CP_Input_KeyTriggered(KEY_1) && player.owned[W_PISTOL]) player.weapon = W_PISTOL;
}

/* =========================================================
   Player Sprite
   ========================================================= */
// Player body + gun barrel.
void DrawPlayerSprite(void)
{
	int alpha = 255;

	CP_Color gunColor       = Col(PISTOL_RGB, alpha);
	CP_Color bodyColor      = Col(60, 130, 220, alpha);
	CP_Color highlightColor = Col(120, 185, 255, alpha);

	// the barrel starts at the player's centre and points toward the mouse
	CP_Vector gunTip = CP_Vector_Add(player.position, CP_Vector_Scale(player.aimDirection, PLAYER_RADIUS + GUN_BARREL_LENGTH));

	DrawColoredLine(player.position.x, player.position.y, gunTip.x, gunTip.y, GUN_BARREL_THICKNESS, gunColor);
	DrawFilledCircle(player.position.x, player.position.y, PLAYER_RADIUS, bodyColor);
	DrawFilledCircle(player.position.x - PLAYER_HIGHLIGHT_OFFSET, player.position.y - PLAYER_HIGHLIGHT_OFFSET,
		PLAYER_RADIUS * 0.45f, highlightColor);
}

/* =========================================================
   Bullet Hitbox
   ========================================================= */
// Checks one bullet against every zombie: do the bullet circle and a zombie circle overlap?
// On a hit: damages the zombie, flashes it, knocks it back, removes the bullet,
// and returns that zombie's index so game.c can handle the kill. Returns -1 if nothing was hit.
int BulletHitZombie(Bullet* b)
{
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

		// Bullet disappears on hit, so stop checking other zombies
		b->active = false;
		return z;
	}
	return -1;
}

/* =========================================================
   Player Hitbox
   ========================================================= */
// True when this zombie's circle is touching the player's circle.
bool ZombieTouchingPlayer(const Zombie* z)
{
	float hitDist = PLAYER_RADIUS + z->radius;
	float dist = CP_Vector_Distance(player.position, z->pos);
	return dist <= hitDist;
}

// Zombie vs player: pushes any zombie that overlaps the player back out to the player's edge.
// shareA = 1.0, so the player never moves and the zombie takes the whole push (the "shove").
void PushZombiesOutOfPlayer(void)
{
	for (int i = 0; i < MAX_ZOMBIES; ++i)
	{
		if (!s_zombies[i].active) continue;
		PushApart(&s_zombies[i].pos, s_zombies[i].radius, &player.position, PLAYER_RADIUS, 1.0f);
	}
}
