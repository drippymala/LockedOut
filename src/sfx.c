//---------------------------------------------------------
// file:	sfx.c
// author:	LockedOut (Sherilyn, Joshua, Sean, Syahmi)
//
// brief:	Loads and plays the game's sound effects. Every sound
//			has its own volume and a minimum gap between plays, so
//			a laser or bomb hitting 10 zombies in one frame doesn't
//			play 10 hit sounds stacked on top of each other.
//
// Copyright © 2026 DigiPen, All rights reserved.
//---------------------------------------------------------

#include "cprocessing.h"
#include "sfx.h"

#define SFX_GROUP          CP_SOUND_GROUP_SFX   // one-shot effects
#define SFX_CHARGE_GROUP   CP_SOUND_GROUP_2     // the bomb charge-up sits alone in its own group so it can be stopped by itself
#define MUSIC_GROUP        CP_SOUND_GROUP_3     // in-game music (the menu music uses group 0)
#define MUSIC_GAME_VOLUME  0.30f                // normal waves: a bit louder than the menu music (0.10)
#define MUSIC_BOSS_VOLUME  0.10f                // the Tyrant fight

// In-game music tracks. They're big mp3s, so each is loaded the first time it's needed and then kept
// (like the menu music), so starting a new game doesn't stall while it decodes again.
static const char* s_musicPath[MUSIC_COUNT] =
{
	[MUSIC_GAME] = "Assets/Sounds/Game Music.mp3",
	[MUSIC_BOSS] = "Assets/Sounds/Boss Music.mp3",
};
static CP_Sound s_music[MUSIC_COUNT];
static MusicId  s_musicPlaying = MUSIC_NONE;

void Music_Play(MusicId id)
{
	if (id == s_musicPlaying) return;   // already on: don't restart it
	Music_Stop();
	if (id <= MUSIC_NONE || id >= MUSIC_COUNT) return;

	if (s_music[id] == NULL) s_music[id] = CP_Sound_LoadMusic(s_musicPath[id]);
	if (s_music[id] == NULL) return;    // file missing: just no music

	float volume = (id == MUSIC_GAME) ? MUSIC_GAME_VOLUME : MUSIC_BOSS_VOLUME;
	CP_Sound_PlayAdvanced(s_music[id], volume, 1.0f, 1 /* loop */, MUSIC_GROUP);
	s_musicPlaying = id;
}

void Music_Stop(void)
{
	CP_Sound_StopGroup(MUSIC_GROUP);
	CP_Sound_ResumeGroup(MUSIC_GROUP);   // in case it was stopped while paused
	s_musicPlaying = MUSIC_NONE;
}

typedef struct
{
	const char* path;
	float volume;     // 0..1
	float minGap;     // seconds: ignore a new play request sooner than this after the last one
} SfxInfo;

static const SfxInfo s_info[SFX_COUNT] =
{
	[SFX_PISTOL]         = { "Assets/Sounds/pistol_shot.wav",          0.07f, 0.00f },
	[SFX_HEALTH_LASER]   = { "Assets/Sounds/health_laser.wav",         0.10f, 0.00f },
	[SFX_LAST_STAND]     = { "Assets/Sounds/last_stand_shot.wav",      0.085f, 0.00f },
	[SFX_RANG_THROW]     = { "Assets/Sounds/bloodrang_throw.wav",      0.07f, 0.00f },
	[SFX_RANG_CATCH]     = { "Assets/Sounds/bloodrang_catch.wav",      0.10f, 0.00f },
	[SFX_BOMB_CHARGE]    = { "Assets/Sounds/blood_bomb_charge.wav",    0.12f, 0.00f },
	[SFX_BOMB_CHARGE_LONG] = { "Assets/Sounds/blood_bomb_charge_long.wav", 0.12f, 0.00f },
	[SFX_BOMB_CHARGE_XLONG] = { "Assets/Sounds/blood_bomb_charge_xlong.wav", 0.12f, 0.00f },
	[SFX_BOMB_EXPLOSION] = { "Assets/Sounds/blood_bomb_explosion.wav", 0.15f, 0.05f },
	[SFX_ZOMBIE_HIT]     = { "Assets/Sounds/zombie_hit.wav",           0.75f, 0.04f },
	[SFX_PLAYER_HURT]    = { "Assets/Sounds/player_hurt.wav",          0.72f, 0.10f },
};

static CP_Sound s_sounds[SFX_COUNT];
static float    s_lastPlayed[SFX_COUNT];   // CP_System_GetSeconds() when each sound last started

void Sfx_Load(void)
{
	for (int i = 0; i < SFX_COUNT; ++i)
	{
		if (s_sounds[i] == NULL) s_sounds[i] = CP_Sound_Load(s_info[i].path);
		s_lastPlayed[i] = -1000.0f;
	}
}

void Sfx_Free(void)
{
	CP_Sound_StopGroup(SFX_GROUP);
	CP_Sound_StopGroup(SFX_CHARGE_GROUP);
	CP_Sound_ResumeGroup(SFX_GROUP);          // quitting from the pause menu leaves the groups paused,
	CP_Sound_ResumeGroup(SFX_CHARGE_GROUP);   // which would silence every sound next game
	Music_Stop();                             // leaving the game: the in-game music stops (the tracks stay loaded)
	for (int i = 0; i < SFX_COUNT; ++i)
	{
		if (s_sounds[i] != NULL) CP_Sound_Free(s_sounds[i]);
		s_sounds[i] = NULL;
	}
}

void Sfx_Play(SfxId id)
{
	if (id < 0 || id >= SFX_COUNT || s_sounds[id] == NULL) return;

	float now = CP_System_GetSeconds();
	if (now - s_lastPlayed[id] < s_info[id].minGap) return;   // too soon after the last one
	s_lastPlayed[id] = now;

	CP_SOUND_GROUP group = (id == SFX_BOMB_CHARGE || id == SFX_BOMB_CHARGE_LONG || id == SFX_BOMB_CHARGE_XLONG) ? SFX_CHARGE_GROUP : SFX_GROUP;
	CP_Sound_PlayAdvanced(s_sounds[id], s_info[id].volume, 1.0f, 0 /* no loop */, group);
}

// Three versions of the same bfxr charge-up, each rising to the same top note over a different time.
// The shortest one that lasts the whole charge is used, so the rise always fits the orb:
//   blood_bomb_charge.wav        2.38s   (every Lv1-3 orb at normal HP)
//   blood_bomb_charge_long.wav   3.91s   (big Lv1-3 orbs, Lv4 up to about 195 max HP)
//   blood_bomb_charge_xlong.wav  5.41s   (Lv4 at up to 272 max HP)
// All play at normal speed and are cut off when the orb stops charging.
#define BOMB_CHARGE_SOUND_LENGTH       2.38f
#define BOMB_CHARGE_LONG_SOUND_LENGTH  3.91f

void Sfx_PlayBombCharge(float chargeSeconds)
{
	SfxId id = SFX_BOMB_CHARGE;
	if (chargeSeconds > BOMB_CHARGE_LONG_SOUND_LENGTH) id = SFX_BOMB_CHARGE_XLONG;
	else if (chargeSeconds > BOMB_CHARGE_SOUND_LENGTH) id = SFX_BOMB_CHARGE_LONG;
	if (s_sounds[id] == NULL) id = SFX_BOMB_CHARGE;   // file missing: fall back to the short one
	if (s_sounds[id] == NULL) return;

	CP_Sound_PlayAdvanced(s_sounds[id], s_info[id].volume, 1.0f, 0 /* no loop */, SFX_CHARGE_GROUP);
}

void Sfx_StopBombCharge(void)
{
	CP_Sound_StopGroup(SFX_CHARGE_GROUP);
}

void Sfx_SetPaused(bool paused)
{
	if (paused)
	{
		CP_Sound_PauseGroup(SFX_GROUP);
		CP_Sound_PauseGroup(SFX_CHARGE_GROUP);
		CP_Sound_PauseGroup(MUSIC_GROUP);
	}
	else
	{
		CP_Sound_ResumeGroup(SFX_GROUP);
		CP_Sound_ResumeGroup(SFX_CHARGE_GROUP);
		CP_Sound_ResumeGroup(MUSIC_GROUP);
	}
}
