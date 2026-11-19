//---------------------------------------------------------
// file:	player.h
// author:	Sean
//
// brief:	Declarations for the player code: WASD movement,
//			mouse aim, gun switching, player sprite, crosshair,
//			bullet hitbox and player hitbox.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#pragma once
#include "game.h"     // Player and Bullet types, player / bullet / window globals
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

/* Bullet Hitbox: returns the index of the zombie the bullet hit, or -1 if it hit nothing */
int BulletHitZombie(Bullet* b);

/* Player Hitbox */
bool ZombieTouchingPlayer(const Zombie* z);
void PushZombiesOutOfPlayer(void);   // zombie vs player push (one pass)
