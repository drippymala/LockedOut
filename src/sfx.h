//---------------------------------------------------------
// file:	sfx.h
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	Sound effects: loads every WAV in Assets/Sounds once
//			and plays them by name (gun shots, hits, explosion...).
//			Made with Bfxr; the settings are in Assets/Sounds/bfxr_params.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#pragma once

#include <stdbool.h>

typedef enum
{
	SFX_PISTOL,
	SFX_HEALTH_LASER,
	SFX_LAST_STAND,
	SFX_RANG_THROW,
	SFX_RANG_CATCH,
	SFX_BOMB_CHARGE,     // plays while the blood bomb charges; stop it with Sfx_StopBombCharge
	SFX_BOMB_EXPLOSION,
	SFX_ZOMBIE_HIT,
	SFX_PLAYER_HURT,
	SFX_COUNT
} SfxId;

void Sfx_Load(void);              // load every sound (safe to call again: already-loaded sounds are skipped)
void Sfx_Free(void);              // stop and free every sound
void Sfx_Play(SfxId id);          // play once
void Sfx_StopBombCharge(void);    // cut the charge-up sound (thrown, cancelled or game reset)
void Sfx_SetPaused(bool paused);  // pause / resume every playing sound effect (pause menu)
