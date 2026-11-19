//---------------------------------------------------------
// file:	player.h
// author:	Sean
//
// brief:	Declarations for the player code: WASD movement,
//			mouse aim, gun switching, player sprite, crosshair
//			and player hitbox. (Bullet / laser hitboxes are in weapon.c.)
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#pragma once
#include "game.h"     // Player type, player / window globals
#include "zombie.h"   // Zombie type and s_zombies

/* WASD Movement */
void MovePlayer(float deltaTime);

/* Mouse Aim */
void AimPlayerAtMouse(void);
void DrawMouseCrosshair(void);

/* Gun Switching */
void HandleGunSwitch(void);

/* Player Sprite */
void DrawPlayerSprite(void);

/* Player Hitbox */
bool ZombieTouchingPlayer(const Zombie* z);
void PushZombiesOutOfPlayer(void);   // zombie vs player push (one pass)
